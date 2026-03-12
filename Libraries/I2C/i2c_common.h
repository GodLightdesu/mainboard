#ifndef I2C_COMMON_H
#define I2C_COMMON_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "i2c.h"
#include "const.h"
#include "dataPrint.h"

/* I2C timeout in milliseconds */
#define I2C_TIMEOUT_MS 300
#define I2C_ERROR_RECOVERY_MS 200  /* Increased for better bus recovery */

/** I2C state machine states */
typedef enum {
  I2C_STATE_IDLE = 0,
  I2C_STATE_READING,
  I2C_STATE_PROCESSING,
  I2C_STATE_ERROR
} I2C_State_t;

/** I2C slave device structure */
typedef struct {
  uint16_t address;
  uint8_t *txBuffer;         /**< TX buffer for register address (must be in non-cacheable RAM) */
  uint8_t *rxBuffer;         /**< RX buffer for DMA (must be in non-cacheable RAM) */
  uint8_t *processBuffer;    /**< Processing buffer (can be in normal RAM) */
  uint16_t bufferSize;       /**< RX buffer size */
  uint8_t txSize;            /**< TX data size (0 = direct read, >0 = write then read) */
  bool enabled;
} I2C_SlaveDevice_t;

/** Forward declaration of I2C module */
typedef struct I2C_Module I2C_Module_t;

/** Callback function type for data processing */
typedef void (*I2C_DataProcessCallback_t)(I2C_Module_t *module, uint8_t slaveId);

/**
 * @brief I2C bus manager for coordinating multiple modules sharing one I2C peripheral
 */
typedef struct {
  I2C_HandleTypeDef *hi2c;           /**< I2C peripheral handle */
  bool locked;                       /**< Bus lock status */
} I2C_BusManager_t;

/**
 * @brief Generic I2C module structure
 */
typedef struct I2C_Module {
  I2C_HandleTypeDef *hi2c;           /**< I2C peripheral handle */
  I2C_SlaveDevice_t *slaves;         /**< Array of slave devices */
  uint8_t slaveCount;                /**< Number of slaves */
  volatile I2C_State_t state;        /**< Current state machine state */
  volatile uint8_t activeSlaveId;    /**< Currently active slave ID */
  uint32_t stateStartTime;           /**< State start time for timeout */
  uint32_t lastPollTime;             /**< Last polling time */
  uint32_t pollInterval;             /**< Polling interval in ms */
  I2C_DataProcessCallback_t processCallback; /**< Data processing callback */
} I2C_Module_t;

/**
 * @brief Initialize I2C module
 * @param module Pointer to I2C module structure
 * @param hi2c I2C peripheral handle
 * @param slaves Array of slave devices
 * @param slaveCount Number of slaves
 * @param pollInterval Polling interval in ms
 * @param processCallback Data processing callback
 */
void I2C_Module_Init(
  I2C_Module_t *module,
  I2C_HandleTypeDef *hi2c,
  I2C_SlaveDevice_t *slaves,
  uint8_t slaveCount,
  uint32_t pollInterval,
  I2C_DataProcessCallback_t processCallback
);

/**
 * @brief Process I2C state machine (call periodically)
 * @param module Pointer to I2C module structure
 */
void I2C_Module_Process(I2C_Module_t *module);

/**
 * @brief I2C RX complete callback
 * @param module Pointer to I2C module structure
 * @param hi2c I2C handle that completed transfer
 */
void I2C_Module_RxCallback(I2C_Module_t *module, I2C_HandleTypeDef *hi2c);

/**
 * @brief I2C error callback
 * @param module Pointer to I2C module structure
 * @param hi2c I2C handle that had error
 */
void I2C_Module_ErrorCallback(I2C_Module_t *module, I2C_HandleTypeDef *hi2c);

/**
 * @brief Start reading from a specific slave
 * @param module Pointer to I2C module structure
 * @param slaveId Slave ID to read
 * @return true if read started successfully
 */
bool I2C_Module_ReadSlave(I2C_Module_t *module, uint8_t slaveId);

/**
 * @brief Enable or disable a specific slave device
 * @param module Pointer to I2C module structure
 * @param slaveId Slave ID to enable/disable
 * @param enable true to enable, false to disable
 * @return true if successful
 */
bool I2C_Module_SetSlaveEnabled(I2C_Module_t *module, uint8_t slaveId, bool enable);

/**
 * @brief Check if a specific slave device is enabled
 * @param module Pointer to I2C module structure
 * @param slaveId Slave ID to check
 * @return true if enabled, false if disabled or invalid
 */
bool I2C_Module_IsSlaveEnabled(I2C_Module_t *module, uint8_t slaveId);

bool I2C_Find(I2C_HandleTypeDef *hi2c, uint16_t addr);
uint16_t I2C_Scan(I2C_HandleTypeDef *hi2c);

/**
 * @brief Initialize I2C bus manager for shared I2C peripheral
 * @param manager Pointer to bus manager structure
 * @param hi2c I2C peripheral handle
 */
void I2C_Bus_Init(I2C_BusManager_t *manager, I2C_HandleTypeDef *hi2c);

/**
 * @brief Try to acquire I2C bus for a module
 * @param manager Pointer to bus manager
 * @return true if bus acquired, false if busy
 */
bool I2C_Bus_TryAcquire(I2C_BusManager_t *manager);

/**
 * @brief Release I2C bus
 * @param manager Pointer to bus manager
 */
void I2C_Bus_Release(I2C_BusManager_t *manager);

/**
 * @brief Get global bus manager for an I2C peripheral
 * @param hi2c I2C peripheral handle
 * @return Pointer to bus manager or NULL if not found
 */
I2C_BusManager_t* I2C_Bus_GetManager(I2C_HandleTypeDef *hi2c);

#endif /* I2C_COMMON_H */
