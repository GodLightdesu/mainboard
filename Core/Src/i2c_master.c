#include "i2c_master.h"

/* ============================================================================
 * Initialization
 * ============================================================================ */

void I2C_Master_Init(I2C_Master_t *master, I2C_HandleTypeDef *hi2c) {
  if (master == NULL || hi2c == NULL) { return; }
  
  master->hi2c = hi2c;
  master->slaveCount = 0;
  master->activeSlaveId = 0xFF;   /* No active slave */
  master->state = I2C_STATE_IDLE;
  master->stateStartTime = 0;
  master->autoRetry = true;       /* Enable auto retry by default */
  master->sequentialMode = false;
  master->nextSlaveIndex = 0;
  master->pollInterval = 20;      /* Default 20ms */
  master->lastPollTime = 0;
  
  /* Clear all slave entries */
  memset(master->slaves, 0, sizeof(master->slaves));
}

/* ============================================================================
 * Slave Control
 * ============================================================================ */

HAL_StatusTypeDef I2C_Master_ReadSlave(I2C_Master_t *master, uint8_t slaveId) {
  if (master == NULL || slaveId >= master->slaveCount) {
    return HAL_ERROR;
  }
  
  I2C_SlaveDevice_t *slave = &master->slaves[slaveId];
  if (!slave->enabled) { return HAL_ERROR; }
  
  /* Check if state machine is ready */
  if (master->state != I2C_STATE_IDLE) {
    return HAL_BUSY;
  }
  
  /* Check if I2C hardware is busy */
  if (HAL_I2C_GetState(master->hi2c) != HAL_I2C_STATE_READY) {
    return HAL_BUSY;
  }
  
  /* Mark this slave as active */
  master->activeSlaveId = slaveId;
  slave->lastAccessTime = HAL_GetTick();
  
  /* Update state machine */
  master->state = I2C_STATE_READING;
  master->stateStartTime = HAL_GetTick();
  
  /* Start DMA receive */
  HAL_StatusTypeDef status = HAL_I2C_Master_Receive_DMA(
    master->hi2c,
    slave->address,
    slave->rxBuffer,
    slave->bufferSize
  );
  
  if (status == HAL_OK) {
    master->state = I2C_STATE_WAIT_COMPLETE;
  } else {
    master->state = I2C_STATE_ERROR;
    slave->errorCount++;
  }
  
  return status;
}

void I2C_Master_SetSlaveEnabled(I2C_Master_t *master, uint8_t slaveId, bool enabled) {
  if (master == NULL || slaveId >= master->slaveCount) {
    return;
  }
  master->slaves[slaveId].enabled = enabled;
}

int8_t I2C_Master_RegisterSlave(
  I2C_Master_t *master,
  uint16_t address,
  uint8_t *rxBuffer,
  uint8_t *processBuffer, 
  uint16_t bufferSize
) {
  if (master == NULL || rxBuffer == NULL || processBuffer == NULL) {
    return -1;  /* Invalid parameters */
  }
  if (master->slaveCount >= I2C_MAX_SLAVES) {
    return -1;  /* No more slots available */
  }
  
  const uint8_t slaveId = master->slaveCount;
  I2C_SlaveDevice_t *slave = &master->slaves[slaveId];
  
  slave->address = address;
  slave->rxBuffer = rxBuffer;
  slave->processBuffer = processBuffer;
  slave->bufferSize = bufferSize;
  slave->dataReady = false;
  slave->enabled = true;
  slave->errorCount = 0;
  slave->successCount = 0;
  slave->retryCount = 0;
  slave->lastAccessTime = 0;
  
  /* Clear buffers */
  memset(slave->rxBuffer, 0, bufferSize);
  memset(slave->processBuffer, 0, bufferSize);
  
  master->slaveCount++;
  return slaveId;
}

/* ============================================================================
 * Slave Data Control
 * ============================================================================ */

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

/* ============================================================================
 * Slave Info
 * ============================================================================ */

