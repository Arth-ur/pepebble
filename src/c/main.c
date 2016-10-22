/*---------------------------------------------------------------------------
Template for TP of the course "System Engineering" 2016, EPFL

Authors: Flavien Bardyn & Martin Savary
Version: 1.0
Date: 10.08.2016

Use this "HelloWorld" example as basis to code your own app, which should at least 
count steps precisely based on accelerometer data. 

- Add the accelerometer data acquisition
- Implement your own pedometer using these data
- (Add an estimation of the distance travelled)

- Make an effort on the design of the app, try to do something fun!
- Comment and indent your code properly!
- Try to use your imagination and not google (we already did it, and it's disappointing!)
  to offer us a smart and original solution of pedometer

Don't hesitate to ask us questions.
Good luck and have fun!
---------------------------------------------------------------------------*/

// Include Pebble library
#include <pebble.h>
#include "src/c/kiss_fft.h"
#include "src/c/data_processor.h"


#define ACCEL_SAMPLING_25HZ 25
#define NBSAMPLE 100
#define M       4
#define N       (1<<M)


/*----------------------------------------------FFT Library----------------------------------------------*/
#define dosFIX_MPY(DEST,A,B)       {       \
        _DX = (B);                      \
        _AX = (A);                      \
        asm imul dx;                    \
        asm add ax,ax;                  \
        asm adc dx,dx;                  \
        DEST = _DX;             }

#define FIX_MPY(DEST,A,B)       DEST = ((long)(A) * (long)(B))>>15

#define N_WAVE          1024    /* dimension of Sinewave[] */
#define LOG2_N_WAVE     10      /* log2(N_WAVE) */
#define N_LOUD          100     /* dimension of Loudampl[] */
#ifndef fixed
#define fixed short
#endif

extern fixed Sinewave[N_WAVE]; /* placed at end of this file for clarity */
extern fixed Loudampl[N_LOUD];
int db_from_ampl(fixed re, fixed im);
fixed fix_mpy(fixed a, fixed b);

/*
        fix_fft() - perform fast Fourier transform.

        if n>0 FFT is done, if n<0 inverse FFT is done
        fr[n],fi[n] are real,imaginary arrays, INPUT AND RESULT.
        size of data = 2**m
        set inverse to 0=dft, 1=idft
*/
int fix_fft(fixed fr[], fixed fi[], int m, int inverse)
{
        int mr,nn,i,j,l,k,istep, n, scale, shift;
        fixed qr,qi,tr,ti,wr,wi,t;

                n = 1<<m;

        if(n > N_WAVE)
                return -1;

        mr = 0;
        nn = n - 1;
        scale = 0;

        /* decimation in time - re-order data */
        for(m=1; m<=nn; ++m) {
                l = n;
                do {
                        l >>= 1;
                } while(mr+l > nn);
                mr = (mr & (l-1)) + l;

                if(mr <= m) continue;
                tr = fr[m];
                fr[m] = fr[mr];
                fr[mr] = tr;
                ti = fi[m];
                fi[m] = fi[mr];
                fi[mr] = ti;
        }

        l = 1;
        k = LOG2_N_WAVE-1;
        while(l < n) {
                if(inverse) {
                        /* variable scaling, depending upon data */
                        shift = 0;
                        for(i=0; i<n; ++i) {
                                j = fr[i];
                                if(j < 0)
                                        j = -j;
                                m = fi[i];
                                if(m < 0)
                                        m = -m;
                                if(j > 16383 || m > 16383) {
                                        shift = 1;
                                        break;
                                }
                        }
                        if(shift)
                                ++scale;
                } else {
                        /* fixed scaling, for proper normalization -
                           there will be log2(n) passes, so this
                           results in an overall factor of 1/n,
                           distributed to maximize arithmetic accuracy. */
                        shift = 1;
                }
                /* it may not be obvious, but the shift will be performed
                   on each data point exactly once, during this pass. */
                istep = l << 1;
                for(m=0; m<l; ++m) {
                        j = m << k;
                        /* 0 <= j < N_WAVE/2 */
                        wr =  Sinewave[j+N_WAVE/4];
                        wi = -Sinewave[j];
                        if(inverse)
                                wi = -wi;
                        if(shift) {
                                wr >>= 1;
                                wi >>= 1;
                        }
                        for(i=m; i<n; i+=istep) {
                                j = i + l;
                                        tr = fix_mpy(wr,fr[j]) -
fix_mpy(wi,fi[j]);
                                        ti = fix_mpy(wr,fi[j]) +
fix_mpy(wi,fr[j]);
                                qr = fr[i];
                                qi = fi[i];
                                if(shift) {
                                        qr >>= 1;
                                        qi >>= 1;
                                }
                                fr[j] = qr - tr;
                                fi[j] = qi - ti;
                                fr[i] = qr + tr;
                                fi[i] = qi + ti;
                        }
                }
                --k;
                l = istep;
        }

        return scale;
}


