#include "i2c_master.h"

void I2C_Master_Init(I2C_Master_t *master, I2C_HandleTypeDef *hi2c) {
  if (master == NULL || hi2c == NULL) { return; }
  
  master->hi2c = hi2c;
  master->slaveCount = 0;
  master->activeSlaveId = 0xFF;
  master->state = I2C_STATE_IDLE;
  master->stateStartTime = 0;
  master->sequentialMode = false;
  master->pollInterval = 20;
  master->lastPollTime = 0;
  
  memset(master->slaves, 0, sizeof(master->slaves));
}

void I2C_Master_SetSlaveCallback(I2C_Master_t *master, uint8_t slaveId, I2C_DataReadyCallback_t callback) {
  if (master == NULL || slaveId >= master->slaveCount) {
    return;
  }
  master->slaves[slaveId].callback = callback;
}

void I2C_Master_SetSlaveEnabled(I2C_Master_t *master, uint8_t slaveId, bool enabled) {
  if (master == NULL || slaveId >= master->slaveCount) {
    return;
  }
  master->slaves[slaveId].enabled = enabled;
}

StatusTypeDef I2C_Master_ReadSlave(I2C_Master_t *master, uint8_t slaveId) {
  if (master == NULL || slaveId >= master->slaveCount) {
    return PARAMETER;
  }
  
  I2C_SlaveDevice_t *slave = &master->slaves[slaveId];
  if (!slave->enabled) { return NO_ENABLED_SLAVE; }
  
  if (master->state != I2C_STATE_IDLE) {
    return BUSY;
  }
  
  HAL_I2C_StateTypeDef i2cState = HAL_I2C_GetState(master->hi2c);
  if (i2cState != HAL_I2C_STATE_READY) {
    if (i2cState == HAL_I2C_STATE_BUSY_RX || i2cState == HAL_I2C_STATE_BUSY_TX) {
      HAL_I2C_Master_Abort_IT(master->hi2c, slave->address);
    }
    return BUSY;
  }
  
  if (slave->rxBuffer == NULL || slave->bufferSize == 0) {
    return PARAMETER;
  }
  
  master->activeSlaveId = slaveId;
  master->state = I2C_STATE_READING;
  master->stateStartTime = HAL_GetTick();
  
  HAL_StatusTypeDef status = HAL_I2C_Master_Receive_DMA(
    master->hi2c,
    slave->address,
    slave->rxBuffer,
    slave->bufferSize
  );
  
  if (status == HAL_OK) {
    return ALL_OK;
  } else {
    master->state = I2C_STATE_ERROR;
    master->activeSlaveId = 0xFF;
    return I2C_ERROR;
  }
}

StatusTypeDef I2C_Master_Process(I2C_Master_t *master) {
  if (master == NULL) { return PARAMETER; }
  
  const uint32_t currentTime = HAL_GetTick();
  
  switch (master->state) {
  case I2C_STATE_IDLE:
    if (master->sequentialMode && (currentTime - master->lastPollTime) >= master->pollInterval) {
      uint8_t currentIndex = (master->activeSlaveId == 0xFF) ? 0 : (master->activeSlaveId + 1) % master->slaveCount;
      
      if (master->slaves[currentIndex].enabled) {
        StatusTypeDef status = I2C_Master_ReadSlave(master, currentIndex);
        if (status == ALL_OK) {
          master->lastPollTime = currentTime;
          return BUSY;
        }
        return status;
      }
      master->activeSlaveId = currentIndex;
    }
    return ALL_OK;
  
  case I2C_STATE_READING:
    if ((currentTime - master->stateStartTime) > I2C_TIMEOUT_MS) {
      if (master->activeSlaveId < master->slaveCount) {
        HAL_I2C_Master_Abort_IT(master->hi2c, 
                                master->slaves[master->activeSlaveId].address);
      }
      master->state = I2C_STATE_ERROR;
      master->stateStartTime = currentTime;
      master->activeSlaveId = 0xFF;
      return TIMEOUT;
    }
    return BUSY;
    
  case I2C_STATE_PROCESSING:
    if (master->activeSlaveId < master->slaveCount) {
      I2C_SlaveDevice_t *slave = &master->slaves[master->activeSlaveId];
      if (slave->enabled && slave->processBuffer != NULL && slave->rxBuffer != NULL) {
        memcpy(slave->processBuffer, slave->rxBuffer, slave->bufferSize);
        slave->dataReady = true;
        
        if (slave->callback != NULL) {
          slave->callback(master->activeSlaveId, slave->processBuffer, slave->bufferSize);
        }
      }
    }
    master->state = I2C_STATE_IDLE;
    return DATA_READY;
    
  case I2C_STATE_ERROR:
    if ((currentTime - master->stateStartTime) > 50) {
      HAL_I2C_StateTypeDef i2cState = HAL_I2C_GetState(master->hi2c);
      if (i2cState != HAL_I2C_STATE_READY) {
        HAL_I2C_DeInit(master->hi2c);
        HAL_I2C_Init(master->hi2c);
      }
      master->state = I2C_STATE_IDLE;
    }
    return I2C_ERROR;
    
  default:
    master->state = I2C_STATE_IDLE;
    master->activeSlaveId = 0xFF;
    return I2C_ERROR;
  }
}

