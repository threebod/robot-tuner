#ifndef TASK12_YYB_MOVE_H
#define TASK12_YYB_MOVE_H
#include <stdint.h>
void car_move(int vx, int vy, int w);
void change_A(int a_in);
double pingtui_control(float target, uint16_t speed, uint8_t accel);
double shengjiang_control(int target, uint16_t speed, uint8_t accel);
#endif
