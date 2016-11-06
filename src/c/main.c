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

static void accel_data_handler(AccelData*, uint32_t);

Layer * graph_layer;
//static GPoint points[SCR_WIDTH];
//static GPoint fftpoints[SCR_WIDTH];
static short fftSamples[NFFT];
static short fftn = 0;
static uint32_t total_steps = 0;

Layer * objective_steps_layer;
Layer * objective_distance_layer;
Layer * objective_calories_layer;

static ActionMenu *s_action_menu;
static ActionMenuLevel *s_root_level;


static char s_nsteps[7];
static char s_distance[20];
static char s_calories[20];
TextLayer* label_today_steps_text_layer;
TextLayer* today_steps_text_layer ;
TextLayer* today_distance_text_layer;
TextLayer* today_calories_text_layer;
TextLayer* objective_steps_text_layer;
TextLayer* objective_distance_text_layer ;
TextLayer* objective_calories_text_layer ;
TextLayer* label_objective_text_layer ;

// Default to the Swiss averages according to Wikipedia
#define DEFAULT_USER_HEIGHT 178
#define DEFAULT_USER_WEIGHT 70
#define DEFAULT_OBJECTIVE_STEPS 10000
#define DEFAULT_OBJECTIVE_DISTANCE 10000
#define DEFAULT_OBJECTIVE_CALORIES 1000
int user_height = DEFAULT_USER_HEIGHT;
int user_weight = DEFAULT_USER_WEIGHT; 
int objective_steps = DEFAULT_OBJECTIVE_STEPS;
int objective_distance = DEFAULT_OBJECTIVE_DISTANCE;
int objective_calories = DEFAULT_OBJECTIVE_CALORIES;

typedef enum UserInfoEdit {
    UserInfoEditHeight,
    UserInfoEditWeight,
    ObjectiveEditSteps,
    ObjectiveEditDistance,
    ObjectiveEditCalories
} UserInfoEdit;

// .update_proc of my_layer:
static void graph_layer_update_proc(Layer *my_layer, GContext* ctx) {
  /*  GPathInfo path_info;
    GPath *path;
        
    path_info.num_points = SCR_WIDTH;
    path_info.points = points;
    path = gpath_create(&path_info);
    
    graphics_context_set_stroke_color(ctx, GColorBlack);
    gpath_draw_outline_open(ctx, path);
    gpath_destroy(path);*/
    
    /*
    path_info.num_points = SCR_WIDTH;
    path_info.points = fftpoints;
    path = gpath_create(&path_info);
    
    graphics_context_set_stroke_color(ctx, GColorBlack);
    gpath_draw_outline_open(ctx, path);
    gpath_destroy(path);*/
}

static void objective_layer_update_proc(Layer *my_layer, GContext* ctx, int y, int progress) {
    const int MARGIN = 10;
    const int PADDING = 2;
    const int HEIGHT = 20;
    
    int fill_width =  (SCR_WIDTH - 2 * MARGIN - 2 * PADDING) * progress / 100;
    
    graphics_draw_rect(ctx, GRect(MARGIN, y, SCR_WIDTH - 2 * MARGIN, HEIGHT));
    graphics_fill_rect(ctx, GRect(MARGIN + PADDING, y + PADDING, fill_width, HEIGHT - 2 * PADDING), 0, GCornerNone);
    
}


static uint32_t compute_distance(uint32_t for_steps){
    const float walkingFactor = 0.57;
    float strip; 
    float distance; 
    strip = user_height * 0.415; 
    distance = (for_steps * strip) / 100;
    
    return distance;
}

static uint32_t compute_calories(uint32_t for_steps){
    const float walkingFactor = 0.57; 
    float caloriesBurnedPerMile; 
    float strip; 
    float stepCountMile; // step/mile 
    float conversationFactor; 
    float caloriesBurned; 
    caloriesBurnedPerMile = walkingFactor * (user_weight * 2.2); 
    strip = user_height * 0.415; 
    stepCountMile = 160934.4 / strip; 
    conversationFactor = caloriesBurnedPerMile / stepCountMile;
    caloriesBurned = for_steps * conversationFactor;

    return caloriesBurned;
}

