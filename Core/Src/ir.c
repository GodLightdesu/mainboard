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

void IR_Init(I2C_Master_t *i2cMaster) {
  if (i2cMaster == NULL) { return; }
  
  g_i2cMaster = i2cMaster;

  /* Register IR slave devices with I2C master */
  I2C_Master_RegisterSlave(i2cMaster, IR_SLAVE_1_ADDR, rxBuffer1, processBuffer1, IR_BUFFER_SIZE);
  I2C_Master_RegisterSlave(i2cMaster, IR_SLAVE_2_ADDR, rxBuffer2, processBuffer2, IR_BUFFER_SIZE);

  /* Reset eye tracking data */
  maxEye = 0;
  maxValue = 0;
  memset(eyeValues, 0, sizeof(eyeValues));
}

HAL_StatusTypeDef IR_ReadData(Slave_ID slaves_id) {
  if (g_i2cMaster == NULL || slaves_id >= IR_SLAVES_NO) {
    return HAL_ERROR;
  }
  
  return I2C_Master_ReadSlave(g_i2cMaster, slaves_id);
}

bool IR_IsDataReady(Slave_ID slave_id) {
  if (g_i2cMaster == NULL) {
    return false;
  }
  return I2C_Master_IsDataReady(g_i2cMaster, slave_id);
}

void IR_ClearDataReady(Slave_ID slave_id) {
  if (g_i2cMaster == NULL) {
    return;
  }
  I2C_Master_ClearDataReady(g_i2cMaster, slave_id);
}

void updateIRValues(Slave_ID slave_id) {
  if (g_i2cMaster == NULL) { return; }
  
  /* Check if any slave has new data */
  if (!IR_IsDataReady(slave_id)) {
    return;  /* No new data from specified slave */
  }

  uint8_t *buffer = I2C_Master_GetProcessBuffer(g_i2cMaster, slave_id);
  if (buffer == NULL) { return; }

  /* Parse eye sensor values from buffer */
  for (uint8_t eye = 0; eye < EYE_NUM; eye++) {
    /* Buffer layout: [Vref_LSB, Vref_MSB, eye0_LSB, eye0_MSB, ...] */
    const uint8_t base_idx = 2 + (eye * 2);
    
    /* Bounds check */
    if (base_idx + 1 >= IR_BUFFER_SIZE) { break; }
    
    const uint8_t lsb = buffer[base_idx];
    const uint8_t msb = buffer[base_idx + 1];
    eyeValues[slave_id * EYE_NUM + eye] = combine_data(msb, lsb);
  }

  /* Reset and find maximum eye value across all sensors */
  maxValue = 0;
  maxEye = 0;
  for (uint8_t i = 0; i < (IR_SLAVES_NO * EYE_NUM); i++) {
    if (eyeValues[i] > maxValue) {
      maxValue = eyeValues[i];
      maxEye = i;
    }
  }
}