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
#include "src/c/kiss_fftr.h"

#define ACCEL_SAMPLING_25HZ 25
#define NBSAMPLE 18
#define NFFT 144
#define SCR_WIDTH 144
#define SCR_HEIGHT 168

// Declare the main window and two text layers
Window *main_window;
TextLayer *background_layer;
TextLayer *helloWorld_layer;

static void accel_data_handler(AccelData*, uint32_t);
static int16_t values[NBSAMPLE];

Layer * graph_layer;
uint32_t accel[NFFT];
uint32_t bigBuffer[NFFT];
static GPoint points[NFFT];
static uint32_t max = 0, min = 0;

// .update_proc of my_layer:
void graph_layer_update_proc(Layer *my_layer, GContext* ctx) {
    GPathInfo path_info = {
        .num_points = NFFT,
        .points = points
    };
    GPath *path = gpath_create(&path_info);
    
    graphics_context_set_stroke_color(ctx, GColorBlack);
    gpath_draw_outline_open(ctx, path);
    gpath_destroy(path);
}
  

// Init function called when app is launched
static void init(void) {
    //How many samples to store before calling the callback function
    int num_samples = NBSAMPLE;
    
  	// Create main Window element and assign to pointer
  	main_window = window_create();
    Layer *window_layer = window_get_root_layer(main_window);  
    
    // Create graph layer
    graph_layer = layer_create(GRect(0, 0, 144, 168));    
    layer_set_update_proc(graph_layer, graph_layer_update_proc);   
    layer_add_child(window_layer, graph_layer);

    
  	// Show the window on the watch, with animated = true
  	window_stack_push(main_window, true);
  
    // Allow accelerometer event
    accel_data_service_subscribe(NBSAMPLE, accel_data_handler);
    
    //Define sampling rate
    accel_service_set_sampling_rate(ACCEL_SAMPLING_25HZ);
    
    memset(points, SCR_HEIGHT / 2,  sizeof(points));
    
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
    for(uint32_t i = 0; i < num_samples && i < NBSAMPLE; i++){
        accel[i] = data[i].x * data[i].x + data[i].y * data[i].y + data[i].z * data[i].z;
        if(max == 0 || max < accel[i]){
            max = accel[i];
        }
        if(min == 0 || min > accel[i]){
            min = accel[i];
        }
    }
        
    // Shift points NBSAMPLE to the left
    memmove(points, &(points[NBSAMPLE]), sizeof(GPoint) * NFFT);
    
    //Normalize (max: 168)
    for(uint32_t i = 0; i < NFFT; i++){        
        // Compute points x and y
        points[i].x = i;
        if(i >= NFFT - NBSAMPLE)
            points[i].y = SCR_HEIGHT - (accel[i + NBSAMPLE - NFFT ] - min) * SCR_HEIGHT/ (max-min) * 0.95;
    }
    APP_LOG(APP_LOG_LEVEL_DEBUG, "%lu; %lu - %lu = %lu; %lu", accel[0] - min, max, min, max - min,  (accel[0] - min) * SCR_HEIGHT/ (max-min));
    layer_mark_dirty(graph_layer);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
