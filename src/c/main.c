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

#define ACCEL_SAMPLING_FREQUENCY ACCEL_SAMPLING_100HZ
#define NBSAMPLE 16
// NFFT in {2, 4, 8, 16, 32, 64, 128, 256, 512, 1024}
#define MFFT 10
#define NFFT (1 << 10)
#define SCR_WIDTH 144
#define SCR_HEIGHT 168
#define ABS(x) ((x) < 0 ? -(x) : (x))
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
    accel_service_set_sampling_rate(ACCEL_SAMPLING_FREQUENCY);
    
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
    if(fftn >= 1024){
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
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "max=%d mu=%d s=%d sum=%lu ssum=%lu", max_fftr, mean, sigma, xsum, x2sum);
        
        // Remove all frequencies below mean+sigma (~70% of frequencies);
        for(uint32_t i = 1; i < NFFT/2; i++){
            if(fftr[i] < mean + sigma){
                fftr[i] = 0;
            }
        }
        
        // Show points
        for(uint32_t i = 0; i < SCR_WIDTH; i++){
            short max_mag = 0;
            for(uint32_t j = i * 4; j < (i + 1) * 4 && j < NFFT / 2; j++){
                if(fftr[j] > max_mag){
                    max_mag = fftr[j];
                }
            }
            fftpoints[i].x = i;
            fftpoints[i].y = max_mag * SCR_HEIGHT * 0.5 / max_fftr;
        }
        
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
        if(max2i >  1 * NFFT / ACCEL_SAMPLING_FREQUENCY && max2i <  3 * NFFT / ACCEL_SAMPLING_FREQUENCY){
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
            #define C_MAX_ITERATION_COUNT 20
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
                text_layer_set_text(helloWorld_layer, ssteps);
            }
            APP_LOG(APP_LOG_LEVEL_DEBUG, END_SPRINTF);

        }
        snprintf(ssteps, SSTEPS_SIZE, END_SPRINTF);

        fftn = 0;
        //vibes_short_pulse();
    }
    layer_mark_dirty(graph_layer);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
