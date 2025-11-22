#include "ir.h"

/* I2C slave addresses */
#define SLAVE_1_ADDR (0x30 << 1)
#define SLAVE_2_ADDR (0x31 << 1)

/* Public variables */
uint8_t maxEye = 0; 
uint16_t maxValue = 0;

/* Private variables */
static I2C_HandleTypeDef *I2C_Handle = NULL;
static uint16_t eyeValues[SLAVES_NO * EYE_NUM] = {0};

static volatile bool DataReady[SLAVES_NO] = {false, false}; // 資料就緒標誌
uint8_t ProcessBuffer[SLAVES_NO][IR_BUFFER_SIZE] = {0}; // 處理用緩衝區
static uint8_t* RxBuffer[SLAVES_NO] = {
  IR_SLAVE1_RXBUF_PTR,
  IR_SLAVE2_RXBUF_PTR
};

void IR_Init(I2C_HandleTypeDef *hi2c) {
  I2C_Handle = hi2c;

  /* Reset eye tracking data */
  maxEye = 0;
  maxValue = 0;
  memset(eyeValues, 0, sizeof(eyeValues));
  
  /* Clear all buffers and status flags */
  for (uint8_t i = 0; i < SLAVES_NO; i++) {
    memset(ProcessBuffer[i], 0, IR_BUFFER_SIZE);
    memset(RxBuffer[i], 0, IR_BUFFER_SIZE);
    DataReady[i] = false;
  }
}

HAL_StatusTypeDef IR_ReadData(Slave_ID slaves_id) {
  if (slaves_id >= SLAVES_NO || I2C_Handle == NULL) {
    return HAL_ERROR;
  }

  /* Check if I2C is busy */
  if (HAL_I2C_GetState(I2C_Handle) != HAL_I2C_STATE_READY) {
    return HAL_BUSY;
  }

  const uint16_t devAddr = (slaves_id == SLAVE_1) ? SLAVE_1_ADDR : SLAVE_2_ADDR;
  return HAL_I2C_Master_Receive_DMA(
    I2C_Handle,
    devAddr,
    RxBuffer[slaves_id],
    IR_BUFFER_SIZE
  );
}

bool IR_SaveData(Slave_ID slave_id, uint8_t *data, uint16_t size) {
  /* Validate parameters */
  if (slave_id >= SLAVES_NO || data == NULL || size == 0) {
    return false;
  }
  
  if (DataReady[slave_id]) {
    /* Clamp size to buffer limit */
    const uint16_t copy_size = (size > IR_BUFFER_SIZE) ? IR_BUFFER_SIZE : size;
    memcpy(data, ProcessBuffer[slave_id], copy_size);

    /* Clear ready flag */
    DataReady[slave_id] = false;
    return true;
  }
  return false;  /* No new data */
}

bool IR_IsDataReady(Slave_ID slave_id) { 
  return (slave_id < SLAVES_NO) ? DataReady[slave_id] : false;
}

void IR_ClearDataReady(Slave_ID slave_id) { 
  if (slave_id < SLAVES_NO) {
    DataReady[slave_id] = false;
  }
}

uint16_t combine_data(uint8_t msb, uint8_t lsb) { 
  return (uint16_t)((msb << 8) | lsb);
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c) {
  if (hi2c != I2C_Handle) {
    return;
  }

  for (uint8_t sid = 0; sid < SLAVES_NO; sid++) {
    if (sid == SLAVE_2) continue;
    /* Copy to process buffer (atomic operation) */
    memcpy(ProcessBuffer[sid], RxBuffer[sid], IR_BUFFER_SIZE);
    DataReady[sid] = true; /* Set data ready flag */
  }
}
void updateValues(void) {
  bool hasNewData = false;
  for (uint8_t sid = 0; sid < SLAVES_NO; sid++) {
    if (sid == SLAVE_2) continue;
    if (IR_IsDataReady(sid)) {
      hasNewData = true;
      break;
    }
  }
  
  if (!hasNewData) { return; }

  /* Reset tracking variables */
  maxValue = 0;
  maxEye = 0;

  /* Extract eye values from all active slaves */
  for (uint8_t sid = 0; sid < SLAVES_NO; sid++) {
    for (uint8_t i = 0; i < EYE_NUM; i++) {
      /* Buffer layout: [Vref_LSB, Vref_MSB, eye0_LSB, eye0_MSB, ...] */
      const uint8_t base_idx = 2 + (i * 2);
      
      /* Bounds check */
      if (base_idx + 1 >= IR_BUFFER_SIZE) { break; }
      
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

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c) {
  if (hi2c == NULL) {
    return;
  }
  
  /* Identify which slave encountered error */
  for (uint8_t sid = 0; sid < SLAVES_NO; sid++) {
    if (hi2c == I2C_Handle) {
      /* Error state cleared automatically by HAL */
      /* TODO: Add error counting/logging if needed */
      break;
    }
  }
}