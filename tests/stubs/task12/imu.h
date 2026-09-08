#ifndef TASK12_IMU_H
#define TASK12_IMU_H
extern float fAcc[3], fGyro[3], fAngle[3];
void imu_scan(float *a, float *g, float *angle);
#endif
