#include "i2c_master.h"

// 掃描 I2C 区域尋找可用設備
uint16_t I2C_Scan(I2C_HandleTypeDef *hi2c) {
  if (hi2c == NULL) {
    return 0;
  }
  
  /* Scan valid I2C address range (1-127) */
  for (uint16_t addr = 1; addr < 128; addr++) {
    /* Check if device responds (3 retries, 100ms timeout) */
    if (HAL_I2C_IsDeviceReady(hi2c, addr << 1, 3, 100) == HAL_OK) {
      return addr;  /* Return first found device */
    }
  }
  
  return 0;  /* No device found */
}