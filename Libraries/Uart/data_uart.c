#include "main.h"
#include "data_uart.h"
#include "string.h"

static UART_HandleTypeDef *dataUart_huart = NULL;

void dataUart_Init(UART_HandleTypeDef *huart) {
  dataUart_huart = huart;
}

void dataUart_SendString(const char *str) {
#ifdef DEBUG_MOTORS
  if (dataUart_huart == NULL || str == NULL) {
    return;
  }
  HAL_UART_Transmit(dataUart_huart, (uint8_t*)str, strlen(str), 100);
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

/* Debug print functions */
void dataUart_PrintMPU6050Data(float ax, float ay, float az, float gx, float gy, float gz, float temp) {
#ifdef DEBUG_MPU6050
  if (dataUart_huart == NULL) return;
  
  char msg[100];
  int len = snprintf(msg, sizeof(msg), 
                     "Ax=%.2f Ay=%.2f Az=%.2f Gx=%.1f Gy=%.1f Gz=%.1f T=%.1f\r\n",
                     ax, ay, az, gx, gy, gz, temp);
  if (len > 0 && len < (int)sizeof(msg)) {
    HAL_UART_Transmit(dataUart_huart, (uint8_t*)msg, len, 100);
  }
#endif
}

void dataUart_PrintMPU6050Attitude(float roll, float pitch, float yaw) {
#ifdef DEBUG_MPU6050_DMP
  if (dataUart_huart == NULL) return;
  
  char msg[80];
  int len = snprintf(msg, sizeof(msg), 
                     "Roll=%.1f Pitch=%.1f Yaw=%.1f\r\n",
                     roll, pitch, yaw);
  if (len > 0 && len < (int)sizeof(msg)) {
    HAL_UART_Transmit(dataUart_huart, (uint8_t*)msg, len, 100);
  }
#endif
}

void dataUart_PrintIRData(uint8_t maxEye, uint16_t maxValue, const uint16_t *eyeValues) {
#ifdef DEBUG_IR
  if (dataUart_huart == NULL || eyeValues == NULL) return;
  
  char outputStr[300];
  int len = snprintf(outputStr, sizeof(outputStr), 
                     "Eye:%d Val:%d | S0:[%d,%d,%d,%d,%d,%d,%d] S1:[%d,%d,%d,%d,%d,%d,%d]\r\n",
                     maxEye, maxValue,
                     eyeValues[0], eyeValues[1], eyeValues[2], eyeValues[3],
                     eyeValues[4], eyeValues[5], eyeValues[6],
                     eyeValues[7], eyeValues[8], eyeValues[9], eyeValues[10],
                     eyeValues[11], eyeValues[12], eyeValues[13]);
  if (len > 0 && len < (int)sizeof(outputStr)) {
    HAL_UART_Transmit(dataUart_huart, (uint8_t*)outputStr, len, 100);
  }
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