static int get_progress_for_layer(Layer * layer){
    if(layer == objective_steps_layer)
        return total_steps * 100 / objective_steps;
    else if(layer == objective_distance_layer)
        return compute_distance(total_steps) * 100 / objective_distance;
    else if(layer == objective_calories_layer)
        return compute_calories(total_steps) * 100 / objective_calories;
    else
        return 0;
}

// update_proc for objective layers
static void objective_steps_layer_update_proc(Layer *layer, GContext* ctx) {
    const int PADDING = 2;
    GRect bounds = layer_get_bounds(layer);
    int progress = get_progress_for_layer(layer);
    
    int fill_width =  (bounds.size.w - 2 * PADDING) * progress / 100;
    
    graphics_draw_rect(ctx, GRect(0, 0, bounds.size.w, bounds.size.h));
    graphics_fill_rect(ctx, GRect(PADDING, PADDING, fill_width, bounds.size.h - 2 * PADDING), 0, GCornerNone);
}

static void objective_distance_layer_update_proc(Layer *my_layer, GContext* ctx) {
    objective_layer_update_proc(my_layer, ctx, 70, 66);
}

static void objective_kcal_layer_update_proc(Layer *my_layer, GContext* ctx) {
    objective_layer_update_proc(my_layer, ctx, 130, 33);
}


ScrollLayer *scroll_layer;
int state_offset = 0;

int clip_value(int x, int min, int max){
    return x < min ? min : (x > max ? max : x);
}

enum STATE_OFFSET {
    RESET_MY_COUNT,
    SETTINGS
};

void inc_state_offset(int n){
    const int MIN_STATE_OFFSET = 0, MAX_STATE_OFFSET = 1;
    state_offset = clip_value(state_offset + n, MIN_STATE_OFFSET, MAX_STATE_OFFSET);
    int scroll_to = 0;
    switch(state_offset){
        case RESET_MY_COUNT:
            scroll_to = 0;
            break;
        case SETTINGS:
            scroll_to = -SCR_HEIGHT;
            break;
    }
    scroll_layer_set_content_offset(scroll_layer, GPoint(0, scroll_to), true) ;
}

void down_single_click_handler(ClickRecognizerRef recognizer, void *context) {
    Window *window = (Window *)context;
    inc_state_offset(+1);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "DOWN");
}

void up_single_click_handler(ClickRecognizerRef recognizer, void *context) {
    Window *window = (Window *)context;
    inc_state_offset(-1);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "UP");
}

void select_single_click_handler(ClickRecognizerRef recognizer, void *context) {
    Window *window = (Window *)context;
    ActionMenuConfig config = (ActionMenuConfig) {
        .root_level = s_root_level,
        .align = ActionMenuAlignCenter
    };

    
    APP_LOG(APP_LOG_LEVEL_DEBUG, "OpnEning The MeNy");
    
    // Show the ActionMenu
    s_action_menu = action_menu_open(&config);
}

static void click_config_provider(Window *window){
    window_single_click_subscribe(BUTTON_ID_DOWN, down_single_click_handler);
    window_single_click_subscribe(BUTTON_ID_UP, up_single_click_handler);
    window_single_click_subscribe(BUTTON_ID_SELECT, select_single_click_handler);
}

static void update_ui(void){
    snprintf(s_nsteps, sizeof(s_nsteps), "%lu", total_steps);
    snprintf(s_distance, sizeof(s_distance), "Distance: %lum", compute_distance(total_steps));
    snprintf(s_calories, sizeof(s_calories), "Calories: %lukcal", compute_calories(total_steps));
    
    text_layer_set_text(today_steps_text_layer, s_nsteps);
    text_layer_set_text(today_distance_text_layer, s_distance);
    text_layer_set_text(today_calories_text_layer, s_calories);
}

static void number_window_set_user_info(NumberWindow *number_window, void* context){
    UserInfoEdit user_info_to_edit = (UserInfoEdit)context;
    int value = number_window_get_value(number_window);
    
    switch(user_info_to_edit){
        case ObjectiveEditSteps:
            objective_steps = value;
            break;
        case ObjectiveEditDistance:
            objective_distance = value;
            break;
        case ObjectiveEditCalories:
            objective_calories = value;
            break;
        case UserInfoEditWeight:
            user_weight = value;
            break;
        case UserInfoEditHeight:
            user_height = value;
            break;
    }
    window_stack_remove((Window*)number_window, true);
    number_window_destroy(number_window);
    update_ui();
}

