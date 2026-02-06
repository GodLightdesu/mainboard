#ifndef DATAPRINT_H
#define DATAPRINT_H

#include "main.h"
#include "usart.h"
#include "string.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "ir.h"
// Forward declaration to avoid circular include
struct Button_t;
typedef struct Button_t Button_t;

void dataUart_Init(UART_HandleTypeDef *huart);
void dataUart_SendString(const char *str);

/* Debug print functions */
void dataUart_PrintInitMessage(const char *moduleName);
void dataUart_PrintInitError(const char *errorMsg, int statusCode);

// IR debug functions
HAL_StatusTypeDef ParseAndDisplayIRData(const uint8_t *data, uint16_t size);
void dataUart_PrintIRData(const IR_t *IR_Module);

// Motor debug functions
void dataUart_PrintMotorTest(int motorId);
void dataUart_SendFormattedPWM(uint16_t pwm, float duty_percent);

// I2C debug functions
HAL_StatusTypeDef DisplayRawHexData(const uint8_t *data, uint16_t size);
void dataUart_PrintI2CError(const char *errorType, int errorCode, int slaveId);
void dataUart_PrintI2CStatus(const char *message);
void dataUart_PrintDeviceFound(uint16_t addr);

// Soccer debug functions
void dataUart_PrintSoccerState(const char *stateName, float ballAngle, float ballDistance, float yawAngle);

// Button debug functions
void dataUart_PrintButtonEvent(uint8_t btnIndex, const char *buttonName, const char *eventName);
void dataUart_PrintButtonState(uint8_t btnIndex, const char *buttonName, const char *stateName);

#endif // DATAPRINT_H