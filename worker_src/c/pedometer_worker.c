#include <pebble_worker.h>

#define ACCEL_SAMPLING_FREQUENCY ACCEL_SAMPLING_100HZ
#define NBSAMPLE 16
// NFFT in {2, 4, 8, 16, 32, 64, 128, 256, 512, 1024}
#define MFFT 10
#define NFFT (1 << 10)

// Small utility macro for absolute values
#define ABS(x) ((x) < 0 ? -(x) : (x))

#ifndef fixed
#define fixed short
#endif

int fix_fft(fixed fr[], fixed fi[], int m, int inverse);

static short fftSamples[NFFT];
static short fftn = 0;
static uint32_t total_steps = 0;
static uint32_t key_total_steps = 10000;

float my_sqrt(float num){
    float a, p, e = 0.001, b;
    a = num;
    p = a * a;
    const int MAX = 40;
    int nb = 0;
    while( p - num >= e && nb++ < MAX ){
        b = ( a + ( num / a ) ) / 2;
        a = b;
        p = a * a;
    }
    return a;
}

typedef struct {
    int count;
    int centroid;
}C_cluster;

int comparC_cluster(const void* a, const void* b){
    int c = 0;
    if ( ((C_cluster*)a)->centroid <  ((C_cluster*)b)->centroid ) c = -1;
    if ( ((C_cluster*)a)->centroid == ((C_cluster*)b)->centroid ) c = 0;
    if ( ((C_cluster*)a)->centroid >  ((C_cluster*)b)->centroid ) c = 1;
    return -c;
}

int compar (const void* a, const void* b){
      if ( *(int16_t*)a <  *(int16_t*)b ) return -1;
      if ( *(int16_t*)a == *(int16_t*)b ) return 0;
      if ( *(int16_t*)a >  *(int16_t*)b ) return 1;
    return 0;
}

static void accel_data_handler(AccelData *data, uint32_t num_samples) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "IAMTHEWORKER");

    uint32_t max = 0, min = 0;
    uint32_t accel[NBSAMPLE];
    
    for(uint32_t i = 0; i < NBSAMPLE; i++){
        accel[i] = data[i].x * data[i].x + data[i].y * data[i].y + data[i].z * data[i].z;
        if(max == 0 || max < accel[i]){
            max = accel[i];
        }
        if(min == 0 || min > accel[i]){
            min = accel[i];
        }
    }
        
    // Shift points NBSAMPLE to the left
//    memmove(points, &(points[NBSAMPLE]), sizeof(points));
    memmove(fftSamples, &(fftSamples[NBSAMPLE]), sizeof(fftSamples));
    
    //Normalize (max: 168)
    //short smax = max * 32768.0 * 2.0 / 4294967296.0; // - 32768;
    //short smin = min * 32768.0 * 2.0 / 4294967296.0; // - 32768;
    
   // APP_LOG(APP_LOG_LEVEL_DEBUG, "max %lu -> %d min %lu -> %d", max, smax, min, smin);
    
    // Add data
    for (uint32_t i = 0; i < NBSAMPLE; i++){
     //   APP_LOG(APP_LOG_LEVEL_DEBUG, "SEGFAULT %lu %lu", NFFT - (NBSAMPLE - i) - 1, SCR_WIDTH - (NBSAMPLE - i) - 1);
        fftSamples[NFFT - (NBSAMPLE - i)] = accel[i] * 32768.0 * 2.0 / 4294967296.0 * 10;
    //    points[SCR_WIDTH - (NBSAMPLE - i)].y = SCR_HEIGHT - (25 * (accel[i] - min)) / (max - min)  ;
    }
    
    // Shift points
