#ifndef IR_H
#define IR_H

/* Debug output control*/
#define IR_DEBUG

/* Configuration constants */
#define EYE_NUM 7               /**< Number of IR sensors per slave */
#define IR_SLAVES_NO 2          /**< Number of I2C slave devices */

/* I2C slave addresses */
#define IR_SLAVE_1_ADDR (0x30 << 1)
#define IR_SLAVE_2_ADDR (0x31 << 1)

#include "i2c_common.h"
#include "const.h"
#include "usart.h"
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
typedef struct {
  bool dataReady;                          /**< Flag indicating if new data is available */
  uint8_t maxEye;                          /**< Index of sensor with maximum value */
  uint16_t maxValue;                       /**< Maximum sensor value */
  uint16_t eyeValues[IR_SLAVES_NO * EYE_NUM]; /**< All sensor values */
  
  /* I2C management using common state machine */
  I2C_Module_t i2cModule;                  /**< Generic I2C module */
  I2C_SlaveDevice_t slaves[IR_SLAVES_NO];  /**< Slave devices */
} IR_t;

/* Global IR instance */
extern IR_t IR;

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

#endif /* IR_H */