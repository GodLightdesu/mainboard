#include "ir.h"

static IR_t IR = {0};

/* 追蹤當前輪詢週期中哪些從設備有新資料 */
static uint8_t freshDataMask = 0;

/* 使用 const.h 中預定義的 RAM_D2 位址的 DMA 緩衝區 */
static uint8_t *rxBuffer1 = IR_SLAVE1_RXBUF_PTR;
static uint8_t *rxBuffer2 = IR_SLAVE2_RXBUF_PTR;
static uint8_t processBuffer1[IR_BUFFER_SIZE];
static uint8_t processBuffer2[IR_BUFFER_SIZE];

/* 每個感測器的角度位置 */
static const float EYE_ANGLE_OFFEST = 11.3f;
static const float EYE_ANGLE_DIFF = 30.0f;
static const float EYE_ANGLES[14] = {
  0.0f + EYE_ANGLE_OFFEST,   // 0°
  EYE_ANGLE_DIFF + EYE_ANGLE_OFFEST,
  EYE_ANGLE_DIFF * 2 + EYE_ANGLE_OFFEST, 
  EYE_ANGLE_DIFF * 3,        // 90°
  EYE_ANGLE_DIFF * 4 + EYE_ANGLE_OFFEST,
  EYE_ANGLE_DIFF * 5 + EYE_ANGLE_OFFEST,
  180.0f - EYE_ANGLE_OFFEST, // 從設備 1 (0-6)
  180.0f + EYE_ANGLE_OFFEST, // 從設備 2 (7-13)
  EYE_ANGLE_DIFF * 7 + EYE_ANGLE_OFFEST,
  EYE_ANGLE_DIFF * 8 + EYE_ANGLE_OFFEST, 
  EYE_ANGLE_DIFF * 9,       // 270°
  EYE_ANGLE_DIFF * 10 + EYE_ANGLE_OFFEST,
  EYE_ANGLE_DIFF * 11 - EYE_ANGLE_OFFEST,
  360.0f - EYE_ANGLE_OFFEST
};

/* 輔助函數：組合 MSB 和 LSB */
static uint16_t combine_data(uint8_t msb, uint8_t lsb) { 
  return (uint16_t)((msb << 8) | lsb);
}

/* 資料處理回調函數 - 由通用 I2C 模組呼叫 */
static void IR_DataProcessCallback(I2C_Module_t *module, uint8_t slaveId) {
  if (slaveId >= IR_SLAVES_NO) return;
  
  I2C_SlaveDevice_t *slave = &IR.slaves[slaveId];
  if (!slave->enabled || slave->processBuffer == NULL) return;
  
  /* 解析感測器資料：[Vref_LSB, Vref_MSB, eye0_LSB, eye0_MSB, ...] */
  const uint16_t offset = slaveId * EYE_NUM;
  for (uint8_t i = 0; i < EYE_NUM; i++) {
    const uint8_t idx = 2 + (i * 2);  // 跳過 Vref（前 2 位元組）
    
    /* 邊界檢查：確保不會超出緩衝區 */
    if (idx + 1 >= slave->bufferSize) {
      IR.eyeValues[offset + i] = 0;
      continue;
    }
    
    IR.eyeValues[offset + i] = combine_data(slave->processBuffer[idx + 1], slave->processBuffer[idx]);
  }
  
  /* 標記新資料並檢查所有已啟用的從設備是否準備好 */
  /* Precompute enabled mask outside critical section */
  uint8_t enabledMask = 0;
  for (uint8_t s = 0; s < IR_SLAVES_NO; s++) {
    if (IR.slaves[s].enabled) enabledMask |= (1 << s);
  }
  
  /* Critical section: only update flags atomically */
  __disable_irq();
  freshDataMask |= (1 << slaveId);
  uint8_t localFreshMask = freshDataMask;
  __enable_irq();
  
  /* 所有已啟用的從設備都有新資料了嗎？*/
  if (localFreshMask == enabledMask) {
    /* Reset mask atomically */
    __disable_irq();
    freshDataMask = 0;
    __enable_irq();
    
    /* 找出所有已啟用感測器中的最大值 */
    IR.maxValue = 0;
    IR.maxEye = 0;
    const uint8_t totalEyes = IR_SLAVES_NO * EYE_NUM;  // 14 個感測器
    for (uint8_t eye = 0; eye < totalEyes; eye++) {
      const uint8_t slaveIdx = eye / EYE_NUM;
      if (IR.slaves[slaveIdx].enabled && IR.eyeValues[eye] > IR.maxValue) {
        IR.maxValue = IR.eyeValues[eye];
        IR.maxEye = eye;
      }
    }
    
    /* 自動計算球的角度（使用拋物線插值法以獲得最高精度）*/
    IR.ballAngle = IR_CalBallAngleInterpolated();
    // IR.ballAngle = IR_CalBallAngle();
    
    IR.dataReady = true;
    
    /* 除錯輸出 */
    dataUart_PrintIRData(IR_GetData());
  }
}

