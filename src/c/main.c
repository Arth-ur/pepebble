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

// We want to include the code, but we do not want the compiler to compile this file
// to an object. The editor won't let us rename the file to 'integer_fft.inc',
// so we use the '.js' extension.
#include "src/c/integer_fft.js"

#define ACCEL_SAMPLING_25HZ 25
#define NBSAMPLE 16
// NFFT in {2, 4, 8, 16, 32, 64, 128, 256, 512, 1024}
#define MFFT 7
#define NFFT (1 << MFFT)
#define SCR_WIDTH 144
#define SCR_HEIGHT 168

// Declare the main window and two text layers
Window *main_window;
TextLayer *background_layer;
TextLayer *helloWorld_layer;

static void accel_data_handler(AccelData*, uint32_t);
static int16_t values[NBSAMPLE];

Layer * graph_layer;
uint32_t accel[NBSAMPLE];
uint32_t bigBuffer[NFFT];
static GPoint points[SCR_WIDTH];
static GPoint fftpoints[SCR_WIDTH];
static short fftSamples[NFFT];
static short fftr[NFFT];
static short ffti[NFFT];
static short fftmag[NFFT];
static uint32_t max = 0, min = 0;
static short fftn = 0;
static uint32_t total_steps = 0;
#define SSTEPS_SIZE 24
char* ssteps;

// .update_proc of my_layer:
static void graph_layer_update_proc(Layer *my_layer, GContext* ctx) {
    GPathInfo path_info;
    GPath *path;
    path_info.num_points = SCR_WIDTH;
    path_info.points = points;
    path = gpath_create(&path_info);
    graphics_context_set_stroke_color(ctx, GColorBlack);
    gpath_draw_outline_open(ctx, path);
    gpath_destroy(path);
    
    path_info.num_points = SCR_WIDTH;
    path_info.points = fftpoints;
    path = gpath_create(&path_info);
    graphics_context_set_stroke_color(ctx, GColorBlack);
    gpath_draw_outline_open(ctx, path);
    gpath_destroy(path);
}
  

// Init function called when app is launched
static void init(void) {
    ssteps = malloc(SSTEPS_SIZE);
    //How many samples to store before calling the callback function
    int num_samples = NBSAMPLE;
    
  	// Create main Window element and assign to pointer
  	main_window = window_create();
    Layer *window_layer = window_get_root_layer(main_window);  
    
    // Create graph layer
    graph_layer = layer_create(GRect(0, 0, 144, 168));    
    layer_set_update_proc(graph_layer, graph_layer_update_proc);   
    layer_add_child(window_layer, graph_layer);

    // Create text layer
    ssteps = "NOSTEPS";
    helloWorld_layer = text_layer_create(GRect(0, 50, 144, 25));
    snprintf(ssteps, SSTEPS_SIZE, "%lu", total_steps);
    text_layer_set_text(helloWorld_layer, ssteps);
    text_layer_set_text_alignment(helloWorld_layer, GTextAlignmentRight);
    layer_add_child(window_layer, text_layer_get_layer(helloWorld_layer));
    
  	// Show the window on the watch, with animated = true
  	window_stack_push(main_window, true);
  
    // Allow accelerometer event
    accel_data_service_subscribe(NBSAMPLE, accel_data_handler);
    
    //Define sampling rate
    accel_service_set_sampling_rate(ACCEL_SAMPLING_25HZ);
    
    memset(fftSamples, 0,  sizeof(fftr));
    memset(fftr, 0,  sizeof(fftr));
    memset(ffti, 0,  sizeof(fftr));
    
    // Add a logging meassage (for debug)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Just write my first app!");
}

// deinit function called when the app is closed
static void deinit(void) {
  
    // Destroy layers and main window 
    text_layer_destroy(background_layer);
    text_layer_destroy(helloWorld_layer);
    window_destroy(main_window);
    
    free(ssteps);
}

float my_sqrt(float num){
    float a, p, e = 0.001, b;
    a = num;
    p = a * a;
    const int MAXITER = 40;
    int nbiter = 0;
    while( p - num >= e && nbiter++ < MAXITER){
        b = ( a + ( num / a ) ) / 2;
        a = b;
        p = a * a;
    }
    return a;
}


