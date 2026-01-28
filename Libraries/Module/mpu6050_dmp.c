#include "mpu6050_dmp.h"
#include "data_uart.h"
#include <math.h>
#include <string.h>

MPU6050_DMP_t MPU6050_DMP = {0};

/**
 * @brief Simple complementary filter for orientation estimation
 */
static void ComplementaryFilter(void) {
  static bool initialized = false;
  static uint32_t lastTime = 0;
  
  if (!MPU6050.dataReady) return;
  
  uint32_t now = HAL_GetTick();
  
  if (!initialized) {
    // Initialize from accelerometer
    float roll = atan2f(MPU6050.ay, MPU6050.az) * 180.0f / M_PI;
    float pitch = atan2f(-MPU6050.ax, sqrtf(MPU6050.ay*MPU6050.ay + MPU6050.az*MPU6050.az)) * 180.0f / M_PI;
    
    MPU6050_DMP.euler.roll = roll;
    MPU6050_DMP.euler.pitch = pitch;
    MPU6050_DMP.euler.yaw = 0.0f;
    
    initialized = true;
    lastTime = now;
    return;
  }
  
  float dt = (now - lastTime) / 1000.0f;
  lastTime = now;
  
  if (dt > 0.5f) dt = 0.01f;  // Prevent large jumps
  
  // Gyroscope integration
  float gyroRoll = MPU6050_DMP.euler.roll + MPU6050.gx * dt;
  float gyroPitch = MPU6050_DMP.euler.pitch + MPU6050.gy * dt;
  float gyroYaw = MPU6050_DMP.euler.yaw + MPU6050.gz * dt;
  
  // Accelerometer angles
  float accelRoll = atan2f(MPU6050.ay, MPU6050.az) * 180.0f / M_PI;
  float accelPitch = atan2f(-MPU6050.ax, sqrtf(MPU6050.ay*MPU6050.ay + MPU6050.az*MPU6050.az)) * 180.0f / M_PI;
  
  // Complementary filter (98% gyro, 2% accel)
  const float alpha = 0.98f;
  MPU6050_DMP.euler.roll = alpha * gyroRoll + (1.0f - alpha) * accelRoll;
  MPU6050_DMP.euler.pitch = alpha * gyroPitch + (1.0f - alpha) * accelPitch;
  MPU6050_DMP.euler.yaw = gyroYaw;  // No accelerometer correction for yaw
  
  // Wrap yaw to [-180, 180]
  while (MPU6050_DMP.euler.yaw > 180.0f) MPU6050_DMP.euler.yaw -= 360.0f;
  while (MPU6050_DMP.euler.yaw < -180.0f) MPU6050_DMP.euler.yaw += 360.0f;
  
  // Convert Euler to Quaternion
  float cy = cosf(MPU6050_DMP.euler.yaw * M_PI / 360.0f);
  float sy = sinf(MPU6050_DMP.euler.yaw * M_PI / 360.0f);
  float cp = cosf(MPU6050_DMP.euler.pitch * M_PI / 360.0f);
  float sp = sinf(MPU6050_DMP.euler.pitch * M_PI / 360.0f);
  float cr = cosf(MPU6050_DMP.euler.roll * M_PI / 360.0f);
  float sr = sinf(MPU6050_DMP.euler.roll * M_PI / 360.0f);
  
  MPU6050_DMP.quaternion.w = cr * cp * cy + sr * sp * sy;
  MPU6050_DMP.quaternion.x = sr * cp * cy - cr * sp * sy;
  MPU6050_DMP.quaternion.y = cr * sp * cy + sr * cp * sy;
  MPU6050_DMP.quaternion.z = cr * cp * sy - sr * sp * cy;
  
  MPU6050_DMP.ready = true;
  
  /* Print attitude at controlled interval */
  static uint32_t lastPrintTime = 0;
  if (now - lastPrintTime >= 100) {  // Print every 100ms
    dataUart_PrintMPU6050Attitude(MPU6050_DMP.euler.roll, 
                                  MPU6050_DMP.euler.pitch, 
                                  MPU6050_DMP.euler.yaw);
    lastPrintTime = now;
  }
}

bool MPU6050_DMP_Init(I2C_HandleTypeDef *hi2c, uint16_t features) {
  if (hi2c == NULL) return false;
  
  memset(&MPU6050_DMP, 0, sizeof(MPU6050_DMP_t));
  
  dataUart_PrintInitMessage("MPU6050: Complementary Filter");
  
  return true;
}

bool MPU6050_DMP_Update(void) {
  if (!MPU6050.dataReady) return false;
  
  // Use complementary filter for orientation estimation
  ComplementaryFilter();
  
  return MPU6050_DMP.ready;
}

bool MPU6050_DMP_GetQuaternion(Quaternion_t *quat) {
  if (quat == NULL || !MPU6050_DMP.ready) return false;
  
  *quat = MPU6050_DMP.quaternion;
  return true;
}

bool MPU6050_DMP_GetEulerAngles(EulerAngles_t *euler) {
  if (euler == NULL || !MPU6050_DMP.ready) return false;
  
  *euler = MPU6050_DMP.euler;
  return true;
}

void MPU6050_QuaternionToEuler(const Quaternion_t *quat, EulerAngles_t *euler) {
  if (quat == NULL || euler == NULL) return;
  
  // Roll (x-axis rotation)
  float sinr_cosp = 2.0f * (quat->w * quat->x + quat->y * quat->z);
  float cosr_cosp = 1.0f - 2.0f * (quat->x * quat->x + quat->y * quat->y);
  euler->roll = atan2f(sinr_cosp, cosr_cosp) * 180.0f / M_PI;
  
  // Pitch (y-axis rotation)
  float sinp = 2.0f * (quat->w * quat->y - quat->z * quat->x);
  if (fabsf(sinp) >= 1.0f)
    euler->pitch = copysignf(90.0f, sinp);  // Use 90 degrees if out of range
  else
    euler->pitch = asinf(sinp) * 180.0f / M_PI;
  
  // Yaw (z-axis rotation)
  float siny_cosp = 2.0f * (quat->w * quat->z + quat->x * quat->y);
  float cosy_cosp = 1.0f - 2.0f * (quat->y * quat->y + quat->z * quat->z);
  euler->yaw = atan2f(siny_cosp, cosy_cosp) * 180.0f / M_PI;
}
