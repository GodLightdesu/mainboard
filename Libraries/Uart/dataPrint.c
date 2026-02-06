#include "dataPrint.h"

static UART_HandleTypeDef *dataUart_huart = NULL;

void dataUart_Init(UART_HandleTypeDef *huart) {
  dataUart_huart = huart;
}

void dataUart_SendString(const char *str) {
#ifdef DEBUG_GENERAL
  if (dataUart_huart == NULL || str == NULL) {
    return;
  }
  HAL_UART_Transmit(dataUart_huart, (uint8_t*)str, strlen(str), 100);
#endif
}

void dataUart_PrintInitMessage(const char *moduleName) {
#if defined(DEBUG_MPU6050) || defined(DEBUG_MPU6050_DMP) || defined(DEBUG_IR) || defined(DEBUG_I2C) || defined(DEBUG_MOTORS)
  if (dataUart_huart == NULL || moduleName == NULL) return;
  
  char msg[50];
  int len = snprintf(msg, sizeof(msg), "%s Ready\r\n", moduleName);
  if (len > 0 && len < (int)sizeof(msg)) {
    HAL_UART_Transmit(dataUart_huart, (uint8_t*)msg, len, 100);
  }
#endif
}

void dataUart_PrintInitError(const char *errorMsg, int statusCode) {
#if defined(DEBUG_MPU6050) || defined(DEBUG_MPU6050_DMP) || defined(DEBUG_IR) || defined(DEBUG_I2C) || defined(DEBUG_MOTORS)
  if (dataUart_huart == NULL || errorMsg == NULL) return;
  
  char msg[80];
  int len = snprintf(msg, sizeof(msg), "INIT ERROR: %s (Status=0x%X)\r\n", errorMsg, statusCode);
  if (len > 0 && len < (int)sizeof(msg)) {
    HAL_UART_Transmit(dataUart_huart, (uint8_t*)msg, len, 100);
  }
#endif
}

// MPU6050 debug functions - DEPRECATED (old API)
// These functions are commented out because we now use the new MPU6050DMP API
// which doesn't provide the same data structures.
// If you need debug output for MPU6050, call MPU6050DMP_GetData() directly.
/*
void dataUart_PrintMPU6050Data(const MPU6050_t *mpuData) {
#ifdef DEBUG_MPU6050
  if (dataUart_huart == NULL || mpuData == NULL) return;
  
  char outputStr[100];
  int len = snprintf(outputStr, sizeof(outputStr), 
                     "Acc: [%.2f, %.2f, %.2f] Gyro: [%.2f, %.2f, %.2f] Temp: %.2f\r\n",
                     mpuData->ax, mpuData->ay, mpuData->az,
                     mpuData->gx, mpuData->gy, mpuData->gz,
                     mpuData->temperature);
  if (len > 0 && len < (int)sizeof(outputStr)) {
    HAL_UART_Transmit(dataUart_huart, (uint8_t*)outputStr, len, 100);
  }
#endif
}

void dataUart_PrintMPU6050Attitude(const MPU6050_DMP_t *dmpData) {
#ifdef DEBUG_MPU6050_DMP
  if (dataUart_huart == NULL || dmpData == NULL) return;
  
  char outputStr[100];
  int len = snprintf(outputStr, sizeof(outputStr), 
                     "Quat: [%.3f, %.3f, %.3f, %.3f] Euler: [%.2f, %.2f, %.2f]\r\n",
                     dmpData->quaternion.w, dmpData->quaternion.x,
                     dmpData->quaternion.y, dmpData->quaternion.z,
                     dmpData->euler.roll, dmpData->euler.pitch, dmpData->euler.yaw);
  if (len > 0 && len < (int)sizeof(outputStr)) {
    HAL_UART_Transmit(dataUart_huart, (uint8_t*)outputStr, len, 100);
  }
#endif
}
*/

