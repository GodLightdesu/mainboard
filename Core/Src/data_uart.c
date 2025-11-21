#include "data_uart.h"
#include "stm32h7xx_hal_def.h"

static UART_HandleTypeDef *dataUart_huart = NULL;

void dataUart_Init(UART_HandleTypeDef *huart) {
  dataUart_huart = huart;
}

HAL_StatusTypeDef ParseAndDisplayIRData(const uint8_t *data, uint16_t size) {
  /* Validate parameters */
  if (dataUart_huart == NULL || data == NULL || size == 0) {
    return HAL_ERROR;
  }
  
  char buffer[128];
  int pos = 0;
  
  /* Add prefix */
  pos += snprintf(&buffer[pos], sizeof(buffer) - pos, "Decimal: ");
  
  /* Parse each 2-byte pair (little-endian) */
  for (uint16_t i = 0; (i + 1) < size && pos < 110; i += 2) {
    const uint16_t value = (uint16_t)((data[i+1] << 8) | data[i]);
    pos += snprintf(&buffer[pos], sizeof(buffer) - pos, "%u ", value);
  }
  
  /* Add line ending */
  if (pos < (int)sizeof(buffer) - 2) {
    buffer[pos++] = '\r';
    buffer[pos++] = '\n';
  }
  
  /* Transmit with timeout */
  return HAL_UART_Transmit(dataUart_huart, (uint8_t*)buffer, pos, 100);
}

HAL_StatusTypeDef DisplayRawHexData(const uint8_t *data, uint16_t size) {
  /* Validate parameters */
  if (dataUart_huart == NULL || data == NULL || size == 0) {
    return HAL_ERROR;
  }
  
  char buffer[128];
  int bufferPos = 0;
  
  /* Add prefix */
  bufferPos += snprintf(&buffer[bufferPos], sizeof(buffer) - bufferPos, "Raw: ");
  
  /* Convert bytes to hex (3 chars per byte: "XX ") */
  for (uint16_t i = 0; i < size && bufferPos < 115; i++) {
    bufferPos += snprintf(&buffer[bufferPos], sizeof(buffer) - bufferPos, "%02x ", data[i]);
  }
  
  /* Add line ending */
  if (bufferPos < (int)sizeof(buffer) - 2) {
    buffer[bufferPos++] = '\r';
    buffer[bufferPos++] = '\n';
  }
  
  /* Transmit with timeout */
  return HAL_UART_Transmit(dataUart_huart, (uint8_t*)buffer, bufferPos, 100);
}