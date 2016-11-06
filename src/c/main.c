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
#include "window.h"

// Declare the main window
Window *main_window;

// Init function called when app is launched
static void init(void) {        
    main_window = window_create();
    
    window_set_window_handlers(main_window, (WindowHandlers) {
        .load = init_ui,
        .unload = deinit_ui
    });
        
  	// Show the window on the watch, with animated = true
  	window_stack_push(main_window, true);
}

// deinit function called when the app is closed
static void deinit(void) {    
    window_destroy(main_window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
