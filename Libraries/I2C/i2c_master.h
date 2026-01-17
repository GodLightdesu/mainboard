#ifndef I2C_MASTER_H
#define I2C_MASTER_H

#include "i2c.h"
#include "const.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define I2C_MAX_SLAVES 4
#define I2C_TIMEOUT_MS 100

typedef enum {
  ALL_OK = 0,
  PARAMETER = 1,
  TIMEOUT = 2,
  EXCEED_MAX_SLAVE = 3,
  BUSY = 4,
  DATA_READY = 5,
  NO_ENABLED_SLAVE = 6,
  I2C_ERROR = 7
} StatusTypeDef;

typedef enum {
  I2C_STATE_IDLE = 0,
  I2C_STATE_READING,
  I2C_STATE_PROCESSING,
  I2C_STATE_ERROR
} I2C_MasterState_t;

typedef void (*I2C_DataReadyCallback_t)(
  uint8_t slaveId, 
  uint8_t* data, 
  uint16_t size
);

typedef struct {
  bool enabled;
  uint16_t address;
  uint8_t *rxBuffer;
  uint8_t *processBuffer;
  uint16_t bufferSize;
  volatile bool dataReady;
  I2C_DataReadyCallback_t callback;
} I2C_SlaveDevice_t;

typedef struct {
  I2C_HandleTypeDef *hi2c;
  I2C_SlaveDevice_t slaves[I2C_MAX_SLAVES];
  uint8_t slaveCount;
  volatile uint8_t activeSlaveId;
  volatile I2C_MasterState_t state;
  uint32_t stateStartTime;
  bool sequentialMode;
  uint32_t pollInterval;
  uint32_t lastPollTime;
} I2C_Master_t;

void I2C_Master_Init(I2C_Master_t *master, I2C_HandleTypeDef *hi2c);

void I2C_Master_SetSlaveCallback(I2C_Master_t *master, uint8_t slaveId, I2C_DataReadyCallback_t callback);
void I2C_Master_SetSlaveEnabled(I2C_Master_t *master, uint8_t slaveId, bool enabled);
StatusTypeDef I2C_Master_ReadSlave(I2C_Master_t *master, uint8_t slaveId);
StatusTypeDef I2C_Master_Process(I2C_Master_t *master);
StatusTypeDef I2C_Master_RegisterSlave(
  I2C_Master_t* master, uint16_t address, 
  uint8_t* rxBuffer, 
  uint8_t* processBuffer, 
  uint16_t bufferSize
);

bool I2C_Master_IsDataReady(I2C_Master_t *master, uint8_t slaveId);
void I2C_Master_ClearDataReady(I2C_Master_t *master, uint8_t slaveId);
uint8_t* I2C_Master_GetProcessBuffer(I2C_Master_t *master, uint8_t slaveId);

void I2C_Master_EnableSequentialMode(I2C_Master_t *master, uint32_t interval);
void I2C_Master_DisableSequentialMode(I2C_Master_t *master);

void I2C_Master_RxCallback(I2C_Master_t *master, I2C_HandleTypeDef *hi2c);
void I2C_Master_ErrorCallback(I2C_Master_t *master, I2C_HandleTypeDef *hi2c);

uint16_t combine_data(uint8_t msb, uint8_t lsb);

#endif