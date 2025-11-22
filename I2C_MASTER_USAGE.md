# I2C Master 統一管理架構使用指南

## 概述

新的 I2C master 架構提供統一的介面來管理所有 I2C 從設備，簡化程式碼並提高可維護性。

## 架構優勢

✅ **統一管理** - 所有 I2C 從設備透過單一 `I2C_Master_t` 實例管理  
✅ **簡化回調** - 自動處理 DMA 完成和錯誤回調  
✅ **易於擴展** - 添加新設備只需幾行程式碼  
✅ **錯誤追蹤** - 自動記錄每個設備的錯誤次數  
✅ **緩衝管理** - 自動處理 DMA 和處理緩衝區  

## 使用範例

### 1. 初始化 I2C Master

```c
#include "i2c_master.h"

static I2C_Master_t i2cMaster;

void setup(void) {
    /* 初始化 I2C master */
    I2C_Master_Init(&i2cMaster, &hi2c3);
}
```

### 2. 註冊 I2C 從設備

#### IR 感測器範例 (已實現)

```c
/* 準備緩衝區 */
static uint8_t rxBuffer1[IR_BUFFER_SIZE];
static uint8_t processBuffer1[IR_BUFFER_SIZE];

/* 註冊設備 */
int8_t slaveId = I2C_Master_RegisterSlave(
    &i2cMaster,
    IR_SLAVE_1_ADDR,      // 0x30 << 1
    rxBuffer1,            // DMA 接收緩衝區
    processBuffer1,       // 處理緩衝區
    IR_BUFFER_SIZE        // 16 bytes
);
```

#### MPU6050 範例 (未來擴展)

```c
#define MPU6050_ADDR (0x68 << 1)
#define MPU6050_BUFFER_SIZE 14  // Accel(6) + Temp(2) + Gyro(6)

static uint8_t mpu6050RxBuf[MPU6050_BUFFER_SIZE];
static uint8_t mpu6050ProcessBuf[MPU6050_BUFFER_SIZE];

/* 註冊 MPU6050 */
int8_t mpu6050Id = I2C_Master_RegisterSlave(
    &i2cMaster,
    MPU6050_ADDR,
    mpu6050RxBuf,
    mpu6050ProcessBuf,
    MPU6050_BUFFER_SIZE
);
```

### 3. 讀取從設備資料

```c
/* 啟動 DMA 讀取 */
HAL_StatusTypeDef status = I2C_Master_ReadSlave(&i2cMaster, SLAVE_1);

if (status == HAL_OK) {
    /* 讀取請求成功發送 */
} else if (status == HAL_BUSY) {
    /* I2C 忙碌，稍後重試 */
}
```

### 4. 檢查並處理資料

```c
/* 檢查資料是否就緒 */
if (I2C_Master_IsDataReady(&i2cMaster, SLAVE_1)) {
    /* 取得處理緩衝區 */
    uint8_t *data = I2C_Master_GetProcessBuffer(&i2cMaster, SLAVE_1);
    
    /* 處理資料 */
    process_sensor_data(data);
    
    /* 清除就緒標誌 */
    I2C_Master_ClearDataReady(&i2cMaster, SLAVE_1);
}
```

### 5. 啟用/停用設備

```c
/* 停用 SLAVE_2 (如果暫時不使用) */
I2C_Master_SetSlaveEnabled(&i2cMaster, SLAVE_2, false);

/* 啟用 SLAVE_2 */
I2C_Master_SetSlaveEnabled(&i2cMaster, SLAVE_2, true);
```

### 6. 錯誤監控

```c
/* 取得設備錯誤次數 */
uint32_t errors = I2C_Master_GetErrorCount(&i2cMaster, SLAVE_1);
if (errors > 10) {
    /* 處理持續錯誤 */
    handle_persistent_errors();
}
```

## HAL 回調設置

在 `stm32h7xx_it.c` 中連接回調：

