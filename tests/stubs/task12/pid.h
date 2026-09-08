#ifndef TASK12_PID_H
#define TASK12_PID_H
typedef struct { float output; } PID;
typedef struct {
    float kp;
    float ki;
    float kd;
    float max_integral;
    float max_output;
} PID_Profile_t;
extern PID mypid;
extern PID_Profile_t PID_Profiles[5];
extern float targetValue, feedbackValue;
#endif
