#include "MPU6050DMP.h"
#include "inv_mpu.h"
#include "inv_mpu_dmp_motion_driver.h"
#include "i2c.h"

#ifdef DEBUG_I2C
#include "dataPrint.h"
#endif

static MPU6050_DMP_t dmpData = {0};
static uint8_t consecutiveI2CErrors = 0;  // Track consecutive I2C errors

// Yaw reset variables
static float prev_yaw = 0.0f;       // Last actual yaw reading from DMP
static float re_zero_yaw = 0.0f;    // Offset to apply for yaw reset

static signed char gyro_orientation[9] = {-1, 0, 0,
                                           0,-1, 0,
                                           0, 0, 1};

static unsigned short inv_row_2_scale(const signed char *row)
{
  unsigned short b;
  if (row[0] > 0) b = 0;
  else if (row[0] < 0) b = 4;
  else if (row[1] > 0) b = 1;
  else if (row[1] < 0) b = 5;
  else if (row[2] > 0) b = 2;
  else if (row[2] < 0) b = 6;
  else   /* Error */   b = 7;      
  return b;
}

static unsigned short inv_orientation_matrix_to_scalar(
  const signed char *mtx)
{
  unsigned short scalar;

  /*
    XYZ  010_001_000 Identity Matrix
    XZY  001_010_000
    YXZ  010_000_001
    YZX  000_010_001
    ZXY  001_000_010
    ZYX  000_001_010
  */

  scalar = inv_row_2_scale(mtx);
  scalar |= inv_row_2_scale(mtx + 3) << 3;
  scalar |= inv_row_2_scale(mtx + 6) << 6;
  return scalar;
}

static int run_self_test(void)
{
  int result;
  long gyro[3], accel[3];
  unsigned char i = 0;
 
#if defined (MPU6500) || defined (MPU9250)
  result = mpu_run_6500_self_test(gyro, accel, 0);
#elif defined (MPU6050) || defined (MPU9150)
  result = mpu_run_self_test(gyro, accel);
#endif
  if (result == 0x07) //判断返回值 新版默认|0x04，不需要由0x07修改至0x03
  {
    /* Test passed. We can trust the gyro data here, so let's push it down
      * to the DMP.
      */
    for(i = 0; i<3; i++)
    {
      gyro[i] = (long)(gyro[i] * 32.8f); //convert to +-1000dps
      accel[i] *= 2048.f; //convert to +-16G
      accel[i] = accel[i] >> 16;
      gyro[i] = (long)(gyro[i] >> 16);
    }
    mpu_set_gyro_bias_reg(gyro);
 
#if defined (MPU6500) || defined (MPU9250)
        mpu_set_accel_bias_6500_reg(accel);
#elif defined (MPU6050) || defined (MPU9150)
        mpu_set_accel_bias_6050_reg(accel);
#endif
  }
  else {
    return -1;
  }
  return 0;
}

int MPU6050_DMP_Init(void) {
  int result;
  struct int_param_s int_param_s;

  result = mpu_init(&int_param_s); // mpu init
  if (result != 0) { return -1; }

  result = mpu_set_sensors(INV_XYZ_GYRO | INV_XYZ_ACCEL); // enable gyro and accel
  if (result != 0) { return -2; }
  
  // result = mpu_set_gyro_fsr(GYRO_FSR_1000DPS);   // Set to ±1000 dps for higher range
  // if (result != 0) { return -3; }
  // result = mpu_set_accel_fsr(ACCEL_FSR_4G);      // Set to ±4g for normal movement
  // if (result != 0) { return -4; }
  
  result = mpu_configure_fifo(INV_XYZ_GYRO | INV_XYZ_ACCEL);  // configure fifo
  if (result != 0) { return -5; }
  result = mpu_set_sample_rate(DEFAULT_MPU_HZ); // set sample rate
  if (result != 0) { return -6; }

  result = dmp_load_motion_driver_firmware();   // load dmp firmware
  if (result != 0) { return -7; }
  result = dmp_set_orientation(inv_orientation_matrix_to_scalar(gyro_orientation)); // set orientation
  if (result != 0) { return -8; }

  result = dmp_enable_feature(  // enable dmp features
    DMP_FEATURE_6X_LP_QUAT | DMP_FEATURE_TAP | DMP_FEATURE_ANDROID_ORIENT | 
    DMP_FEATURE_SEND_RAW_ACCEL | DMP_FEATURE_SEND_CAL_GYRO | DMP_FEATURE_GYRO_CAL
  );
  if (result != 0) { return -9; }
  result = dmp_set_fifo_rate(DEFAULT_MPU_HZ); // set fifo rate
  if (result != 0) { return -10; }

  result = run_self_test(); // run self test and push gyro and accel bias to the hardware registers
  if (result != 0) { return -11; }
  result = mpu_set_dmp_state(1);  // enable dmp
  if (result != 0) { return -12; }
  return 0;
}

