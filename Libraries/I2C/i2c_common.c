#include "i2c_common.h"

/* Global bus managers (support up to 4 I2C peripherals) */
#define MAX_I2C_BUSES 4
static I2C_BusManager_t busManagers[MAX_I2C_BUSES] = {0};
static uint8_t busManagerCount = 0;

void I2C_Bus_Init(I2C_BusManager_t *manager, I2C_HandleTypeDef *hi2c) {
  if (manager == NULL || hi2c == NULL) return;
  
  manager->hi2c = hi2c;
  manager->owner = NULL;
  manager->locked = false;
  
  /* Register in global array if not already registered */
  for (uint8_t i = 0; i < busManagerCount; i++) {
    if (busManagers[i].hi2c == hi2c) {
      return;  /* Already registered */
    }
  }
  
  if (busManagerCount < MAX_I2C_BUSES) {
    busManagers[busManagerCount] = *manager;
    busManagerCount++;
  }
}

I2C_BusManager_t* I2C_Bus_GetManager(I2C_HandleTypeDef *hi2c) {
  if (hi2c == NULL) return NULL;
  
  for (uint8_t i = 0; i < busManagerCount; i++) {
    if (busManagers[i].hi2c == hi2c) {
      return &busManagers[i];
    }
  }
  
  return NULL;
}

bool I2C_Bus_TryAcquire(I2C_BusManager_t *manager, I2C_Module_t *module) {
  if (manager == NULL || module == NULL) return false;
  
  __disable_irq();
  
  /* Check if bus is free or already owned by this module */
  if (!manager->locked || manager->owner == module) {
    manager->locked = true;
    manager->owner = module;
    __enable_irq();
    return true;
  }
  
  __enable_irq();
  return false;  /* Bus is owned by another module */
}

void I2C_Bus_Release(I2C_BusManager_t *manager, I2C_Module_t *module) {
  if (manager == NULL || module == NULL) return;
  
  __disable_irq();
  
  /* Only release if this module owns the bus */
  if (manager->owner == module) {
    manager->locked = false;
    manager->owner = NULL;
  }
  
  __enable_irq();
}

void I2C_Module_Init(
  I2C_Module_t *module,
  I2C_HandleTypeDef *hi2c,
  I2C_SlaveDevice_t *slaves,
  uint8_t slaveCount,
  uint32_t pollInterval,
  I2C_DataProcessCallback_t processCallback
) {
  if (module == NULL || hi2c == NULL || slaves == NULL) return;
  
  module->hi2c = hi2c;
  module->slaves = slaves;
  module->slaveCount = slaveCount;
  module->state = I2C_STATE_IDLE;
  module->activeSlaveId = 0xFF;
  module->pollInterval = pollInterval;
  module->lastPollTime = HAL_GetTick();
  module->stateStartTime = 0;
  module->processCallback = processCallback;
}

