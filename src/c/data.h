#pragma once

typedef struct data_UserInfo UserInfo;

void data_init();
void data_deinit();

UserInfo* data_get_user_info(int id);

int data_get_id(UserInfo* info);

void data_set_weight(UserInfo* info, int weight);
void data_set_height(UserInfo* info, int height);
int data_get_weight(UserInfo* info);
int data_get_height(UserInfo* info);

int data_get_total_steps(UserInfo*);
void data_set_total_steps(UserInfo*, int);

int data_get_distance(UserInfo*);
int data_get_calories(UserInfo*);


void data_set_objective_steps(UserInfo*, int);
void data_set_objective_distance(UserInfo*, int);
void data_set_objective_calories(UserInfo*, int);
int data_get_objective_steps(UserInfo*);
int data_get_objective_distance(UserInfo*);
int data_get_objective_calories(UserInfo*);

void data_clear(UserInfo* info);