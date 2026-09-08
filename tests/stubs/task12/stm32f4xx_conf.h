#ifndef TASK12_STM32F4XX_CONF_H
#define TASK12_STM32F4XX_CONF_H

#include <stdint.h>

typedef struct { uint32_t GPIO_Pin, GPIO_Mode, GPIO_OType, GPIO_Speed, GPIO_PuPd; } GPIO_InitTypeDef;
typedef struct { uint32_t USART_BaudRate, USART_WordLength, USART_StopBits, USART_Parity, USART_HardwareFlowControl, USART_Mode; } USART_InitTypeDef;
typedef struct { uint32_t NVIC_IRQChannel, NVIC_IRQChannelPreemptionPriority, NVIC_IRQChannelSubPriority, NVIC_IRQChannelCmd; } NVIC_InitTypeDef;
typedef struct { uint32_t TIM_Prescaler, TIM_Period, TIM_ClockDivision, TIM_CounterMode, TIM_RepetitionCounter; } TIM_TimeBaseInitTypeDef;

#define ENABLE 1u
#define RESET 0u
#define SET 1u
#define GPIOB ((void *)0x1)
#define USART3 ((void *)0x2)
#define UART5 ((void *)0x3)
#define TIM7 ((void *)0x4)
#define RCC_AHB1Periph_GPIOB 1u
#define RCC_APB1Periph_USART3 2u
#define RCC_APB1Periph_TIM7 3u
#define GPIO_PinSource10 10u
#define GPIO_PinSource11 11u
#define GPIO_Pin_10 10u
#define GPIO_Pin_11 11u
#define GPIO_AF_USART3 7u
#define GPIO_Mode_AF 1u
#define GPIO_OType_PP 1u
#define GPIO_Speed_50MHz 50u
#define GPIO_PuPd_UP 1u
#define USART_Mode_Rx 1u
#define USART_Mode_Tx 2u
#define USART_IT_RXNE 1u
#define USART_IT_TXE 4u
#define USART_FLAG_TXE 2u
#define USART_FLAG_TC 3u
#define USART3_IRQn 39u
#define TIM7_IRQn 55u
#define TIM_IT_Update 1u
#define TIM_FLAG_Update 1u
#define TIM_CKD_DIV1 1u
#define TIM_CounterMode_Up 1u

static inline void RCC_AHB1PeriphClockCmd(uint32_t p, uint32_t e) {(void)p;(void)e;}
static inline void RCC_APB1PeriphClockCmd(uint32_t p, uint32_t e) {(void)p;(void)e;}
static inline void GPIO_PinAFConfig(void *p, uint32_t s, uint32_t a) {(void)p;(void)s;(void)a;}
static inline void GPIO_Init(void *p, GPIO_InitTypeDef *v) {(void)p;(void)v;}
static inline void USART_StructInit(USART_InitTypeDef *v) {(void)v;}
static inline void USART_Init(void *p, USART_InitTypeDef *v) {(void)p;(void)v;}
static inline void NVIC_Init(NVIC_InitTypeDef *v) {(void)v;}
static inline void USART_ITConfig(void *p, uint32_t i, uint32_t e) {(void)p;(void)i;(void)e;}
static inline void USART_Cmd(void *p, uint32_t e) {(void)p;(void)e;}
static inline uint32_t USART_GetFlagStatus(void *p, uint32_t f) {(void)p;(void)f;return SET;}
static inline uint16_t USART_ReceiveData(void *p) {(void)p;return 0u;}
static inline void USART_SendData(void *p, uint16_t d) {(void)p;(void)d;}
static inline uint32_t USART_GetITStatus(void *p, uint32_t i) {(void)p;(void)i;return RESET;}
static inline void USART_ClearITPendingBit(void *p, uint32_t i) {(void)p;(void)i;}
static inline void TIM_InternalClockConfig(void *p) {(void)p;}
static inline void TIM_TimeBaseStructInit(TIM_TimeBaseInitTypeDef *v) {(void)v;}
static inline void TIM_TimeBaseInit(void *p, TIM_TimeBaseInitTypeDef *v) {(void)p;(void)v;}
static inline void TIM_ClearFlag(void *p, uint32_t f) {(void)p;(void)f;}
static inline void TIM_ITConfig(void *p, uint32_t i, uint32_t e) {(void)p;(void)i;(void)e;}
static inline void TIM_Cmd(void *p, uint32_t e) {(void)p;(void)e;}
static inline uint32_t TIM_GetITStatus(void *p, uint32_t i) {(void)p;(void)i;return RESET;}
static inline void TIM_ClearITPendingBit(void *p, uint32_t i) {(void)p;(void)i;}
static inline uint32_t __get_PRIMASK(void) { return 0u; }
static inline void __disable_irq(void) {}
static inline void __set_PRIMASK(uint32_t p) {(void)p;}

#endif
