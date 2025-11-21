#include "ir.h"

/* I2C slave addresses */
#define SLAVE_1_ADDR (0x30 << 1)
#define SLAVE_2_ADDR (0x31 << 1)

/* Private variables */
static I2C_HandleTypeDef *I2C_Handle[SLAVES_NO] = { NULL, NULL };

/* Public variables */
uint8_t maxEye = 0;
uint16_t maxValue = 0;

/* Private variables */
static uint16_t eyeValues[SLAVES_NO * EYE_NUM] = {0};

static volatile uint8_t DataReady[SLAVES_NO] = {0}; // 資料就緒標誌
static uint8_t* RxBuffer[SLAVES_NO] = {
  (uint8_t *)DMA_BUFFER_ADDRESS,
  (uint8_t *)DMA_BUFFER_ADDRESS + IR_BUFFER_SIZE
};
uint8_t ProcessBuffer[SLAVES_NO][IR_BUFFER_SIZE] = {0}; // 處理用緩衝區

void IR_Init(I2C_HandleTypeDef *hi2c1, I2C_HandleTypeDef *hi2c2) {
  I2C_Handle[SLAVE_1] = hi2c1;
  I2C_Handle[SLAVE_2] = hi2c2;
  
  /* Clear all buffers and status flags */
  for (uint8_t i = 0; i < SLAVES_NO; i++) {
    memset(RxBuffer[i], 0, IR_BUFFER_SIZE);
    memset(ProcessBuffer[i], 0, IR_BUFFER_SIZE);
    DataReady[i] = 0;
  }
  
  /* Reset eye tracking data */
  maxEye = 0;
  maxValue = 0;
  memset(eyeValues, 0, sizeof(eyeValues));
}

HAL_StatusTypeDef IR_ReadData(Slave_ID slaves_id) {
  /* Validate slave ID */
  if (slaves_id >= SLAVES_NO) {
    return HAL_ERROR;
  }
  
  /* Check handle validity */
  if (I2C_Handle[slaves_id] == NULL) { 
    return HAL_ERROR; 
  }

  /* Check if I2C is busy */
  if (HAL_I2C_GetState(I2C_Handle[slaves_id]) != HAL_I2C_STATE_READY) {
    return HAL_BUSY;
  }

  /* Determine slave address */
  const uint16_t devAddr = (slaves_id == SLAVE_1) ? SLAVE_1_ADDR : SLAVE_2_ADDR;
  
  /* Initiate DMA receive */
  return HAL_I2C_Master_Receive_DMA(
    I2C_Handle[slaves_id],
    devAddr,
    RxBuffer[slaves_id],
    IR_BUFFER_SIZE
  );
}

uint8_t IR_SaveData(Slave_ID slave_id, uint8_t *data, uint16_t size) {
  /* Validate parameters */
  if (slave_id >= SLAVES_NO || data == NULL || size == 0) {
    return 0;
  }
  
  if (DataReady[slave_id]) {
    /* Clamp size to buffer limit */
    const uint16_t copy_size = (size > IR_BUFFER_SIZE) ? IR_BUFFER_SIZE : size;
    memcpy(data, ProcessBuffer[slave_id], copy_size);

    /* Clear ready flag */
    DataReady[slave_id] = 0;
    return 1;
  }
  return 0;  /* No new data */
}

uint8_t IR_IsDataReady(Slave_ID slave_id) { 
  return (slave_id < SLAVES_NO) ? DataReady[slave_id] : 0;
}

void IR_ClearDataReady(Slave_ID slave_id) { 
  if (slave_id < SLAVES_NO) {
    DataReady[slave_id] = 0;
  }
}

uint16_t combine_data(uint8_t msb, uint8_t lsb) { 
  return (uint16_t)((msb << 8) | lsb);
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c) {
  if (hi2c == NULL) {
    return;
  }
  
  for (uint8_t sid = 0; sid < SLAVES_NO; sid++) {
    // if (hi2c == I2C_Handle[sid] && I2C_Handle[sid] != NULL) {
    if (hi2c == I2C_Handle[sid]) {
      /* Copy to process buffer (atomic operation) */
      memcpy(ProcessBuffer[sid], RxBuffer[sid], IR_BUFFER_SIZE);
      
      /* Set data ready flag */
      DataReady[sid] = 1;
      
      break;
    }
  }
}
void updateValues(void) {
  /* Require at least SLAVE_1 to have new data */
  if (!IR_IsDataReady(SLAVE_1)) {
    return;
  }

  /* Reset tracking variables */
  maxValue = 0;
  maxEye = 0;

  /* Extract eye values from all active slaves */
  for (uint8_t sid = 0; sid < SLAVES_NO; sid++) {
    /* Skip inactive slaves */
    if (I2C_Handle[sid] == NULL) {
      continue;
    }
    
    for (uint8_t i = 0; i < EYE_NUM; i++) {
      /* Buffer layout: [Vref_LSB, Vref_MSB, eye0_LSB, eye0_MSB, ...] */
      const uint8_t base_idx = 2 + (i * 2);
      
      /* Bounds check */
      if (base_idx + 1 >= IR_BUFFER_SIZE) {
        break;
      }
      
      const uint8_t lsb = ProcessBuffer[sid][base_idx];
      const uint8_t msb = ProcessBuffer[sid][base_idx + 1];
      eyeValues[sid * EYE_NUM + i] = combine_data(msb, lsb);
    }
  }

  /* Find maximum eye value across all sensors */
  for (uint8_t i = 0; i < (SLAVES_NO * EYE_NUM); i++) {
    if (eyeValues[i] > maxValue) {
      maxValue = eyeValues[i];
      maxEye = i;
    }
  }
}

/**
 * @brief   I2C Error Callback
 * @param   hi2c: Pointer to I2C handle
 * @note    Called from interrupt context
 * @details Error state is cleared automatically; next request will retry
 */
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c) {
  if (hi2c == NULL) {
    return;
  }
  
  /* Identify which slave encountered error */
  for (uint8_t sid = 0; sid < SLAVES_NO; sid++) {
    if (hi2c == I2C_Handle[sid]) {
      /* Error state cleared automatically by HAL */
      /* TODO: Add error counting/logging if needed */
      break;
    }
  }
}