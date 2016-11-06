#include <pebble.h>

#include "data.h"
#include "peworker.h"

#define SCR_WIDTH 144
#define SCR_HEIGHT 168

// How many users do we allow ? 
#define USER_COUNT 3

// Store scrolling offset
static int state_offset = 0;

enum STATE_OFFSET {
    HOME,
    OBJECTIVES
};

// Buffers for text layers strings
static char s_nsteps[7];
static char s_distance[20];
static char s_calories[20];
static char s_current_user[30];

// All layers
static TextLayer* label_today_steps_text_layer;
static TextLayer* today_steps_text_layer;
static TextLayer* today_distance_text_layer;
static TextLayer* today_calories_text_layer;
static TextLayer* current_user_text_layer;
static TextLayer* objective_steps_text_layer;
static TextLayer* objective_distance_text_layer;
static TextLayer* objective_calories_text_layer;
static TextLayer* label_objective_text_layer ;
static ScrollLayer *scroll_layer;
static Layer * objective_steps_layer;
static Layer * objective_distance_layer;
static Layer * objective_calories_layer;
static ActionMenu *s_action_menu;
static ActionMenuLevel *s_root_level;

// Current user
static UserInfo* user;

// Used as flags for number windows
typedef enum UserInfoEdit {
    UserInfoEditHeight,
    UserInfoEditWeight,
    ObjectiveEditSteps,
    ObjectiveEditDistance,
    ObjectiveEditCalories
} UserInfoEdit;


// Compute the progress in %
static int get_progress_for_layer(Layer * layer){
    int ret = 0;
    if(layer == objective_steps_layer)
        ret = data_get_total_steps(user) * 100 / data_get_objective_steps(user);
    else if(layer == objective_distance_layer)
        ret = data_get_distance(user) * 100 / data_get_objective_distance(user);
    else if(layer == objective_calories_layer)
        ret = data_get_calories(user) * 100 / data_get_objective_calories(user);
    return ret < 100 ? ret : 100;
}

// Update the progress bar layers for the objectives
static void objective_layer_update_proc(Layer *layer, GContext* ctx) {
    const int PADDING = 2;
    GRect bounds = layer_get_bounds(layer);
    int progress = get_progress_for_layer(layer);
    
    int fill_width =  (bounds.size.w - 2 * PADDING) * progress / 100;
    
    graphics_draw_rect(ctx, GRect(0, 0, bounds.size.w, bounds.size.h));
    graphics_fill_rect(ctx, GRect(PADDING, PADDING, fill_width, bounds.size.h - 2 * PADDING), 0, GCornerNone);
}

// Clip x between min included and max included
static int clip_value(int x, int min, int max){
    return x < min ? min : (x > max ? max : x);
}

// Increment or decrement the scroll layer offset
static void inc_state_offset(int n){
    const int MIN_STATE_OFFSET = 0, MAX_STATE_OFFSET = 1;
    state_offset = clip_value(state_offset + n, MIN_STATE_OFFSET, MAX_STATE_OFFSET);
    int scroll_to = 0;
    switch(state_offset){
        case HOME:
            scroll_to = 0;
            break;
        case OBJECTIVES:
            scroll_to = -SCR_HEIGHT;
            break;
    }
    scroll_layer_set_content_offset(scroll_layer, GPoint(0, scroll_to), true) ;
}

// Single click on down button -> scroll down
static void down_single_click_handler(ClickRecognizerRef recognizer, void *context) {
    Window *window = (Window *)context;
    inc_state_offset(+1);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "DOWN");
}

// Single click on up button -> scroll up
static void up_single_click_handler(ClickRecognizerRef recognizer, void *context) {
    Window *window = (Window *)context;
    inc_state_offset(-1);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "UP");
}

// Single click on select button -> open action menu
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

// Configure the click handlers
static void click_config_provider(Window *window){
    window_single_click_subscribe(BUTTON_ID_DOWN, down_single_click_handler);
    window_single_click_subscribe(BUTTON_ID_UP, up_single_click_handler);
    window_single_click_subscribe(BUTTON_ID_SELECT, select_single_click_handler);
}