int MPU6050DMP_updateData(void) {
  short gyro[3];
  short accel[3];
  long quat[4];
  unsigned long timestamp;
  short sensors;
  unsigned char more;

  float q0, q1, q2, q3 = 0.0f;
  int result = dmp_read_fifo(gyro, accel, quat, &timestamp, &sensors, &more);
  if (result != 0) {
    // Failed to read fifo - could be no data available or I2C error
    return -1;
  }

  if (sensors & INV_WXYZ_QUAT) {
    q0 = quat[0] / QUAT_SCALE;
    q1 = quat[1] / QUAT_SCALE;
    q2 = quat[2] / QUAT_SCALE;
    q3 = quat[3] / QUAT_SCALE;

    dmpData.quaternion.w = q0;
    dmpData.quaternion.x = q1;
    dmpData.quaternion.y = q2;
    dmpData.quaternion.z = q3;

    // Convert quaternion to Euler angles (in degrees)
    float roll, pitch, yaw;
    pitch = arm_sin_f32(-2 * (q1 * q3 - q0 * q2)) * 57.295779513f; // Pitch
    arm_atan2_f32(2 * (q2 * q3 + q0 * q1), -2 * (q1 * q1 + q2 * q2) + 1, &roll);
    arm_atan2_f32(2 * (q1 * q2 + q0 * q3), q0 * q0 + q1 * q1 - q2 * q2 - q3 * q3, &yaw);
    roll *= 57.295779513f; // Convert to degrees
    yaw *= 57.295779513f;  // Convert to degrees

    dmpData.euler.roll = roll;
    dmpData.euler.pitch = pitch;
    
    // Store actual yaw for reset functionality
    prev_yaw = yaw;
    
    // Apply yaw re-zero offset: apparent_yaw = actual_yaw - re_zero_yaw
    float apparent_yaw = yaw - re_zero_yaw;
    
    // Normalize to [-180, 180] range
    if (apparent_yaw > 180.0f) {
      apparent_yaw -= 360.0f;
    } else if (apparent_yaw < -180.0f) {
      apparent_yaw += 360.0f;
    }
    
    dmpData.euler.yaw = apparent_yaw;
    
    dmpData.ready = true;  // Mark data as ready
    consecutiveI2CErrors = 0;  // Reset error counter on successful read
  }

  return 0; // success
}

void MPU6050DMP_HandleI2CError(void) {
  uint32_t i2cError = HAL_I2C_GetError(&hi2c3);
  HAL_I2C_StateTypeDef i2cState = HAL_I2C_GetState(&hi2c3);
  
  #ifdef DEBUG_I2C
  char msg[100];
  snprintf(msg, sizeof(msg), "MPU6050 I2C: State=%d, Error=0x%lX, ErrCount=%d\r\n", 
           i2cState, i2cError, consecutiveI2CErrors);
  dataUart_SendString(msg);
  #endif
  
  if (i2cError != HAL_I2C_ERROR_NONE) {
    consecutiveI2CErrors++;
    // Soft recovery: Clear error flags and reset FIFO
    HAL_I2C_Master_Abort_IT(&hi2c3, 0x68 << 1);
    __HAL_I2C_CLEAR_FLAG(&hi2c3, I2C_FLAG_BERR | I2C_FLAG_ARLO | I2C_FLAG_AF | I2C_FLAG_OVR);
    mpu_reset_fifo();
  } else {
    // No error - reset counter
    consecutiveI2CErrors = 0;
  }
}

bool MPU6050_DMP_IsDataReady(void) {
  return dmpData.ready;
}

void MPU6050_DMP_ClearReady(void) {
  dmpData.ready = false;
}

void MPU6050DMP_ResetErrorCounter(void) {
  consecutiveI2CErrors = 0;
}

const MPU6050_DMP_t* MPU6050DMP_GetData(void) {
  return &dmpData;
}

/**
 * @brief Set gyroscope full scale range
 * @param fsr_dps Full scale range in dps
 *                Use: GYRO_FSR_250DPS, GYRO_FSR_500DPS, GYRO_FSR_1000DPS, or GYRO_FSR_2000DPS
 * @return 0 on success, -1 on error
 */
int MPU6050_DMP_SetGyroFSR(unsigned short fsr_dps) {
  return mpu_set_gyro_fsr(fsr_dps);
}

/**
 * @brief Set accelerometer full scale range
 * @param fsr_g Full scale range in g
 *              Use: ACCEL_FSR_2G, ACCEL_FSR_4G, ACCEL_FSR_8G, or ACCEL_FSR_16G
 * @return 0 on success, -1 on error
 */
int MPU6050_DMP_SetAccelFSR(unsigned char fsr_g) {
  return mpu_set_accel_fsr(fsr_g);
}

/**
 * @brief Get current gyroscope full scale range
 * @param fsr_dps Pointer to store FSR value in degrees per second
 * @return 0 on success, -1 on error
 */
int MPU6050_DMP_GetGyroFSR(unsigned short *fsr_dps) {
  return mpu_get_gyro_fsr(fsr_dps);
}

/**
 * @brief Get current accelerometer full scale range
 * @param fsr_g Pointer to store FSR value in g
 * @return 0 on success, -1 on error
 */
int MPU6050_DMP_GetAccelFSR(unsigned char *fsr_g) {
  return mpu_get_accel_fsr(fsr_g);
}

/**
 * @brief Reset yaw to zero at current orientation
 * 
 * This function captures the current yaw angle and sets it as the new zero reference.
 * Subsequent yaw readings will be relative to this new zero point.
 * Formula: apparent_yaw = actual_yaw - re_zero_yaw
 * 
 * @return true if reset successful, false if no data ready
 */
bool MPU6050_DMP_ResetYaw(void) {
  re_zero_yaw = prev_yaw;
  return true;
}