//    for(uint32_t i = 0; i < SCR_WIDTH; i++){
    //    APP_LOG(APP_LOG_LEVEL_DEBUG, "SEGFAULT 2 %lu", i);
 //       points[i].x = i;
  //  }
    
    fftn += NBSAMPLE;
    
    // if 1024 samples have been collected
    if(fftn >= 1024){
        short * fftr = calloc(NFFT, sizeof(short));
        short * ffti = calloc(NFFT, sizeof(short));
        
        APP_LOG(APP_LOG_LEVEL_DEBUG, "ALGO STARTED");
        // Copy fftsamples to fftr
        memcpy(fftr, fftSamples, sizeof(fftSamples));
        memset(ffti, 0, sizeof(fftSamples));
        
        // Perform FFT
        fix_fft(fftr, ffti, MFFT, 0);
        
        // Compute magnitude, find max, compute sigma and mu
        short max_fftr = 0;
        uint32_t xsum = 0, x2sum = 0; // sum and sum of sqaure
        for(uint32_t i = 1; i < NFFT/2; i++){
            fftr[i] = fftr[i] * fftr[i] + ffti[i] * ffti[i];
            xsum += fftr[i];
            x2sum += fftr[i] * fftr[i];
            if(fftr[i] > max_fftr){
                max_fftr = fftr[i];
            }
        }
        
        // Compute mean and variance
        short mean = xsum * 2 / NFFT;
        short sigma = (short) my_sqrt((x2sum - xsum*xsum/(NFFT/2))/((NFFT/2) + 1));
        
        // Remove all frequencies below mean+sigma (~70% of frequencies);
        for(uint32_t i = 1; i < NFFT/2; i++){
            if(fftr[i] < mean + sigma){
                fftr[i] = 0;
            }
        }
        
        free(fftr);
        free(ffti);
        
        // Show points
     /*   for(uint32_t i = 0; i < SCR_WIDTH; i++){
            short max_mag = 0;
            for(uint32_t j = i * 4; j < (i + 1) * 4 && j < NFFT / 2; j++){
                if(fftr[j] > max_mag){
                    max_mag = fftr[j];
                }
            }
            fftpoints[i].x = i;
            fftpoints[i].y = max_mag * SCR_HEIGHT * 0.5 / max_fftr;
        }
       */ 
        // Find the max frequency between 1.5Hz and 2.5Hz
        short max2i = 0;
        //for(short i = 1.5 * NFFT / 100.0; i < 2.5 * NFFT / 100.0; i++){
        for(short i = 0; i < NFFT; i++){
            if(fftr[i] > fftr[max2i]){
                max2i = i;
            }
        }
        
        float max2Hz = 0;
        float walktime = 0;      
        short steps = 0;
        
        int peakNSup = 0;

        fftn = 0;   
        
        if(max2i >  1 * NFFT / ACCEL_SAMPLING_FREQUENCY && max2i <  3 * NFFT / ACCEL_SAMPLING_FREQUENCY){
            APP_LOG(APP_LOG_LEVEL_DEBUG, "ALGO STARTED 88888");
            // Find the walking time
            // Find maximums
            #define MAX_COUNT 30
            int16_t maxIndex [MAX_COUNT];
            uint16_t nbIndex=0;
            memset(maxIndex, -1, sizeof(maxIndex));
            int32_t mean2=0, sigma2=0, sum2=0,ssum2=0;
            
            for(short i = 0; i < MAX_COUNT; i++){
                for(short j = 0; j < NFFT; j++){
                    if((fftSamples[j] > 200) && ((maxIndex[i] == -1) || (fftSamples[maxIndex[i]] < fftSamples[j]))){
                        int8_t tooClose = 0;
                        for(short k = 0; k < i; k++){
                            if((j > maxIndex[k] - 14 && j < maxIndex[k] + 14)){
                                tooClose = 1;
                            }
                        }
                        if(!tooClose){
                            maxIndex[i] = j;
                        }
                    }
                }
                if(maxIndex[i] > -1){
                   // APP_LOG(APP_LOG_LEVEL_DEBUG, "%d %d %lu", i, maxIndex[i], fftSamples[maxIndex[i]]);
                    sum2 += fftSamples[maxIndex[i]];
                    ssum2 += fftSamples[maxIndex[i]]*fftSamples[maxIndex[i]];
                    nbIndex +=1;
                }
                
            }
            
         //   mean2 = sum2 / nbIndex;
         //   sigma2 = my_sqrt((ssum2-(sum2*sum2))/(nbIndex-1));
         //   sigma2 = 100000;
            //APP_LOG(APP_LOG_LEVEL_DEBUG, "sum %lu mean %lu mean+sigma %lu mean-sigma %lu sigma %lu", sum, mean, mean2+sigma, mean-sigma, sigma);
            // clustering
            #define CENTROID_COUNT 4
            #define C_MAX_ITERATION_COUNT 10
            int centroids [CENTROID_COUNT];    // 4 clusters
            // initialize centroids:
            for (int i = 0; i < CENTROID_COUNT; i++){
                centroids[i] = fftSamples[maxIndex[nbIndex / CENTROID_COUNT * i]];
                APP_LOG(APP_LOG_LEVEL_DEBUG, "Init centroid {%d} to [%d] [%d] %d", i, nbIndex / CENTROID_COUNT * i, maxIndex[nbIndex / CENTROID_COUNT * i], centroids[i]);
            }
            
            // sort index
            qsort(maxIndex, nbIndex, sizeof(int16_t), compar);
            
            int c_iteration_count = 0;
            int c_changed = 0;
            int c_cluster [nbIndex];
            int c_sum [CENTROID_COUNT];
            int c_count[CENTROID_COUNT];
            C_cluster c_output[CENTROID_COUNT];
            do{
                // Compute distance to each centroid
                int c_distance[CENTROID_COUNT][nbIndex];
                memset(c_distance, 0, sizeof(c_distance));
                for(int i = 0; i < CENTROID_COUNT; i++){
                    // Reset distances
                    for(int j = 0; j < nbIndex; j++){
                        c_distance[i][j] = ABS(centroids[i] - fftSamples[maxIndex[j]]);
                    }
                }
                
                // Assign to cluster
                memset(c_cluster, -1, sizeof(c_cluster));
                for(int i = 0; i < nbIndex; i++){
                    int min_distance = -1;
                    int assign_to_cluster = 0;
                    for(int j = 0; j < CENTROID_COUNT; j++){
                        if(min_distance < 0 || c_distance[j][i] < min_distance){
                            //APP_LOG(APP_LOG_LEVEL_DEBUG, "%d < %d => {%d}", c_distance[j][i], min_distance, j );
                            assign_to_cluster = j;
                            min_distance = c_distance[j][i];
                        }
                    }
                    //APP_LOG(APP_LOG_LEVEL_DEBUG, "maxindex [%d] assigned to cluster cluster {%d} with distance %d", i, assign_to_cluster, c_distance[assign_to_cluster][i]);
                    c_cluster[i] = assign_to_cluster;
                }
                
                // Compute centroids
                memset(c_sum, 0, sizeof(c_sum));
                memset(c_count, 0, sizeof(c_count));
                for(int i = 0; i < nbIndex; i++){
                    int assigned_cluster = c_cluster[i];
                    c_sum[assigned_cluster] += fftSamples[maxIndex[i]];
                    c_count[assigned_cluster] += 1;
                    //APP_LOG(APP_LOG_LEVEL_DEBUG, "[%d] %d {%d}", i, fftSamples[maxIndex[i]], assigned_cluster);
                }
                c_changed = 0;
                for(int i = 0; i < CENTROID_COUNT; i++){
                    int new_centroid = c_sum[i] / c_count[i];
                    if(new_centroid != centroids[i]){
                        c_changed = 1;
                    }
                    centroids[i] = new_centroid;
                    C_cluster my_cluster = {
                        .count = c_count[i],
                        .centroid = centroids[i]
                    };
                    c_output[i] = my_cluster;
                    //APP_LOG(APP_LOG_LEVEL_DEBUG, "Recompute centroid {%d} to %d, (%d assigned)", i, new_centroid, c_count[i]);
                }
                
                if(c_iteration_count++ > C_MAX_ITERATION_COUNT){
                    APP_LOG(APP_LOG_LEVEL_DEBUG, "max iteration count C_MAX_ITERATION_COUNT reached");
                    break;
                }
            }while(c_changed);
            APP_LOG(APP_LOG_LEVEL_DEBUG, "Clustering: done in %d iterations", c_iteration_count);
            
            // Count assigned peak in the highest clusters
            qsort(c_output, CENTROID_COUNT, sizeof(C_cluster), comparC_cluster);
            peakNSup = 0;
            for(int i = 0; i < CENTROID_COUNT; i++){
                if(c_output[i].centroid >= 250 && i < CENTROID_COUNT - 1)
                    peakNSup += c_output[i].count;
                APP_LOG(APP_LOG_LEVEL_DEBUG, "Cluster {%d} %d [=%d]", i, c_output[i].centroid, c_output[i].count);
            }
            
            // select only some of the peaks, and compute the distance between each peaks.
          /*  for(int i = 0; i < nbIndex; i++){
                if(maxIndex[i] > -1 && fftSamples[maxIndex[i]]<(mean2+sigma2 * 10) && fftSamples[maxIndex[i]]>(mean2-sigma2 * 10)){
                    peakNSup ++;
                    if(max2i > 0){
                        APP_LOG(APP_LOG_LEVEL_DEBUG, "[%d] %d {%d}", maxIndex[i], fftSamples[maxIndex[i]],  c_cluster[i]);
                    }
                }
            }*/        
            
             max2Hz = max2i * ACCEL_SAMPLING_FREQUENCY / NFFT;
             walktime = peakNSup / max2Hz;      
             steps = max2Hz * walktime;    
            if(max2Hz < 1e-8){
                walktime = 0;
            }
            
            #define END_SPRINTF "%lu(%d/%d+%d=%d(%d|%d", total_steps, max2i, (int)(max2Hz*10), steps, peakNSup, (int) walktime, fftr[max2i]
            if(max2i > 1.5 * NFFT / ACCEL_SAMPLING_FREQUENCY){
                total_steps += peakNSup;
          //      text_layer_set_text(helloWorld_layer, ssteps);
            }
            APP_LOG(APP_LOG_LEVEL_DEBUG, END_SPRINTF);

        }

        // Send total steps to foreground
        AppWorkerMessage message = {
            .data0 = total_steps
        };

        app_worker_send_message(0, &message);
    }
    //layer_mark_dirty(graph_layer);
}