// IR debug functions
HAL_StatusTypeDef ParseAndDisplayIRData(const uint8_t *data, uint16_t size) {
#ifdef DEBUG_IR
  if (dataUart_huart == NULL || data == NULL || size == 0) {
    return HAL_ERROR;
  }
  
  if (size % 2 != 0 || size > 32) {  /* Limit to 16 values max */
    return HAL_ERROR;
  }
  
  char buffer[128];
  int pos = 0;
  
  pos += snprintf(&buffer[pos], sizeof(buffer) - pos, "Decimal: ");
  
  for (uint16_t i = 0; (i + 1) < size && pos < 110; i += 2) {
    const uint16_t value = (uint16_t)((data[i+1] << 8) | data[i]);
    int written = snprintf(&buffer[pos], sizeof(buffer) - pos, "%u ", value);
    if (written < 0 || written >= (int)(sizeof(buffer) - pos)) {
      break;
    }
    pos += written;
  }
  
  if (pos < (int)sizeof(buffer) - 2) {
    buffer[pos++] = '\r';
    buffer[pos++] = '\n';
  }
  
  return HAL_UART_Transmit(dataUart_huart, (uint8_t*)buffer, pos, 100);
#else
  return HAL_OK;
#endif
}

void dataUart_PrintIRData(const IR_t *IR_Module) {
#ifdef DEBUG_IR
  if (dataUart_huart == NULL || IR_Module == NULL) return;
  
  char outputStr[300];
  // int len = snprintf(outputStr, sizeof(outputStr),
  //                    "Eye:%d Val:%d ballAngle:%f | S0:[%d,%d,%d,%d,%d,%d,%d]
  //                    S1:[%d,%d,%d,%d,%d,%d,%d]\r\n", IR_Module->maxEye,
  //                    IR_Module->maxValue, IR_Module->ballAngle,
  //                    IR_Module->eyeValues[0], IR_Module->eyeValues[1],
  //                    IR_Module->eyeValues[2], IR_Module->eyeValues[3],
  //                    IR_Module->eyeValues[4], IR_Module->eyeValues[5],
  //                    IR_Module->eyeValues[6], IR_Module->eyeValues[7],
  //                    IR_Module->eyeValues[8], IR_Module->eyeValues[9],
  //                    IR_Module->eyeValues[10], IR_Module->eyeValues[11],
  //                    IR_Module->eyeValues[12], IR_Module->eyeValues[13]);

  // simplified output
   int len = snprintf(outputStr, sizeof(outputStr), 
                     "Eye:%d Val:%d ballAngle:%f \r\n", 
                     IR_Module->maxEye, IR_Module->maxValue, IR_Module->ballAngle);
  if (len > 0 && len < (int)sizeof(outputStr)) {
    HAL_UART_Transmit(dataUart_huart, (uint8_t*)outputStr, len, 100);
  }
#endif
}

// Motor debug functions
void dataUart_PrintMotorTest(int motorId) {
#ifdef DEBUG_MOTORS
  if (dataUart_huart == NULL) return;
  
  char buffer[64];
  int len = snprintf(buffer, sizeof(buffer), "\r\n=== Testing Motor %d ===\r\n", motorId);
  if (len > 0 && len < (int)sizeof(buffer)) {
    HAL_UART_Transmit(dataUart_huart, (uint8_t*)buffer, len, 100);
  }
#endif
}

void dataUart_SendFormattedPWM(uint16_t pwm, float duty_percent) {
#ifdef DEBUG_MOTORS
  if (dataUart_huart == NULL) {
    return;
  }
  
  char buffer[64];
  int len = snprintf(buffer, sizeof(buffer), "PWM: %4u (%.1f%%)\r\n", pwm, duty_percent);
  
  if (len > 0 && len < (int)sizeof(buffer)) {
    HAL_UART_Transmit(dataUart_huart, (uint8_t*)buffer, len, 100);
  }
#endif
}