/*      window() - apply a Hanning window       */
void window(fixed fr[], int n)
{
        int i,j,k;

        j = N_WAVE/n;
        n >>= 1;
        for(i=0,k=N_WAVE/4; i<n; ++i,k+=j)
                FIX_MPY(fr[i],fr[i],16384-(Sinewave[k]>>1));
        n <<= 1;
        for(k-=j; i<n; ++i,k-=j)
                FIX_MPY(fr[i],fr[i],16384-(Sinewave[k]>>1));
}

/*      fix_loud() - compute loudness of freq-spectrum components.
        n should be ntot/2, where ntot was passed to fix_fft();
        6 dB is added to account for the omitted alias components.
        scale_shift should be the result of fix_fft(), if the time-series
        was obtained from an inverse FFT, 0 otherwise.
        loud[] is the loudness, in dB wrt 32767; will be +10 to -N_LOUD.
*/
void fix_loud(fixed loud[], fixed fr[], fixed fi[], int n, int scale_shift)
{
        int i, max;

        max = 0;
        if(scale_shift > 0)
                max = 10;
        scale_shift = (scale_shift+1) * 6;

        for(i=0; i<n; ++i) {
                loud[i] = db_from_ampl(fr[i],fi[i]) + scale_shift;
                if(loud[i] > max)
                        loud[i] = max;
        }
}

/*      db_from_ampl() - find loudness (in dB) from
        the complex amplitude.
*/
int db_from_ampl(fixed re, fixed im)
{
        static long loud2[N_LOUD] = {0};
        long v;
        int i;

        if(loud2[0] == 0) {
                loud2[0] = (long)Loudampl[0] * (long)Loudampl[0];
                for(i=1; i<N_LOUD; ++i) {
                        v = (long)Loudampl[i] * (long)Loudampl[i];
                        loud2[i] = v;
                        loud2[i-1] = (loud2[i-1]+v) / 2;
                }
        }

        v = (long)re * (long)re + (long)im * (long)im;

        for(i=0; i<N_LOUD; ++i)
                if(loud2[i] <= v)
                        break;

        return (-i);
}

/*
        fix_mpy() - fixed-point multiplication
*/
fixed fix_mpy(fixed a, fixed b)
{
        FIX_MPY(a,a,b);
        return a;
}

/*
        iscale() - scale an integer value by (numer/denom)
*/
int iscale(int value, int numer, int denom)
{
#ifdef  DOS
        asm     mov ax,value
        asm     imul WORD PTR numer
        asm     idiv WORD PTR denom

        return _AX;
#else
                return (long) value * (long)numer/(long)denom;
#endif
}

