#ifndef MPU6050_DMP_H
#define MPU6050_DMP_H

/* DMP Configuration */
#define DMP_FEATURE_6X_LP_QUAT          0x0001  // 6-axis quaternion (accel + gyro)
#define DMP_FEATURE_GYRO_CAL            0x0002  // Gyroscope calibration
#define DMP_FEATURE_SEND_RAW_ACCEL      0x0004  // Send raw accelerometer data
#define DMP_FEATURE_SEND_RAW_GYRO       0x0008  // Send raw gyroscope data

/* Complementary filter constants */
#define COMPLEMENTARY_FILTER_ALPHA      0.98f   /**< Gyro weight in complementary filter */
#define MAX_DT                          0.5f    /**< Maximum allowed dt to prevent large jumps */
#define DEFAULT_DT                      0.01f   /**< Default dt when max exceeded */
#define ATTITUDE_PRINT_INTERVAL_MS      100     /**< Print attitude every 100ms */

#include <math.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "i2c.h"
#include "mpu6050.h"

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
typedef struct MPU6050_DMP_t {
  bool ready;
  bool calibrated;
  Quaternion_t quaternion;
  EulerAngles_t euler;
  float gyroBias[3];
  int16_t accel[3];  // Raw accelerometer data
  int16_t gyro[3];   // Raw gyroscope data
} MPU6050_DMP_t;

/**
 * @brief Get pointer to MPU6050 DMP data structure (read-only)
 * @return Const pointer to DMP data
 */
const MPU6050_DMP_t* MPU6050_DMP_GetData(void);

/**
 * @brief Check if DMP data is ready
 * @return true if new data is available
 */
bool MPU6050_DMP_IsDataReady(void);

/**
 * @brief Get Euler angles from DMP
 * @param euler Pointer to EulerAngles_t to fill
 * @return true if successful
 */
bool MPU6050_DMP_GetEulerAngles(EulerAngles_t *euler);

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
 * @brief Calibrate gyroscope zero-point offsets
 * @param samples Number of samples to average (default: 100)
 * @note Keep sensor stationary during calibration
 */
void MPU6050_DMP_CalibrateGyro(uint16_t samples);

/**
 * @brief Reset yaw angle to zero
 * @note Use this to prevent yaw drift accumulation
 */
void MPU6050_DMP_ResetYaw(void);

/**
 * @brief Convert quaternion to Euler angles
 * @param quat Input quaternion
 * @param euler Output Euler angles
 */
void MPU6050_QuaternionToEuler(const Quaternion_t *quat, EulerAngles_t *euler);

/**
 * @brief Get pointer to DMP data structure
 * @return Pointer to const MPU6050_DMP_t structure
 */
const MPU6050_DMP_t* MPU6050_DMP_GetData(void);

#endif // MPU6050_DMP_H
