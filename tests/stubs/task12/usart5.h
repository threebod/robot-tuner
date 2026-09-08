#ifndef TASK12_USART5_H
#define TASK12_USART5_H
#include <stdint.h>
uint8_t UART5_SendArray(const uint8_t *data, uint16_t length);
uint8_t UART5_SendString(const char *data);
#endif
