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
#define WALK_MAX_FREQ 1
#define NBSAMPLE 16
// NFFT in {2, 4, 8, 16, 32, 64, 128, 256, 512, 1024}
#define NFFT 128
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
static GPoint fftpoints[6];
static uint32_t fftSamples[NFFT];
static uint32_t max = 0, min = 0;
static short fftn = 0;
static uint32_t total_steps = 0;
uint32_t last = 0;
uint32_t last_peak_time = -1;
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
    
    
    path_info.num_points = 6;
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
    
    memset(fftSamples, 0,  sizeof(fftSamples));
    
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
    const uint8_t MAX_STEP = 40;
    uint8_t i = 0;
    float a, p, e = 0.001, b;
    a = num;
    p = a * a;
    while( p - num >= e && (i++ < MAX_STEP) ){
        b = ( a + ( num / a ) ) / 2;
        a = b;
        p = a * a;
    }
    return a;
}

double xv[3];
double yv[3];

void filter(uint32_t* samples, int count)
{
   double ax[3] = {0.198215, 0.396430, 0.198215};
    double by[3] = {1, -2.22693, 2.019794};

//   getLPCoefficientsButterworth2Pole(25, 100, ax, by);

   for (int i=0;i<count;i++)
   {
       xv[2] = xv[1]; xv[1] = xv[0];
       xv[0] = samples[i];
       yv[2] = yv[1]; yv[1] = yv[0];

       yv[0] =   (ax[0] * xv[0] + ax[1] * xv[1] + ax[2] * xv[2]
                    - by[1] * yv[0]
                    - by[2] * yv[1]);

       samples[i] = yv[0];
   }
}
int compar (const void* a, const void* b){
      if ( *(int16_t*)a <  *(int16_t*)b ) return -1;
      if ( *(int16_t*)a == *(int16_t*)b ) return 0;
      if ( *(int16_t*)a >  *(int16_t*)b ) return 1;
    return 0;
}

        int last_time_peak = -1;
        int spree = 0;
