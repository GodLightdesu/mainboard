#ifndef IR_H
#define IR_H

/* Debug output control*/
#define IR_DEBUG

/* Configuration constants */
#define EYE_NUM 7               /**< Number of IR sensors per slave */
#define IR_SLAVES_NO 2          /**< Number of I2C slave devices */

/* Ball detection threshold */
#define IR_DETECTION_THRESHOLD 100  /**< Minimum signal strength to consider ball detected */

/* Mathematical constants */
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* I2C slave addresses */
#define IR_SLAVE_1_ADDR (0x30 << 1)
#define IR_SLAVE_2_ADDR (0x31 << 1)

#include "i2c_common.h"
#include "const.h"
#include "usart.h"
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/** Slave device identifiers */
typedef enum { 
  IR_SLAVE_1 = 0,  /**< First IR sensor slave */
  IR_SLAVE_2       /**< Second IR sensor slave */
} IR_SlaveID;

/**
 * @brief IR module structure
 */
typedef struct IR_t {
  bool dataReady;                          /**< Flag indicating if new data is available */
  uint8_t maxEye;                          /**< Index of sensor with maximum value */
  uint16_t maxValue;                       /**< Maximum sensor value */
  float ballAngle;                         /**< Ball angle in degrees (0-360°), auto-updated via interpolation method, -1.0f if no ball */
  uint16_t eyeValues[IR_SLAVES_NO * EYE_NUM]; /**< All sensor values */
  
  /* I2C management using common state machine */
  I2C_Module_t i2cModule;                  /**< Generic I2C module */
  I2C_SlaveDevice_t slaves[IR_SLAVES_NO];  /**< Slave devices */
} IR_t;

/**
 * @brief Initialize IR sensor module with I2C peripheral
 * @param hi2c Pointer to I2C peripheral handle
 */
void IR_Init(I2C_HandleTypeDef *hi2c);

/**
 * @brief Process IR state machine (must be called periodically)
 * @note Handles automatic sequential reading of all slaves
 */
void IR_Process(void);

/**
 * @brief I2C RX complete callback (call from HAL callback)
 * @param hi2c I2C handle that completed transfer
 */
void IR_RxCallback(I2C_HandleTypeDef *hi2c);

/**
 * @brief I2C error callback (call from HAL error callback)
 * @param hi2c I2C handle that had error
 */
void IR_ErrorCallback(I2C_HandleTypeDef *hi2c);

/**
 * @brief Calculate ball angle using weighted average method
 * @return Ball angle in degrees (0-360°), or -1.0f if no ball detected
 * @note Uses all sensor values above threshold for weighted calculation
 * @note Recommended for soccer robots - more accurate when ball is close
 * @note IR.ballAngle is auto-updated using interpolation method, call this for alternative calculation
 */
float IR_CalBallAngle(void);

/**
 * @brief Calculate ball angle using three-point interpolation
 * @return Ball angle in degrees (0-360°), or -1.0f if no ball detected
 * @note Uses parabolic interpolation between max and adjacent sensors
 * @note Highest precision - recommended when ball is between sensors
 * @note This method is automatically called in IR_DataProcessCallback to update IR.ballAngle
 */
float IR_CalBallAngleInterpolated(void);

/**
 * @brief Get pointer to IR data structure (read-only)
 * @return Const pointer to IR data
 */
const IR_t* IR_GetData(void);

/**
 * @brief Check if IR data is ready
 * @return true if new data is available
 */
bool IR_IsDataReady(void);

/**
 * @brief Enable or disable a specific IR slave device
 * @param slaveId The slave ID (IR_SLAVE_1 or IR_SLAVE_2)
 * @param enable true to enable, false to disable
 * @return true if successful, false if invalid parameters
 */
bool IR_SetSlaveEnabled(uint8_t slaveId, bool enable);

/**
 * @brief Check if a specific IR slave device is enabled
 * @param slaveId The slave ID (IR_SLAVE_1 or IR_SLAVE_2)
 * @return true if enabled, false otherwise
 */
bool IR_IsSlaveEnabled(uint8_t slaveId);

/**
 * @brief Get address of a specific IR slave device
 * @param slaveId The slave ID (IR_SLAVE_1 or IR_SLAVE_2)
 * @return Slave address, or 0 if invalid
 */
uint16_t IR_GetSlaveAddress(uint8_t slaveId);

#endif /* IR_H */