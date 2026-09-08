#ifndef TASK12_USART5_H
#define TASK12_USART5_H
#include <stdint.h>
void UART5_SendArray(uint8_t *data, uint16_t length);
void UART5_TxPump(void);
#endif