/*
        fix_dot() - dot product of two fixed arrays
*/
fixed fix_dot(fixed *hpa, fixed *pb, int n)
{
        fixed *pa;
        pa=NULL;
        long sum;
        register fixed a,b;
        unsigned int seg,off;
    

/*      seg = FP_SEG(hpa);
        off = FP_OFF(hpa);
        seg += off>>4;
        off &= 0x000F;
        pa = MK_FP(seg,off);
 */
        sum = 0L;
        while(n--) {
                a = *pa++;
                b = *pb++;
                FIX_MPY(a,a,b);
                sum += a;
        }

        if(sum > 0x7FFF)
                sum = 0x7FFF;
        else if(sum < -0x7FFF)
                sum = -0x7FFF;

        return (fixed)sum;
#ifdef  DOS
        /* ASSUMES hpa is already normalized so FP_OFF(hpa) < 16 */
        asm     push    ds
        asm     lds     si,hpa
        asm     les     di,pb
        asm     xor     bx,bx

        asm     xor     cx,cx

loop:   /* intermediate values can overflow by a factor of 2 without
           causing an error; the final value must not overflow! */
        asm     lodsw
.
        asm     imul    word ptr es:[di]
        asm     add     bx,ax
        asm     adc     cx,dx
        asm     jo      overflow
        asm     add     di,2
        asm     dec     word ptr n
        asm     jg      loop

        asm     add     bx,bx
        asm     adc     cx,cx
        asm     jo      overflow

        asm     pop     ds
        return _CX;

overflow:
        asm     mov     cx,7FFFH
        asm     adc     cx,0

        asm     pop     ds
        return _CX;
#endif

}


#if N_WAVE != 1024
        ERROR: N_WAVE != 1024
