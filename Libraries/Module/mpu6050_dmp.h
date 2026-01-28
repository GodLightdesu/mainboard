#ifndef MPU6050_DMP_H
#define MPU6050_DMP_H

#include "mpu6050.h"
#include <stdint.h>
#include <stdbool.h>

/* DMP Configuration */
#define DMP_FEATURE_6X_LP_QUAT          0x0001  // 6-axis quaternion (accel + gyro)
#define DMP_FEATURE_GYRO_CAL            0x0002  // Gyroscope calibration
#define DMP_FEATURE_SEND_RAW_ACCEL      0x0004  // Send raw accelerometer data
#define DMP_FEATURE_SEND_RAW_GYRO       0x0008  // Send raw gyroscope data

/**
 * @brief DMP quaternion data structure
 */
typedef struct {
  float w, x, y, z;  // Quaternion components
} Quaternion_t;

/**
 * @brief Euler angles (in degrees)
 */
typedef struct {
  float roll;   // Roll angle (X-axis rotation)
  float pitch;  // Pitch angle (Y-axis rotation) 
  float yaw;    // Yaw angle (Z-axis rotation)
} EulerAngles_t;

/**
 * @brief DMP data structure
 */
typedef struct {
  bool ready;
  Quaternion_t quaternion;
  EulerAngles_t euler;
  int16_t accel[3];  // Raw accelerometer data
  int16_t gyro[3];   // Raw gyroscope data
} MPU6050_DMP_t;

extern MPU6050_DMP_t MPU6050_DMP;

/**
 * @brief Initialize DMP
 * @param hi2c I2C handle
 * @param features DMP features to enable (OR of DMP_FEATURE_* flags)
 * @return true if successful
 */
bool MPU6050_DMP_Init(I2C_HandleTypeDef *hi2c, uint16_t features);

/**
 * @brief Update DMP data (call in main loop)
 * @return true if new data available
 */
bool MPU6050_DMP_Update(void);

/**
 * @brief Get quaternion data
 * @param quat Pointer to store quaternion
 * @return true if data is valid
 */
bool MPU6050_DMP_GetQuaternion(Quaternion_t *quat);

/**
 * @brief Get Euler angles
 * @param euler Pointer to store Euler angles
 * @return true if data is valid
 */
bool MPU6050_DMP_GetEulerAngles(EulerAngles_t *euler);

/**
 * @brief Convert quaternion to Euler angles
 * @param quat Input quaternion
 * @param euler Output Euler angles
 */
void MPU6050_QuaternionToEuler(const Quaternion_t *quat, EulerAngles_t *euler);

#endif /* MPU6050_DMP_H */
