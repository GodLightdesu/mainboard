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

/* Debug print functions */
void dataUart_PrintMPU6050Data(float ax, float ay, float az, float gx, float gy, float gz, float temp);
void dataUart_PrintMPU6050Attitude(float roll, float pitch, float yaw);
void dataUart_PrintIRData(uint8_t maxEye, uint16_t maxValue, const uint16_t *eyeValues);
void dataUart_PrintI2CError(const char *errorType, int errorCode, int slaveId);
void dataUart_PrintI2CStatus(const char *message);
void dataUart_PrintMotorTest(int motorId);
void dataUart_PrintDeviceFound(uint16_t addr);
void dataUart_PrintInitMessage(const char *moduleName);

#endif // DATA_UART_H