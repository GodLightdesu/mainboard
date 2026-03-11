#ifndef XSOUND_H
#define XSOUND_H

/* Configuration constants */
#define XSOUND_NUM_DISTANCES 4  /**< Number of distance measurements */

/* I2C slave address */
#define XSOUND_ADDR (0x50 << 1)

#include "i2c_common.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/**
 * @brief Xsound module structure
 */
typedef struct Xsound_t {
  bool dataReady;                          /**< Flag indicating if new data is available */
  float distances[XSOUND_NUM_DISTANCES];   /**< Distance measurements in meters/centimeters */
  
  /* I2C management using common state machine */
  I2C_Module_t i2cModule;                  /**< Generic I2C module */
  I2C_SlaveDevice_t slave;                 /**< Slave device */
} Xsound_t;

/**
 * @brief Initialize Xsound module with I2C peripheral
 * @param hi2c Pointer to I2C peripheral handle
 */
void Xsound_Init(I2C_HandleTypeDef *hi2c);

/**
 * @brief Process Xsound state machine (must be called periodically)
 * @note Handles automatic reading of sensor data
 */
void Xsound_Process(void);

/**
 * @brief I2C RX complete callback (call from HAL callback)
 * @param hi2c I2C handle that completed transfer
 */
void Xsound_RxCallback(I2C_HandleTypeDef *hi2c);

/**
 * @brief I2C error callback (call from HAL error callback)
 * @param hi2c I2C handle that had error
 */
void Xsound_ErrorCallback(I2C_HandleTypeDef *hi2c);

/**
 * @brief Get pointer to Xsound data structure (read-only)
 * @return Const pointer to Xsound data
 */
const Xsound_t* Xsound_GetData(void);

/**
 * @brief Clear data ready flag after processing
 */
void Xsound_ClearDataReady(void);

#endif // XSOUND_H