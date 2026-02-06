#ifndef MPU6050DMP_h
#define MPU6050DMP_h

/* Starting sampling rate. */
#define DEFAULT_MPU_HZ (100)

#define QUAT_SCALE 1073741824.0f /**< 2^30 used to convert DMP quaternions to float */

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

const MPU6050_DMP_t* MPU6050DMP_GetData(void);

#endif /* MPU6050DMP_h */ 