bool I2C_Module_ReadSlave(I2C_Module_t *module, uint8_t slaveId) {
  if (module == NULL || slaveId >= module->slaveCount) return false;
  
  I2C_SlaveDevice_t *slave = &module->slaves[slaveId];
  if (!slave->enabled) return false;
  
  /* Get bus manager once and reuse throughout function */
  I2C_BusManager_t *busManager = I2C_Bus_GetManager(module->hi2c);
  if (busManager != NULL && !I2C_Bus_TryAcquire(busManager, module)) {
    /* Bus is owned by another module - wait */
    return false;
  }
  
  bool success = false;
  do {
    /* Atomic state check */
    __disable_irq();
    I2C_State_t currentState = module->state;
    __enable_irq();
    
    if (currentState != I2C_STATE_IDLE) break;
    
    /* Check HAL I2C state */
    HAL_I2C_StateTypeDef i2cState = HAL_I2C_GetState(module->hi2c);
    if (i2cState != HAL_I2C_STATE_READY) break;
    
    /* Atomically update state and active slave */
    __disable_irq();
    module->activeSlaveId = slaveId;
    module->stateStartTime = HAL_GetTick();
    module->state = I2C_STATE_READING;
    __enable_irq();
    
    HAL_StatusTypeDef status = HAL_ERROR;  /* Initialize to error state */
    
    /* If txSize == 1, use Mem_Read (register read with repeated start) */
    if (slave->txSize == 1 && slave->txBuffer != NULL) {
      uint8_t regAddr = slave->txBuffer[0];
      status = HAL_I2C_Mem_Read_DMA(
        module->hi2c,
        slave->address,
        regAddr,
        I2C_MEMADD_SIZE_8BIT,
        slave->rxBuffer,
        slave->bufferSize
      );
    } 
    /* If txSize == 0, direct read without register address */
    else if (slave->txSize == 0) {
      status = HAL_I2C_Master_Receive_DMA(
        module->hi2c,
        slave->address,
        slave->rxBuffer,
        slave->bufferSize
      );
    }
    /* Unsupported txSize */
    else {
      __disable_irq();
      module->state = I2C_STATE_ERROR;
      module->activeSlaveId = 0xFF;
      __enable_irq();
      break;
    }
    
    if (status != HAL_OK) {
      __disable_irq();
      module->state = I2C_STATE_ERROR;
      module->activeSlaveId = 0xFF;
      __enable_irq();
      
      /* Debug: Report DMA start failure */
      dataUart_PrintI2CError("I2C_Read FAIL", status, HAL_I2C_GetState(module->hi2c));
      break;
    }
    
    success = true;
  } while (0);
  
  /* Unified cleanup: release bus on failure */
  if (!success && busManager != NULL) {
    I2C_Bus_Release(busManager, module);
  }
  
  return success;
}

void I2C_Module_Process(I2C_Module_t *module) {
  if (module == NULL) return;
  
  const uint32_t currentTime = HAL_GetTick();
  
  /* Read state atomically to avoid race conditions */
  __disable_irq();
  I2C_State_t currentState = module->state;
  __enable_irq();
  
  switch (currentState) {
  case I2C_STATE_IDLE:
    /* Sequential polling: read next slave if interval elapsed (overflow-safe) */
    if (TIME_DIFF(currentTime, module->lastPollTime) >= module->pollInterval) {
      /* Find next enabled slave */
      uint8_t startId = (module->activeSlaveId == 0xFF) ? 0 : ((module->activeSlaveId + 1) % module->slaveCount);
      uint8_t nextSlaveId = startId;
      uint8_t attempts = 0;
      
      /* Skip disabled slaves (max one full loop) */
      while (attempts < module->slaveCount) {
        if (module->slaves[nextSlaveId].enabled) {
          break;  // Found an enabled slave
        }
        nextSlaveId = (nextSlaveId + 1) % module->slaveCount;
        attempts++;
      }
      
      /* Try to read the enabled slave (or retry if all disabled) */
      if (I2C_Module_ReadSlave(module, nextSlaveId)) {
        module->lastPollTime = currentTime;
      } else {
        /* Retry sooner on error */
        module->lastPollTime = currentTime - module->pollInterval + 5;
      }
    }
    break;
    
  case I2C_STATE_READING:
    /* Check for timeout (overflow-safe) */
    if (TIME_DIFF(currentTime, module->stateStartTime) > I2C_TIMEOUT_MS) {
      /* Debug: Report timeout */
      dataUart_PrintI2CError("I2C_Timeout", 0, module->activeSlaveId);
      
      /* Abort current transaction */
      if (module->activeSlaveId < module->slaveCount) {
        HAL_I2C_Master_Abort_IT(module->hi2c, module->slaves[module->activeSlaveId].address);
      }
      
      __disable_irq();
      module->state = I2C_STATE_ERROR;
      module->stateStartTime = currentTime;
      module->activeSlaveId = 0xFF;
      __enable_irq();
      
      /* Release bus on timeout */
      I2C_BusManager_t *busManager = I2C_Bus_GetManager(module->hi2c);
      if (busManager != NULL) {
        I2C_Bus_Release(busManager, module);
      }
    }
    break;
    
  case I2C_STATE_PROCESSING:
    /* Process received data (already copied in RxCallback) */
    if (module->activeSlaveId < module->slaveCount) {
      /* Call module-specific processing callback */
      if (module->processCallback != NULL) {
        module->processCallback(module, module->activeSlaveId);
      }
    }
    __disable_irq();
    module->state = I2C_STATE_IDLE;
    __enable_irq();
    
    /* Release bus after processing complete */
    I2C_BusManager_t *busManager = I2C_Bus_GetManager(module->hi2c);
    if (busManager != NULL) {
      I2C_Bus_Release(busManager, module);
    }
    break;
    
  case I2C_STATE_ERROR:
    /* Recovery from error state (overflow-safe) */
    if (TIME_DIFF(currentTime, module->stateStartTime) > I2C_ERROR_RECOVERY_MS) {
      /* Always reset I2C for clean recovery */
      HAL_I2C_DeInit(module->hi2c);
      HAL_I2C_Init(module->hi2c);
      
      __disable_irq();
      module->state = I2C_STATE_IDLE;
      module->activeSlaveId = 0xFF;
      __enable_irq();
      
      /* Release bus after recovery */
      I2C_BusManager_t *busManager = I2C_Bus_GetManager(module->hi2c);
      if (busManager != NULL) {
        I2C_Bus_Release(busManager, module);
      }
    }
    break;
  }
}

