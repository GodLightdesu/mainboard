# 通用 I2C 狀態機使用指南

[![Version](https://img.shields.io/badge/version-2.3.0-blue.svg)]()
[![I2C](https://img.shields.io/badge/interface-I2C-green.svg)]()

本指南說明如何使用通用 I2C 狀態機框架創建新的 I2C 模組。

## 📋 目錄

- [概述](#概述)
- [架構](#架構)
- [主要特性](#主要特性)
- [快速開始](#快速開始)
- [完整範例](#完整範例)
- [調試配置](#調試配置)
- [故障排除](#故障排除)

---

## 📝 概述

通用 I2C 狀態機（`i2c_common.h/c`）提供：
- ✅ **全局總線管理器**：多模組共享 I2C 協調機制
- ✅ **可重用狀態機**：I2C DMA 操作  
- ✅ **順序輪詢**：多個從機設備
- ✅ **自動錯誤恢復**
- ✅ **溢位安全超時**：處理 49+ 天運行
- ✅ **回調驅動**：數據處理
- ✅ **線程安全設計**：原子操作
- ✅ **防止競爭條件**：臨界區保護
- ✅ **靈活 TX/RX 緩衝**：支持寫後讀

## 架構

```
┌─────────────────────────────────────┐
│     您的模組（例如 IR、MPU）         │
│  - 模組特定的資料結構               │
│  - 數據處理回調                     │
│  - 公開 API（Init、Process）        │
└──────────────┬──────────────────────┘
               │
               │ 使用
               ▼
┌─────────────────────────────────────┐
│      I2C_Module_t (i2c_common)      │
│  - 通用狀態機                       │
│  - DMA 傳輸管理                     │
│  - 從機設備管理                     │
│  - 順序輪詢邏輯                     │
│  - 原子化狀態轉換                   │
└──────────────┬──────────────────────┘
               │
               │ 管理於
               ▼
┌─────────────────────────────────────┐
│    I2C_BusManager_t (i2c_common)    │
│  - 全局總線鎖機制                   │
│  - 多模組協調                       │
│  - 防止總線衝突                     │
│  - 公平調度（先到先得）             │
└─────────────────────────────────────┘
```

## 主要特性

### I2C 總線管理器（v2.3.0 新增）
- **原子化總線獲取**：`I2C_Bus_TryAcquire()` 確保獨佔訪問
- **多模組支持**：多個模組可以安全共享一個 I2C 周邊
- **自動釋放**：傳輸、錯誤或超時後釋放總線
- **公平調度**：先到先得，失敗者在下次輪詢重試

### 線程安全
- **原子操作**：所有狀態讀寫使用 `__disable_irq()/__enable_irq()`
- **臨界區**：狀態轉換受保護免受中斷拒占
- **回調保護**：ISR 回調在修改前驗證狀態

### 溢位安全計時
- **TIME_DIFF 宏**：處理 HAL_GetTick() 32-bit 環繞
- 應用於：超時檢查、輪詢間隔、錯誤恢復
- 可靠運行超過 49.7 天

### 靈活緩衝
- **txBuffer**：獨立 TX 緩衝區用於寄存器地址（DMA 安全）
- **txSize**：可配置 TX 長度（0 = 直接讀，>0 = 寫後讀）
- **32-byte 對齊**：所有 DMA 緩衝區對齊到快取行邊界

### 動態從機管理
- **啟用標誌**：每個從機有 `enabled` 字段以啟用/禁用輪詢
- **自動跳過**：狀態機在輪詢時自動跳過禁用的從機
- **熱插拔**：可在運行時啟用/禁用從機而無需重新初始化模組

## 調試配置

### 模塊化 DEBUG 宏

專案使用在 `CMakeLists.txt` 中定義的模塊化 DEBUG 宏。每個模組可以獨立啟用/禁用調試輸出。

#### 啟用/禁用調試輸出

在 [CMakeLists.txt](../CMakeLists.txt) 中取消註解所需的 DEBUG 宏：

```cmake
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # 取消註解以下行以啟用 UART 調試打印
    # DEBUG_MPU6050        # MPU6050 傳感器數據和初始化
    # DEBUG_MPU6050_DMP    # MPU6050 姿態 (Roll/Pitch/Yaw)
    # DEBUG_IR             # IR 感測器眼睛數據
    # DEBUG_I2C            # I2C 通訊錯誤和狀態
    # DEBUG_MOTORS         # 馬達測試 PWM 值
)
```

#### DEBUG 宏參考表

| DEBUG 宏    | 模組     | 輸出                                              | 函數                                                                                       |
| ----------- | -------- | ------------------------------------------------- | ------------------------------------------------------------------------------------------ |
| `DEBUG_I2C` | I2C 通用 | 錯誤消息<br>超時警告<br>RX 回調計數器<br>設備發現 | `dataUart_PrintI2CError()`<br>`dataUart_PrintI2CStatus()`<br>`dataUart_PrintDeviceFound()` |

#### 輸出範例

**啟用 DEBUG_I2C**：
```
Device found at 0x68
RxCallback: 50
I2C_Error: Code=0x4, Slave=1
I2C_Timeout: Slave=0
I2C_Read FAIL: Code=1, HAL_state=32
```

### dataPrint 模組

所有調試打印函數集中在 `dataPrint` 模組：

```c
// I2C 調試函數（由 DEBUG_I2C 控制）
void dataUart_PrintI2CError(const char *errorType, int errorCode, int slaveId);
void dataUart_PrintI2CStatus(const char *message);
void dataUart_PrintDeviceFound(uint16_t addr);
```

**優點**：
- ✅ 集中管理所有 UART 打印
- ✅ 一致的輸出格式
- ✅ 易於維護和修改
- ✅ 內置 NULL 檢查和緩衝區溢位保護

### 最佳實踐

1. **開發階段**：啟用所有相關 DEBUG 宏
2. **優化階段**：禁用工作正常模組的 DEBUG 宏
3. **發布版本**：禁用所有 DEBUG 宏以節省 Flash 和 RAM
4. **故障排除**：只啟用特定模組的 DEBUG 宏

---

## 創建新的 I2C 模組

### 步驟 1：定義模組結構

```c
#include "i2c_common.h"

typedef struct {
  // 您的模組特定數據
  bool dataReady;
  uint16_t sensorValue;
  
  // 通用 I2C 狀態機
  I2C_Module_t i2cModule;
  I2C_SlaveDevice_t slaves[NUM_SLAVES];
} MyModule_t;

extern MyModule_t MyModule;
```

### 步驟 2：實現數據處理回調

此回調由通用狀態機在收到數據後調用。
**注意**：數據已由中斷處理程序從 `rxBuffer` 複製到 `processBuffer`。

```c
static void MyModule_DataProcessCallback(I2C_Module_t *module, uint8_t slaveId) {
  I2C_SlaveDevice_t *slave = &MyModule.slaves[slaveId];
  
  // 從 slave->processBuffer 處理接收的數據
  // 數據已由 RxCallback 在中斷上下文中複製
  MyModule.sensorValue = (slave->processBuffer[0] << 8) | slave->processBuffer[1];
  
  // 更新模組狀態
  MyModule.dataReady = true;
}
```

### 步驟 3：初始化模組

```c
void MyModule_Init(I2C_HandleTypeDef *hi2c) {
  // 設置從機設備使用 DMA 安全緩衝區
  MyModule.slaves[0].address = DEVICE_ADDR;
  MyModule.slaves[0].txBuffer = txBufferDMA;      // 必須在非快取 RAM
  MyModule.slaves[0].rxBuffer = rxBufferDMA;      // 必須在非快取 RAM  
  MyModule.slaves[0].processBuffer = processBuffer; // 可以是普通 RAM
  MyModule.slaves[0].bufferSize = BUFFER_SIZE;
  MyModule.slaves[0].txSize = 1;                  // 1 = 寫寄存器，然後讀取
                                                   // 0 = 直接讀取（無 TX）
  MyModule.slaves[0].enabled = true;               // 設為 false 以禁用輪詢
  
  // 可選：設置附加從機
  MyModule.slaves[1].address = DEVICE2_ADDR;
  MyModule.slaves[1].enabled = false;              // 已禁用 - 將被跳過
  
  // 初始化通用 I2C 模組
  I2C_Module_Init(
    &MyModule.i2cModule,
    hi2c,
    MyModule.slaves,
    NUM_SLAVES,
    POLL_INTERVAL_MS,
    MyModule_DataProcessCallback
  );
}
```

**重要緩衝區要求**：
- `txBuffer` 和 `rxBuffer`：必須在 **非快取記憶體**（MPU 配置）
- 建議：對齊到 32-byte 邊界以獲得最佳 DMA 效能
- 參見 `const.h` 的緩衝區分配範例

### 步驟 3.5：初始化總線管理器（如果共享 I2C）

**v2.3.0 新增**：如果多個模組共享同一個 I2C 周邊，在 `main.c` 中 **先**初始化總線管理器，**再**初始化模組：

```c
int main(void) {
  // ... HAL 初始化 ...
  
  /* 為共享的 I2C3 周邊初始化總線管理器 */
  I2C_BusManager_t i2c3_bus;
  I2C_Bus_Init(&i2c3_bus, &hi2c3);
  
  /* 現在初始化使用 hi2c3 的模組 */
  IR_Init(&hi2c3);
  Xsound_Init(&hi2c3);
  
  /* MPU6050 使用專用的 I2C2（不共享）*/
  MPU6050_DMP_Init();
  
  // ... 其余初始化 ...
}
```

**注意**：
- 只在 2+ 模組共享一個 I2C 周邊時需要
- 總線管理器自動協調訪問
- 模組代碼不需要更改 - 透明運作

**最佳實踐（v2.3.u0 優化）**：
- ✅ 獲取總線管理器一次並重用指針（避免重複查找）
- ✅ 使用 do-while(0) 模式進行統一錯誤清理
- ✅ 初始化所有局部變量（防禦性編程）
- ✅ 在所有退出路徑釋放總線（成功、錯誤、超時）
- ✅ 啟動 DMA 操作前檢查總線可用性

### 步驟 4：處理狀態機

在主迴圈或 updateData() 中定期調用：

```c
void MyModule_Process(void) {
  I2C_Module_Process(&MyModule.i2cModule);
}
```

### 步驟 5：處理 I2C 回調

在 `stm32h7xx_it.c` 中：

```c
/* 直接讀模式回調（HAL_I2C_Master_Receive_DMA）*/
void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c) {
  I2C_Module_RxCallback(&MyModule.i2cModule, hi2c);
}

/* 寫後讀模式回調（HAL_I2C_Mem_Read_DMA）*/
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c) {
  I2C_Module_RxCallback(&MyModule.i2cModule, hi2c);
}

/* 錯誤回調（兩種模式共用）*/
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c) {
  I2C_Module_ErrorCallback(&MyModule.i2cModule, hi2c);
}
```

**重要提醒**：
- 直接讀模式（txSize=0）使用 `HAL_I2C_MasterRxCpltCallback`
- 寫後讀模式（txSize>0）使用 `HAL_I2C_MemRxCpltCallback`
- 兩種模式都調用相同的 `I2C_Module_RxCallback()`

## 完整範例：IR 模組

參見 `Libraries/Module/ir.c` 的完整實現。

### 模組結構

```c
typedef struct {
  bool dataReady;
  uint8_t maxEye;
  uint16_t maxValue;
  uint16_t eyeValues[IR_SLAVES_NO * EYE_NUM];
  
  I2C_Module_t i2cModule;           // 通用狀態機
  I2C_SlaveDevice_t slaves[IR_SLAVES_NO];
} IR_t;
```

### 數據處理回調

```c
static uint8_t freshDataMask = 0;  // 追蹤哪些從機有新數據

static void IR_DataProcessCallback(I2C_Module_t *module, uint8_t slaveId) {
  I2C_SlaveDevice_t *slave = &IR.slaves[slaveId];
  
  // 從 processBuffer 解析傳感器數據
  const uint16_t offset = slaveId * EYE_NUM;
  for (uint8_t i = 0; i < EYE_NUM; i++) {
    const uint8_t idx = 2 + (i * 2);
    IR.eyeValues[offset + i] = combine_data(
      slave->processBuffer[idx + 1], 
      slave->processBuffer[idx]
    );
  }
  
  // 標記新數據並檢查所有啟用的從機是否就緒（原子操作）
  __disable_irq();
  freshDataMask |= (1 << slaveId);
  
  // 內聯計算啟用遮罩
  uint8_t enabledMask = 0;
  for (uint8_t s = 0; s < IR_SLAVES_NO; s++) {
    if (IR.slaves[s].enabled) enabledMask |= (1 << s);
  }
  
  // 所有啟用的從機都有新數據？
  if (freshDataMask == enabledMask) {
    freshDataMask = 0;  // 為下一週期重置
    __enable_irq();
    
    // 在所有啟用的眼睛中找到最大值（優化的單循環）
    IR.maxValue = 0;
    IR.maxEye = 0;
    for (uint8_t eye = 0; eye < IR_SLAVES_NO * EYE_NUM; eye++) {
      const uint8_t slaveIdx = eye / EYE_NUM;  // 動態從機計算
      if (IR.slaves[slaveIdx].enabled && IR.eyeValues[eye] > IR.maxValue) {
        IR.maxValue = IR.eyeValues[eye];
        IR.maxEye = eye;
      }
    }
    IR.dataReady = true;
  } else {
    __enable_irq();
  }
}
```

### 初始化

```c
void IR_Init(I2C_HandleTypeDef *hi2c) {
  // 設置從機
  IR.slaves[0].address = IR_SLAVE_1_ADDR;
  IR.slaves[0].rxBuffer = rxBuffer1;
  IR.slaves[0].processBuffer = processBuffer1;
  IR.slaves[0].bufferSize = IR_BUFFER_SIZE;
  IR.slaves[0].enabled = true;
  
  // 初始化通用狀態機
  I2C_Module_Init(
    &IR.i2cModule,
    hi2c,
    IR.slaves,
    IR_SLAVES_NO,
    IR_SAMPLE_PERIOD_MS,
    IR_DataProcessCallback
  );
}
```

### 處理

```c
void IR_Process(void) {
  I2C_Module_Process(&IR.i2cModule);
}
```

## 狀態機流程

### 帶總線管理器的寫後讀模式（例如 MPU6050）- v2.3.0

**使用 HAL_I2C_Mem_Read_DMA() - 單次調用完成寫後讀**

```
     ┌──────────────┐
     │     IDLE     │◄────────────────────────────────┐
     └──┬───────────┘                                 │
        │ 輪詢間隔到期（原子操作）                      │
        ▼                                             │
   ┌──────────────────┐                               │
   │ 總線獲取         │ I2C_Bus_TryAcquire()          │
   │   （原子操作）    │                               │
   └──┬────────┬──────┘                               │
      │        │ 失敗 → 等待下次輪詢                   │
      │ 成功                                          │
      │ 調用 HAL_I2C_Mem_Read_DMA()                   │
      │  ↳ HAL 內部發送寄存器地址（IT）                │
      │  ↳ HAL 自動啟動 RX DMA                        │
      ▼                                               │
   ┌─────────────┐                                   │
   │   READING   │ RX DMA 傳輸                        │
   │  （超時）    │ （do-while 錯誤處理）               │
   └──┬──────────┘                                   │
      │ HAL_I2C_MemRxCpltCallback                    │
      │ （非 MasterRxCpltCallback！）                 │
      │ memcpy(rxBuf→procBuf) 在 ISR 中               │
      ▼                                              │
┌──────────────┐                                      │
│  PROCESSING  │ 主循環（原子操作）                     │
│ 調用回調      │ 處理後釋放總線                        │
└──┬───────────┘                                      │
   │                                                  │
   └──────────────────────────────────────────────────┘

錯誤/超時：釋放總線 → 返回 IDLE

**關鍵點**：
- ✅ 單次 HAL_I2C_Mem_Read_DMA() 調用處理寫+讀
- ✅ 只需要 IDLE → READING → PROCESSING → IDLE
- ❌ 不需要 WRITING 或 READY_TO_READ 狀態
- ❌ 不需要 TxCallback
- ✅ 使用 HAL_I2C_MemRxCpltCallback（非 MasterRxCpltCallback）
```

### 帶總線管理器的直接讀模式（例如 IR 感測器）- v2.3.0
     ┌──────────────┐
     │     IDLE     │◄──────────────┐
     └──┬───────────┘               │
        │ 輪詢間隔（原子操作）        │
        ▼                           │
   ┌─────────────┐                 │
   │   READING   │ DMA 傳輸         │
   │  （超時）    │                  │
   └──┬──────────┘                  │
      │ RX 完成（ISR + 原子操作）    │
      │ memcpy() 在 ISR 中          │
      ▼                             │
┌──────────────┐                    │
│  PROCESSING  │ 主循環（原子操作）   │
│ 調用回調      │                    │
└──┬───────────┘                    │
   │                               │
   └───────────────────────────────┘
   
### 錯誤處理（帶原子保護）
   ERROR 狀態：
   - 50ms 後自動恢復（溢位安全）
   - 如需要則重新初始化 I2C
   - 原子轉換到 IDLE
```

## 線程安全設計

### 原子狀態轉換
所有狀態讀寫都使用臨界區：
```c
__disable_irq();
module->state = NEW_STATE;
__enable_irq();
```

### 關鍵保護點
1. **Process() 中的狀態讀取**：原子快照防止轉換中的不一致
2. **回調中的狀態寫入**：保護免受主循環干擾  
3. **ReadSlave()**：原子狀態檢查和從機 ID 分配
4. **錯誤路徑**：所有錯誤狀態轉換都是原子的

### 溢位安全計時
使用 `TIME_DIFF(current, start)` 宏：
- 正確處理 HAL_GetTick() 32-bit 環繞
- 超時 > 49.7 天仍可靠
- 應用於：輪詢間隔、超時檢查、錯誤恢復

## 中斷安全設計

### 關鍵原則
1. **ISR 中的數據複製**：`RxCallback` 立即複製 `rxBuffer → processBuffer`
   - 防止下次 DMA 傳輸覆蓋數據
   - 中斷中的最小時間關鍵操作
   - **原子狀態轉換**到 PROCESSING
   
2. **主循環中啟動 DMA**：`READY_TO_READ` 狀態在 `Process()` 中處理
   - 避免 HAL 忙狀態問題
   - 適當的錯誤處理和超時重置
   - **超時保護**防止卡在狀態
   
3. **主循環中的回調**：`PROCESSING` 狀態調用數據處理回調
   - 允許耗時操作（例如 printf、計算）
   - 無中斷嵌套問題
   - **原子轉換**回到 IDLE

## 優點

1. **代碼重用**：狀態機編寫一次，所有 I2C 模組使用
2. **一致性**：所有模組行為相同
3. **可維護性**：錯誤修復適用於所有模組
4. **靈活性**：每個模組處理自己的數據格式
5. **可測試性**：狀態機可以獨立測試
6. **線程安全**：原子操作防止競爭條件
7. **溢位安全**：計時計算處理 tick 環繞
8. **並發訪問**：多個模組安全共享同一 I2C 外設
9. **DMA 優化**：32-byte 對齊的非快取記憶體緩衝區

## 關鍵函數

### I2C_Module_Init()
使用從機和回調初始化通用 I2C 模組。

### I2C_Module_Process()
在主循環上下文中處理狀態轉換和 DMA 啟動。
**使用原子狀態讀取**以防止競爭條件。
**溢位安全計時**使用 TIME_DIFF 宏。

### I2C_Module_RxCallback()
從 HAL I2C RX 完成中斷調用。
**關鍵**：立即複製 `rxBuffer → processBuffer` 以防止數據丟失。
**原子狀態轉換**到 PROCESSING。

### I2C_Module_TxCallback()
從 HAL I2C TX 完成中斷調用。
設置 `READY_TO_READ` 狀態，讓主循環啟動 DMA 接收。
**原子狀態檢查和更新**。

### I2C_Module_ErrorCallback()
從 HAL I2C 錯誤中斷調用。
啟動錯誤恢復計時器。
**原子錯誤狀態轉換**。

### I2C_Module_ReadSlave()
內部函數，從特定從機啟動 I2C 讀取操作。
處理寫後讀（txSize > 0）和直接讀（txSize = 0）模式。
**原子狀態檢查和從機 ID 分配**。

## 配置

- **I2C_TIMEOUT_MS**：I2C 操作超時（默認：100ms）
- **I2C_ERROR_RECOVERY_MS**：錯誤恢復延遲（默認：50ms）
- **pollInterval**：在 Init() 中設置的每模組輪詢間隔

## 多個 I2C 外設

每個模組可以使用不同的 I2C 外設：

```c
IR_Init(&hi2c1);      // IR 在 I2C1
MPU6050_Init(&hi2c2); // MPU6050 在 I2C2
```

## 中斷上下文中的回調

更新 `stm32h7xx_it.c` 以派發到所有模組：

```c
/* 直接讀模式（HAL_I2C_Master_Receive_DMA）*/
void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c) {
  if (hi2c == &hi2c1) {
    IR_RxCallback(hi2c);
  } else if (hi2c == &hi2c2) {
    OtherModule_RxCallback(hi2c);
  }
}

/* 寫後讀模式（HAL_I2C_Mem_Read_DMA）*/
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c) {
  if (hi2c == &hi2c3) {
    MPU6050_RxCallback(hi2c);
  }
}
```

## 從自定義狀態機遷移

之前：
```c
// 模組中的自定義狀態機
IR.state = IR_STATE_READING;
// ... 大量狀態機代碼 ...
```

之後：
```c
// 使用通用狀態機
I2C_Module_Process(&IR.i2cModule);
```

## 總結

通用 I2C 狀態機為 I2C 模組提供了強大、經過測試的框架。通過使用回調進行數據處理，每個模組在受益於共享狀態機邏輯的同時保留對其數據的完全控制。

---

## 🔧 故障排除

詳細的故障排除指南請參閱 [I2C_ERROR_TROUBLESHOOTING.md](I2C_ERROR_TROUBLESHOOTING.md)

### 快速檢查清單

- ✅ DMA 緩衝區位於 0x30000000 (SRAM_D2)
- ✅ 緩衝區 32-byte 對齊
- ✅ MPU 配置禁用 D2 域快取
- ✅ 所有共享 I2C 的模組使用相同總線管理器
- ✅ 回調在 ISR 中正確調用
- ✅ `DEBUG_I2C` 啟用以查看調試訊息

---

## 📚 參考資料

- [I2C_ERROR_TROUBLESHOOTING.md](I2C_ERROR_TROUBLESHOOTING.md) - I2C 錯誤排除詳細指南
- [DEBUG_SYSTEM.md](DEBUG_SYSTEM.md) - 調試系統使用指南
- [STM32H7 參考手冊](https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-stm32h743753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf) - I2C 外設詳細說明

---

**版本**: 2.3.0  
**最後更新**: 2026年3月12日  
**平台**: STM32H750XX
