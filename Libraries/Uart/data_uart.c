#include "main.h"
#include "data_uart.h"
#include "string.h"

static UART_HandleTypeDef *dataUart_huart = NULL;

void dataUart_Init(UART_HandleTypeDef *huart) {
  dataUart_huart = huart;
}

void dataUart_SendString(const char *str) {
  if (dataUart_huart == NULL || str == NULL) {
    return;
  }
  HAL_UART_Transmit(dataUart_huart, (uint8_t*)str, strlen(str), 100);
}

void dataUart_SendFormattedPWM(uint16_t pwm, float duty_percent) {
  if (dataUart_huart == NULL) {
    return;
  }
  
  char buffer[64];
  int len = snprintf(buffer, sizeof(buffer), "PWM: %4u (%.1f%%)\r\n", pwm, duty_percent);
  
  if (len > 0 && len < (int)sizeof(buffer)) {
    HAL_UART_Transmit(dataUart_huart, (uint8_t*)buffer, len, 100);
  }
}

HAL_StatusTypeDef ParseAndDisplayIRData(const uint8_t *data, uint16_t size) {
  if (dataUart_huart == NULL || data == NULL || size == 0) {
    return HAL_ERROR;
  }
  
  if (size % 2 != 0) {
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
}

HAL_StatusTypeDef DisplayRawHexData(const uint8_t *data, uint16_t size) {
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
}