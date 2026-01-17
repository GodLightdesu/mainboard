#ifndef IR_H
#define IR_H

#include "i2c_master.h"
#include "const.h"
#include "usart.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

/* Configuration constants */
#define EYE_NUM 7               /**< Number of IR sensors per slave */
#define IR_SLAVES_NO 2             /**< Number of I2C slave devices */

/* I2C slave addresses */
#define IR_SLAVE_1_ADDR (0x30 << 1)
#define IR_SLAVE_2_ADDR (0x31 << 1)

/** Slave device identifiers */
typedef enum { 
  SLAVE_1 = 0,  /**< First IR sensor slave */
  SLAVE_2       /**< Second IR sensor slave */
} Slave_ID;

/* Public variables */
extern uint8_t maxEye;      /**< Index of sensor with maximum value */
extern uint16_t maxValue;   /**< Maximum sensor value */

void IR_Init(I2C_Master_t *i2cMaster);

StatusTypeDef IR_ReadData(Slave_ID slaves_id);

bool IR_IsDataReady(Slave_ID slave_id);

void IR_ClearDataReady(Slave_ID slave_id);

void updateIRValues(Slave_ID slave_id);

#endif /* IR_H */