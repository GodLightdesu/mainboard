#include "MPU6050.h"
#include "data_uart.h"

static MPU6050_t MPU6050 = {0};

static uint8_t *rxBuffer1 = MPU6050_RXBUF_PTR;
static uint8_t processBuffer1[MPU6050_BUFFER_SIZE];
static uint8_t txBuffer1[1] = {MPU6050_REG_ACCEL_XOUT_H};  // Register address

/* Helper function to combine MSB and LSB */
static int16_t combine_data(uint8_t msb, uint8_t lsb) { 
  return (int16_t)((msb << 8) | lsb);
}

/* Data processing callback - called by generic I2C module */
static void MPU6050_DataProcessCallback(I2C_Module_t *module, uint8_t slaveId) {  
  if (slaveId >= MPU6050_SLAVES_NO) return;

  I2C_SlaveDevice_t *slave = &MPU6050.slaves[slaveId];
  if (!slave->enabled || slave->processBuffer == NULL) return;
  
  /* Parse sensor data from register 0x3B onwards:
   * [ACCEL_X_H, ACCEL_X_L, ACCEL_Y_H, ACCEL_Y_L, ACCEL_Z_H, ACCEL_Z_L,
   *  TEMP_H, TEMP_L, GYRO_X_H, GYRO_X_L, GYRO_Y_H, GYRO_Y_L, GYRO_Z_H, GYRO_Z_L]
   */
  int16_t accelX = combine_data(slave->processBuffer[0], slave->processBuffer[1]);
  int16_t accelY = combine_data(slave->processBuffer[2], slave->processBuffer[3]);
  int16_t accelZ = combine_data(slave->processBuffer[4], slave->processBuffer[5]);
  int16_t temp   = combine_data(slave->processBuffer[6], slave->processBuffer[7]);
  int16_t gyroX  = combine_data(slave->processBuffer[8], slave->processBuffer[9]);
  int16_t gyroY  = combine_data(slave->processBuffer[10], slave->processBuffer[11]);
  int16_t gyroZ  = combine_data(slave->processBuffer[12], slave->processBuffer[13]);
  
  /* Convert to physical units */
  MPU6050.ax = accelX / MPU6050_ACCEL_SENS_4G;  // ±4g sensitivity
  MPU6050.ay = accelY / MPU6050_ACCEL_SENS_4G;
  MPU6050.az = accelZ / MPU6050_ACCEL_SENS_4G;
  MPU6050.temperature = temp / 340.0f + 36.53f;  // Temperature formula
  MPU6050.gx = gyroX / MPU6050_GYRO_SENS_1000;  // ±1000°/s sensitivity
  MPU6050.gy = gyroY / MPU6050_GYRO_SENS_1000;
  MPU6050.gz = gyroZ / MPU6050_GYRO_SENS_1000;
  
  MPU6050.dataReady = true;
  
  /* Print data immediately after processing */
  dataUart_PrintMPU6050Data(MPU6050_GetData());
}

void MPU6050_init(I2C_HandleTypeDef *hi2c) {
  if (hi2c == NULL) return;

  /* Initialize MPU6050 structure */
  memset(&MPU6050, 0, sizeof(MPU6050_t));
  MPU6050.dataReady = false;
  
  /* Hardware initialization - configure MPU6050 registers */
  uint8_t config_data;
  
  /* 1. Reset MPU6050 (set DEVICE_RESET bit) */
  config_data = 0x80;
  HAL_I2C_Mem_Write(hi2c, MPU6050_SLAVE_ADDR, MPU6050_REG_PWR_MGMT_1, 
                    I2C_MEMADD_SIZE_8BIT, &config_data, 1, 100);
  HAL_Delay(100);  // Wait for reset to complete
  
  /* 2. Wake up MPU6050 (PWR_MGMT_1 register, clear sleep bit) */
  config_data = 0x00;
  HAL_I2C_Mem_Write(hi2c, MPU6050_SLAVE_ADDR, MPU6050_REG_PWR_MGMT_1, 
                    I2C_MEMADD_SIZE_8BIT, &config_data, 1, 100);
  
  /* 3. Configure Gyroscope full scale range (±1000°/s) */
  config_data = MPU6050_GYRO_FS_1000;
  HAL_I2C_Mem_Write(hi2c, MPU6050_SLAVE_ADDR, MPU6050_REG_GYRO_CONFIG, 
                    I2C_MEMADD_SIZE_8BIT, &config_data, 1, 100);
  
  /* 4. Configure Accelerometer full scale range (±4g) */
  config_data = MPU6050_ACCEL_FS_4G;
  HAL_I2C_Mem_Write(hi2c, MPU6050_SLAVE_ADDR, MPU6050_REG_ACCEL_CONFIG, 
                    I2C_MEMADD_SIZE_8BIT, &config_data, 1, 100);
  
  /* 5. Configure low-pass filter (DLPF_CFG = 3, ~44Hz) */
  config_data = MPU6050_DLPF_44HZ;
  HAL_I2C_Mem_Write(hi2c, MPU6050_SLAVE_ADDR, MPU6050_REG_CONFIG, 
                    I2C_MEMADD_SIZE_8BIT, &config_data, 1, 100);
  
  /* Setup slave device for continuous reading */
  MPU6050.slaves[0].address = MPU6050_SLAVE_ADDR;
  MPU6050.slaves[0].txBuffer = txBuffer1;
  MPU6050.slaves[0].rxBuffer = rxBuffer1;
  MPU6050.slaves[0].processBuffer = processBuffer1;
  MPU6050.slaves[0].bufferSize = MPU6050_BUFFER_SIZE;
  MPU6050.slaves[0].txSize = 1;  // Send register address before read
  MPU6050.slaves[0].enabled = true;
  
  /* Clear buffers */
  if (rxBuffer1) memset(rxBuffer1, 0, MPU6050_BUFFER_SIZE);
  memset(processBuffer1, 0, MPU6050_BUFFER_SIZE);
  
  /* Initialize generic I2C module for periodic reading */
  I2C_Module_Init(
    &MPU6050.i2cModule,
    hi2c,
    MPU6050.slaves,
    MPU6050_SLAVES_NO,
    100,  // 100ms polling interval
    MPU6050_DataProcessCallback
  );
  
  dataUart_PrintInitMessage("MPU6050");
}

void MPU6050_Process(void) {
  /* Delegate to common I2C module state machine */
  I2C_Module_Process(&MPU6050.i2cModule);
}

void MPU6050_RxCallback(I2C_HandleTypeDef *hi2c) {
  /* Delegate to common I2C module callback */
  I2C_Module_RxCallback(&MPU6050.i2cModule, hi2c);
}

void MPU6050_ErrorCallback(I2C_HandleTypeDef *hi2c) {
  /* Delegate to common I2C module callback */
  I2C_Module_ErrorCallback(&MPU6050.i2cModule, hi2c);
}

const MPU6050_t* MPU6050_GetData(void) {
  return &MPU6050;
}

bool MPU6050_IsDataReady(void) {
  return MPU6050.dataReady;
}

void MPU6050_SetDataReady(bool ready) {
  MPU6050.dataReady = ready;
}