// I2C debug functions
HAL_StatusTypeDef DisplayRawHexData(const uint8_t *data, uint16_t size) {
#ifdef DEBUG_I2C
  if (dataUart_huart == NULL || data == NULL || size == 0) {
    return HAL_ERROR;
  }
  
  char buffer[128];
  int bufferPos = 0;
  
  bufferPos += snprintf(&buffer[bufferPos], sizeof(buffer) - bufferPos, "Raw: ");
  
  for (uint16_t i = 0; i < size && bufferPos < 115; i++) {
    int written = snprintf(&buffer[bufferPos], sizeof(buffer) - bufferPos, "%02x ", data[i]);
    if (written < 0 || written >= (int)(sizeof(buffer) - bufferPos)) {
      break;
    }
    bufferPos += written;
  }
  
  if (bufferPos < (int)sizeof(buffer) - 2) {
    buffer[bufferPos++] = '\r';
    buffer[bufferPos++] = '\n';
  }
  
  return HAL_UART_Transmit(dataUart_huart, (uint8_t*)buffer, bufferPos, 100);
#else
  return HAL_OK;
#endif
}

void dataUart_PrintI2CError(const char *errorType, int errorCode, int slaveId) {
#ifdef DEBUG_I2C
  if (dataUart_huart == NULL || errorType == NULL) return;
  
  char msg[60];
  int len;
  if (slaveId >= 0) {
    len = snprintf(msg, sizeof(msg), "%s: Code=0x%X, Slave=%d\r\n", 
                   errorType, errorCode, slaveId);
  } else {
    len = snprintf(msg, sizeof(msg), "%s: Code=0x%X\r\n", 
                   errorType, errorCode);
  }
  if (len > 0 && len < (int)sizeof(msg)) {
    HAL_UART_Transmit(dataUart_huart, (uint8_t*)msg, len, 100);
  }
#endif
}

void dataUart_PrintI2CStatus(const char *message) {
#ifdef DEBUG_I2C
  if (dataUart_huart == NULL || message == NULL) return;
  
  char msg[60];
  int len = snprintf(msg, sizeof(msg), "%s\r\n", message);
  if (len > 0 && len < (int)sizeof(msg)) {
    HAL_UART_Transmit(dataUart_huart, (uint8_t*)msg, len, 100);
  }
#endif
}

void dataUart_PrintDeviceFound(uint16_t addr) {
#ifdef DEBUG_I2C
  if (dataUart_huart == NULL) return;
  
  char msg[32];
  int len = snprintf(msg, sizeof(msg), "Device found at 0x%02X\r\n", addr);
  if (len > 0 && len < (int)sizeof(msg)) {
    HAL_UART_Transmit(dataUart_huart, (uint8_t*)msg, len, 100);
  }
#endif
}

// Soccer debug functions
void dataUart_PrintSoccerState(const char *stateName, float ballAngle, float ballDistance, float yawAngle) {
#ifdef DEBUG_SOCCER
  if (dataUart_huart == NULL || stateName == NULL) return;
  
  char msg[80];
  int len = snprintf(msg, sizeof(msg), "Soccer State: %s | Ball: %.1f° %.1f | Yaw: %.1f°\r\n", 
                     stateName, ballAngle, ballDistance, yawAngle);
  if (len > 0 && len < (int)sizeof(msg)) {
    HAL_UART_Transmit(dataUart_huart, (uint8_t*)msg, len, 100);
  }
#endif
}

// Button debug functions
void dataUart_PrintButtonEvent(uint8_t btnIndex, const char *buttonName, const char *eventName) {
#ifdef DEBUG_BUTTON
  if (dataUart_huart == NULL || buttonName == NULL || eventName == NULL) return;
  
  char msg[64];
  int len = snprintf(msg, sizeof(msg), "[%s] Event: %s\r\n", buttonName, eventName);
  if (len > 0 && len < (int)sizeof(msg)) {
    HAL_UART_Transmit(dataUart_huart, (uint8_t*)msg, len, 100);
  }
#endif
}

void dataUart_PrintButtonState(uint8_t btnIndex, const char *buttonName, const char *stateName) {
#ifdef DEBUG_BUTTON
  if (dataUart_huart == NULL || buttonName == NULL || stateName == NULL) return;
  
  char msg[64];
  int len = snprintf(msg, sizeof(msg), "[%s] State: %s\r\n", buttonName, stateName);
  if (len > 0 && len < (int)sizeof(msg)) {
    HAL_UART_Transmit(dataUart_huart, (uint8_t*)msg, len, 100);
  }
#endif
}