static void action_clear(ActionMenu *action_menu, const ActionMenuItem *action, void *context) {
    user_height = DEFAULT_USER_HEIGHT;
    user_weight = DEFAULT_USER_WEIGHT; 
    objective_steps = DEFAULT_OBJECTIVE_STEPS;
    objective_distance = DEFAULT_OBJECTIVE_DISTANCE;
    objective_calories = DEFAULT_OBJECTIVE_CALORIES;
    total_steps = 0;
    update_ui();
}

static void action_edit_info(ActionMenu *action_menu, const ActionMenuItem *action, void *context){
    UserInfoEdit user_info_to_edit = (UserInfoEdit)action_menu_item_get_action_data(action);
    
    NumberWindowCallbacks callbacks = {
        .incremented = NULL,
        .decremented = NULL,
        .selected = number_window_set_user_info
    };
    
    char * label;
    int value;
    switch(user_info_to_edit){
        case ObjectiveEditSteps:
            label = "Steps";
            value = objective_steps;
            break;
        case ObjectiveEditDistance:
            label = "Distance (m)";
            value = objective_distance;
            break;
        case ObjectiveEditCalories:
            label = "Calories";
            value = objective_calories;
            break;
        case UserInfoEditHeight:
            label = "Height (cm)";
            value = user_height;
            break;
        case UserInfoEditWeight:
        default:
            label = "Weight (kg)";
            user_info_to_edit = UserInfoEditWeight;
            value = user_weight;
            break;
    }
    
    APP_LOG(APP_LOG_LEVEL_DEBUG, "ATTEMPT tO CREATE WINDOW");
    
    NumberWindow* number_window = number_window_create(label, callbacks, (void *) user_info_to_edit); 
    number_window_set_min(number_window, 0);
    number_window_set_value(number_window, value);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "ATTEMPT tO Push WINDOW");

    window_stack_push((Window*)number_window, false);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "YEAH");
}

