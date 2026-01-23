#include "ir.h"

/* Private variables */
static I2C_Master_t *g_i2cMaster = NULL;
static uint16_t eyeValues[IR_SLAVES_NO * EYE_NUM] = {0};

/* DMA buffers using predefined RAM_D2 addresses from const.h */
static uint8_t *rxBuffer1 = IR_SLAVE1_RXBUF_PTR;
static uint8_t *rxBuffer2 = IR_SLAVE2_RXBUF_PTR;
static uint8_t processBuffer1[IR_BUFFER_SIZE];
static uint8_t processBuffer2[IR_BUFFER_SIZE];

/* Public variables */
uint8_t maxEye = 0; 
uint16_t maxValue = 0;

static void IR_DataReadyCallback(uint8_t slaveId, uint8_t* data, uint16_t size) {
  if (data == NULL || size != IR_BUFFER_SIZE || slaveId >= IR_SLAVES_NO) {
    return;
  }
  
  /* Buffer layout: [Vref_LSB, Vref_MSB, eye0_LSB, eye0_MSB, ...] */
  const uint16_t offset = slaveId * EYE_NUM;
  for (uint8_t i = 0; i < EYE_NUM; i++) {
    const uint8_t idx = 2 + (i * 2);  // Skip Vref (first 2 bytes)
    if (idx + 1 < size) {
      eyeValues[offset + i] = combine_data(data[idx + 1], data[idx]);  // MSB, LSB
    }
  }
  
  if (slaveId == IR_SLAVES_NO - 1) {
    maxValue = 0;
    maxEye = 0;
    for (uint16_t i = 0; i < IR_SLAVES_NO * EYE_NUM; i++) {
      if (eyeValues[i] > maxValue) {
        maxValue = eyeValues[i];
        maxEye = i % EYE_NUM;
      }
    }
    
    /* Print data when all slaves have been read */
    char outputStr[100];
    int len = snprintf(outputStr, sizeof(outputStr), "Eye:%d Val:%d\r\n", maxEye, maxValue);
    HAL_UART_Transmit(&huart4, (const uint8_t *)outputStr, len, 100);
  }
}

void IR_Init(I2C_Master_t *i2cMaster) {
  if (i2cMaster == NULL) { return; }
  
  g_i2cMaster = i2cMaster;

  if (rxBuffer1 == NULL || rxBuffer2 == NULL) {
    return;
  }
  
  StatusTypeDef status1 = I2C_Master_RegisterSlave(i2cMaster, IR_SLAVE_1_ADDR, rxBuffer1, processBuffer1, IR_BUFFER_SIZE);
  StatusTypeDef status2 = I2C_Master_RegisterSlave(i2cMaster, IR_SLAVE_2_ADDR, rxBuffer2, processBuffer2, IR_BUFFER_SIZE);
  
  if (status1 != ALL_OK || status2 != ALL_OK) {
    return;
  }
  
  I2C_Master_SetSlaveCallback(i2cMaster, SLAVE_1, IR_DataReadyCallback);
  I2C_Master_SetSlaveCallback(i2cMaster, SLAVE_2, IR_DataReadyCallback);

  maxEye = 0;
  maxValue = 0;
  memset(eyeValues, 0, sizeof(eyeValues));
}