void I2C_Module_RxCallback(I2C_Module_t *module, I2C_HandleTypeDef *hi2c) {
  if (module == NULL || hi2c != module->hi2c) return;
  
  /* Atomic state check and update */
  __disable_irq();
  if (module->state != I2C_STATE_READING) {
    __enable_irq();
    return;
  }
  
  #ifdef DEBUG_I2C
  /* Debug: Confirm RX callback is called */
  static uint32_t rxCount = 0;
  rxCount++;
  if (rxCount % 50 == 0) {
    char msg[40];
    snprintf(msg, sizeof(msg), "RxCallback: %u", (unsigned int)rxCount);
    dataUart_PrintI2CStatus(msg);
  }
  #endif
  
  /* Copy DMA data immediately in interrupt to avoid overwrite */
  if (module->activeSlaveId < module->slaveCount) {
    I2C_SlaveDevice_t *slave = &module->slaves[module->activeSlaveId];
    if (slave->enabled && slave->processBuffer != NULL && slave->rxBuffer != NULL) {
      /* Cache invalidation not needed: DMA buffer is non-cacheable */
      /* Fast copy for small buffers (typically <= 16 bytes) */
      memcpy(slave->processBuffer, slave->rxBuffer, slave->bufferSize);
    }
  }
  
  /* Mark ready for processing - callback will be called in main loop */
  module->state = I2C_STATE_PROCESSING;
  __enable_irq();
}

void I2C_Module_ErrorCallback(I2C_Module_t *module, I2C_HandleTypeDef *hi2c) {
  if (module == NULL || hi2c != module->hi2c) return;
  
  /* Debug: Report I2C error code */
  dataUart_PrintI2CError("I2C_Error", hi2c->ErrorCode, module->activeSlaveId);
  
  /* Atomic error state update */
  __disable_irq();
  module->state = I2C_STATE_ERROR;
  module->stateStartTime = HAL_GetTick();  // Start error recovery timer
  module->activeSlaveId = 0xFF;
  __enable_irq();
  
  /* Release bus on error */
  I2C_BusManager_t *busManager = I2C_Bus_GetManager(module->hi2c);
  if (busManager != NULL) {
    I2C_Bus_Release(busManager, module);
  }
}

bool I2C_Find(UART_HandleTypeDef* huart, I2C_HandleTypeDef *hi2c, uint16_t addr) {
  if (hi2c == NULL) { return false; }

  if (HAL_I2C_IsDeviceReady(hi2c, addr << 1, 3, 100) == HAL_OK) {
    dataUart_PrintDeviceFound(addr);
    return true;
  }

  return false;
}

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

bool I2C_Module_SetSlaveEnabled(I2C_Module_t *module, uint8_t slaveId, bool enable) {
  if (module == NULL || slaveId >= module->slaveCount) return false;
  
  module->slaves[slaveId].enabled = enable;
  return true;
}

bool I2C_Module_IsSlaveEnabled(I2C_Module_t *module, uint8_t slaveId) {
  if (module == NULL || slaveId >= module->slaveCount) return false;
  
  return module->slaves[slaveId].enabled;
}