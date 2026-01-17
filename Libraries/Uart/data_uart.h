#ifndef DATA_UART_H
#define DATA_UART_H

#include "usart.h"
#include <stdio.h>
#include <stdint.h>

void dataUart_Init(UART_HandleTypeDef *huart);
void dataUart_SendString(const char *str);
void dataUart_SendFormattedPWM(uint16_t pwm, float duty_percent);

HAL_StatusTypeDef ParseAndDisplayIRData(const uint8_t *data, uint16_t size);
HAL_StatusTypeDef DisplayRawHexData(const uint8_t *data, uint16_t size);

#endif // DATA_UART_H