#ifndef IR_H
#define IR_H

#include "i2c.h"
#include <stdint.h>
#include <string.h>

/* Configuration constants */
#define IR_BUFFER_SIZE 16       /**< Buffer size: Vref + 7 sensors, 2 bytes each */
#define EYE_NUM 7               /**< Number of IR sensors per slave */
#define SLAVES_NO 2             /**< Number of I2C slave devices */
#define DMA_BUFFER_ADDRESS 0x30000000  /**< DMA buffer base address */

/** Slave device identifiers */
typedef enum { 
  SLAVE_1 = 0,  /**< First IR sensor slave */
  SLAVE_2       /**< Second IR sensor slave */
} Slave_ID;

/* Public variables */
extern uint8_t maxEye;      /**< Index of sensor with maximum value */
extern uint16_t maxValue;   /**< Maximum sensor value */
extern uint8_t ProcessBuffer[SLAVES_NO][IR_BUFFER_SIZE];  /**< Processed data buffers */

void IR_Init(I2C_HandleTypeDef *hi2c1, I2C_HandleTypeDef *hi2c2);

HAL_StatusTypeDef IR_ReadData(Slave_ID slaves_id);

uint8_t IR_SaveData(Slave_ID slave_id, uint8_t *data, uint16_t size);

uint8_t IR_IsDataReady(Slave_ID slave_id);

void IR_ClearDataReady(Slave_ID slave_id);

uint16_t combine_data(uint8_t msb, uint8_t lsb);

void updateValues(void);

#endif /* IR_H */