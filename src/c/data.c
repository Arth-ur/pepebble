#include <pebble.h>
#include "data.h"

#define DEFAULT_USER_HEIGHT 178
#define DEFAULT_USER_WEIGHT 70
#define DEFAULT_OBJECTIVE_STEPS 10000
#define DEFAULT_OBJECTIVE_DISTANCE 10000
#define DEFAULT_OBJECTIVE_CALORIES 1000

struct data_UserInfo {
    int id;
    int weight;
    int height;
    int total_steps;
    int objective_steps;
    int objective_distance;
    int objective_calories;
};

static int user_count;
static int current_user;
static UserInfo ** data;
static int start_key = 100;

void data_init(int requested_user_count){
    int key;
    user_count = requested_user_count;
    data = calloc(user_count, sizeof(UserInfo*));
    for(int i = 0; i < user_count; i++){
        data[i] = malloc(sizeof(UserInfo));
        APP_LOG(APP_LOG_LEVEL_DEBUG, "New user %d: %p", i, data[i]);
        key = start_key + i;
        if (persist_exists(key)) {
            persist_read_data(key, data[i], sizeof(UserInfo));
        }else{
            data_clear(data[i]);
            data[i]->id = i;
            persist_write_data(key, data[i], sizeof(UserInfo));
        }
    }
}

void data_deinit(){
    int key;
    for(int i = 0; i < user_count; i++){
        key = start_key + i;
        persist_write_data(key, data[i], sizeof(UserInfo));
        free(data[i]);
    }
    free(data);
}

UserInfo* data_get_user_info(int id){
    return data[id];
}

int data_get_id(UserInfo* info){
    return info->id;
}

void data_set_weight(UserInfo* info, int weight){
    info->weight = weight;
}

void data_set_height(UserInfo* info, int height){
    info->height = height;
}

int data_get_weight(UserInfo* info){
    return info->weight;
}

int data_get_height(UserInfo* info){
    return info->height;
}

int data_get_total_steps(UserInfo* info){
    return info->total_steps;
}

void data_set_total_steps(UserInfo* info, int total_steps){
    info->total_steps = total_steps;
}

int data_get_distance(UserInfo* info){
    int data_compute_distance(UserInfo* info);
    return data_compute_distance(info);
}

int data_get_calories(UserInfo* info){
    int data_compute_calories(UserInfo* info);
    return data_compute_calories(info);
}

void data_set_objective_steps(UserInfo* info, int objective){
    info->objective_steps = objective;
}

void data_set_objective_distance(UserInfo* info, int objective){
    info->objective_distance = objective;
}

void data_set_objective_calories(UserInfo* info, int objective){
    info->objective_calories = objective;
}

int data_get_objective_steps(UserInfo* info){
    return info->objective_steps;
}

int data_get_objective_distance(UserInfo* info){
    return info->objective_distance;    
}

int data_get_objective_calories(UserInfo* info){
    return info->objective_calories;
}

int data_compute_distance(UserInfo* info){
    const float walkingFactor = 0.57;
    float strip; 
    float distance; 
    strip = info->height * 0.415; 
    distance = (info->total_steps * strip) / 100;
    
    return distance;
}

int data_compute_calories(UserInfo* info){
    const float walkingFactor = 0.57; 
    float caloriesBurnedPerMile; 
    float strip; 
    float stepCountMile;
    float conversationFactor; 
    float caloriesBurned; 
    caloriesBurnedPerMile = walkingFactor * (info->weight * 2.2); 
    strip = info->height * 0.415; 
    stepCountMile = 160934.4 / strip; 
    conversationFactor = caloriesBurnedPerMile / stepCountMile;
    caloriesBurned = info->total_steps * conversationFactor;

    return caloriesBurned;
}

void data_clear(UserInfo* info){
    info->weight = DEFAULT_USER_WEIGHT;
    info->height = DEFAULT_USER_HEIGHT;
    info->total_steps = 0;
    info->objective_steps = DEFAULT_OBJECTIVE_STEPS;
    info->objective_distance = DEFAULT_OBJECTIVE_DISTANCE;
    info->objective_calories = DEFAULT_OBJECTIVE_CALORIES;
}