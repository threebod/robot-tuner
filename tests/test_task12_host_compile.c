#include <stdint.h>

#include "host_debug.h"
#include "pid.h"

float fAcc[3], fGyro[3], fAngle[3];
int16_t sReg[16];
float targetValue, feedbackValue;
PID mypid;
PID_Profile_t PID_Profiles[5];

void PWM_StopAll(void) {}
float PWM_GetWukuaipingtaiCurrentAngle(void) { return 20.0f; }
float PWM_GetDebugServoCurrentAngle(uint8_t channel)
{
    (void)channel;
    return 0.0f;
}
uint8_t PWM_SetDebugServoAngle(uint8_t channel, float target,
                               float duration_ms)
{
    (void)channel;
    (void)target;
    (void)duration_ms;
    return 1u;
}
void set_zhuashou_Angle_NonBlocking(float target, float time) {(void)target;(void)time;}
void set_wukuaipingtai_Angle(float target, float time) {(void)target;(void)time;}
void set_wukuaipingtai_weizhi(int pos) {(void)pos;}
void imu_scan(float *a, float *g, float *angle) {(void)a;(void)g;(void)angle;}
int32_t WHT101_ANGLEZCali(void) { return 0; }
void car_move(int vx, int vy, int w) {(void)vx;(void)vy;(void)w;}
void change_A(int a_in) {(void)a_in;}
double pingtui_control(float target, uint16_t speed, uint8_t accel) {(void)target;(void)speed;(void)accel;return 0.0;}
double shengjiang_control(int target, uint16_t speed, uint8_t accel) {(void)target;(void)speed;(void)accel;return 0.0;}
uint8_t UART5_SendArray(const uint8_t *data, uint16_t length) {(void)data;(void)length;return 1u;}
uint8_t UART5_SendString(const char *data) {(void)data;return 1u;}

int main(void)
{
    HostDebug_Init();
    return 0;
}
