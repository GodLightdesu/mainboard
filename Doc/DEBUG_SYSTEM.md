# 調試系統使用指南

本文檔說明專案中模塊化調試系統的架構和使用方法。

## 概述

本專案採用**模塊化 DEBUG 宏系統**，允許為每個模塊獨立控制 UART 調試輸出。所有打印函數集中在 `data_uart` 模塊中管理。

### 主要特性

- ✅ **模塊化控制**：每個模塊獨立的 DEBUG 宏
- ✅ **集中管理**：所有打印函數在 `data_uart` 模塊
- ✅ **統一配置**：在 `CMakeLists.txt` 中統一控制
- ✅ **靈活調試**：可選擇性啟用需要的模塊
- ✅ **資源節省**：Release 版本可完全禁用調試輸出

## 架構

```
CMakeLists.txt
    ↓ 定義 DEBUG_XXX 宏
    ↓
data_uart.c/h
    ↓ 提供打印函數
    ↓
各模塊 (mpu6050.c, ir.c, i2c_common.c, etc.)
    ↓ 調用 dataUart_PrintXXX()
    ↓
UART4 輸出 (115200 baud)
```

## DEBUG 宏配置

### 在 CMakeLists.txt 中配置

文件位置：[CMakeLists.txt](../CMakeLists.txt#L68-L76)

```cmake
# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
    # Uncomment the lines below to enable debug UART printing for each module
    # DEBUG_MPU6050        # MPU6050 sensor data and initialization
    # DEBUG_MPU6050_DMP    # MPU6050 attitude (Roll/Pitch/Yaw)
    # DEBUG_IR             # IR sensor eye data
    # DEBUG_I2C            # I2C communication errors and status
    # DEBUG_MOTORS         # Motor testing PWM values
)
```

### DEBUG 宏對照表

| DEBUG 宏 | 控制的模塊 | 輸出內容 |
|---------|----------|---------|
| `DEBUG_MPU6050` | MPU6050 傳感器 | • 傳感器數據 (Ax/Ay/Az/Gx/Gy/Gz/溫度)<br>• 初始化完成消息 |
| `DEBUG_MPU6050_DMP` | MPU6050 姿態估計 | • 姿態數據 (Roll/Pitch/Yaw)<br>• 互補濾波器初始化消息 |
| `DEBUG_IR` | IR 感測器 | • 眼睛數據 (Eye/Val)<br>• 所有感測器數值 |
| `DEBUG_I2C` | I2C 通信 | • I2C 錯誤碼和從機 ID<br>• 超時警告<br>• RX 回調計數<br>• 設備發現消息<br>• 原始十六進制數據 |
| `DEBUG_MOTORS` | 馬達控制 | • 馬達測試標題<br>• PWM 值和占空比<br>• 測試字符串 |

## data_uart 模塊

### 打印函數列表

文件位置：[data_uart.c](../Libraries/Uart/data_uart.c)

```c
// MPU6050 調試打印
void dataUart_PrintMPU6050Data(float ax, float ay, float az, 
                                float gx, float gy, float gz, float temp);
void dataUart_PrintMPU6050Attitude(float roll, float pitch, float yaw);

// IR 感測器調試打印
void dataUart_PrintIRData(uint8_t maxEye, uint16_t maxValue, 
                          const uint16_t *eyeValues);

// I2C 調試打印
void dataUart_PrintI2CError(const char *errorType, int errorCode, int slaveId);
void dataUart_PrintI2CStatus(const char *message);
void dataUart_PrintDeviceFound(uint16_t addr);

// 馬達調試打印
void dataUart_PrintMotorTest(int motorId);

// 通用調試打印
void dataUart_PrintInitMessage(const char *moduleName);
void dataUart_SendString(const char *str);
void dataUart_SendFormattedPWM(uint16_t pwm, float duty_percent);

// IR 數據顯示
HAL_StatusTypeDef ParseAndDisplayIRData(const uint8_t *data, uint16_t size);
HAL_StatusTypeDef DisplayRawHexData(const uint8_t *data, uint16_t size);
```

### 函數特性

所有打印函數都包含：

- ✅ **NULL 檢查**：自動檢查參數有效性
- ✅ **緩衝區保護**：防止緩衝區溢出
- ✅ **條件編譯**：只在對應 DEBUG 宏啟用時編譯
- ✅ **格式化輸出**：統一的輸出格式

## 使用示例

### 示例 1：僅調試 MPU6050

```cmake
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    DEBUG_MPU6050        # 只啟用 MPU6050
)
```

**輸出**：
```
MPU6050 Ready
Ax=0.12 Ay=-0.03 Az=0.98 Gx=0.5 Gy=-0.2 Gz=0.1 T=25.3
```

### 示例 2：調試 I2C 通信問題

```cmake
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    DEBUG_I2C            # 啟用 I2C 調試
)
```

**輸出**：
```
Device found at 0x68
Device found at 0x30
Device found at 0x31
RxCallback: 50
RxCallback: 100
```

### 示例 3：全面調試（開發階段）

```cmake
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    DEBUG_MPU6050
    DEBUG_MPU6050_DMP
    DEBUG_IR
    DEBUG_I2C
    DEBUG_MOTORS
)
```

### 示例 4：Release 版本（禁用所有調試）

```cmake
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # 全部註解 = 禁用所有調試輸出
)
```

## 輸出格式參考

### MPU6050 傳感器數據
```
Ax=0.12 Ay=-0.03 Az=0.98 Gx=0.5 Gy=-0.2 Gz=0.1 T=25.3
```
格式：`Ax=%.2f Ay=%.2f Az=%.2f Gx=%.1f Gy=%.1f Gz=%.1f T=%.1f`

### MPU6050 姿態數據
```
Roll=2.3 Pitch=-1.5 Yaw=45.2
```
格式：`Roll=%.1f Pitch=%.1f Yaw=%.1f`

### IR 感測器數據
```
Eye:3 Val:1024 | S0:[100,200,300,400,500,600,700] S1:[150,250,350,450,550,650,750]
```
格式：顯示最大值眼睛編號、最大值，以及所有感測器數值

### I2C 錯誤
```
I2C_Error: Code=0x4, Slave=1
I2C_Timeout: Slave=0
I2C_Read FAIL: Code=1, HAL_state=32
```

### 馬達測試
```
=== Testing Motor 0 ===
PWM: 400 (40.0%)
PWM: 420 (42.0%)
```

## 在模塊中使用

### 在現有模塊中添加調試輸出

1. **包含頭文件**：
```c
#include "data_uart.h"
```

2. **調用打印函數**：
```c
// 在需要調試的地方
dataUart_PrintI2CError("Custom Error", errorCode, slaveId);
```

3. **函數自動受 DEBUG 宏控制**：
   - 無需在調用處添加 `#ifdef DEBUG_XXX`
   - 打印函數內部已包含條件編譯

### 為新模塊添加調試支持

1. **在 CMakeLists.txt 中添加新 DEBUG 宏**：
```cmake
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # DEBUG_MY_NEW_MODULE  # 新模塊的 DEBUG 宏
)
```

2. **在 data_uart.h 中聲明新函數**：
```c
void dataUart_PrintMyModuleData(int value);
```

3. **在 data_uart.c 中實現函數**：
```c
void dataUart_PrintMyModuleData(int value) {
#ifdef DEBUG_MY_NEW_MODULE
  if (dataUart_huart == NULL) return;
  
  char msg[50];
  int len = snprintf(msg, sizeof(msg), "MyModule: %d\r\n", value);
  if (len > 0 && len < (int)sizeof(msg)) {
    HAL_UART_Transmit(dataUart_huart, (uint8_t*)msg, len, 100);
  }
#endif
}
```

4. **在模塊中調用**：
```c
#include "data_uart.h"

void MyModule_Process(void) {
  // ...
  dataUart_PrintMyModuleData(myValue);
}
```

## 最佳實踐

### 開發階段
1. ✅ 啟用所有相關模塊的 DEBUG 宏
2. ✅ 觀察輸出確認系統正常運作
3. ✅ 使用 DEBUG 輸出定位問題

### 優化階段
1. ✅ 逐步禁用已確認正常的模塊
2. ✅ 只保留需要監控的關鍵模塊
3. ✅ 減少 UART 帶寬占用

### 發布階段
1. ✅ 禁用所有 DEBUG 宏
2. ✅ 減少 Flash 和 RAM 使用
3. ✅ 提升系統效能

### 故障排查
1. ✅ 根據問題啟用特定模塊的 DEBUG
2. ✅ 使用 `DEBUG_I2C` 排查通信問題
3. ✅ 使用模塊專用 DEBUG 排查邏輯問題

## 資源使用

### Flash 使用
- 每個 DEBUG 宏約增加 **200-500 bytes**（取決於打印函數數量）
- 全部啟用約增加 **2-3 KB**
- 全部禁用：**0 bytes** 額外開銷

### RAM 使用
- 打印緩衝區（棧上）：**50-300 bytes**（臨時）
- 無全局緩衝區開銷

### UART 帶寬
- 115200 baud = ~11.5 KB/s
- 建議限制打印頻率（如每 100ms 一次）以避免阻塞

## 故障排查

### 問題：無法看到調試輸出

**檢查清單**：
1. ✅ 確認在 CMakeLists.txt 中取消了對應 DEBUG 宏的註解
2. ✅ 確認重新編譯了專案（CMake 配置 + 建構）
3. ✅ 確認 UART4 已初始化：`dataUart_Init(&huart4)`
4. ✅ 確認串口監視器設置正確（115200 baud, 8N1）
5. ✅ 確認 UART4 引腳連接正確

### 問題：輸出亂碼

**可能原因**：
1. 鮑率設置不正確（應為 115200）
2. UART 初始化順序問題（應在使用前初始化）
3. DMA 配置衝突

### 問題：部分消息丟失

**可能原因**：
1. UART 緩衝區溢出（打印過於頻繁）
2. 中斷優先級設置不當
3. 建議：降低打印頻率或增加緩衝區大小

## 相關文件

- [README.md](../README.md) - 專案概述和系統架構
- [I2C_COMMON_USAGE.md](I2C_COMMON_USAGE.md) - I2C 通用狀態機使用指南
- [data_uart.c](../Libraries/Uart/data_uart.c) - 調試打印實現
- [CMakeLists.txt](../CMakeLists.txt) - DEBUG 宏配置位置