void IR_Init(I2C_HandleTypeDef *hi2c) {
  if (hi2c == NULL) return;
  
  /* 初始化 IR 結構 */
  memset(&IR, 0, sizeof(IR_t));
  IR.dataReady = false;
  
  /* 設定從設備 1 */
  IR.slaves[IR_SLAVE_1].address = IR_SLAVE_1_ADDR;
  IR.slaves[IR_SLAVE_1].txBuffer = NULL;  // 直接讀取，無需 TX
  IR.slaves[IR_SLAVE_1].rxBuffer = rxBuffer1;
  IR.slaves[IR_SLAVE_1].processBuffer = processBuffer1;
  IR.slaves[IR_SLAVE_1].bufferSize = IR_BUFFER_SIZE;
  IR.slaves[IR_SLAVE_1].txSize = 0;  // 0 = 直接讀取，不寫入暫存器
  IR.slaves[IR_SLAVE_1].enabled = true;  // 已啟用
  
  /* 設定從設備 2 - 暫時停用以供測試 */
  IR.slaves[IR_SLAVE_2].address = IR_SLAVE_2_ADDR;
  IR.slaves[IR_SLAVE_2].txBuffer = NULL;  // 直接讀取，無需 TX
  IR.slaves[IR_SLAVE_2].rxBuffer = rxBuffer2;
  IR.slaves[IR_SLAVE_2].processBuffer = processBuffer2;
  IR.slaves[IR_SLAVE_2].bufferSize = IR_BUFFER_SIZE;
  IR.slaves[IR_SLAVE_2].txSize = 0;  // 0 = 直接讀取，不寫入暫存器
  IR.slaves[IR_SLAVE_2].enabled = true;  // 已啟用
  
  /* 清空緩衝區 */
  if (rxBuffer1) memset(rxBuffer1, 0, IR_BUFFER_SIZE);
  if (rxBuffer2) memset(rxBuffer2, 0, IR_BUFFER_SIZE);
  memset(processBuffer1, 0, IR_BUFFER_SIZE);
  memset(processBuffer2, 0, IR_BUFFER_SIZE);
  
  /* 使用資料處理回調函數初始化通用 I2C 模組 */
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
  /* 委派給通用 I2C 模組狀態機 */
  I2C_Module_Process(&IR.i2cModule);
}

void IR_RxCallback(I2C_HandleTypeDef *hi2c) {
  /* 委派給通用 I2C 模組回調 */
  I2C_Module_RxCallback(&IR.i2cModule, hi2c);
}

void IR_ErrorCallback(I2C_HandleTypeDef *hi2c) {
  /* 委派給通用 I2C 模組回調 */
  I2C_Module_ErrorCallback(&IR.i2cModule, hi2c);
}

/**
 * @brief 使用加權平均法計算球的角度（方法 1）
 * 
 * 此方法使用向量加法，以每個感測器的訊號強度作為權重。
 * 使用從 IR.eyeValues[] 預先計算的資料（由 IR_DataProcessCallback 填充）。
 * 當球靠近且多個感測器偵測到時更準確。
 * 
 * @return 球的角度（度數，0-360°），如果未偵測到球則返回 -1.0f
 */
