#include <stdint.h>

#include "host_debug.h"

float fAcc[3], fGyro[3], fAngle[3];
int16_t sReg[16];
float targetValue, feedbackValue;
PID mypid;

void PWM_StopAll(void) {}
void set_zhuashou_Angle_NonBlocking(float target, float time) {(void)target;(void)time;}
void set_wukuaipingtai_Angle(float target, float time) {(void)target;(void)time;}
void set_wukuaipingtai_weizhi(int pos) {(void)pos;}
void imu_scan(float *a, float *g, float *angle) {(void)a;(void)g;(void)angle;}
int32_t WHT101_ANGLEZCali(void) { return 0; }
void car_move(int vx, int vy, int w) {(void)vx;(void)vy;(void)w;}
double pingtui_control(float target, uint16_t speed, uint8_t accel) {(void)target;(void)speed;(void)accel;return 0.0;}
double shengjiang_control(int target, uint16_t speed, uint8_t accel) {(void)target;(void)speed;(void)accel;return 0.0;}
void UART5_SendArray(uint8_t *data, uint16_t length) {(void)data;(void)length;}
void UART5_TxPump(void) {}

int main(void)
{
    HostDebug_Init();
    return 0;
}