#endif
fixed Sinewave[1024] = {
      0,    201,    402,    603,    804,   1005,   1206,   1406,
   1607,   1808,   2009,   2209,   2410,   2610,   2811,   3011,
   3211,   3411,   3611,   3811,   4011,   4210,   4409,   4608,
   4807,   5006,   5205,   5403,   5601,   5799,   5997,   6195,
   6392,   6589,   6786,   6982,   7179,   7375,   7571,   7766,
   7961,   8156,   8351,   8545,   8739,   8932,   9126,   9319,
   9511,   9703,   9895,  10087,  10278,  10469,  10659,  10849,
  11038,  11227,  11416,  11604,  11792,  11980,  12166,  12353,
  12539,  12724,  12909,  13094,  13278,  13462,  13645,  13827,
  14009,  14191,  14372,  14552,  14732,  14911,  15090,  15268,
  15446,  15623,  15799,  15975,  16150,  16325,  16499,  16672,
  16845,  17017,  17189,  17360,  17530,  17699,  17868,  18036,
  18204,  18371,  18537,  18702,  18867,  19031,  19194,  19357,
  19519,  19680,  19840,  20000,  20159,  20317,  20474,  20631,
  20787,  20942,  21096,  21249,  21402,  21554,  21705,  21855,
  22004,  22153,  22301,  22448,  22594,  22739,  22883,  23027,
  23169,  23311,  23452,  23592,  23731,  23869,  24006,  24143,
  24278,  24413,  24546,  24679,  24811,  24942,  25072,  25201,
  25329,  25456,  25582,  25707,  25831,  25954,  26077,  26198,
  26318,  26437,  26556,  26673,  26789,  26905,  27019,  27132,
  27244,  27355,  27466,  27575,  27683,  27790,  27896,  28001,
  28105,  28208,  28309,  28410,  28510,  28608,  28706,  28802,
  28897,  28992,  29085,  29177,  29268,  29358,  29446,  29534,
  29621,  29706,  29790,  29873,  29955,  30036,  30116,  30195,
  30272,  30349,  30424,  30498,  30571,  30643,  30713,  30783,
  30851,  30918,  30984,  31049,
  31113,  31175,  31236,  31297,
  31356,  31413,  31470,  31525,  31580,  31633,  31684,  31735,
  31785,  31833,  31880,  31926,  31970,  32014,  32056,  32097,
  32137,  32176,  32213,  32249,  32284,  32318,  32350,  32382,
  32412,  32441,  32468,  32495,  32520,  32544,  32567,  32588,
  32609,  32628,  32646,  32662,  32678,  32692,  32705,  32717,
  32727,  32736,  32744,  32751,  32757,  32761,  32764,  32766,
  32767,  32766,  32764,  32761,  32757,  32751,  32744,  32736,
  32727,  32717,  32705,  32692,  32678,  32662,  32646,  32628,
  32609,  32588,  32567,  32544,  32520,  32495,  32468,  32441,
  32412,  32382,  32350,  32318,  32284,  32249,  32213,  32176,
  32137,  32097,  32056,  32014,  31970,  31926,  31880,  31833,
  31785,  31735,  31684,  31633,  31580,  31525,  31470,  31413,
  31356,  31297,  31236,  31175,  31113,  31049,  30984,  30918,
  30851,  30783,  30713,  30643,  30571,  30498,  30424,  30349,
  30272,  30195,  30116,  30036,  29955,  29873,  29790,  29706,
  29621,  29534,  29446,  29358,  29268,  29177,  29085,  28992,
  28897,  28802,  28706,  28608,  28510,  28410,  28309,  28208,
  28105,  28001,  27896,  27790,  27683,  27575,  27466,  27355,
  27244,  27132,  27019,  26905,  26789,  26673,  26556,  26437,
  26318,  26198,  26077,  25954,  25831,  25707,  25582,  25456,
  25329,  25201,  25072,  24942,  24811,  24679,  24546,  24413,
  24278,  24143,  24006,  23869,  23731,  23592,  23452,  23311,
  23169,  23027,  22883,  22739,  22594,  22448,  22301,  22153,
  22004,  21855,  21705,  21554,  21402,  21249,  21096,  20942,
  20787,  20631,  20474,  20317,  20159,  20000,  19840,  19680,
  19519,  19357,  19194,  19031,  18867,  18702,  18537,  18371,
  18204,  18036,  17868,  17699,  17530,  17360,  17189,  17017,
  16845,  16672,  16499,  16325,  16150,  15975,  15799,  15623,
  15446,  15268,  15090,  14911,  14732,  14552,  14372,  14191,
  14009,  13827,  13645,  13462,  13278,  13094,  12909,  12724,
  12539,  12353,  12166,  11980,  11792,  11604,  11416,  11227,
  11038,  10849,  10659,  10469,  10278,  10087,   9895,   9703,
   9511,   9319,   9126,   8932,   8739,   8545,   8351,   8156,
   7961,   7766,   7571,   7375,   7179,   6982,   6786,   6589,
   6392,   6195,   5997,   5799,   5601,   5403,   5205,   5006,
   4807,   4608,   4409,   4210,   4011,   3811,   3611,   3411,
   3211,   3011,   2811,   2610,   2410,   2209,   2009,   1808,
   1607,   1406,   1206,   1005,    804,    603,    402,    201,
      0,   -201,   -402,   -603,   -804,  -1005,  -1206,  -1406,
  -1607,  -1808,  -2009,  -2209,  -2410,  -2610,  -2811,  -3011,
  -3211,  -3411,  -3611,  -3811,  -4011,  -4210,  -4409,  -4608,
  -4807,  -5006,  -5205,  -5403,  -5601,  -5799,  -5997,  -6195,
  -6392,  -6589,  -6786,  -6982,  -7179,  -7375,  -7571,  -7766,
  -7961,  -8156,  -8351,  -8545,  -8739,  -8932,  -9126,  -9319,
  -9511,  -9703,  -9895, -10087, -10278, -10469, -10659, -10849,
 -11038, -11227, -11416, -11604, -11792, -11980, -12166, -12353,
 -12539, -12724, -12909, -13094, -13278, -13462, -13645, -13827,
 -14009, -14191, -14372, -14552, -14732, -14911, -15090, -15268,
 -15446, -15623, -15799, -15975, -16150, -16325, -16499, -16672,
 -16845, -17017, -17189, -17360, -17530, -17699, -17868, -18036,
 -18204, -18371, -18537, -18702, -18867, -19031, -19194, -19357,
 -19519, -19680, -19840, -20000, -20159, -20317, -20474, -20631,
 -20787, -20942, -21096, -21249, -21402, -21554, -21705, -21855,
 -22004, -22153, -22301, -22448, -22594, -22739, -22883, -23027,
 -23169, -23311, -23452, -23592, -23731, -23869, -24006, -24143,
 -24278, -24413, -24546, -24679, -24811, -24942, -25072, -25201,
 -25329, -25456, -25582, -25707, -25831, -25954, -26077, -26198,
 -26318, -26437, -26556, -26673, -26789, -26905, -27019, -27132,
 -27244, -27355, -27466, -27575, -27683, -27790, -27896, -28001,
 -28105, -28208, -28309, -28410, -28510, -28608, -28706, -28802,
 -28897, -28992, -29085, -29177, -29268, -29358, -29446, -29534,
 -29621, -29706, -29790, -29873, -29955, -30036, -30116, -30195,
 -30272, -30349, -30424, -30498, -30571, -30643, -30713, -30783,
 -30851, -30918, -30984, -31049, -31113, -31175, -31236, -31297,
 -31356, -31413, -31470, -31525, -31580, -31633, -31684, -31735,
 -31785, -31833, -31880, -31926, -31970, -32014, -32056, -32097,
 -32137, -32176, -32213, -32249, -32284, -32318, -32350, -32382,
 -32412, -32441, -32468, -32495, -32520, -32544, -32567, -32588,
 -32609, -32628, -32646, -32662, -32678, -32692, -32705, -32717,
 -32727, -32736, -32744, -32751, -32757, -32761, -32764, -32766,
 -32767, -32766, -32764, -32761, -32757, -32751, -32744, -32736,
 -32727, -32717, -32705, -32692, -32678, -32662, -32646, -32628,
 -32609, -32588, -32567, -32544, -32520, -32495, -32468, -32441,
 -32412, -32382, -32350, -32318, -32284, -32249, -32213, -32176,
 -32137, -32097, -32056, -32014, -31970, -31926, -31880, -31833,
 -31785, -31735, -31684, -31633, -31580, -31525, -31470, -31413,
 -31356, -31297, -31236, -31175, -31113, -31049, -30984, -30918,
 -30851, -30783, -30713, -30643, -30571, -30498, -30424, -30349,
 -30272, -30195, -30116, -30036, -29955, -29873, -29790, -29706,
 -29621, -29534, -29446, -29358, -29268, -29177, -29085, -28992,
 -28897, -28802, -28706, -28608, -28510, -28410, -28309, -28208,
 -28105, -28001, -27896, -27790, -27683, -27575, -27466, -27355,
 -27244, -27132, -27019, -26905, -26789, -26673, -26556, -26437,
 -26318, -26198, -26077, -25954, -25831, -25707, -25582, -25456,
 -25329, -25201, -25072, -24942, -24811, -24679, -24546, -24413,
 -24278, -24143, -24006, -23869, -23731, -23592, -23452, -23311,
 -23169, -23027, -22883, -22739, -22594, -22448, -22301, -22153,
 -22004, -21855, -21705, -21554, -21402, -21249, -21096, -20942,
 -20787, -20631, -20474, -20317, -20159, -20000, -19840, -19680,
 -19519, -19357, -19194, -19031, -18867, -18702, -18537, -18371,
 -18204, -18036, -17868, -17699, -17530, -17360, -17189, -17017,
 -16845, -16672, -16499, -16325, -16150, -15975, -15799, -15623,
 -15446, -15268, -15090, -14911, -14732, -14552, -14372, -14191,
 -14009, -13827, -13645, -13462, -13278, -13094, -12909, -12724,
 -12539, -12353, -12166, -11980, -11792, -11604, -11416, -11227,
 -11038, -10849, -10659, -10469, -10278, -10087,  -9895,  -9703,
  -9511,  -9319,  -9126,  -8932,  -8739,  -8545,  -8351,  -8156,
  -7961,  -7766,  -7571,  -7375,  -7179,  -6982,  -6786,  -6589,
  -6392,  -6195,  -5997,  -5799,  -5601,  -5403,  -5205,  -5006,
  -4807,  -4608,  -4409,  -4210,  -4011,  -3811,  -3611,  -3411,
  -3211,  -3011,  -2811,  -2610,  -2410,  -2209,  -2009,  -1808,
  -1607,  -1406,  -1206,  -1005,   -804,   -603,   -402,   -201,
};

