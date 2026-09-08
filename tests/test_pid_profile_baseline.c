#include <stdint.h>
#include <stdio.h>

#ifndef STM32F40_41xxx
#define STM32F40_41xxx
#endif

#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"
#include "yyb_move.h"
#include "imu.h"
#include "delay.h"

typedef struct {
    float kp;
    float ki;
    float kd;
    float error;
    float lastError;
    float integral;
    float maxIntegral;
    float output;
    float maxOutput;
} TestPid;

typedef struct {
    float kp;
    float ki;
    float kd;
    float max_integral;
    float max_output;
} TestPidProfile;

extern TestPid mypid;
extern TestPidProfile PID_Profiles[5];
extern uint8_t PID_move(int v_x, int v_y, int w, uint32_t time,
                        float target, int pid_choose);
extern int flag;

static int require_condition(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "%s\n", message);
    }
    return condition;
}

void TIM_Cmd(TIM_TypeDef *tim, FunctionalState state)
{
    (void)tim;
    if (state == ENABLE) {
        flag = 1;
    }
}

void USART_ITConfig(USART_TypeDef *usart, uint16_t interrupt,
                   FunctionalState state)
{
    (void)usart;
    (void)interrupt;
    (void)state;
}

void change_A(int acceleration)
{
    (void)acceleration;
    flag = 1;
}

void delay_ms1(uint32_t milliseconds)
{
    (void)milliseconds;
}

ITStatus TIM_GetITStatus(TIM_TypeDef *tim, uint16_t interrupt)
{
    (void)tim;
    (void)interrupt;
    return RESET;
}

void TIM_ClearITPendingBit(TIM_TypeDef *tim, uint16_t interrupt)
{
    (void)tim;
    (void)interrupt;
}

void change_SNA1(int acceleration, int speed)
{
    (void)acceleration;
    (void)speed;
}

void car_move(int vx, int vy, int w)
{
    (void)vx;
    (void)vy;
    (void)w;
}

void car_move_distance_x(float distance)
{
    (void)distance;
}

void car_move_distance_y(float distance)
{
    (void)distance;
}

double car_move_delay(float distance, uint16_t acceleration,
                      uint16_t speed, uint8_t direction)
{
    (void)distance;
    (void)acceleration;
    (void)speed;
    (void)direction;
    return 0.0;
}

void imu_scan(float *acceleration, float *gyro, float *angle)
{
    (void)acceleration;
    (void)gyro;
    (void)angle;
}

void RCC_APB1PeriphClockCmd(uint32_t peripheral, FunctionalState state)
{
    (void)peripheral;
    (void)state;
}

void TIM_InternalClockConfig(TIM_TypeDef *tim)
{
    (void)tim;
}

void TIM_TimeBaseInit(TIM_TypeDef *tim,
                      TIM_TimeBaseInitTypeDef *configuration)
{
    (void)tim;
    (void)configuration;
}

void TIM_ClearFlag(TIM_TypeDef *tim, uint16_t flag_value)
{
    (void)tim;
    (void)flag_value;
}

void TIM_ITConfig(TIM_TypeDef *tim, uint16_t interrupt,
                  FunctionalState state)
{
    (void)tim;
    (void)interrupt;
    (void)state;
}

void NVIC_PriorityGroupConfig(uint32_t priority_group)
{
    (void)priority_group;
}

void NVIC_Init(NVIC_InitTypeDef *configuration)
{
    (void)configuration;
}

int main(void)
{
    static const int profiles[] = { 0, 2, 3, 4 };
    static const float expected_max_output[] = {
        230.0f, 30.0f, 230.0f, 230.0f, 230.0f
    };
    unsigned int index;
    unsigned int repeat;

    for (index = 0u; index < 5u; ++index) {
        if (!require_condition(PID_Profiles[index].max_output ==
                                   expected_max_output[index],
                               "PID profile default max_output changed")) {
            return 1;
        }
    }
    for (repeat = 0u; repeat < 2u; ++repeat) {
        for (index = 0u;
             index < sizeof(profiles) / sizeof(profiles[0]); ++index) {
            if (!require_condition(
                    PID_move(0, 0, 0, 0u, 0.0f, profiles[index]) == 1u,
                    "PID_move did not complete in the profile test") ||
                !require_condition(mypid.maxOutput == 230.0f,
                                   "PID_move did not restore baseline max_output")) {
                return 1;
            }
        }
    }
    return 0;
}
