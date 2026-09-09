#ifndef TASK12_PWM_H
#define TASK12_PWM_H
#include <stdint.h>
void PWM_StopAll(void);
float PWM_GetWukuaipingtaiCurrentAngle(void);
float PWM_GetDebugServoCurrentAngle(uint8_t channel);
uint8_t PWM_SetDebugServoAngle(uint8_t channel, float target, float duration_ms);
void set_zhuashou_Angle_NonBlocking(float target, float time);
void set_wukuaipingtai_Angle(float target, float time);
void set_wukuaipingtai_weizhi(int pos);
#endif