static void accel_data_handler(AccelData *data, uint32_t num_samples) {
            APP_LOG(APP_LOG_LEVEL_DEBUG, "NoSegFaultYet");
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
    memmove(points, &(points[NBSAMPLE]), sizeof(points));
    memmove(fftSamples, &(fftSamples[NBSAMPLE]), sizeof(fftSamples));
    
    //Normalize (max: 168)
    //short smax = max * 32768.0 * 2.0 / 4294967296.0; // - 32768;
    //short smin = min * 32768.0 * 2.0 / 4294967296.0; // - 32768;
    
   // APP_LOG(APP_LOG_LEVEL_DEBUG, "max %lu -> %d min %lu -> %d", max, smax, min, smin);
                
    // Add data
    for (uint32_t i = 0; i < NBSAMPLE; i++){
     //   APP_LOG(APP_LOG_LEVEL_DEBUG, "SEGFAULT %lu %lu", NFFT - (NBSAMPLE - i) - 1, SCR_WIDTH - (NBSAMPLE - i) - 1);
        fftSamples[NFFT - (NBSAMPLE - i)] = accel[i] * 32768.0 * 2.0 / 4294967296.0 * 10;
        points[SCR_WIDTH - (NBSAMPLE - i)].y = SCR_HEIGHT - accel[i] * 32768.0 * 2.0 / 4294967296.0;
    }
    
    // Shift points
    for(uint32_t i = 0; i < SCR_WIDTH; i++){
    //    APP_LOG(APP_LOG_LEVEL_DEBUG, "SEGFAULT 2 %lu", i);
        points[i].x = i;
    }
    
    fftn += NBSAMPLE;
    
    // if 1024 samples have been collected
    if(fftn >= NFFT){
        // Copy fftsamples to fftr
        memcpy(fftr, fftSamples, sizeof(fftSamples));
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
        APP_LOG(APP_LOG_LEVEL_DEBUG, "max=%d mu=%d s=%d sum=%lu ssum=%lu", max_fftr, mean, sigma, xsum, x2sum);
        
        // Remove all frequencies below mean+sigma (~70% of frequencies);
        for(uint32_t i = 1; i < NFFT/2; i++){
            if(fftr[i] < mean + sigma){
                //fftr[i] = 0;
            }
        }

        // Show points
        for(uint32_t i = 0; i < SCR_WIDTH; i++){
            short max_mag = 0;
            /*for(uint32_t j = i; j < (i + 1) && j < NFFT / 2; j++){
                if(fftr[j] > max_mag){
                    max_mag = fftr[j];
                }
            }*/
            fftpoints[i].x = i;
            if(i < NFFT /2){
                fftpoints[i].y = fftr[i] * SCR_HEIGHT * 0.5 / max_fftr;
            }else{
                fftpoints[i].y = 0;
            }
        }
        
        // Find the max frequency between 1.5Hz and 2.5Hz
        short max2i = 0;
        //for(short i = 1.5 * NFFT / 25; i < 2.5 * NFFT / 25; i++){
        for(short i = 0; i < NFFT/2; i++){
            if(fftr[i] > fftr[max2i]){
                max2i = i;
                APP_LOG(APP_LOG_LEVEL_DEBUG, "i=%d, f=%f, m=%d", i, i*25.0/NFFT, fftr[i]);
            }
        }
        if(max2i > 1.5 * NFFT / NBSAMPLE){
            float max2Hz = max2i * 25.0 / NFFT;
            short steps = max2Hz * NFFT * 25;
        
            APP_LOG(APP_LOG_LEVEL_DEBUG, "max fq=%fHz (i=%d) (|%d|) ======> %d steps", max2Hz, max2i, fftr[max2i], steps);
            total_steps += steps;
            snprintf(ssteps, SSTEPS_SIZE, "%lu (+%d)", total_steps, steps);
            text_layer_set_text(helloWorld_layer, ssteps);
        }else{
            APP_LOG(APP_LOG_LEVEL_DEBUG, "NO STEPS");
        }
        
        fftn = 0;
        //vibes_short_pulse();*/
    }
    layer_mark_dirty(graph_layer);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