// update the ui with new value
static void update_ui(void){
    snprintf(s_nsteps, sizeof(s_nsteps), "%d", data_get_total_steps(user));
    snprintf(s_distance, sizeof(s_distance), "Distance: %dm", data_get_distance(user));
    snprintf(s_calories, sizeof(s_calories), "Calories: %dkcal", data_get_calories(user));
    
    snprintf(s_current_user, sizeof(s_current_user), "User #%d", data_get_id(user) + 1);        
        
    text_layer_set_text(today_steps_text_layer, s_nsteps);
    text_layer_set_text(today_distance_text_layer, s_distance);
    text_layer_set_text(today_calories_text_layer, s_calories);
    text_layer_set_text(current_user_text_layer, s_current_user);
    
    layer_mark_dirty(objective_steps_layer);
    layer_mark_dirty(objective_distance_layer);
    layer_mark_dirty(objective_calories_layer);
}

// Handler called when a number window is closed
static void number_window_set_user_info(NumberWindow *number_window, void* context){
    UserInfoEdit user_info_to_edit = (UserInfoEdit)context;
    int value = number_window_get_value(number_window);
    
    switch(user_info_to_edit){
        case ObjectiveEditSteps:
            data_set_objective_steps(user, value);
            break;
        case ObjectiveEditDistance:
            data_set_objective_distance(user, value);
            break;
        case ObjectiveEditCalories:
            data_set_objective_calories(user, value);
            break;
        case UserInfoEditWeight:
            data_set_weight(user, value);
            break;
        case UserInfoEditHeight:
            data_set_height(user, value);
            break;
    }
    
    // Close and destroy the number window
    window_stack_remove((Window*)number_window, true);
    number_window_destroy(number_window);
    update_ui();
}

// Switch user
static void action_switch_user(ActionMenu *action_menu, const ActionMenuItem *action, void *context){
    int id = (UserInfoEdit)action_menu_item_get_action_data(action);
    user = data_get_user_info(id);
    
    pe_set(data_get_total_steps(user));
    
    update_ui();
}

// Clear the current user data
static void action_clear(ActionMenu *action_menu, const ActionMenuItem *action, void *context) {
    data_clear(user);
    update_ui();
}

// This action open a number window to edit a user property
static void action_edit_info(ActionMenu *action_menu, const ActionMenuItem *action, void *context){
    UserInfoEdit user_info_to_edit = (UserInfoEdit)action_menu_item_get_action_data(action);
    
    NumberWindowCallbacks callbacks = {
        .incremented = NULL,
        .decremented = NULL,
        .selected = number_window_set_user_info
    };
    
    char * label;
    int value;
    int step = 1;
    switch(user_info_to_edit){
        case ObjectiveEditSteps:
            label = "Steps";
            value = data_get_objective_steps(user);
            step = 500;
            break;
        case ObjectiveEditDistance:
            label = "Distance (m)";
            value = data_get_objective_distance(user);
            step = 500;
            break;
        case ObjectiveEditCalories:
            label = "Calories";
            value = data_get_objective_calories(user);
            step = 50;
            break;
        case UserInfoEditHeight:
            label = "Height (cm)";
            value = data_get_height(user);
            break;
        case UserInfoEditWeight:
        default:
            label = "Weight (kg)";
            user_info_to_edit = UserInfoEditWeight;
            value = data_get_weight(user);
            break;
    }
        
    NumberWindow* number_window = number_window_create(label, callbacks, (void *) user_info_to_edit); 
    number_window_set_min(number_window, step);
    number_window_set_step_size(number_window, step) ;
    number_window_set_value(number_window, value);

    window_stack_push((Window*)number_window, false);
}

// Handler which receive the updted total steps from the pedometer module
static void update_steps(int s){
    data_set_total_steps(user, s);
    update_ui();
}