static void accel_data_handler(AccelData *data, uint32_t num_samples) {
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
    uint32_t old = fftSamples[NFFT - NBSAMPLE];
    memmove(points, &(points[NBSAMPLE]), sizeof(points));
    memmove(fftSamples, &(fftSamples[NBSAMPLE]), sizeof(fftSamples));
        
    //Normalize (max: 168)
    //short smax = max * 32768.0 * 2.0 / 4294967296.0; // - 32768;
    //short smin = min * 32768.0 * 2.0 / 4294967296.0; // - 32768;
    
   // APP_LOG(APP_LOG_LEVEL_DEBUG, "max %lu -> %d min %lu -> %d", max, smax, min, smin);
    
    // Add data
    double RC = 100.0/(1*2*3.14); 
    double dt = 1.0/25; 
    double alpha = 7;
   // filter(accel, NBSAMPLE);
    for (uint32_t i = 0; i < NBSAMPLE; i++){
     //   APP_LOG(APP_LOG_LEVEL_DEBUG, "SEGFAULT %lu %lu", NFFT - (NBSAMPLE - i) - 1, SCR_WIDTH - (NBSAMPLE - i) - 1);
        // y[i] := ß * x[i] + (1-ß) * y[i-1]
        last = 0;
        fftSamples[NFFT - (NBSAMPLE - i)] = accel[i] + last; // * 32768.0 * 2.0 / 4294967296.0 * 10;
        last = accel[i];
        points[SCR_WIDTH - (NBSAMPLE - i)].y = SCR_HEIGHT - fftSamples[NFFT - (NBSAMPLE - i)] * 32768.0 * 2.0 / 4294967296.0;
    }
    
    // Shift points
    for(uint32_t i = 0; i < SCR_WIDTH; i++){
    //    APP_LOG(APP_LOG_LEVEL_DEBUG, "SEGFAULT 2 %lu", i);
        points[i].x = i;
    }
    
    fftn += NBSAMPLE;
    
    // if 1024 samples have been collected
    if(fftn >= NFFT){
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Entering 8s");
        #define MAX_COUNT 30
        int16_t maxIndex [MAX_COUNT];
        uint16_t nbIndex=0;
        memset(maxIndex, -1, sizeof(maxIndex));
        uint32_t mean=0, sigma=0, sum=0,ssum=0;
        
        for(short i = 0; i < MAX_COUNT; i++){
            for(short j = 0; j < NFFT; j++){
                if((fftSamples[j] > 1500000) && ((maxIndex[i] == -1) || (fftSamples[maxIndex[i]] < fftSamples[j]))){
                    int8_t tooClose = 0;
                    for(short k = 0; k < i; k++){
                        if((j > maxIndex[k] - 3 && j < maxIndex[k] + 3)){
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
                sum += fftSamples[maxIndex[i]];
                ssum += fftSamples[maxIndex[i]]*fftSamples[maxIndex[i]];
                nbIndex +=1;
            }
        }
        
        APP_LOG(APP_LOG_LEVEL_DEBUG, "nbIndex %u", nbIndex);
        
        mean = sum / nbIndex;
        sigma = my_sqrt((ssum-(sum*sum))/(nbIndex-1));
        sigma = 100000;
        APP_LOG(APP_LOG_LEVEL_DEBUG, "sum %lu mean %lu mean+sigma %lu mean-sigma %lu sigma %lu", sum, mean, mean+sigma, mean-sigma, sigma);
        
        // sort index
        qsort(maxIndex, nbIndex, sizeof(int16_t), compar);
        
        // select only some of the peaks, and compute the distance between each peaks.
        for(int i = 0; i < nbIndex; i++){
            if(maxIndex[i] > -1 && fftSamples[maxIndex[i]]<(mean+sigma * 10) && fftSamples[maxIndex[i]]>(mean-sigma * 10)){
                if(last_time_peak > 0){
                    int distance =  maxIndex[i] - last_time_peak;
                    const int cost = 3;
                    APP_LOG(APP_LOG_LEVEL_DEBUG, "DISTANCE %d %d", distance, spree);
                    if(distance >= 4 && distance <= 30){
                        spree += 1 * cost;
                    }
                    else if(spree > 0){
                        spree -= 1;
                    }
                    
                    if(spree >= 3 * cost){
                        spree = 0;
                        total_steps += 3;
                        
                        APP_LOG(APP_LOG_LEVEL_DEBUG, "STEP SPREE");    
                    }
                }
                last_time_peak = maxIndex[i];
            }
        }
        last_time_peak = last_time_peak - NFFT;
        
        snprintf(ssteps, SSTEPS_SIZE, "%lu", total_steps);
        text_layer_set_text(helloWorld_layer, ssteps);
        fftn = 0;
        fftpoints[0].x = 0;
        fftpoints[1].x = SCR_WIDTH;
        fftpoints[2].x = SCR_WIDTH;
        fftpoints[3].x = 0;
        fftpoints[4].x = 0;
        fftpoints[5].x = SCR_WIDTH;
        fftpoints[0].y = fftpoints[1].y = SCR_HEIGHT - (mean + sigma) * 32768.0 * 2.0 / 4294967296.0;
        fftpoints[2].y = fftpoints[3].y = SCR_HEIGHT - mean * 32768.0 * 2.0 / 4294967296.0;
        fftpoints[4].y = fftpoints[5].y = SCR_HEIGHT - (mean - sigma) * 32768.0 * 2.0 / 4294967296.0;
        fftn = 0; 
    }
    
    // Add data
    /*for (uint32_t i = 0; i < NBSAMPLE; i++){
     //   APP_LOG(APP_LOG_LEVEL_DEBUG, "SEGFAULT %lu %lu", NFFT - (NBSAMPLE - i) - 1, SCR_WIDTH - (NBSAMPLE - i) - 1);
        fftSamples[NFFT - (NBSAMPLE - i)] = accel[i] * 32768.0 * 2.0 / 4294967296.0 * 10;
        points[SCR_WIDTH - (NBSAMPLE - i)].y = SCR_HEIGHT - accel[i] * 32768.0 * 2.0 / 4294967296.0;
    }*/
    // Dirty layer
    layer_mark_dirty(graph_layer);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