static void init_ui(Window *window){
    // Create main Window element and assign to pointer
    Layer *root_layer = window_get_root_layer(main_window);  
    
    // Create scroll layer
    scroll_layer = scroll_layer_create(GRect(0, 0, SCR_WIDTH, SCR_HEIGHT));
    layer_add_child(root_layer, scroll_layer_get_layer(scroll_layer));
    scroll_layer_set_content_size(scroll_layer, GSize(SCR_WIDTH, SCR_HEIGHT * 2));
    
    // Create graph layer
    graph_layer = layer_create(GRect(0, 0, SCR_WIDTH, SCR_HEIGHT));    
    layer_set_update_proc(graph_layer, graph_layer_update_proc);   
    scroll_layer_add_child(scroll_layer, graph_layer);

    // Create today's steps text layer
    /*ssteps = "NOSTEPS";
    label_today_steps_text_layer = text_layer_create(GRect(0, 0, SCR_WIDTH, 25));
    snprintf(ssteps, SSTEPS_SIZE, "%lu", total_steps);
    text_layer_set_text(helloWorld_layer, ssteps);
    text_layer_set_text_alignment(helloWorld_layer, GTextAlignmentRight);
    scroll_layer_add_child(scroll_layer, text_layer_get_layer(helloWorld_layer));*/
    
    // helpers
    int y = 0;
    int height = 0;
    // STEPS.............................
    height = 40;
    label_today_steps_text_layer = text_layer_create(GRect(0, y, SCR_WIDTH, height));
    y += height;
    text_layer_set_text(label_today_steps_text_layer, "Steps");
    text_layer_set_font(label_today_steps_text_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
    text_layer_set_text_alignment(label_today_steps_text_layer, GTextAlignmentCenter);
    scroll_layer_add_child(scroll_layer, text_layer_get_layer(label_today_steps_text_layer));
    
    height = 40;
    today_steps_text_layer = text_layer_create(GRect(0,  y, SCR_WIDTH, height));
    y += height;
    text_layer_set_text(today_steps_text_layer, s_nsteps);
    text_layer_set_font(today_steps_text_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
    text_layer_set_text_alignment(today_steps_text_layer, GTextAlignmentCenter);
    scroll_layer_add_child(scroll_layer, text_layer_get_layer(today_steps_text_layer));
    
    // DISTANCE............................
    height = 30;
    today_distance_text_layer = text_layer_create(GRect(0, y, SCR_WIDTH, height));
    y += height;
    text_layer_set_text(today_distance_text_layer, s_distance);
    text_layer_set_font(today_distance_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_text_alignment(today_distance_text_layer, GTextAlignmentCenter);
    scroll_layer_add_child(scroll_layer, text_layer_get_layer(today_distance_text_layer));
    
    // CALORIES............................
    height = 30;
    today_calories_text_layer = text_layer_create(GRect(0, y, SCR_WIDTH, height));
    y += height;
    text_layer_set_text(today_calories_text_layer, s_calories);
    text_layer_set_font(today_calories_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_text_alignment(today_calories_text_layer, GTextAlignmentCenter);
    scroll_layer_add_child(scroll_layer, text_layer_get_layer(today_calories_text_layer));
    
    // Create step objective layer. --------
    objective_steps_layer = layer_create(GRect(10, SCR_HEIGHT + 25, SCR_WIDTH - 20, 10));
    layer_set_update_proc(objective_steps_layer, objective_steps_layer_update_proc);
    scroll_layer_add_child(scroll_layer, objective_steps_layer);
    objective_steps_text_layer = text_layer_create(GRect(10, SCR_HEIGHT + 35, SCR_WIDTH - 20, 16));
    text_layer_set_text(objective_steps_text_layer, "% Steps");
    text_layer_set_font(objective_steps_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    text_layer_set_text_alignment(objective_steps_text_layer, GTextAlignmentRight);
    scroll_layer_add_child(scroll_layer, text_layer_get_layer(objective_steps_text_layer));
    
    // Create distance objective layer. --------
    objective_distance_layer = layer_create(GRect(10, SCR_HEIGHT + 61, SCR_WIDTH - 20, 10));
    layer_set_update_proc(objective_distance_layer, objective_steps_layer_update_proc);
    scroll_layer_add_child(scroll_layer, objective_distance_layer);
    objective_distance_text_layer = text_layer_create(GRect(10, SCR_HEIGHT + 71, SCR_WIDTH - 20, 16));
    text_layer_set_text(objective_distance_text_layer, "% Distance");
    text_layer_set_font(objective_distance_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    text_layer_set_text_alignment(objective_distance_text_layer, GTextAlignmentRight);
    scroll_layer_add_child(scroll_layer, text_layer_get_layer(objective_distance_text_layer));
    
    // Create calories objective layer. --------
    objective_calories_layer = layer_create(GRect(10, SCR_HEIGHT + 97, SCR_WIDTH - 20, 10));
    layer_set_update_proc(objective_calories_layer, objective_steps_layer_update_proc);
    scroll_layer_add_child(scroll_layer, objective_calories_layer);
    objective_calories_text_layer = text_layer_create(GRect(10, SCR_HEIGHT + 107, SCR_WIDTH - 20, 16));
    text_layer_set_text(objective_calories_text_layer, "% Calories");
    text_layer_set_font(objective_calories_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    text_layer_set_text_alignment(objective_calories_text_layer, GTextAlignmentRight);
    scroll_layer_add_child(scroll_layer, text_layer_get_layer(objective_calories_text_layer));
    
    // big OBJECTIVE
    label_objective_text_layer = text_layer_create(GRect(10, 2 * SCR_HEIGHT - 35, SCR_WIDTH - 20, 35));
    text_layer_set_text(label_objective_text_layer, "Objectives");
    text_layer_set_font(label_objective_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
    text_layer_set_text_alignment(label_objective_text_layer, GTextAlignmentRight);
    scroll_layer_add_child(scroll_layer, text_layer_get_layer(label_objective_text_layer));
    
    // Action menu levels
    ActionMenuLevel *s_clear_all_level       = action_menu_level_create(1);
    ActionMenuLevel *s_edit_info_level       = action_menu_level_create(2);
    ActionMenuLevel *s_edit_objectives_level = action_menu_level_create(3);
    s_root_level                             = action_menu_level_create(3);
    
    // Root level
    action_menu_level_add_child(s_root_level, s_edit_info_level, "Edit Info");
    action_menu_level_add_child(s_root_level, s_edit_objectives_level, "Objectives");
    action_menu_level_add_child(s_root_level, s_clear_all_level, "Clear All");

    // Root level
    //// Edit Info level 
    action_menu_level_add_action(s_edit_info_level, "Height", action_edit_info, (void *)UserInfoEditHeight);
    action_menu_level_add_action(s_edit_info_level, "Weight", action_edit_info, (void *)UserInfoEditWeight);
    
    // Root level
    //// Objectives level
    action_menu_level_add_action(s_edit_objectives_level, "Steps", action_edit_info, (void *)ObjectiveEditSteps);
    action_menu_level_add_action(s_edit_objectives_level, "Distance", action_edit_info, (void *)ObjectiveEditDistance);
    action_menu_level_add_action(s_edit_objectives_level, "Calories", action_edit_info, (void *)ObjectiveEditCalories);
    
    // Root level
    //// Clear all level
    action_menu_level_add_action(s_clear_all_level, "Confirm", action_clear, (void *)NULL);
    
    
    // Button click handler
    window_set_click_config_provider(main_window, (ClickConfigProvider) click_config_provider);
    
    update_ui();
}

static void deinit_ui(Window *window) { 
    APP_LOG(APP_LOG_LEVEL_DEBUG, "DEINIT_I");
    // Destroy layers and main window 
    text_layer_destroy(label_today_steps_text_layer);
    text_layer_destroy(today_steps_text_layer);
    text_layer_destroy(today_distance_text_layer);
    text_layer_destroy(today_calories_text_layer);
    text_layer_destroy(objective_steps_text_layer);
    text_layer_destroy(objective_distance_text_layer);
    text_layer_destroy(objective_calories_text_layer);
    text_layer_destroy(label_objective_text_layer);
    
    layer_destroy(objective_steps_layer);
    layer_destroy(objective_distance_layer);
    layer_destroy(objective_calories_layer);
    
    action_menu_hierarchy_destroy(s_root_level, NULL, NULL);
}

// Init function called when app is launched
static void init(void) {
    memset(fftSamples, 0,  sizeof(fftSamples));
    
    main_window = window_create();
    
    window_set_window_handlers(main_window, (WindowHandlers) {
        .load = init_ui,
        .unload = deinit_ui,
    });
        
  	// Show the window on the watch, with animated = true
  	window_stack_push(main_window, true);

    // Allow accelerometer event
    accel_data_service_subscribe(NBSAMPLE, accel_data_handler);
    
    //Define sampling rate
    accel_service_set_sampling_rate(ACCEL_SAMPLING_FREQUENCY);
}

// deinit function called when the app is closed
static void deinit(void) {
  
    // Destroy layers and main window 
    text_layer_destroy(label_today_steps_text_layer);
    text_layer_destroy(today_steps_text_layer);
    text_layer_destroy(today_distance_text_layer);
    text_layer_destroy(today_calories_text_layer);
    text_layer_destroy(objective_steps_text_layer);
    text_layer_destroy(objective_distance_text_layer);
    text_layer_destroy(objective_calories_text_layer);
    text_layer_destroy(label_objective_text_layer);
    action_menu_hierarchy_destroy(s_root_level, NULL, NULL);
    
    window_destroy(main_window);
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
    for(uint32_t i = 0; i < SCR_WIDTH; i++){
    //    APP_LOG(APP_LOG_LEVEL_DEBUG, "SEGFAULT 2 %lu", i);
 //       points[i].x = i;
    }
    
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
                update_ui();
            }
            APP_LOG(APP_LOG_LEVEL_DEBUG, END_SPRINTF);

        }

        //vibes_short_pulse();
    }
    //layer_mark_dirty(graph_layer);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
