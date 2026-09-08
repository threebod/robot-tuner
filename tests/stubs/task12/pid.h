#ifndef TASK12_PID_H
#define TASK12_PID_H
typedef struct { float output; } PID;
extern PID mypid;
extern float targetValue, feedbackValue;
#endif