```c
/* USER CODE BEGIN Includes */
#include "i2c_master.h"
extern I2C_Master_t i2cMaster;
/* USER CODE END Includes */

/* USER CODE BEGIN 1 */
void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c) {
    I2C_Master_RxCallback(&i2cMaster, hi2c);
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c) {
    I2C_Master_ErrorCallback(&i2cMaster, hi2c);
}
/* USER CODE END 1 */
```

## 主循環範例

```c
uint32_t lastReadTime = HAL_GetTick();

while (1) {
    uint32_t now = HAL_GetTick();
    
    /* 每 20ms 請求一次資料 */
    if ((now - lastReadTime >= 20) && !I2C_Master_IsDataReady(&i2cMaster, SLAVE_1)) {
        if (I2C_Master_ReadSlave(&i2cMaster, SLAVE_1) == HAL_OK) {
            lastReadTime = now;
        }
    }
    
    /* 處理就緒的資料 */
    if (I2C_Master_IsDataReady(&i2cMaster, SLAVE_1)) {
        uint8_t *data = I2C_Master_GetProcessBuffer(&i2cMaster, SLAVE_1);
        process_data(data);
        I2C_Master_ClearDataReady(&i2cMaster, SLAVE_1);
    }
    
    HAL_Delay(1);
}
```

## 添加新 I2C 設備的步驟

1. **定義設備地址和緩衝區大小**
   ```c
   #define NEW_DEVICE_ADDR (0x42 << 1)
   #define NEW_DEVICE_BUFFER_SIZE 32
   ```

2. **準備緩衝區**
   ```c
   static uint8_t newDeviceRxBuf[NEW_DEVICE_BUFFER_SIZE];
   static uint8_t newDeviceProcessBuf[NEW_DEVICE_BUFFER_SIZE];
   ```

3. **註冊設備**
   ```c
   int8_t deviceId = I2C_Master_RegisterSlave(
       &i2cMaster,
       NEW_DEVICE_ADDR,
       newDeviceRxBuf,
       newDeviceProcessBuf,
       NEW_DEVICE_BUFFER_SIZE
   );
   ```

4. **使用 API 讀取和處理**
   ```c
   I2C_Master_ReadSlave(&i2cMaster, deviceId);
   // ... 等待資料就緒
   uint8_t *data = I2C_Master_GetProcessBuffer(&i2cMaster, deviceId);
   ```

## API 完整列表

| 函數 | 說明 |
|------|------|
| `I2C_Master_Init()` | 初始化 I2C master |
| `I2C_Master_RegisterSlave()` | 註冊從設備 |
| `I2C_Master_ReadSlave()` | 啟動 DMA 讀取 |
| `I2C_Master_IsDataReady()` | 檢查資料是否就緒 |
| `I2C_Master_ClearDataReady()` | 清除就緒標誌 |
| `I2C_Master_GetProcessBuffer()` | 取得處理緩衝區 |
| `I2C_Master_SetSlaveEnabled()` | 啟用/停用設備 |
| `I2C_Master_GetErrorCount()` | 取得錯誤計數 |
| `I2C_Master_RxCallback()` | RX 完成回調 (內部) |
| `I2C_Master_ErrorCallback()` | 錯誤回調 (內部) |

## 注意事項

⚠️ **DMA 緩衝區** - RX 緩衝區應放在 RAM_D2 (0x30000000) 以避免快取問題  
⚠️ **同時讀取** - 一次只能有一個 I2C 傳輸進行中  
⚠️ **緩衝區大小** - 確保緩衝區足夠大以容納設備回應  
⚠️ **最大設備數** - 目前最多支援 4 個設備 (可在 `i2c_master.h` 修改)  

## 疑難排解

### 資料永遠不會就緒
- 檢查 HAL 回調是否正確連接
- 確認 DMA 和中斷已啟用
- 驗證設備地址正確

### HAL_BUSY 錯誤
- 等待前一次傳輸完成
- 檢查 `I2C_Master_IsDataReady()` 後再讀取

### 錯誤計數增加
- 檢查設備連接和電源
- 驗證 I2C 時鐘和時序
- 確認設備地址正確
