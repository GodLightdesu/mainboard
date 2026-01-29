#include "MPU6050.h"
#include "inv_mpu.h"
#include "inv_mpu_dmp_motion_driver.h"
#include "main.h"

/* Starting sampling rate. */
#define DEFAULT_MPU_HZ (100)

static signed char gyro_orientation[9] = {
  -1, 0, 0,
  0,-1, 0,
  0, 0, 1
};

static unsigned short inv_row_2_scale(const signed char *row)
{
  unsigned short b;
  if (row[0] > 0)      b = 0;
  else if (row[0] < 0) b = 4;
  else if (row[1] > 0) b = 1;
  else if (row[1] < 0) b = 5;
  else if (row[2] > 0) b = 2;
  else if (row[2] < 0) b = 6;
  else   /* error */   b = 7;
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
  else
  {
    return -1;
  }

  return 0;
}

int MPU6050_Init(void)
{
	int result;
	struct int_param_s int_param;
 
	HAL_Delay(100); // 一定要加延时！！
	result = mpu_init(&int_param); //MPU 初始化
	if (result != 0) return -1; 
 
	result = mpu_set_sensors(INV_XYZ_GYRO | INV_XYZ_ACCEL); // 设置传感器
	if (result != 0) return -2;
 
	result = mpu_configure_fifo(INV_XYZ_GYRO | INV_XYZ_ACCEL); // 设置fifo
	if (result != 0) return -3;
 
	result = mpu_set_sample_rate(DEFAULT_MPU_HZ); // 设置采样率
	if (result != 0) return -4;
 
  result = dmp_load_motion_driver_firmware(); // 加载DMP固件
	if (result != 0) return -5;
 
  result = dmp_set_orientation(inv_orientation_matrix_to_scalar(gyro_orientation)); // 设置陀螺仪方向
	if (result != 0) return -6;
 
  result = dmp_enable_feature(DMP_FEATURE_6X_LP_QUAT | DMP_FEATURE_TAP | DMP_FEATURE_ANDROID_ORIENT |
                      DMP_FEATURE_SEND_RAW_ACCEL | DMP_FEATURE_SEND_CAL_GYRO | DMP_FEATURE_GYRO_CAL);
  if (result != 0) return -7;
 
  result = dmp_set_fifo_rate(DEFAULT_MPU_HZ); // 设置输出速率
	if (result != 0) return -8;
 
	result = run_self_test(); // 自检
	if (result != 0) return -9;
 
  result = mpu_set_dmp_state(1); // 使能DMP
	if (result != 0) return -10;
  
	return 0; 
}