static void worker_message_handler(uint16_t type, AppWorkerMessage *message) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Worker received %d", message->data0);
    total_steps = message->data0;
}

static void prv_init() {
    memset(fftSamples, 0,  sizeof(fftSamples));

    // Allow accelerometer event
    accel_data_service_subscribe(NBSAMPLE, accel_data_handler);
    
    //Define sampling rate
    accel_service_set_sampling_rate(ACCEL_SAMPLING_FREQUENCY);
    
    // Read total steps from persistent memory
    if (persist_exists(key_total_steps)) {
        total_steps = persist_read_int(key_total_steps);
    } else {
        total_steps = 0;
        persist_write_int(key_total_steps, total_steps);
    }

    app_worker_message_subscribe(worker_message_handler);
}

static void prv_deinit() {
    // Unsubscribe from the accelerometer events
    accel_data_service_unsubscribe();
    
    // Write total steps count to persistent memory
    persist_write_int(key_total_steps, total_steps);
    
    
    app_worker_message_unsubscribe();
}

int main(void) {
  prv_init();
  worker_event_loop();
  prv_deinit();
}


/*      fix_fft.c - Fixed-point Fast Fourier Transform  */
/*
        fix_fft()       perform FFT or inverse FFT
        window()        applies a Hanning window to the (time) input
        fix_loud()      calculates the loudness of the signal, for
                        each freq point. Result is an integer array,
                        units are dB (values will be negative).
        iscale()        scale an integer value by (numer/denom).
        fix_mpy()       perform fixed-point multiplication.
        Sinewave[1024]  sinewave normalized to 32767 (= 1.0).
        Loudampl[100]   Amplitudes for lopudnesses from 0 to -99 dB.
        Low_pass        Low-pass filter, cutoff at sample_freq / 4.
        All data are fixed-point short integers, in which
        -32768 to +32768 represent -1.0 to +1.0. Integer arithmetic
        is used for speed, instead of the more natural floating-point.
        For the forward FFT (time -> freq), fixed scaling is
        performed to prevent arithmetic overflow, and to map a 0dB
        sine/cosine wave (i.e. amplitude = 32767) to two -6dB freq
        coefficients; the one in the lower half is reported as 0dB
        by fix_loud(). The return value is always 0.
        For the inverse FFT (freq -> time), fixed scaling cannot be
        done, as two 0dB coefficients would sum to a peak amplitude of
        64K, overflowing the 32k range of the fixed-point integers.
        Thus, the fix_fft() routine performs variable scaling, and
        returns a value which is the number of bits LEFT by which
        the output must be shifted to get the actual amplitude
        (i.e. if fix_fft() returns 3, each value of fr[] and fi[]
        must be multiplied by 8 (2**3) for proper scaling.
        Clearly, this cannot be done within the fixed-point short
        integers. In practice, if the result is to be used as a
        filter, the scale_shift can usually be ignored, as the
        result will be approximately correctly normalized as is.
        TURBO C, any memory model; uses inline assembly for speed
        and for carefully-scaled arithmetic.
        Written by:  Tom Roberts  11/8/89
        Made portable:  Malcolm Slaney 12/15/94 malcolm@interval.com
                Timing on a Macintosh PowerBook 180.... (using Symantec C6.0)
                        fix_fft (1024 points)             8 ticks
                        fft (1024 points - Using SANE)  112 Ticks
                        fft (1024 points - Using FPU)    11
*/

/* FIX_MPY() - fixed-point multiplication macro.
   This macro is a statement, not an expression (uses asm).
   BEWARE: make sure _DX is not clobbered by evaluating (A) or DEST.
   args are all of type fixed.
   Scaling ensures that 32767*32767 = 32767. */
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
#ifndef fixed
#define fixed short
#endif

extern fixed Sinewave[N_WAVE]; /* placed at end of this file for clarity */
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

/*
        fix_mpy() - fixed-point multiplication
*/
fixed fix_mpy(fixed a, fixed b)
{
        FIX_MPY(a,a,b);
        return a;
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