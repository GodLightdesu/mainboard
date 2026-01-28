#include "mpu6050_dmp.h"
#include "data_uart.h"

static MPU6050_DMP_t MPU6050_DMP = {0};
/**
 * @brief Simple complementary filter for orientation estimation
 */
static void ComplementaryFilter(void) {
  static bool initialized = false;
  static uint32_t lastTime = 0;
  
  if (!MPU6050_IsDataReady()) return;
  
  const MPU6050_t* mpu = MPU6050_GetData();
  
  uint32_t now = HAL_GetTick();
  
  if (!initialized) {
    // Initialize from accelerometer
    float roll = atan2f(mpu->ay, mpu->az) * 180.0f / M_PI;
    float pitch = atan2f(-mpu->ax, sqrtf(mpu->ay*mpu->ay + mpu->az*mpu->az)) * 180.0f / M_PI;
    
    MPU6050_DMP.euler.roll = roll;
    MPU6050_DMP.euler.pitch = pitch;
    MPU6050_DMP.euler.yaw = 0.0f;
    
    // Initialize gyro bias if not calibrated
    if (!MPU6050_DMP.calibrated) {
      memset(MPU6050_DMP.gyroBias, 0, sizeof(MPU6050_DMP.gyroBias));
    }
    
    initialized = true;
    lastTime = now;
    return;
  }
  
  float dt = (now - lastTime) / 1000.0f;
  lastTime = now;
  
  if (dt > MAX_DT) dt = DEFAULT_DT;  // Prevent large jumps
  
  // Apply gyro bias calibration
  float gx_calibrated = mpu->gx - MPU6050_DMP.gyroBias[0];
  float gy_calibrated = mpu->gy - MPU6050_DMP.gyroBias[1];
  float gz_calibrated = mpu->gz - MPU6050_DMP.gyroBias[2];
  
  // Gyroscope integration with calibrated data
  float gyroRoll = MPU6050_DMP.euler.roll + gx_calibrated * dt;
  float gyroPitch = MPU6050_DMP.euler.pitch + gy_calibrated * dt;
  float gyroYaw = MPU6050_DMP.euler.yaw + gz_calibrated * dt;
  
  // Accelerometer angles
  float accelRoll = atan2f(mpu->ay, mpu->az) * 180.0f / M_PI;
  float accelPitch = atan2f(-mpu->ax, sqrtf(mpu->ay*mpu->ay + mpu->az*mpu->az)) * 180.0f / M_PI;
  
  // Complementary filter (98% gyro, 2% accel)
  const float alpha = COMPLEMENTARY_FILTER_ALPHA;
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
  if (now - lastPrintTime >= ATTITUDE_PRINT_INTERVAL_MS) {  // Print every 100ms
    dataUart_PrintMPU6050Attitude(MPU6050_DMP_GetData());
    lastPrintTime = now;
  }
}

bool MPU6050_DMP_Init(I2C_HandleTypeDef *hi2c, uint16_t features) {
  if (hi2c == NULL) return false;

  memset(&MPU6050_DMP, 0, sizeof(MPU6050_DMP_t));
  MPU6050_DMP_CalibrateGyro(100);
  dataUart_PrintInitMessage("MPU6050: Complementary Filter");
  
  return true;
}

/**
 * @brief Calibrate gyroscope zero-point offsets
 * @param samples Number of samples to average (default: 100)
 * @note Keep sensor stationary during calibration
 */
void MPU6050_DMP_CalibrateGyro(uint16_t samples) {
  if (samples == 0) samples = 100;
  
  float sum_gx = 0, sum_gy = 0, sum_gz = 0;
  uint16_t valid_samples = 0;
  
  dataUart_PrintInitMessage("Gyro Calibration: Keep sensor stationary!");
  HAL_Delay(300); // Give user time to place sensor
  
  for (uint16_t i = 0; i < samples + 10; i++) {
    // Skip first 10 samples
    if (i < 10) {
      HAL_Delay(10);
      continue;
    }
    
    // Wait for new data
    while (!MPU6050_IsDataReady()) {
      MPU6050_Process();
      HAL_Delay(1);
    }
    
    const MPU6050_t* mpu_cal = MPU6050_GetData();
    sum_gx += mpu_cal->gx;
    sum_gy += mpu_cal->gy;
    sum_gz += mpu_cal->gz;
    valid_samples++;
    
    MPU6050_SetDataReady(false);
    HAL_Delay(10);
  }
  
  // Calculate average bias
  MPU6050_DMP.gyroBias[0] = sum_gx / valid_samples;
  MPU6050_DMP.gyroBias[1] = sum_gy / valid_samples;
  MPU6050_DMP.gyroBias[2] = sum_gz / valid_samples;
  
  // Save calibration for persistence (could be stored in flash)
  MPU6050_DMP.calibrated = true;
  
  char buffer[100];
  snprintf(buffer, sizeof(buffer), 
            "Gyro Bias: X:%.2f, Y:%.2f, Z:%.2f deg/s", 
            MPU6050_DMP.gyroBias[0], 
            MPU6050_DMP.gyroBias[1], 
            MPU6050_DMP.gyroBias[2]);
  dataUart_PrintInitMessage(buffer);
}

/**
 * @brief Reset yaw angle to zero
 * @note Use this to prevent yaw drift accumulation
 */
void MPU6050_DMP_ResetYaw(void) {
  // Store current roll and pitch
  float roll = MPU6050_DMP.euler.roll;
  float pitch = MPU6050_DMP.euler.pitch;
  
  // Reset yaw to 0
  MPU6050_DMP.euler.yaw = 0.0f;
  
  // Recalculate quaternion with zero yaw
  float cy = cosf(0.0f);
  float sy = sinf(0.0f);
  float cp = cosf(pitch * M_PI / 360.0f);
  float sp = sinf(pitch * M_PI / 360.0f);
  float cr = cosf(roll * M_PI / 360.0f);
  float sr = sinf(roll * M_PI / 360.0f);
  
  MPU6050_DMP.quaternion.w = cr * cp * cy + sr * sp * sy;
  MPU6050_DMP.quaternion.x = sr * cp * cy - cr * sp * sy;
  MPU6050_DMP.quaternion.y = cr * sp * cy + sr * cp * sy;
  MPU6050_DMP.quaternion.z = cr * cp * sy - sr * sp * cy;
  
  dataUart_PrintInitMessage("Yaw angle reset to 0");
}

bool MPU6050_DMP_Update(void) {
  if (!MPU6050_IsDataReady()) return false;
  
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

const MPU6050_DMP_t* MPU6050_DMP_GetData(void) {
  return &MPU6050_DMP;
}

bool MPU6050_DMP_IsDataReady(void) {
  return MPU6050_DMP.ready;
}