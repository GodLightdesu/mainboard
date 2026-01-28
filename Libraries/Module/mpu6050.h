#ifndef MPU6050_H
#define MPU6050_H

/* Debug configuration - comment out to disable debug output */
#define MPU6050_DEBUG
#define MPU6050_DEBUG_INTERVAL 50  /* Print every N readings */

#define MPU6050_SLAVE_ADDR (0x68 << 1)
#define MPU6050_SLAVES_NO 1

/* MPU6050 Registers */
#define MPU6050_REG_PWR_MGMT_1    0x6B  // Power management: reset, sleep, clock source
#define MPU6050_REG_GYRO_CONFIG   0x1B  // Gyroscope full scale range configuration
#define MPU6050_REG_ACCEL_CONFIG  0x1C  // Accelerometer full scale range configuration
#define MPU6050_REG_CONFIG        0x1A  // Digital low pass filter (DLPF) configuration
#define MPU6050_REG_ACCEL_XOUT_H  0x3B  // Start address for sensor data (14 bytes)

/* Full Scale Ranges */
#define MPU6050_GYRO_FS_250       0x00  // ±250°/s
#define MPU6050_GYRO_FS_500       0x08  // ±500°/s
#define MPU6050_GYRO_FS_1000      0x10  // ±1000°/s
#define MPU6050_GYRO_FS_2000      0x18  // ±2000°/s

#define MPU6050_ACCEL_FS_2G       0x00  // ±2g
#define MPU6050_ACCEL_FS_4G       0x08  // ±4g
#define MPU6050_ACCEL_FS_8G       0x10  // ±8g
#define MPU6050_ACCEL_FS_16G      0x18  // ±16g

/* Digital Low Pass Filter (DLPF) Configuration */
#define MPU6050_DLPF_260HZ        0x00  // Accel: 260Hz, Gyro: 256Hz, Delay: 0ms
#define MPU6050_DLPF_184HZ        0x01  // Accel: 184Hz, Gyro: 188Hz, Delay: 2.0ms
#define MPU6050_DLPF_94HZ         0x02  // Accel: 94Hz,  Gyro: 98Hz,  Delay: 3.0ms
#define MPU6050_DLPF_44HZ         0x03  // Accel: 44Hz,  Gyro: 42Hz,  Delay: 4.9ms (Default)
#define MPU6050_DLPF_21HZ         0x04  // Accel: 21Hz,  Gyro: 20Hz,  Delay: 8.5ms
#define MPU6050_DLPF_10HZ         0x05  // Accel: 10Hz,  Gyro: 10Hz,  Delay: 13.8ms
#define MPU6050_DLPF_5HZ          0x06  // Accel: 5Hz,   Gyro: 5Hz,   Delay: 19.0ms

/* Gyroscope Sensitivity Scale Factors (LSB/(°/s)) */
#define MPU6050_GYRO_SENS_250     131.0f   // ±250°/s  → LSB/°/s
#define MPU6050_GYRO_SENS_500     65.5f    // ±500°/s  → LSB/°/s
#define MPU6050_GYRO_SENS_1000    32.8f    // ±1000°/s → LSB/°/s
#define MPU6050_GYRO_SENS_2000    16.4f    // ±2000°/s → LSB/°/s

/* Accelerometer Sensitivity Scale Factors (LSB/g) */
#define MPU6050_ACCEL_SENS_2G     16384.0f // ±2g  → LSB/g
#define MPU6050_ACCEL_SENS_4G     8192.0f  // ±4g  → LSB/g
#define MPU6050_ACCEL_SENS_8G     4096.0f  // ±8g  → LSB/g
#define MPU6050_ACCEL_SENS_16G    2048.0f  // ±16g → LSB/g

#include "const.h"
#include "usart.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "i2c_common.h"

typedef struct MPU6050_t 
{
  bool dataReady;
  float ax, ay, az;
  float temperature;
  float gx, gy, gz;

  /* I2C management using common state machine */
  I2C_Module_t i2cModule;                  /**< Generic I2C module */
  I2C_SlaveDevice_t slaves[MPU6050_SLAVES_NO];  /**< Slave devices */
} MPU6050_t;

/**
 * @brief Get pointer to MPU6050 data structure (read-only)
 * @return Const pointer to MPU6050 data
 */
const MPU6050_t* MPU6050_GetData(void);

/**
 * @brief Check if MPU6050 data is ready
 * @return true if new data is available
 */
bool MPU6050_IsDataReady(void);

/**
 * @brief Set MPU6050 data ready flag
 * @param ready true to set ready
 */
void MPU6050_SetDataReady(bool ready);

void MPU6050_init(I2C_HandleTypeDef *hi2c);

void MPU6050_Process(void);

void MPU6050_RxCallback(I2C_HandleTypeDef *hi2c);

void MPU6050_ErrorCallback(I2C_HandleTypeDef *hi2c);

#endif // MPU6050_H