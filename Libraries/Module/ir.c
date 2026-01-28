                         #include "ir.h"

/* Global IR instance */
IR_t IR = {0};

/* Track which slaves have fresh data in current polling cycle */
static uint8_t freshDataMask = 0;

/* DMA buffers using predefined RAM_D2 addresses from const.h */
static uint8_t *rxBuffer1 = IR_SLAVE1_RXBUF_PTR;
static uint8_t *rxBuffer2 = IR_SLAVE2_RXBUF_PTR;
static uint8_t processBuffer1[IR_BUFFER_SIZE];
static uint8_t processBuffer2[IR_BUFFER_SIZE];

/* Helper function to combine MSB and LSB */
static uint16_t combine_data(uint8_t msb, uint8_t lsb) { 
  return (uint16_t)((msb << 8) | lsb);
}

/* Data processing callback - called by generic I2C module */
static void IR_DataProcessCallback(I2C_Module_t *module, uint8_t slaveId) {
  if (slaveId >= IR_SLAVES_NO) return;
  
  I2C_SlaveDevice_t *slave = &IR.slaves[slaveId];
  if (!slave->enabled || slave->processBuffer == NULL) return;
  
  /* Parse sensor data: [Vref_LSB, Vref_MSB, eye0_LSB, eye0_MSB, ...] */
  const uint16_t offset = slaveId * EYE_NUM;
  for (uint8_t i = 0; i < EYE_NUM; i++) {
    const uint8_t idx = 2 + (i * 2);  // Skip Vref (first 2 bytes)
    IR.eyeValues[offset + i] = combine_data(slave->processBuffer[idx + 1], slave->processBuffer[idx]);
  }
  
  /* Mark fresh data and check if all enabled slaves are ready */
  __disable_irq();
  freshDataMask |= (1 << slaveId);
  
  /* Calculate enabled mask inline */
  uint8_t enabledMask = 0;
  for (uint8_t s = 0; s < IR_SLAVES_NO; s++) {
    if (IR.slaves[s].enabled) enabledMask |= (1 << s);
  }
  
  /* All enabled slaves have fresh data? */
  if (freshDataMask == enabledMask) {
    freshDataMask = 0;  // Reset for next cycle
    __enable_irq();
    
    /* Find maximum value across all enabled eyes */
    IR.maxValue = 0;
    IR.maxEye = 0;
    for (uint8_t eye = 0; eye < IR_SLAVES_NO * EYE_NUM; eye++) {
      const uint8_t slaveIdx = eye / EYE_NUM;
      if (IR.slaves[slaveIdx].enabled && IR.eyeValues[eye] > IR.maxValue) {
        IR.maxValue = IR.eyeValues[eye];
        IR.maxEye = eye;
      }
    }
    IR.dataReady = true;
    
    /* Debug output */
    dataUart_PrintIRData(IR.maxEye, IR.maxValue, IR.eyeValues);
  }
}

void IR_Init(I2C_HandleTypeDef *hi2c) {
  if (hi2c == NULL) return;
  
  /* Initialize IR structure */
  memset(&IR, 0, sizeof(IR_t));
  IR.dataReady = false;
  
  /* Setup slave 1 */
  IR.slaves[IR_SLAVE_1].address = IR_SLAVE_1_ADDR;
  IR.slaves[IR_SLAVE_1].txBuffer = NULL;  // Direct read, no TX needed
  IR.slaves[IR_SLAVE_1].rxBuffer = rxBuffer1;
  IR.slaves[IR_SLAVE_1].processBuffer = processBuffer1;
  IR.slaves[IR_SLAVE_1].bufferSize = IR_BUFFER_SIZE;
  IR.slaves[IR_SLAVE_1].txSize = 0;  // 0 = direct read without register write
  IR.slaves[IR_SLAVE_1].enabled = true;  // Enabled
  
  /* Setup slave 2 - TEMPORARILY DISABLED FOR TESTING */
  IR.slaves[IR_SLAVE_2].address = IR_SLAVE_2_ADDR;
  IR.slaves[IR_SLAVE_2].txBuffer = NULL;  // Direct read, no TX needed
  IR.slaves[IR_SLAVE_2].rxBuffer = rxBuffer2;
  IR.slaves[IR_SLAVE_2].processBuffer = processBuffer2;
  IR.slaves[IR_SLAVE_2].bufferSize = IR_BUFFER_SIZE;
  IR.slaves[IR_SLAVE_2].txSize = 0;  // 0 = direct read without register write
  IR.slaves[IR_SLAVE_2].enabled = true;  // DISABLED - Only use slave 1 for now
  
  /* Clear buffers */
  if (rxBuffer1) memset(rxBuffer1, 0, IR_BUFFER_SIZE);
  if (rxBuffer2) memset(rxBuffer2, 0, IR_BUFFER_SIZE);
  memset(processBuffer1, 0, IR_BUFFER_SIZE);
  memset(processBuffer2, 0, IR_BUFFER_SIZE);
  
  /* Initialize generic I2C module with data processing callback */
  I2C_Module_Init(
    &IR.i2cModule,
    hi2c,
    IR.slaves,
    IR_SLAVES_NO,
    IR_SAMPLE_PERIOD_MS,
    IR_DataProcessCallback
  );
}

void IR_Process(void) {
  /* Delegate to common I2C module state machine */
  I2C_Module_Process(&IR.i2cModule);
}

void IR_RxCallback(I2C_HandleTypeDef *hi2c) {
  /* Delegate to common I2C module callback */
  I2C_Module_RxCallback(&IR.i2cModule, hi2c);
}

void IR_ErrorCallback(I2C_HandleTypeDef *hi2c) {
  /* Delegate to common I2C module callback */
  I2C_Module_ErrorCallback(&IR.i2cModule, hi2c);
}