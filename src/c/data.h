#pragma once

/*************************************************************
 *                                                           *
 * Data module. Holds user's data. Read user's data from     *
 * persistent storage. Write user's data to persistent       *
 * storage.                                                  *
 *                                                           *
 *************************************************************/

// Holds a reference to a user.
typedef struct data_UserInfo UserInfo;

// Initialize/Deinitialize the module
// Number of users cann be specified with the user_count argument
void data_init(int user_count);
void data_deinit();

// Return reference to the user #id
UserInfo* data_get_user_info(int id);

// Return the user id
int data_get_id(UserInfo* info);

// Setters/Getters for user's height and weight
void data_set_weight(UserInfo* info, int weight);
void data_set_height(UserInfo* info, int height);
int data_get_weight(UserInfo* info);
int data_get_height(UserInfo* info);

// Setter/Getters for user's total step count
int data_get_total_steps(UserInfo*);
void data_set_total_steps(UserInfo*, int);

// Compute and return the walked distance and  burnt calories
int data_get_distance(UserInfo*);
int data_get_calories(UserInfo*);

// Setters/Getters for the user's objective
void data_set_objective_steps(UserInfo*, int);
void data_set_objective_distance(UserInfo*, int);
void data_set_objective_calories(UserInfo*, int);
int data_get_objective_steps(UserInfo*);
int data_get_objective_distance(UserInfo*);
int data_get_objective_calories(UserInfo*);

void data_clear(UserInfo* info);