StatusTypeDef I2C_Master_RegisterSlave(
  I2C_Master_t *master,
  uint16_t address,
  uint8_t *rxBuffer,
  uint8_t *processBuffer, 
  uint16_t bufferSize
) {
  if (master == NULL || rxBuffer == NULL || processBuffer == NULL) {
    return PARAMETER;
  }
  if (master->slaveCount >= I2C_MAX_SLAVES) {
    return EXCEED_MAX_SLAVE;
  }
  
  const uint8_t slaveId = master->slaveCount;
  I2C_SlaveDevice_t *slave = &master->slaves[slaveId];
  
  slave->address = address;
  slave->rxBuffer = rxBuffer;
  slave->processBuffer = processBuffer;
  slave->bufferSize = bufferSize;
  slave->dataReady = false;
  slave->enabled = true;
  slave->callback = NULL;
  
  memset(slave->rxBuffer, 0, bufferSize);
  memset(slave->processBuffer, 0, bufferSize);
  
  master->slaveCount++;
  return ALL_OK;
}


bool I2C_Master_IsDataReady(I2C_Master_t *master, uint8_t slaveId) {
  if (master == NULL || slaveId >= master->slaveCount) {
    return false;
  }
  return master->slaves[slaveId].dataReady;
}

void I2C_Master_ClearDataReady(I2C_Master_t *master, uint8_t slaveId) {
  if (master == NULL || slaveId >= master->slaveCount) {
    return;
  }
  master->slaves[slaveId].dataReady = false;
}

uint8_t* I2C_Master_GetProcessBuffer(I2C_Master_t *master, uint8_t slaveId) {
  if (master == NULL || slaveId >= master->slaveCount) {
    return NULL;
  }
  return master->slaves[slaveId].processBuffer;
}


void I2C_Master_EnableSequentialMode(I2C_Master_t *master, uint32_t interval) {
  if (master == NULL) { return; }
  master->sequentialMode = true;
  master->pollInterval = (interval > 0) ? interval : 20;
  master->lastPollTime = HAL_GetTick();
}

void I2C_Master_DisableSequentialMode(I2C_Master_t *master) {
  if (master == NULL) { return; }
  master->sequentialMode = false;
}

void I2C_Master_RxCallback(I2C_Master_t *master, I2C_HandleTypeDef *hi2c) {
  if (master == NULL || hi2c != master->hi2c) {
    return;
  }
  
  master->state = I2C_STATE_PROCESSING;
}

void I2C_Master_ErrorCallback(I2C_Master_t *master, I2C_HandleTypeDef *hi2c) {
  if (master == NULL || hi2c != master->hi2c) {
    return;
  }
  
  master->state = I2C_STATE_ERROR;
  master->activeSlaveId = 0xFF;
}

uint16_t combine_data(uint8_t msb, uint8_t lsb) { 
  return (uint16_t)((msb << 8) | lsb);
}