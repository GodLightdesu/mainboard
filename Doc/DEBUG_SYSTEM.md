# 調試系統使用指南

[![Debug](https://img.shields.io/badge/feature-debug-yellow.svg)]()
[![UART](https://img.shields.io/badge/interface-UART-blue.svg)]()

本文檔說明專案中模塊化調試系統的架構和使用方法。

## 📋 目錄

- [概述](#概述)
- [架構](#架構)
- [Printf 重定向實現](#printf-重定向實現)
- [DEBUG 宏配置](#debug-宏配置)
- [使用方法](#使用方法)
- [最佳實踐](#最佳實踐)

---

## 📝 概述

本專案採用**模塊化 DEBUG 宏系統**，允許為每個模塊獨立控制 UART 調試輸出。所有打印函數集中在 `dataPrint` 模塊中管理，並使用標準 **printf()** 函數進行輸出。

### 主要特性

- ✅ **標準 printf**：使用標準 C 庫 printf() 函數
- ✅ **批次傳輸**：覆寫 _write() 實現高效批次 UART 傳輸
- ✅ **模塊化控制**：每個模塊獨立的 DEBUG 宏
- ✅ **集中管理**：所有打印函數在 `dataPrint` 模塊
- ✅ **統一配置**：在 `CMakeLists.txt` 中統一控制
- ✅ **靈活調試**：可選擇性啟用需要的模塊
- ✅ **資源節省**：Release 版本可完全禁用調試輸出

## 架構

```
CMakeLists.txt
    ↓ 定義 DEBUG_XXX 宏
    ↓
dataPrint.c
    ↓ 實現 _write() 和 __io_putchar()
    ↓ printf() 重定向到 UART
    ↓ 提供包裝函數
    ↓
各模塊 (main.c, soccer.c, button.c, etc.)
    ↓ 使用 printf() 或 dataUart_PrintXXX()
    ↓
UART4 輸出 (9600 baud, 500ms 超時)
```

## Printf 重定向實現

### 核心機制

文件位置：[dataPrint.c](../Libraries/Uart/dataPrint.c#L6-L41)

```c
/* Static UART handle pointer */
static UART_HandleTypeDef *dataUart_huart = NULL;

/**
  * @brief  Retargets C library printf to UART (backup path)
  * @note   Used by putchar() and as fallback for old toolchains
  */
int __io_putchar(int ch) {
  if (dataUart_huart == NULL) return -1;
  // 50ms timeout for single character (~1ms at 9600 baud)
  HAL_UART_Transmit(dataUart_huart, (uint8_t *)&ch, 1, 50);
  return ch;
}

/**
  * @brief  Batch mode write for better performance
  * @note   Overrides weak _write in syscalls.c
  *         This is the PRIMARY path used by printf()
  *         Timeout = 500ms allows ~480 bytes at 9600 baud
  */
int _write(int file, char *ptr, int len) {
  (void)file;
  if (dataUart_huart == NULL) return -1;
  HAL_StatusTypeDef status = HAL_UART_Transmit(dataUart_huart, (uint8_t *)ptr, len, 500);
  return (status == HAL_OK) ? len : -1;
}

void dataUart_Init(UART_HandleTypeDef *huart) {
  dataUart_huart = huart;
}
```

### 調用鏈分析

#### 當前實現（批次模式）
```
printf("Hello %d\r\n", 123)
  ↓
_write(1, "Hello 123\r\n", 11)  ← 我們覆寫的強實現
  ↓
HAL_UART_Transmit(huart, buffer, 11, 500)  ← 一次傳輸 11 字節
  ↓
UART 硬體 (~11.5ms @ 9600 baud)
```

#### 預設實現（字符模式，未覆寫時）
```
printf("Hello %d\r\n", 123)
  ↓
_write(1, "Hello 123\r\n", 11)  ← syscalls.c 中的 weak 實現
  ↓
for (i=0; i<11; i++)
    __io_putchar(buffer[i])  ← 逐字符調用
  ↓
HAL_UART_Transmit(huart, &ch, 1, 50) × 11 次  ← 11 次傳輸
  ↓
UART 硬體 (~11.5ms × 11 = 126ms)
```

#### putchar() 調用路徑
```
putchar('A')
  ↓
__io_putchar('A')  ← 直接使用，不經過 _write()
  ↓
HAL_UART_Transmit(huart, &ch, 1, 50)
```

### 函數職責

| 函數 | 用途 | 是否必需 | 優先級 |
|------|------|---------|--------|
| `_write()` | **printf() 主路徑** | ✅ 必需 | 高 |
| `__io_putchar()` | putchar() 支持 | ⚠️ 可選 | 低 |
| `dataUart_Init()` | 初始化 UART 句柄 | ✅ 必需 | - |

**注意**：
- 如果只使用 `printf()`，理論上只需實現 `_write()` 即可
- 保留 `__io_putchar()` 是為了兼容性和支持 `putchar()` 系列函數
- 某些舊工具鏈可能依賴 `__io_putchar()`

### 性能優勢

| 方法 | 傳輸時間 (100 字節 @ 9600 baud) | UART 調用次數 | 使用函數 |
|------|----------------------------------|---------------|----------|
| _write (批次模式) | ~104ms | 1 次 | **printf()** ← 主要使用 |
| __io_putchar (字符模式) | ~1040ms | 100 次 | putchar() |
| **性能提升** | **10x 更快** | **100x 更少調用** | - |

**說明**：
- `printf()` 通過覆寫的 `_write()` 實現批次傳輸（快）
- `putchar()` 通過 `__io_putchar()` 逐字符傳輸（慢，但必要）
- 兩者互不干擾，分別服務不同的 C 庫函數

## DEBUG 宏配置

### 在 CMakeLists.txt 中配置

文件位置：[CMakeLists.txt](../CMakeLists.txt#L130-L145)

```cmake
# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Debug output control macros
    DEBUG_GENERAL      # 启用通用调试信息输出
    DEBUG_BUTTON       # 启用按键调试信息输出
    # DEBUG_IR         # 启用红外传感器调试信息
    DEBUG_SOCCER       # 启用足球机器人状态机调试信息输出
    # DEBUG_XS         # 启用Xsound传感器调试信息
    
    # Uncomment to enable module-specific debug output:
    # DEBUG_MPU6050        # MPU6050传感器数据和初始化调试
    # DEBUG_MPU6050_DMP    # MPU6050姿态（滚转/俯仰/偏航）调试
    # DEBUG_I2C            # I2C通信错误和状态调试
    # DEBUG_MOTORS         # 电机测试PWM值调试
)
```

### DEBUG 宏對照表

| DEBUG 宏            | 控制的模塊       | 輸出內容                                                                                                      |
| ------------------- | ---------------- | ------------------------------------------------------------------------------------------------------------- |
| `DEBUG_GENERAL`     | 通用訊息         | • 系統重置訊息<br>• IWDG 看門狗重置偵測<br>• 模塊初始化完成訊息                                               |
| `DEBUG_BUTTON`      | 按鍵控制         | • 按鍵事件 (CLICK/DOUBLE_CLICK/LONG_PRESS)<br>• 狀態轉換<br>• 原始狀態變化<br>• 系統啟動等待訊息              |
| `DEBUG_SOCCER`      | 足球機器人       | • 狀態機狀態<br>• 球位置與角度<br>• IR/MPU 就緒狀態                                                           |
| `DEBUG_IR`          | IR 感測器        | • 最大值眼睛編號與數值<br>• 球角度<br>• 所有感測器數值                                                        |
| `DEBUG_XS`          | Xsound 超聲波    | • 4 個超聲波傳感器距離數據                                                                                    |
| `DEBUG_MPU6050`     | MPU6050 傳感器   | • 原始加速度計/陀螺儀數據<br>• 溫度<br>• 初始化狀態                                                            |
| `DEBUG_MPU6050_DMP` | MPU6050 姿態估計 | • 姿態數據 (Roll/Pitch/Yaw)<br>• 四元數<br>• DMP 初始化訊息<br>• 重試訊息                                     |
| `DEBUG_I2C`         | I2C 通信         | • I2C 錯誤碼和從機 ID<br>• 超時警告<br>• RX 回調計數<br>• 設備發現訊息<br>• 原始十六進制數據                 |
| `DEBUG_MOTORS`      | 馬達控制         | • 馬達測試標題<br>• PWM 值和占空比<br>• 測試進度                                                              |

## dataPrint 模塊

### 打印函數列表

文件位置：[dataPrint.c](../Libraries/Uart/dataPrint.c)

#### 初始化與通用函數
```c
void dataUart_Init(UART_HandleTypeDef *huart);  // 必須在 main.c 中調用
void dataUart_SendString(const char *str);       // DEBUG_GENERAL 控制
void dataUart_PrintInitMessage(const char *moduleName);  // 多 DEBUG 宏控制
void dataUart_PrintInitError(const char *errorMsg, int statusCode);
```

#### 模塊專用打印函數
```c
// IR 感測器 (DEBUG_IR)
void dataUart_PrintIRData(const IR_t *IR_Module);
HAL_StatusTypeDef ParseAndDisplayIRData(const uint8_t *data, uint16_t size);

// Xsound 超聲波 (DEBUG_XS)
void dataUart_PrintXsoundData(const Xsound_t *xsound);

// 馬達控制 (DEBUG_MOTORS)
void dataUart_PrintMotorTest(int motorId);
void dataUart_SendFormattedPWM(uint16_t pwm, float duty_percent);

// I2C 通信 (DEBUG_I2C)
void dataUart_PrintI2CError(const char *errorType, int errorCode, int slaveId);
void dataUart_PrintI2CStatus(const char *message);
void dataUart_PrintDeviceFound(uint16_t addr);
HAL_StatusTypeDef DisplayRawHexData(const uint8_t *data, uint16_t size);

// 足球機器人 (DEBUG_SOCCER)
void dataUart_PrintSoccerState(const char *stateName, float ballAngle, 
                                float ballDistance, float yawAngle);

// 按鍵控制 (DEBUG_BUTTON)
void dataUart_PrintButtonEvent(uint8_t btnIndex, const char *buttonName, 
                                const char *eventName);
void dataUart_PrintButtonState(uint8_t btnIndex, const char *buttonName, 
                                const char *stateName);
```

### 函數特性

所有打印函數都包含：

- ✅ **NULL 檢查**：自動檢查參數有效性
- ✅ **條件編譯**：只在對應 DEBUG 宏啟用時編譯
- ✅ **參數消除**：未定義 DEBUG 時使用 `(void)param` 消除警告
- ✅ **統一 printf**：所有函數內部使用 printf() 實現
- ✅ **批次傳輸**：通過 _write() 實現高效傳輸

### 直接使用 Printf

在受 DEBUG 宏保護的代碼中，可以直接使用 printf：

```c
#ifdef DEBUG_MPU6050_DMP
  printf("MPU6050 initialized successfully!\r\n");
#endif

#ifdef DEBUG_SOCCER
  printf("IR: rdy=%d ang=%.1f max=%d eye=%d | Yaw=%.1f MPU=%d | State=%d\r\n",
         irReady, ballAngle, data->irData->maxValue, 
         data->irData->maxEye, yaw, mpuReady, currentState);
#endif
```

## 使用示例

### 示例 1：僅調試 MPU6050 姿態

```cmake
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    DEBUG_MPU6050_DMP    # 只啟用 MPU6050 姿態調試
)
```

**輸出**：
```
MPU6050 initialized successfully!
Euler: Roll=2.34 Pitch=-1.56 Yaw=45.23
Euler: Roll=2.35 Pitch=-1.55 Yaw=45.28
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
I2C_Error: Code=0x4, Slave=1
```

### 示例 3：調試按鍵與系統啟動

```cmake
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    DEBUG_GENERAL        # 通用訊息
    DEBUG_BUTTON         # 按鍵調試
)
```

**輸出**：
```
MPU6050 Ready
Button Module Ready
Waiting for system start...
[START_BTN] Event: CLICK
System started successfully!
```

### 示例 4：全面調試（開發階段）

```cmake
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    DEBUG_GENERAL
    DEBUG_BUTTON
    DEBUG_SOCCER
    DEBUG_MPU6050_DMP
    DEBUG_IR
    DEBUG_XS
    DEBUG_I2C
    DEBUG_MOTORS
)
```

### 示例 5：Release 版本（禁用所有調試）

```cmake
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # 全部註解 = 禁用所有調試輸出
)
```

## 輸出格式參考

### MPU6050 姿態數據（DEBUG_MPU6050_DMP）
```
Euler: Roll=2.34 Pitch=-1.56 Yaw=45.23
```
格式：`Euler: Roll=%.2f Pitch=%.2f Yaw=%.2f`

### IR 感測器數據（DEBUG_IR）
```
Eye:3 Val:1024 ballAngle:45.500000
Decimal: 100 200 300 400 500 600 700 
```
格式：`Eye:%d Val:%d ballAngle:%f`

### Xsound 超聲波數據（DEBUG_XS）
```
Xsound: D0=12.34 D1=23.45 D2=34.56 D3=45.67
```
格式：`Xsound: D0=%.2f D1=%.2f D2=%.2f D3=%.2f`

### 足球機器人狀態（DEBUG_SOCCER）
```
IR: rdy=1 ang=45.5 max=1024 eye=3 | Yaw=90.2 MPU=1 | State=2
Soccer State: SEARCH | Ball: 45.5° 120.3 | Yaw: 90.2°
```

### I2C 通信（DEBUG_I2C）
```
Device found at 0x68
I2C_Error: Code=0x4, Slave=1
I2C_Timeout: Code=0x0
RxCallback: 50
Raw: 01 02 03 04 05 
```

### 馬達測試（DEBUG_MOTORS）
```
=== Testing Motor 0 ===
PWM: 400 (40.0%)
PWM: 420 (42.0%)
=== Test Complete ===
```

### 按鍵事件（DEBUG_BUTTON）
```
[START_BTN] Event: CLICK
[STOP_BTN] Event: DOUBLE_CLICK
[START_BTN] Event: LONG_PRESS_START
[START_BTN] Event: LONG_PRESS_END
[START_BTN] State: PRESSED
[STOP_BTN] State: WAIT_DOUBLE
[START_BTN] Raw state changed to: PRESSED
[START_BTN] Stable state changed to: PRESSED
```

### 通用訊息（DEBUG_GENERAL）
```
MPU6050 Ready
Button Module Ready
System Reset - Running from Flash...
IWDG Reset detected - Auto-starting system...
Waiting for system start...
System started successfully!
```

## 在模塊中使用

### 方法 1：直接使用 printf（推薦）

1. **包含頭文件**：
```c
#include <stdio.h>  // 標準 printf
```

2. **使用 DEBUG 宏保護**：
```c
#ifdef DEBUG_MY_MODULE
  printf("MyModule: value=%d status=%s\r\n", myValue, statusStr);
#endif
```

3. **優點**：
   - ✅ 標準 C 語法
   - ✅ 靈活的格式化
   - ✅ 編譯器優化支持
   - ✅ 不需要預分配緩衝區

### 方法 2：使用 dataPrint 包裝函數

1. **包含頭文件**：
```c
#include "dataPrint.h"
```

2. **調用包裝函數**：
```c
// 函數內部已包含 DEBUG 宏檢查
dataUart_PrintI2CError("Custom Error", errorCode, slaveId);
dataUart_PrintButtonEvent(btnIndex, "MY_BTN", "CLICK");
```

3. **優點**：
   - ✅ 統一的輸出格式
   - ✅ 自動 NULL 檢查
   - ✅ 無需手動添加 #ifdef

### 初始化要求

在 main.c 中必須調用初始化：

```c
int main(void) {
  // ... HAL_Init(), 系統時鐘配置 ...
  
  MX_UART4_Init();           // 初始化 UART4 硬件
  dataUart_Init(&huart4);    // ✅ 設置 printf 重定向
  
  // 現在可以使用 printf 或 dataUart_Print* 函數
  printf("System starting...\r\n");
}
```

### 為新模塊添加調試支持

#### 步驟 1：在 CMakeLists.txt 添加 DEBUG 宏

```cmake
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # DEBUG_MY_NEW_MODULE  # 新模塊的 DEBUG 宏
)
```

#### 步驟 2：選擇實現方式

**選項 A：直接在模塊中使用 printf**

```c
// my_module.c
#include <stdio.h>

void MyModule_Process(int value) {
  #ifdef DEBUG_MY_NEW_MODULE
    printf("MyModule: processing value=%d\r\n", value);
  #endif
  
  // ... 模塊邏輯 ...
}
```

**選項 B：在 dataPrint 中添加包裝函數**

```c
// dataPrint.h
void dataUart_PrintMyModuleData(int value);

// dataPrint.c
void dataUart_PrintMyModuleData(int value) {
  #ifdef DEBUG_MY_NEW_MODULE
    printf("MyModule: value=%d\r\n", value);
  #else
    (void)value;
  #endif
}

// my_module.c
#include "dataPrint.h"

void MyModule_Process(int value) {
  dataUart_PrintMyModuleData(value);  // 自動檢查 DEBUG 宏
}
```

**選擇建議**：
- 🎯 **選項 A**：適合臨時調試、快速迭代
- 🎯 **選項 B**：適合長期維護、統一格式

## 最佳實踐

### 開發階段
1. ✅ 啟用 `DEBUG_GENERAL` + 相關模塊 DEBUG 宏
2. ✅ 使用 `DEBUG_I2C` 監控通信問題
3. ✅ 使用 `DEBUG_BUTTON` 確認用戶輸入
4. ✅ 觀察輸出確認系統正常運作

### 優化階段
1. ✅ 逐步禁用已確認正常的模塊
2. ✅ 只保留需要監控的關鍵模塊（如 `DEBUG_SOCCER`）
3. ✅ 減少 UART 帶寬占用（9600 baud 較慢）
4. ✅ 注意超時設置（500ms 適合長訊息）

### 發布階段
1. ✅ 禁用所有 DEBUG 宏
2. ✅ 減少 Flash 和 RAM 使用
3. ✅ 提升系統效能（減少 printf 開銷）
4. ✅ 保留 `dataUart_Init()` 調用（不影響運行）

### 故障排查
1. ✅ 根據問題啟用特定模塊的 DEBUG
2. ✅ `DEBUG_I2C` → 排查通信問題
3. ✅ `DEBUG_MPU6050_DMP` → 排查姿態異常
4. ✅ `DEBUG_BUTTON` → 排查按鍵響應問題
5. ✅ `DEBUG_SOCCER` → 排查狀態機邏輯問題

### 性能考慮

#### UART 傳輸時間計算
- **波特率**：9600 bps (960 字節/秒)
- **超時設置**：500ms
- **最大緩衝**：約 480 字節/次

| 訊息長度 | 傳輸時間 | 建議 |
|---------|---------|------|
| 20 字節 | ~21ms | ✅ 安全 |
| 100 字節 | ~104ms | ✅ 安全 |
| 200 字節 | ~208ms | ⚠️ 注意頻率 |
| 480 字節 | ~500ms | ⚠️ 接近超時 |
| >480 字節 | 超時風險 | ❌ 避免 |

#### 建議
- 🎯 單次 printf 控制在 100 字節以內
- 🎯 高頻輸出（>10Hz）考慮降低頻率或增加波特率
- 🎯 使用 `static uint32_t lastPrint` 限制打印頻率

### Printf 使用技巧

#### 限制打印頻率
```c
#ifdef DEBUG_SOCCER
static uint32_t lastDebug = 0;
if (HAL_GetTick() - lastDebug > 300) {  // 每 300ms
  printf("State: %d Ball: %.1f\r\n", state, ballAngle);
  lastDebug = HAL_GetTick();
}
#endif
```

#### 條件打印
```c
#ifdef DEBUG_I2C
if (errorCode != HAL_OK) {  // 只在錯誤時打印
  printf("I2C Error: 0x%X\r\n", errorCode);
}
#endif
```

#### 多行格式化
```c
#ifdef DEBUG_GENERAL
printf("System Info:\r\n");
printf("  CPU: %luMHz\r\n", SystemCoreClock / 1000000);
printf("  Tick: %lu\r\n", HAL_GetTick());
printf("  State: %d\r\n", currentState);
#endif
```

## 資源使用

### Flash 使用
- 每個 DEBUG 宏約增加 **200-800 bytes**（取決於打印頻率）
- 全部啟用約增加 **3-5 KB**
- 全部禁用：**~100 bytes** 開銷（僅 _write 和 __io_putchar）
- Printf 庫：**~2 KB**（標準 newlib-nano）

### RAM 使用
- Printf 棧使用：**約 200-500 bytes**（動態）
- dataUart_huart：**4 bytes**（靜態）
- 無額外緩衝區（直接 UART 傳輸）

### CPU 使用
- Printf 格式化：**~1-10μs**（取決於複雜度）
- UART 傳輸：**阻塞時間 = 訊息長度 / 960 字節/秒**
- 批次模式比字符模式快 **10 倍**

## 常見問題

### Q: Printf 沒有輸出？
**A**: 檢查以下項目：
1. ✅ 確認 `dataUart_Init(&huart4)` 已調用
2. ✅ 確認對應 DEBUG 宏已在 CMakeLists.txt 中啟用
3. ✅ 確認 UART4 硬件已初始化（`MX_UART4_Init()`）
4. ✅ 檢查 UART 連接（TX=PA0, 9600 baud, 8N1）

### Q: 需要同時實現 `_write()` 和 `__io_putchar()` 嗎？
**A**: 
- **printf() 只需 `_write()`**：覆寫 `_write()` 後，printf 不會使用 `__io_putchar()`
- **putchar() 需要 `__io_putchar()`**：putchar(), putc() 等函數直接調用它
- **建議兩者都實現**：提供完整的標準 C 庫支持，代碼開銷小

### Q: 為什麼有兩個不同的超時時間？
**A**:
- `_write()` 500ms：支持長訊息（~480 字節）
- `__io_putchar()` 50ms：單字符僅需 ~1ms，50ms 提供足夠安全邊際
- 不同函數、不同用途、不同超時策略

### Q: 輸出不完整或亂碼？
**A**: 可能原因：
1. ⚠️ 波特率不匹配（確認 9600）
2. ⚠️ 訊息過長超過 500ms 超時
3. ⚠️ 高頻輸出導致 UART 阻塞
4. ⚠️ 中斷中調用 printf（避免在 ISR 中使用）

### Q: Release 版本還有 printf 開銷？
**A**: 
- 如果禁用所有 DEBUG 宏，printf 調用會被條件編譯消除
- _write() 和 __io_putchar() 仍存在（~100 bytes），但不會被調用
- Printf 庫會被鏈接（~2KB），除非使用 `-fno-builtin-printf`

### Q: 如何提高波特率？
**A**: 修改 `usart.c` 中的配置：
```c
huart4.Init.BaudRate = 115200;  // 從 9600 改為 115200
```
同時調整超時時間：
```c
// dataPrint.c 中的 _write()
HAL_UART_Transmit(dataUart_huart, (uint8_t *)ptr, len, 50);  // 500ms → 50ms
```

## 總結

本專案的調試系統提供：
- ✅ **標準 printf** 支持，易於使用
- ✅ **批次傳輸** 模式，性能提升 10 倍
- ✅ **模塊化控制**，靈活啟用/禁用
- ✅ **統一管理**，便於維護
- ✅ **資源可控**，適合嵌入式環境

建議根據開發階段選擇合適的 DEBUG 宏組合，平衡調試需求與系統性能。


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
- [dataPrint.c](../Libraries/Uart/dataPrint.c) - 調試打印實現
- [CMakeLists.txt](../CMakeLists.txt) - DEBUG 宏配置位置
