#ifndef I2C_MASTER_H
#define I2C_MASTER_H

#include "i2c.h"
#include "const.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Configuration */
#define I2C_MAX_SLAVES 4          /**< Maximum number of I2C slave devices */
#define I2C_MAX_RETRY_COUNT 3     /**< Maximum retry attempts */
#define I2C_TIMEOUT_MS 100        /**< Transaction timeout in ms */

/* I2C Master state machine */
typedef enum {
  I2C_STATE_IDLE = 0,        /**< Idle, ready for new transaction */
  I2C_STATE_READING,         /**< Reading from slave */
  I2C_STATE_WAIT_COMPLETE,   /**< Waiting for DMA completion */
  I2C_STATE_PROCESSING,      /**< Processing received data */
  I2C_STATE_ERROR,           /**< Error state */
  I2C_STATE_RETRY            /**< Retrying after error */
} I2C_MasterState_t;

/* I2C slave device structure */
typedef struct {
  uint16_t address;           /**< I2C 7-bit address (left-shifted) */
  uint8_t *rxBuffer;          /**< DMA receive buffer pointer */
  uint8_t *processBuffer;     /**< Processing buffer pointer */
  uint16_t bufferSize;        /**< Buffer size in bytes */
  volatile bool dataReady;    /**< Data ready flag */
  bool enabled;               /**< Device enabled status */
  uint32_t errorCount;        /**< Error counter */
  uint32_t successCount;      /**< Success counter */
  uint8_t retryCount;         /**< Current retry count */
  uint32_t lastAccessTime;    /**< Last access timestamp (HAL_GetTick) */
} I2C_SlaveDevice_t;

/* I2C Master handle structure */
typedef struct {
  I2C_HandleTypeDef *hi2c;                  /**< HAL I2C handle */
  I2C_SlaveDevice_t slaves[I2C_MAX_SLAVES]; /**< Registered slave devices */
  uint8_t slaveCount;                       /**< Number of registered slaves */
  volatile uint8_t activeSlaveId;           /**< Currently active slave ID */
  volatile I2C_MasterState_t state;         /**< Current state machine state */
  uint32_t stateStartTime;                  /**< State entry timestamp */
  bool autoRetry;                           /**< Enable automatic retry on error */
  bool sequentialMode;                      /**< Enable sequential polling mode */
  uint8_t nextSlaveIndex;                   /**< Next slave to poll in sequence */
  uint32_t pollInterval;                    /**< Polling interval in ms */
  uint32_t lastPollTime;                    /**< Last poll timestamp */
} I2C_Master_t;

/* Public functions */
void I2C_Master_Init(I2C_Master_t *master, I2C_HandleTypeDef *hi2c);

/* Slave control */
HAL_StatusTypeDef I2C_Master_ReadSlave(I2C_Master_t *master, uint8_t slaveId);
void I2C_Master_SetSlaveEnabled(I2C_Master_t *master, uint8_t slaveId, bool enabled);
int8_t I2C_Master_RegisterSlave(
  I2C_Master_t* master,
  uint16_t      address, 
  uint8_t*      rxBuffer, 
  uint8_t*      processBuffer, 
  uint16_t      bufferSize
);

/* Slave Data control */
bool I2C_Master_IsDataReady(I2C_Master_t *master, uint8_t slaveId);
void I2C_Master_ClearDataReady(I2C_Master_t *master, uint8_t slaveId);
uint8_t* I2C_Master_GetProcessBuffer(I2C_Master_t *master, uint8_t slaveId);

/* Slave info */
uint32_t I2C_Master_GetErrorCount(I2C_Master_t *master, uint8_t slaveId);
uint32_t I2C_Master_GetSuccessCount(I2C_Master_t *master, uint8_t slaveId);

/* Master control */
void I2C_Master_Process(I2C_Master_t *master);
I2C_MasterState_t I2C_Master_GetState(I2C_Master_t *master);
void I2C_Master_ResetStats(I2C_Master_t *master, uint8_t slaveId);
void I2C_Master_SetAutoRetry(I2C_Master_t *master, bool enable);

/* Sequential mode control */
void I2C_Master_EnableSequentialMode(I2C_Master_t *master, uint32_t interval);
void I2C_Master_DisableSequentialMode(I2C_Master_t *master);
HAL_StatusTypeDef I2C_Master_ReadNextSlave(I2C_Master_t *master);

/* Callbacks */
void I2C_Master_RxCallback(I2C_Master_t *master, I2C_HandleTypeDef *hi2c);
void I2C_Master_ErrorCallback(I2C_Master_t *master, I2C_HandleTypeDef *hi2c);

/* Utility Functions */
uint16_t I2C_Scan(I2C_HandleTypeDef *hi2c);
uint16_t combine_data(uint8_t msb, uint8_t lsb);

#endif  // I2C_MASTER_H