// Initialize UI
void init_ui(Window *window){
    // Initialize pedometer modul
    pe_init();
    
    // Initialize data module
    data_init(USER_COUNT);
    
    // Subscribe to receive total steps
    pe_subscribe(update_steps);
    
    // Load user #1
    user = data_get_user_info(0);
    
    // Initialize pedometer total steps
    pe_set(data_get_total_steps(user));
    
    // Create main Window element and assign to pointer
    Layer *root_layer = window_get_root_layer(window);  
    
    // Create scroll layer
    scroll_layer = scroll_layer_create(GRect(0, 0, SCR_WIDTH, SCR_HEIGHT));
    layer_add_child(root_layer, scroll_layer_get_layer(scroll_layer));
    scroll_layer_set_content_size(scroll_layer, GSize(SCR_WIDTH, SCR_HEIGHT * 2));
    
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
    
    // USER............................
    height = 30;
    current_user_text_layer = text_layer_create(GRect(0, y, SCR_WIDTH, height));
    y += height;
    text_layer_set_text(current_user_text_layer, s_current_user);
    text_layer_set_font(current_user_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_text_alignment(current_user_text_layer, GTextAlignmentRight);
    scroll_layer_add_child(scroll_layer, text_layer_get_layer(current_user_text_layer));
    
    // Create step objective layer. --------
    objective_steps_layer = layer_create(GRect(10, SCR_HEIGHT + 25, SCR_WIDTH - 20, 10));
    layer_set_update_proc(objective_steps_layer, objective_layer_update_proc);
    scroll_layer_add_child(scroll_layer, objective_steps_layer);
    objective_steps_text_layer = text_layer_create(GRect(10, SCR_HEIGHT + 35, SCR_WIDTH - 20, 16));
    text_layer_set_text(objective_steps_text_layer, "% Steps");
    text_layer_set_font(objective_steps_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    text_layer_set_text_alignment(objective_steps_text_layer, GTextAlignmentRight);
    scroll_layer_add_child(scroll_layer, text_layer_get_layer(objective_steps_text_layer));
    
    // Create distance objective layer. --------
    objective_distance_layer = layer_create(GRect(10, SCR_HEIGHT + 61, SCR_WIDTH - 20, 10));
    layer_set_update_proc(objective_distance_layer, objective_layer_update_proc);
    scroll_layer_add_child(scroll_layer, objective_distance_layer);
    objective_distance_text_layer = text_layer_create(GRect(10, SCR_HEIGHT + 71, SCR_WIDTH - 20, 16));
    text_layer_set_text(objective_distance_text_layer, "% Distance");
    text_layer_set_font(objective_distance_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    text_layer_set_text_alignment(objective_distance_text_layer, GTextAlignmentRight);
    scroll_layer_add_child(scroll_layer, text_layer_get_layer(objective_distance_text_layer));
    
    // Create calories objective layer. --------
    objective_calories_layer = layer_create(GRect(10, SCR_HEIGHT + 97, SCR_WIDTH - 20, 10));
    layer_set_update_proc(objective_calories_layer, objective_layer_update_proc);
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
    ActionMenuLevel *s_switch_user           = action_menu_level_create(USER_COUNT);
    s_root_level                             = action_menu_level_create(4);
    
    // Root level
    action_menu_level_add_child(s_root_level, s_edit_info_level, "Edit Info");
    action_menu_level_add_child(s_root_level, s_edit_objectives_level, "Objectives");
    action_menu_level_add_child(s_root_level, s_switch_user, "Switch user");
    action_menu_level_add_child(s_root_level, s_clear_all_level, "Clear User");

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
    //// Switch user level
    action_menu_level_add_action(s_switch_user, "User 1", action_switch_user, (void *) 0);
    action_menu_level_add_action(s_switch_user, "User 2", action_switch_user, (void *) 1);
    action_menu_level_add_action(s_switch_user, "User 3", action_switch_user, (void *) 2);
    
    // Root level
    //// Clear all level
    action_menu_level_add_action(s_clear_all_level, "Confirm", action_clear, (void *)NULL);
    
    // Button click handler
    window_set_click_config_provider(window, (ClickConfigProvider) click_config_provider);
    
    update_ui();
}

void deinit_ui(Window *window) { 
    pe_deinit();
    data_deinit();
    
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