#if N_LOUD != 100
        ERROR: N_LOUD != 100
#endif
fixed Loudampl[100] = {
  32767,  29203,  26027,  23197,  20674,  18426,  16422,  14636,
  13044,  11626,  10361,   9234,   8230,   7335,   6537,   5826,
   5193,   4628,   4125,   3676,   3276,   2920,   2602,   2319,
   2067,   1842,   1642,   1463,   1304,   1162,   1036,    923,
    823,    733,    653,    582,    519,    462,    412,    367,
    327,    292,    260,    231,    206,    184,    164,    146,
    130,    116,    103,     92,     82,     73,     65,     58,
     51,     46,     41,     36,     32,     29,     26,     23,
     20,     18,     16,     14,     13,     11,     10,      9,
      8,      7,      6,      5,      5,      4,      4,      3,
      3,      2,      2,      2,      2,      1,      1,      1,
      1,      1,      1,      0,      0,      0,      0,      0,
      0,      0,      0,      0,
};


/*----------------------------------------------Library END----------------------------------------------*/



// Declare the main window and two text layers
Window *main_window;
TextLayer *background_layer;
TextLayer *helloWorld_layer;

static void accel_data_handler(AccelData*, uint32_t);
static int16_t values[NBSAMPLE];

  

// Init function called when app is launched
static void init(void) {
    //How many samples to store before calling the callback function
    int num_samples = 25;
    
  	// Create main Window element and assign to pointer
  	main_window = window_create();
    Layer *window_layer = window_get_root_layer(main_window);  

		// Create background Layer
		background_layer = text_layer_create(GRect( 0, 0, 144, 168));
		// Setup background layer color (black)
		text_layer_set_background_color(background_layer, GColorBlack);

		// Create text Layer
		helloWorld_layer = text_layer_create(GRect( 20, 65, 100, 20));
		// Setup layer Information
		text_layer_set_background_color(helloWorld_layer, GColorClear);
		text_layer_set_text_color(helloWorld_layer, GColorWhite);	
		text_layer_set_font(helloWorld_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  	text_layer_set_text_alignment(helloWorld_layer, GTextAlignmentCenter);
    text_layer_set_text(helloWorld_layer, "Hi, I'm a Pebble!");

  	// Add layers as childs layers to the Window's root layer
    layer_add_child(window_layer, text_layer_get_layer(background_layer));
	  layer_add_child(window_layer, text_layer_get_layer(helloWorld_layer));
  
  	// Show the window on the watch, with animated = true
  	window_stack_push(main_window, true);
  
    // Allow accelerometer event
    accel_data_service_subscribe(num_samples, accel_data_handler);
    
    //Define sampling rate
    accel_service_set_sampling_rate(ACCEL_SAMPLING_25HZ);
    
    // Add a logging meassage (for debug)
	  APP_LOG(APP_LOG_LEVEL_DEBUG, "Just write my first app!");
}

// deinit function called when the app is closed
static void deinit(void) {
  
    // Destroy layers and main window 
    text_layer_destroy(background_layer);
	  text_layer_destroy(helloWorld_layer);
    window_destroy(main_window);
}

static void accel_data_handler(AccelData *data, uint32_t num_samples) {
    int16_t x = data[0].x;
    int16_t y = data[0].y;
    int16_t z = data[0].z;
    
    
    static char results[60];
    static int16_t i=0;
    static int16_t real[NBSAMPLE], imag[NBSAMPLE];
  
    int16_t j=0;
    
    
    if(i<NBSAMPLE){
        values[i]=x;
        i++;
    }
    else{
        
        
        
        for(j=0;j<NBSAMPLE;j++){
            real[j] = values[j];
            imag[j] = 0;
        }
        i=0;
        fix_fft(real, imag, M, 0);
        
    }
    
    //APP_LOG(APP_LOG_LEVEL_INFO, "x:%d,y:%d,z:%d", x, y, z);
    //snprintf(results, 60,"Hz:%d", hz[50]);
    //j=50;
    snprintf(results, 60,"%d: %d, %d\n", i, real[i], imag[i]);
 
    text_layer_set_text(helloWorld_layer, results);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
