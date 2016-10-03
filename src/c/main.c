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

#define ACCEL_SAMPLING_25HZ 25

// Declare the main window and two text layers
Window *main_window;
TextLayer *background_layer;
TextLayer *helloWorld_layer;

static void accel_data_handler(AccelData*, uint32_t);

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
    
    APP_LOG(APP_LOG_LEVEL_INFO, "x:%d,y:%d,z:%d", x, y, z);
    snprintf(results, 60, "x:%d;y:%d;z:%d", x, y, z);
    text_layer_set_text(helloWorld_layer, results);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