uint32_t I2C_Master_GetErrorCount(I2C_Master_t *master, uint8_t slaveId) {
  if (master == NULL || slaveId >= master->slaveCount) {
    return 0;
  }
  return master->slaves[slaveId].errorCount;
}

uint32_t I2C_Master_GetSuccessCount(I2C_Master_t *master, uint8_t slaveId) {
  if (master == NULL || slaveId >= master->slaveCount) {
    return 0;
  }
  return master->slaves[slaveId].successCount;
}

/* ============================================================================
 * Master Control
 * ============================================================================ */

void I2C_Master_Process(I2C_Master_t *master) {
  if (master == NULL) { return; }
  
  const uint32_t currentTime = HAL_GetTick();
  
  switch (master->state) {
  case I2C_STATE_IDLE:
    /* Sequential polling mode */
    if (master->sequentialMode) {
      /* Check if it's time to poll next slave */
      if ((currentTime - master->lastPollTime) >= master->pollInterval) {
        if (I2C_Master_ReadNextSlave(master) == HAL_OK) {
          master->lastPollTime = currentTime;
        }
      }
    }
    break;
  
  case I2C_STATE_READING:
  case I2C_STATE_WAIT_COMPLETE:
    /* Check for timeout */
    if ((currentTime - master->stateStartTime) > I2C_TIMEOUT_MS) {
      /* Timeout occurred */
      if (master->activeSlaveId < master->slaveCount) {
        master->slaves[master->activeSlaveId].errorCount++;
        master->slaves[master->activeSlaveId].retryCount++;
      }
      /* Abort current transaction */
      HAL_I2C_Master_Abort_IT(master->hi2c, master->slaves[master->activeSlaveId].address);
      master->state = I2C_STATE_ERROR;
      master->activeSlaveId = 0xFF;
    }
    break;
    
  case I2C_STATE_PROCESSING:
    /* Automatically handled in callback */
    master->state = I2C_STATE_IDLE;
    break;
    
  case I2C_STATE_RETRY:
    /* Retry the transaction */
    if (master->activeSlaveId < master->slaveCount) {
      I2C_SlaveDevice_t *slave = &master->slaves[master->activeSlaveId];
      
      /* Small delay before retry */
      if ((currentTime - master->stateStartTime) > 10) {
        /* Retry read */
        master->state = I2C_STATE_READING;
        master->stateStartTime = currentTime;
        
        HAL_StatusTypeDef status = HAL_I2C_Master_Receive_DMA(
          master->hi2c,
          slave->address,
          slave->rxBuffer,
          slave->bufferSize
        );
        
        if (status == HAL_OK) {
          master->state = I2C_STATE_WAIT_COMPLETE;
        } else {
          master->state = I2C_STATE_ERROR;
          master->activeSlaveId = 0xFF;
        }
      }
    } else {
      master->state = I2C_STATE_IDLE;
    }
    break;
    
  case I2C_STATE_ERROR:
    /* Clear error state after a delay */
    if ((currentTime - master->stateStartTime) > 50) {
      master->state = I2C_STATE_IDLE;
    }
    break;
    
  default:
    master->state = I2C_STATE_IDLE;
    break;
  }
}

I2C_MasterState_t I2C_Master_GetState(I2C_Master_t *master) {
  if (master == NULL) { return I2C_STATE_ERROR; }
  return master->state;
}

void I2C_Master_ResetStats(I2C_Master_t *master, uint8_t slaveId) {
  if (master == NULL || slaveId >= master->slaveCount) {
    return;
  }
  master->slaves[slaveId].errorCount = 0;
  master->slaves[slaveId].successCount = 0;
  master->slaves[slaveId].retryCount = 0;
}

void I2C_Master_SetAutoRetry(I2C_Master_t *master, bool enable) {
  if (master == NULL) { return; }
  master->autoRetry = enable;
}