float IR_CalBallAngle(void) {
  // 檢查是否偵測到球
  if (IR.maxValue < IR_DETECTION_THRESHOLD) {
    return -1.0f;  // 未偵測到球
  }
  
  // 加權向量計算
  float sumX = 0.0f, sumY = 0.0f, sumWeight = 0.0f;
  
  // 使用預先計算的 eyeValues 處理所有 14 個感測器
  const uint8_t totalEyes = IR_SLAVES_NO * EYE_NUM;  // 14 個感測器
  for (uint8_t eye = 0; eye < totalEyes; eye++) {
    uint16_t value = IR.eyeValues[eye];
    
    // 僅使用超過閾值的感測器
    if (value > IR_DETECTION_THRESHOLD) {
      /* 邊界檢查：確保眼睛索引有效 */
      if (eye >= 14) continue;  // 安全檢查
      
      float weight = (float)value;
      float rad = EYE_ANGLES[eye] * (PI / 180.0f);
      
      sumX += weight * arm_cos_f32(rad);
      sumY += weight * arm_sin_f32(rad);
      sumWeight += weight;
    }
  }
  
  // 未找到有效感測器
  if (sumWeight == 0.0f) { return -1.0f; }
  
  // 使用 atan2 計算加權方向
  float angle;
  // change x and y to match 0° at front and clockwise positive
  arm_atan2_f32(sumX, sumY, &angle);
  angle *= (180.0f / PI);

  // map to 0-360° (use modulo to prevent infinite loop on NaN)
  if (!isnan(angle)) {
    angle = fmodf(angle, 360.0f);
    if (angle < 0.0f) { angle += 360.0f; }
  } else {
    return -1.0f;  // Invalid angle
  }
  
  return angle;
}

/**
 * @brief 使用三點拋物線插值法計算球的角度（方法 3）
 * 
 * 此方法使用最強感測器及其兩個相鄰感測器，
 * 透過拋物線插值找到精確的峰值位置。
 * 使用從 IR.eyeValues[] 預先計算的資料（由 IR_DataProcessCallback 填充）。
 * 當球在相鄰感測器之間時精度最高。
 * 
 * @return 球的角度（度數，0-360°），如果未偵測到球則返回 -1.0f
 */
float IR_CalBallAngleInterpolated(void) {
  // 檢查是否偵測到球
  if (IR.maxValue < IR_DETECTION_THRESHOLD) {
    return -1.0f;  // 未偵測到球
  }
  
  uint8_t maxIdx = IR.maxEye;
  
  /* 邊界檢查：驗證最大眼睛索引在有效範圍內 */
  if (maxIdx >= 14) {
    return -1.0f;  // 無效索引，返回無球
  }
  
  // 取得相鄰感測器索引（循環包裹）
  uint8_t prevIdx = (maxIdx == 0) ? 13 : maxIdx - 1;
  uint8_t nextIdx = (maxIdx == 13) ? 0 : maxIdx + 1;
  
  // 從預先計算的 eyeValues 陣列取得感測器值
  uint16_t prevVal = IR.eyeValues[prevIdx];
  uint16_t maxVal = IR.eyeValues[maxIdx];
  uint16_t nextVal = IR.eyeValues[nextIdx];
  
  // 拋物線插值以找出峰值偏移
  // 公式：offset = (prevVal - nextVal) / (2 * (prevVal - 2*maxVal + nextVal))
  float offset = 0.0f;
  float denominator = 2.0f * (prevVal - 2 * maxVal + nextVal);
  
  if (fabsf(denominator) > 1.0f) {  // 避免除以零
    offset = (prevVal - nextVal) / denominator;
    
    // 將偏移限制在 [-1, 1] 以確保安全
    if (offset > 1.0f) offset = 1.0f;
    if (offset < -1.0f) offset = -1.0f;
  }
  
  // 計算插值角度
  // 25.7° = 360° / 14 個感測器（相鄰感測器之間的角度）
  float angle = EYE_ANGLES[maxIdx] + offset * 25.7f;
  
  angle -= 90.0f;          // 調整為 0° 在前方
  angle = 360.0f - angle;  // 改為順時針方向

  while (angle >= 360.0f) { angle -= 360.0f; }
  while (angle < 0.0f) { angle += 360.0f; }
  
  return angle;
}

const IR_t* IR_GetData(void) { return &IR; }

bool IR_IsDataReady(void) { return IR.dataReady; }

void IR_ClearDataReady(void) { IR.dataReady = false; }

bool IR_SetSlaveEnabled(uint8_t slaveId, bool enable) {
  return I2C_Module_SetSlaveEnabled(&IR.i2cModule, slaveId, enable);
}

bool IR_IsSlaveEnabled(uint8_t slaveId) {
  return I2C_Module_IsSlaveEnabled(&IR.i2cModule, slaveId);
}

uint16_t IR_GetSlaveAddress(uint8_t slaveId) {
  if (slaveId >= IR_SLAVES_NO) return 0;
  return IR.slaves[slaveId].address;
}