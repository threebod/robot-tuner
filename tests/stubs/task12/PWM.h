#ifndef TASK12_PWM_H
#define TASK12_PWM_H
#include <stdint.h>
void PWM_StopAll(void);
void set_zhuashou_Angle_NonBlocking(float target, float time);
void set_wukuaipingtai_Angle(float target, float time);
void set_wukuaipingtai_weizhi(int pos);
#endif
