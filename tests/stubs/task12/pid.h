#ifndef TASK12_PID_H
#define TASK12_PID_H
typedef struct { float output; } PID;
#ifndef HOST_PID_PROFILE_T_DEFINED
#define HOST_PID_PROFILE_T_DEFINED
typedef struct {
    float kp;
    float ki;
    float kd;
    float max_integral;
    float max_output;
} PID_Profile_t;
#endif
extern PID mypid;
extern PID_Profile_t PID_Profiles[5];
extern float targetValue, feedbackValue;
#endif
