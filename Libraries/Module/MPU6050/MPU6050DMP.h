#ifndef MPU6050DMP_h
#define MPU6050DMP_h

/* Starting sampling rate. */
#define DEFAULT_MPU_HZ (100)

#define QUAT_SCALE 1073741824.0f /**< 2^30 used to convert DMP quaternions to float */

/* Full Scale Range Values - pass these to SetGyroFSR/SetAccelFSR */
#define GYRO_FSR_250DPS   250   /**< Gyroscope full scale range: ±250 dps */
#define GYRO_FSR_500DPS   500   /**< Gyroscope full scale range: ±500 dps */
#define GYRO_FSR_1000DPS  1000  /**< Gyroscope full scale range: ±1000 dps */
#define GYRO_FSR_2000DPS  2000  /**< Gyroscope full scale range: ±2000 dps */

#define ACCEL_FSR_2G      2     /**< Accelerometer full scale range: ±2g */
#define ACCEL_FSR_4G      4     /**< Accelerometer full scale range: ±4g */
#define ACCEL_FSR_8G      8     /**< Accelerometer full scale range: ±8g */
#define ACCEL_FSR_16G     16    /**< Accelerometer full scale range: ±16g */

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "arm_math.h"

typedef struct {
  float w, x, y, z; // Quaternion components
} Quaternion_t;

typedef struct
{
  float roll;  // Roll angle (X-axis rotation)
  float pitch; // Pitch angle (Y-axis rotation)
  float yaw;   // +ve face left, -ve face right (Z-axis rotation)
} EulerAngles_t;

typedef struct
{
  bool ready;  // Data ready flag
  Quaternion_t quaternion;
  EulerAngles_t euler;
  // int16_t accel[3]; // Raw accelerometer data
  // int16_t gyro[3];  // Raw gyroscope data
} MPU6050_DMP_t;

int MPU6050_DMP_Init(void);
int MPU6050DMP_updateData(void);
bool MPU6050_DMP_IsDataReady(void);
void MPU6050_DMP_ClearReady(void);
void MPU6050DMP_HandleI2CError(void);
void MPU6050DMP_ResetErrorCounter(void);

const MPU6050_DMP_t* MPU6050DMP_GetData(void);

/* Full Scale Range Configuration */
int MPU6050_DMP_SetGyroFSR(unsigned short fsr_dps);
int MPU6050_DMP_SetAccelFSR(unsigned char fsr_g);
int MPU6050_DMP_GetGyroFSR(unsigned short *fsr_dps);
int MPU6050_DMP_GetAccelFSR(unsigned char *fsr_g);

/* Yaw Reset Function */
bool MPU6050_DMP_ResetYaw(void);

#endif /* MPU6050DMP_h */ 