/* ============================================================================
 * Sequential Mode Control
 * ============================================================================ */

void I2C_Master_EnableSequentialMode(I2C_Master_t *master, uint32_t interval) {
  if (master == NULL) { return; }
  master->sequentialMode = true;
  master->pollInterval = (interval > 0) ? interval : 20;
  master->nextSlaveIndex = 0;
  master->lastPollTime = HAL_GetTick();
}

void I2C_Master_DisableSequentialMode(I2C_Master_t *master) {
  if (master == NULL) { return; }
  master->sequentialMode = false;
}

HAL_StatusTypeDef I2C_Master_ReadNextSlave(I2C_Master_t *master) {
  if (master == NULL || master->slaveCount == 0) {
    return HAL_ERROR;
  }
  
  /* Find next enabled slave */
  uint8_t attempts = 0;
  while (attempts < master->slaveCount) {
    uint8_t slaveId = master->nextSlaveIndex;
    
    /* Move to next slave for next call */
    master->nextSlaveIndex = (master->nextSlaveIndex + 1) % master->slaveCount;
    
    /* Check if this slave is enabled */
    if (master->slaves[slaveId].enabled) {
      /* Try to read from this slave */
      return I2C_Master_ReadSlave(master, slaveId);
    }
    
    attempts++;
  }
  
  /* No enabled slaves found */
  return HAL_ERROR;
}

/* ============================================================================
 * Callbacks
 * ============================================================================ */

void I2C_Master_RxCallback(I2C_Master_t *master, I2C_HandleTypeDef *hi2c) {
  if (master == NULL || hi2c != master->hi2c) {
    return;
  }
  
  /* Process the active slave that completed */
  if (master->activeSlaveId < master->slaveCount) {
    I2C_SlaveDevice_t *slave = &master->slaves[master->activeSlaveId];
    if (slave->enabled) {
      /* Copy from RX buffer to process buffer */
      memcpy(slave->processBuffer, slave->rxBuffer, slave->bufferSize);
      slave->dataReady = true;
      slave->successCount++;
      slave->retryCount = 0;  /* Reset retry count on success */
    }
  }
  
  /* Update state machine */
  master->state = I2C_STATE_PROCESSING;
  
  /* Clear active slave */
  master->activeSlaveId = 0xFF;
  
  /* Return to idle */
  master->state = I2C_STATE_IDLE;
}

void I2C_Master_ErrorCallback(I2C_Master_t *master, I2C_HandleTypeDef *hi2c) {
  if (master == NULL || hi2c != master->hi2c) {
    return;
  }
  
  /* Increment error count for active slave */
  if (master->activeSlaveId < master->slaveCount) {
    I2C_SlaveDevice_t *slave = &master->slaves[master->activeSlaveId];
    slave->errorCount++;
    slave->retryCount++;
    
    /* Check if should retry */
    if (master->autoRetry && slave->retryCount < I2C_MAX_RETRY_COUNT) {
      master->state = I2C_STATE_RETRY;
    } else {
      master->state = I2C_STATE_ERROR;
      slave->retryCount = 0;
      master->activeSlaveId = 0xFF;
    }
  } else {
    master->state = I2C_STATE_ERROR;
    master->activeSlaveId = 0xFF;
  }
  
  /* Error state is automatically cleared by HAL */
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

uint16_t I2C_Scan(I2C_HandleTypeDef *hi2c) {
  if (hi2c == NULL) { return 0; }
  
  /* Scan valid I2C address range (1-127) */
  for (uint16_t addr = 1; addr < 128; addr++) {
    /* Check if device responds (3 retries, 100ms timeout) */
    if (HAL_I2C_IsDeviceReady(hi2c, addr << 1, 3, 100) == HAL_OK) {
      return addr;  /* Return first found device */
    }
  }
  
  return 0;  /* No device found */
}

uint16_t combine_data(uint8_t msb, uint8_t lsb) { 
  return (uint16_t)((msb << 8) | lsb);
}