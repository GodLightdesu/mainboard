# STM32H750 主控板專案

這是一個基於 STM32H750 微控制器的主控板韌體專案，用於控制多馬達機器人系統並整合 IR 感測器陣列。

## 🎉 最新更新 (v2.6.0 - 2026-01-30)

### 🎮 按鍵控制模組新增
- **完整按鍵狀態機**：實現消抖、單擊、雙擊、長按等多種按鍵功能
  - 支持 8 個按鍵 (GPIOD/GPIOC)
  - 事件驅動設計：CLICK、DOUBLE_CLICK、LONG_PRESS 等
- **系統控制整合**：與足球機器人控制系統完全整合
  - Button 1：啟動機器人 (長按)
  - Button 2：緊急停止 (雙擊)
- **調試支持**：完整的 UART 調試輸出和狀態監控

### 🚀 ARM CMSIS DSP 效能優化
- **硬體加速數學運算**：全面整合 ARM CMSIS DSP 函數庫
  - 正弦/餘弦函數：`sinf()` → `arm_sin_f32()` / `cosf()` → `arm_cos_f32()`
  - 平方根運算：`sqrtf()` → `arm_sqrt_f32()`
  - 反正切函數：`atan2f()` → `arm_atan2_f32()`
  - 常數標準化：`M_PI` → `PI` (DSP 定義)
- **優化模組**：MPU6050 姿態估計、IR 角度計算、馬達運動學
- **效能提升**：利用 STM32H7 Cortex-M7 FPU 和 DSP 指令集

### 🤖 足球機器人基礎控制邏輯
- **基礎控制實現**：基於 IR 和 MPU6050 數據的簡單決策邏輯
  - 偏航角校正：當 yaw > 10° 時後退，yaw < -10° 時前進
  - 足球追逐：當 yaw 在範圍內且檢測到足球時，使用極座標移動追逐
  - 停止邏輯：無足球數據時停止所有馬達
- **響應式決策**：基於即時 IR 和 MPU6050 數據的控制
- **模組化設計**：清晰的數據處理和控制分離

### 模組封裝重構 (v2.4.0)
- **靜態變數私有化**：所有模組變數改為靜態，通過 getter 函數訪問
  - `MPU6050_GetData()`: 返回 `const MPU6050_Data_t*`
  - `IR_GetData()`: 返回 `const IR_Data_t*`
  - `Motors_GetData()`: 返回 `const Motors_Data_t*`
- **指針快取機制**：`ModuleData_t` 結構整合所有模組指針，減少 getter 調用
- **記憶體優化**：FLASH 使用率 67.23%，消除循環依賴

### I2C 總線管理器重構 (v2.3.0)
- **全局總線鎖機制**：實現真正的多模組輪流使用共享 I2C 總線
- **原子化總線獲取**：`I2C_Bus_TryAcquire()` 確保只有一個模組可佔用總線
- **自動總線釋放**：傳輸完成、錯誤、超時時自動釋放總線
- **防止衝突**：消除多模組同時訪問同一 I2C 硬體的競爭條件
- **支援多總線**：可管理最多 4 條 I2C 總線 (I2C1-I2C4)

### 調試系統重構 (v2.2.0)
- **模塊化 DEBUG 宏**：為每個模塊獨立控制 UART 打印
  - `DEBUG_MPU6050`: MPU6050 傳感器數據
  - `DEBUG_MPU6050_DMP`: MPU6050 姿態數據 (Roll/Pitch/Yaw)
  - `DEBUG_IR`: IR 感測器眼睛數據
  - `DEBUG_I2C`: I2C 通訊錯誤和狀態
  - `DEBUG_MOTORS`: 馬達測試 PWM 值
- **集中式打印管理**：所有 UART 打印功能移至 `data_uart` 模塊
- **統一配置**：在 CMakeLists.txt 中統一控制調試輸出

詳見 [CHANGELOG.md](CHANGELOG.md)

---

## 📋 目錄

- [硬體規格](#硬體規格)
- [系統架構](#系統架構)
- [功能特性](#功能特性)
- [專案結構](#專案結構)
- [編譯與燒錄](#編譯與燒錄)
- [硬體配置](#硬體配置)
- [模組說明](#模組說明)
- [開發指南](#開發指南)
- [相關文件](#相關文件)

## 🔧 硬體規格

- **微控制器**: STM32H750XX
- **核心**: ARM Cortex-M7
- **系統時鐘**: 240 MHz (透過 HSI + PLL)
- **記憶體**: 
  - Flash: 128 KB (使用 67.23%)
  - SRAM: 1 MB (含 DMA 緩衝區於 D2 域)
- **調試介面**: SWD (Serial Wire Debug)

## 🏗️ 系統架構

```
┌─────────────────────────────────────────┐
│         STM32H750 主控板                │
├─────────────────────────────────────────┤
│  ┌────────────┐  ┌────────────┐        │
│  │ IR Module  │  │   Motors   │        │
│  │(I2C3, 2x)  │  │  (4xPWM)   │        │
│  └────────────┘  └────────────┘        │
│  ┌────────────┐  ┌────────────┐        │
│  │  MPU6050   │  │ Data UART  │        │
│  │  (I2C3)    │  │  (UART4)   │        │
│  └─────┬──────┘  └────────────┘        │
│        │                                │
│  ┌─────┴──────────────────────┐        │
│  │  I2C Bus Manager (I2C3)    │        │
│  │  - 原子化總線鎖機制        │        │
│  │  - 多模組輪流使用          │        │
│  │  - 先到先得公平調度        │        │
│  │  - 防止總線衝突            │        │
│  └────────────────────────────┘        │
│  ┌────────────┐                        │
│  │ Button Ctrl│                        │
│  │  (GPIO)    │                        │
│  │  - 8按鍵狀態機                      │
│  │  - 事件驅動控制                      │
│  └────────────┘                        │
│                                         │
│  使用通用 I2C 狀態機 (i2c_common)       │
│  - 原子操作保護                         │
│  - 溢位安全時間計算                     │
│  - DMA 優化緩衝區                       │
│  - 統一錯誤處理                         │
│                                         │
│  按鍵控制系統 (button)                  │
│  - 完整的狀態機實現                     │
│  - 消抖、單擊、雙擊、長按檢測           │
│  - 系統控制回調整合                     │
└─────────────────────────────────────────┘
```

## ✨ 功能特性

### 1. **通用 I2C 狀態機框架 (i2c_common)**

#### 全局總線管理器 (I2C_BusManager_t)
- **多模組協調**：MPU6050、IR 等模組安全共享同一 I2C 周邊
- **原子化獲取**：`I2C_Bus_TryAcquire()` 確保獨佔訪問
  - 使用 `__disable_irq()/__enable_irq()` 保證原子性
  - 總線被佔用時返回 false，模組等待下次輪詢
  - 支援同一模組重入（已擁有時直接返回 true）
- **自動釋放**：`I2C_Bus_Release()` 在以下時機自動釋放總線
  - ✅ 傳輸完成後 (PROCESSING → IDLE)
  - ✅ 超時錯誤時 (READING timeout → ERROR)
  - ✅ I2C 錯誤回調時 (ErrorCallback)
  - ✅ 錯誤恢復完成後 (ERROR → IDLE)
  - ✅ DMA 啟動失敗時 (ReadSlave failure)
- **先到先得調度**：公平的總線訪問機制
  - 早到模組優先獲得總線
  - 晚到模組自動等待並在下次輪詢重試
- **防止衝突**：徹底消除多模組同時訪問 I2C 硬體的競爭條件

#### 通用狀態機特性
- 可重用的狀態機，支援任意 I2C 模組
- 支援最多 4 個從機裝置（每個模組）
- DMA 非阻塞式資料傳輸
- 順序輪詢模式或手動控制
- **動態從機管理**：自動跳過禁用的從機
- 完整的錯誤處理與超時保護（溢位安全）
- 原子操作保護，消除競態條件
- 回調驅動的數據處理
- 支援寫後讀（register-based）和直接讀取模式
- 32-byte 對齊的 DMA 緩衝區

### 2. **馬達控制系統**
- 4 個獨立 DC 馬達控制 (FL, FR, RL, RR)
- H 橋驅動器介面
- PWM 頻率: 10 kHz
- 速度範圍: 0-100%
- 支援衰減模式:
  - **快速衰減** (Fast Decay): 適合高速運轉
  - **慢速衰減** (Slow Decay): 適合低速與平滑控制
- 極座標移動控制 (角度 + 速度)
- 馬達測試與校準功能

### 3. **IR 感測器陣列**
- 2 個 I2C 從機，每個配備 7 個感測器
- 總計 14 個 IR 感測器
- 自動偵測最大值與位置
- 即時資料串流輸出

### 4. **MPU6050 IMU 模塊**
- 6 軸慣性測量單元 (3 軸加速度 + 3 軸陀螺儀)
- 互補濾波器 (Complementary Filter) 姿態估計
- 實時 Roll/Pitch/Yaw 計算
- 四元數輸出支援
- 溫度讀取

### 5. **模塊化調試系統**
- **獨立 DEBUG 宏**：每個模塊可單獨啟用/禁用調試輸出
- **集中式打印管理**：所有 UART 打印函數位於 data_uart 模塊
- **統一配置**：在 CMakeLists.txt 中控制所有調試輸出
- **格式化輸出**：專用的打印函數用於不同類型的數據
- **節省資源**：Release 版本可完全禁用調試輸出

### 6. **UART 資料輸出**
- 格式化資料傳輸
- 感測器數值顯示
- 調試訊息輸出
- PWM 監控資訊

### 7. **按鍵控制系統**
- **完整狀態機**：支援消抖、單擊、雙擊、長按等多種按鍵事件
- **8 個按鍵支持**：配置在 GPIOD 和 GPIOC 上
  - Button 1: 啟動按鈕 (GPIOD)
  - Button 2: 緊急停止按鈕 (GPIOD)
  - Button 3-8: 擴展控制按鈕
- **事件驅動設計**：
  - `CLICK`: 單擊事件
  - `DOUBLE_CLICK`: 雙擊事件（緊急停止）
  - `LONG_PRESS_START/END`: 長按開始/結束（啟動控制）
  - `LONG_PRESS_HOLD`: 長按保持
- **系統整合**：與足球機器人控制系統完全整合
- **調試支持**：完整的 UART 調試輸出和狀態監控

## 📁 專案結構

```
mainboard/
├── Core/                      # STM32CubeMX 產生的核心檔案
│   ├── Inc/                   # HAL 標頭檔
│   │   ├── main.h
│   │   ├── i2c.h
│   │   ├── tim.h
│   │   ├── usart.h
│   │   └── stm32h7xx_it.h
│   └── Src/                   # HAL 實作檔案
│       ├── main.c             # 主程式進入點
│       ├── i2c.c
│       ├── tim.c
│       ├── stm32h7xx_it.c     # 中斷處理
│       └── ...
│
├── Libraries/                 # 自訂函式庫
│   ├── const.h                # 全域常數與 DMA 緩衝區定義
│   ├── soccer.h/c             # 上層邏輯
│   │
│   ├── Button/                # 🆕 按鍵控制模組
│   │   ├── button.h           # 按鍵狀態機與事件定義
│   │   └── button.c           # 按鍵控制實作
│   │
│   ├── i2c_common.h/c         # 🆕 通用 I2C 狀態機框架
│   │
│   ├── Module/                # 週邊模組（使用 i2c_common）
│   │   ├── ir.h/c             # IR 感測器模組
│   │   ├── mpu6050.h/c        # 🆕 MPU6050 IMU 模組
│   │   ├── mpu6050_dmp.h/c    # 🆕 MPU6050 姿態估計
│   │   └── motors.h/c         # 馬達控制實作
│   │
│   └── Uart/                  # UART 通訊
│       ├── data_uart.h
│       └── data_uart.c        # 🆕 格式化資料輸出與調試打印
│
├── Drivers/                   # STM32 HAL 驅動程式
│   ├── CMSIS/                 # ARM CMSIS 標頭檔
│   ├── Dsp/                   # 🆕 ARM CMSIS DSP 函數庫
│   │   ├── Include/           # DSP 標頭檔 (arm_math.h)
│   │   ├── Source/            # DSP 實作 (FastMath, BasicMath, etc.)
│   │   └── PrivateInclude/    # 內部 DSP 標頭檔
│   └── STM32H7xx_HAL_Driver/  # STM32H7 HAL 函式庫
│
├── Doc/                       # 📚 文檔
│   ├── I2C_COMMON_USAGE.md           # 🆕 通用 I2C 框架使用指南
│   ├── I2C_Communication_Setup_Guide.md
│   └── I2C_MASTER_USAGE.md           # ⚠️ 已棄用
│
├── cmake/                     # CMake 建構系統
├── build/                     # 建構輸出目錄
├── CMakeLists.txt
├── CMakePresets.json
├── mainboard.ioc              # STM32CubeMX 專案檔
├── startup_stm32h750xx.s
├── STM32H750XX_FLASH.ld
├── CHANGELOG.md               # 版本變更記錄
└── README.md                  # 本文件
```

## 🔨 編譯與燒錄

### 環境需求

- **CMake**: >= 3.22
- **ARM GCC 工具鏈**: `arm-none-eabi-gcc`
- **STM32CubeProgrammer CLI**: 用於燒錄韌體

### 編譯步驟

使用 VS Code 內建 CMake 工具:

1. 選擇建構預設組態 (Debug/Release)
2. 執行 CMake 配置
3. 建構專案

或使用終端:

```bash
# 配置
cmake --preset=Debug

# 建構
cmake --build build/Debug
```

### 燒錄韌體

本專案提供以下 VS Code Tasks:

#### 1. **列出可用介面**
```bash
Task: CubeProg: List all available communication interfaces
```
檢視連接的 ST-Link 除錯器。

#### 2. **燒錄韌體 (SWD)**
```bash
Task: CubeProg: Flash project (SWD)
```
透過 SWD 介面燒錄已編譯的韌體。

#### 3. **建構 + 燒錄**
```bash
Task: Build + Flash
```
完整重新建構並燒錄韌體 (建議使用)。

## ⚙️ 硬體配置

### I2C3 配置
- **用途**: IR 感測器（2個從機）+ MPU6050 IMU
- **IR 從機 1**: 0x30 (7個感測器)
- **IR 從機 2**: 0x31 (7個感測器)
- **MPU6050**: 0x68
- **DMA**: DMA1 Stream 0/1 (RX/TX)
- **緩衝區**: 0x30000000 (SRAM_D2, 非快取)
- **狀態機**: 使用 `i2c_common` 通用框架
- **記憶體配置**:
  ```
  0x30000000: IR Slave 1 RX  (16B, 32B aligned)
  0x30000020: IR Slave 2 RX  (16B, 32B aligned)
  0x30000040: MPU6050 TX     (1B,  32B aligned)
  0x30000060: MPU6050 RX     (14B, 32B aligned)
  ```

### 定時器配置

| 定時器 | 用途          | 頻率     | 通道        |
|--------|---------------|----------|-------------|
| TIM1   | 馬達 0 (FL)   | 10 kHz   | CH3, CH4    |
| TIM2   | 馬達 1 (FR)   | 10 kHz   | CH3, CH4    |
| TIM3   | 馬達 2 (RL)   | 10 kHz   | CH3, CH4    |
| TIM4   | 馬達 3 (RR)   | 10 kHz   | CH3, CH4    |
| TIM7   | 系統計時      | -        | -           |

### UART 配置

| UART   | 用途           | 鮑率       |
|--------|----------------|------------|
| UART4  | 資料輸出       | 9600       |
| UART5  | 保留           | 115200     |
| UART7  | 保留           | 115200     |
| UART8  | 保留           | 115200     |

### GPIO 配置

| Pin     | 功能       | 說明          |
|---------|------------|---------------|
| PD12    | LED_2      | 狀態指示燈    |
| PB0     | LED_3      | 保留          |
| PB1     | LED_4      | 保留          |

## 📚 模組說明

### 通用 I2C 狀態機 (`i2c_common.c`)

#### 架構特性
- **可重用設計**: 一次編寫，所有 I2C 模組共用
- **線程安全**: 所有狀態轉換使用原子操作 (`__disable_irq`)
- **溢位安全**: `TIME_DIFF` 巨集處理 tick 溢位，可靠運行 >49 天
- **DMA 優化**: 32-byte 對齊緩衝區，非快取記憶體區域
- **動態從機管理**: 自動跳過禁用的從機，運行時可啟用/禁用
- **雙模式支援**:
  - **寫後讀模式**: 先寫暫存器地址，再 DMA 讀取（MPU6050）
  - **直接讀模式**: 不寫入，直接 DMA 讀取（IR 感測器）

#### 狀態機流程

**帶總線管理器的完整流程** (v2.3.0):
```
IDLE → 總線獲取 → READING (DMA) → PROCESSING → 總線釋放 → IDLE
  ↓       ↓            ↓              ↓            ↓
  輪詢   TryAcquire   HAL_I2C_Mem    RX完成(ISR)   Release
  間隔   (成功)       _Read_DMA      回調處理      (自動)
  到期                (HAL內部處理
                      寄存器寫入)
  
  總線獲取失敗 → 等待下次輪詢
       ↓
    其他模組正在使用總線
```

**錯誤處理流程**:
```
READING (超時) → ERROR → 等待恢復 → I2C重置 → 總線釋放 → IDLE
       ↓           ↓         ↓          ↓          ↓
    200ms無響應  記錄錯誤   200ms   DeInit/Init  Release
```

**兩種讀取模式**:

1. **寫後讀模式** (MPU6050, txSize=1):
   - 使用 `HAL_I2C_Mem_Read_DMA(hi2c, devAddr, memAddr, memSize, pData, size, timeout)`
   - **自動化操作**：HAL 內部先發送寄存器地址，然後執行 DMA 讀取
   - **適用場景**：讀取特定寄存器的傳感器數據
   - **範例**：MPU6050 讀取 0x3B 寄存器（加速度+陀螺儀數據）
   - **優點**：一次函數調用完成寫後讀，代碼簡潔

2. **直接讀模式** (IR, txSize=0):
   - 使用 `HAL_I2C_Master_Receive_DMA(hi2c, devAddr, pData, size, timeout)`
   - **直接讀取**：不發送任何寄存器地址，直接從從機讀取數據流
   - **適用場景**：從機自動輸出當前數據（無需指定寄存器）
   - **範例**：IR 傳感器陣列連續輸出眼睛數據
   - **優點**：更快速，適合流式數據讀取

**總線管理器整合** (v2.3.0):
- 兩種模式都在 `I2C_Module_ReadSlave()` 中**自動獲取總線**
- 使用 `do-while(0)` 模式統一錯誤處理
- 所有錯誤路徑（狀態檢查失敗、硬體忙、DMA 失敗）都會**自動釋放總線**
- 確保即使發生錯誤，總線也不會被永久佔用

#### API 使用範例

**創建新 I2C 模組**:

```c
// 1. 定義模組結構
typedef struct {
  bool dataReady;
  uint16_t sensorValue;
  
  I2C_Module_t i2cModule;    // 通用狀態機
  I2C_SlaveDevice_t slaves[2]; // 從機陣列
} MyModule_t;

// 2. 實現數據處理回調
static void MyModule_DataCallback(I2C_Module_t *module, uint8_t slaveId) {
  // 數據已在 processBuffer，直接處理
  MyModule.sensorValue = module->slaves[slaveId].processBuffer[0];
  MyModule.dataReady = true;
}

// 3. 初始化模組（main.c 中的完整範例）
int main(void) {
  // ... HAL 初始化 ...
  MX_I2C3_Init();
  
  /* 🔑 步驟 1: 初始化總線管理器（多模組共享時必須）*/
  I2C_BusManager_t i2c3_bus;
  I2C_Bus_Init(&i2c3_bus, &hi2c3);
  
  /* 🔑 步驟 2: 初始化使用 hi2c3 的模組 */
  IR_Init(&hi2c3);       // IR 模組使用 hi2c3
  MPU6050_init(&hi2c3);  // MPU6050 模組也使用 hi2c3
  
  // ... 其他初始化 ...
}

void MyModule_Init(I2C_HandleTypeDef *hi2c) {
  // 設置從機（緩衝區必須在非快取記憶體）
  MyModule.slaves[0].address = 0x30 << 1;
  MyModule.slaves[0].txBuffer = txBufferDMA;  // NULL = 直接讀
  MyModule.slaves[0].rxBuffer = rxBufferDMA;
  MyModule.slaves[0].processBuffer = processBuffer;
  MyModule.slaves[0].bufferSize = 16;
  MyModule.slaves[0].txSize = 1;  // 0=直接讀, >0=寫後讀
  MyModule.slaves[0].enabled = true;
  
  // 初始化通用狀態機
  I2C_Module_Init(
    &MyModule.i2cModule,
    hi2c,
    MyModule.slaves,
    2,                          // 從機數量
    20,                         // 輪詢間隔 (ms)
    MyModule_DataCallback       // 處理回調
  );
}

// 4. 主迴圈處理
void MyModule_Process(void) {
  I2C_Module_Process(&MyModule.i2cModule);
}

// 5. 中斷回調（在 stm32h7xx_it.c 中）
void MyModule_RxCallback(I2C_HandleTypeDef *hi2c) {
  I2C_Module_RxCallback(&MyModule.i2cModule, hi2c);
}

void MyModule_TxCallback(I2C_HandleTypeDef *hi2c) {
  I2C_Module_TxCallback(&MyModule.i2cModule, hi2c);
}

void MyModule_ErrorCallback(I2C_HandleTypeDef *hi2c) {
  I2C_Module_ErrorCallback(&MyModule.i2cModule, hi2c);
}
```

#### 中斷處理（stm32h7xx_it.c）

**重要**：`HAL_I2C_Mem_Read_DMA()` 使用不同的回調函數！

```c
/* 🔹 直接讀模式回調（HAL_I2C_Master_Receive_DMA）*/
void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c) {
  // 派發到所有使用直接讀的模組（IR 感測器）
  IR_RxCallback(hi2c);
}

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c) {
  // 直接讀模式不需要 TX（IR 無 TX）
}

/* 🔸 寫後讀模式回調（HAL_I2C_Mem_Read_DMA）*/
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c) {
  // 派發到所有使用寫後讀的模組（MPU6050）
  MPU6050_RxCallback(hi2c);
}

/* ⚠️ 錯誤回調（兩種模式共用）*/
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c) {
  IR_ErrorCallback(hi2c);
  MPU6050_ErrorCallback(hi2c);
}
```

**回調函數說明**：
- `HAL_I2C_Master_Receive_DMA()` → `HAL_I2C_MasterRxCpltCallback()`
  - 用於直接讀模式（txSize = 0）
  - 範例：IR 感測器陣列
  
- `HAL_I2C_Mem_Read_DMA()` → `HAL_I2C_MemRxCpltCallback()`
  - 用於寫後讀模式（txSize > 0）
  - 範例：MPU6050 讀取寄存器
  - HAL 內部先發送寄存器地址，完成後自動啟動 RX DMA
  
- `HAL_I2C_ErrorCallback()`
  - 兩種模式共用
  - 處理總線錯誤、NACK、仲裁丟失等錯誤

#### Bus Manager API 函數

**初始化總線管理器**:
```c
void I2C_Bus_Init(I2C_BusManager_t *manager, I2C_HandleTypeDef *hi2c);
```
- 在 `main.c` 中模組初始化之前調用
- 註冊到全局 `busManagers[]` 陣列（最多 4 條總線）
- 初始化總線為空閒狀態（owner = NULL, locked = false）

**獲取總線管理器**:
```c
I2C_BusManager_t* I2C_Bus_GetManager(I2C_HandleTypeDef *hi2c);
```
- 根據 `hi2c` 指針查找對應的總線管理器
- 在 `I2C_Module_ReadSlave()` 中調用一次並重用
- 返回 NULL 表示總線未初始化

**嘗試獲取總線所有權**:
```c
bool I2C_Bus_TryAcquire(I2C_BusManager_t *manager, I2C_Module_t *module);
```
- **原子操作**：使用 `__disable_irq()` 保護
- 返回 true：成功獲取，可以開始 DMA 傳輸
- 返回 false：總線被其他模組佔用，等待下次輪詢
- 同一模組重入：已擁有總線時直接返回 true

**釋放總線所有權**:
```c
void I2C_Bus_Release(I2C_BusManager_t *manager, I2C_Module_t *module);
```
- **原子操作**：使用 `__disable_irq()` 保護
- 只有擁有者可以釋放總線（所有權檢查）
- 自動在以下情況調用：
  - ✅ 正常完成：`I2C_STATE_PROCESSING` 結束
  - ✅ 超時錯誤：`I2C_STATE_READING` 超時
  - ✅ I2C 錯誤：`ErrorCallback` 中
  - ✅ 錯誤恢復：`I2C_STATE_ERROR` 恢復完成
  - ✅ DMA 失敗：`ReadSlave` 啟動失敗

#### 關鍵設計決策

1. **為何需要 Bus Manager？**
   - STM32 的 I2C 周邊硬體只有一套寄存器
   - 多個模組同時訪問會造成寄存器衝突
   - Bus Manager 提供軟體層級的互斥鎖

2. **為何 TX 用 IT，RX 用 DMA？**
   - TX 通常只有 1 byte（暫存器地址），IT 更高效
   - RX 可能 14-16 bytes，DMA 釋放 CPU

3. **為何在主迴圈啟動 DMA？**
   - 避免在中斷中啟動 DMA（HAL 可能未就緒）
   - 允許完整的錯誤處理和超時重置
   - 使用 `READY_TO_READ` 狀態作為標誌

4. **為何 memcpy 在中斷？**
   - 防止下次 DMA 覆蓋 rxBuffer
   - 小緩衝區（<= 16B）複製非常快（< 1μs）
   - 數據處理回調在主迴圈執行

### 馬達控制模組 (`motors.c`)

#### H 橋控制邏輯

| FI  | BI  | 狀態     | 說明                |
|-----|-----|----------|---------------------|
| H   | L   | 正轉     | 馬達正向旋轉         |
| L   | H   | 反轉     | 馬達反向旋轉         |
| H   | H   | 主動煞車 | 快速停止             |
| L   | L   | 滑行停止 | 自然停止             |

#### API 使用範例

```c
// 初始化所有馬達
Mtrs_Init();

// 單馬達控制
mtr_Forward(MTR0, 80);      // 馬達 0 正轉 80% 速度
mtr_Backward(MTR1, 60);     // 馬達 1 反轉 60% 速度
mtr_Brake(MTR2);            // 馬達 2 煞車
mtr_Stop(MTR3);             // 馬達 3 滑行停止

// 設定衰減模式
mtr_SetDecayMode(MTR0, SLOW_DECAY);

// 四輪同時控制 (正值=正轉, 負值=反轉)
mtrs_Set4Speed(80, 80, 80, 80);     // 全速前進
mtrs_Set4Speed(-50, -50, -50, -50); // 半速後退

// 極座標移動 (角度 + 速度)
polarMove(0, 70);      // 0° (前進) 70% 速度
polarMove(90, 60);     // 90° (右移) 60% 速度
polarMove(180, 50);    // 180° (後退) 50% 速度
polarMove(270, 60);    // 270° (左移) 60% 速度

// 測試功能
mtr_TestAllMotors();                // 測試所有馬達
mtr_FindMinimumStartupPWM(MTR0);    // 找最小啟動 PWM
mtr_TestAcceleration(MTR0, 5, 100); // 加速度測試
```

#### 極座標移動原理

```
      0° (前)
       ↑
270° ← + → 90°
       ↓
     180° (後)
```

系統根據輸入角度自動計算四輪速度向量:
- **0°**: 全輪前進
- **90°**: 麥克納姆輪右移 (FL+RR 前, FR+RL 後)
- **180°**: 全輪後退
- **270°**: 麥克納姆輪左移 (FR+RL 前, FL+RR 後)

### IR 感測器模組 (`ir.c`)

#### 調試功能

在 [ir.h](Libraries/Module/ir.h) 中定義 `IR_DEBUG` 宏可啟用 UART 調試輸出：

```c
// 在 ir.h 頂部
#define IR_DEBUG  // 取消註解以啟用調試輸出
```

啟用後，每次接收到 IR 數據會輸出：
```
Eye:3 Val:1024
```

#### 禁用從機

有兩種方式禁用特定從機：

**初始化時禁用**（靜態）:
```c
IR.slaves[IR_SLAVE_2].enabled = false;  // 禁用第二個從機
```

**運行時禁用**（動態）:
```c
IR_SetSlaveEnabled(IR_SLAVE_2, false);  // 運行時禁用第二個從機
IR_SetSlaveEnabled(IR_SLAVE_2, true);   // 運行時重新啟用
```

狀態機會自動跳過禁用的從機，只輪詢已啟用的從機。

#### 資料格式

每個從機傳送 16 bytes:
```
[Vref_LSB, Vref_MSB, Eye0_LSB, Eye0_MSB, ..., Eye6_LSB, Eye6_MSB]
```

#### API 使用範例

```c
// 初始化 IR 模組
IR_Init(&hi2c3);

// 主迴圈中處理狀態機
while(1) {
    IR_Process();  // 自動每 20ms 輪詢兩個從機
    
    if (IR.dataReady) {
        // 使用感測器數據
        printf("Max Eye: %d, Value: %d\n", IR.maxEye, IR.maxValue);
        IR.dataReady = false;
    }
}

// 訪問所有感測器數據
for (int i = 0; i < 14; i++) {
    uint16_t value = IR.eyeValues[i];
}
```

### UART 資料輸出模組 (`data_uart.c`)

```c
// 初始化
dataUart_Init(&huart4);

// 發送字串
dataUart_SendString("Hello World\r\n");

// 格式化 PWM 輸出
dataUart_SendFormattedPWM(500, 50.0);  // "PWM:  500 (50.0%)"

// 解析並顯示 IR 資料
ParseAndDisplayIRData(buffer, 16);

// 顯示原始 HEX 資料
DisplayRawHexData(buffer, 16);
```

## 🎯 主程式流程 (`main.c`)

```c
int main(void) {
    // 1. 硬體初始化
    HAL_Init();
    SystemClock_Config();
    
    // 2. 週邊初始化
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_I2C3_Init();
    MX_UART4_Init();
    MX_TIM1_Init();  // 馬達 PWM
    // ... 其他 TIM
    
    // 3. 應用層初始化
    dataUart_Init(&huart4);    /* UART for data output */
    
    /* Initialize I2C bus manager for shared I2C3 peripheral */
    I2C_BusManager_t i2c3_bus;
    I2C_Bus_Init(&i2c3_bus, &hi2c3);
    
    IR_Init(&hi2c3);           /* IR sensor module */
    MPU6050_Init(&hi2c3);      /* MPU6050 IMU module */
    MPU6050_DMP_Init(&hi2c3, DMP_FEATURE_6X_LP_QUAT);
    Mtrs_Init();               /* Motor control */
    
    /* Cache module data pointers for efficiency */
    static const IR_t* irDataPtr = IR_GetData();
    static const MPU6050_t* mpuDataPtr = MPU6050_GetData();
    static const MPU6050_DMP_t* dmpDataPtr = MPU6050_DMP_GetData();
    
    static ModuleData_t moduleData = {
        .irData = irDataPtr,
        .mpuData = mpuDataPtr,
        .dmpData = dmpDataPtr
    };
    
    // 4. 主迴圈
    while(1) {
        // 狀態 LED 心跳
        if (HAL_GetTick() - lastLedToggleTime >= LED_HEARTBEAT_MS) {
            HAL_GPIO_TogglePin(GPIOD, LED_2_Pin);
            lastLedToggleTime = HAL_GetTick();
        }
        
        // 更新所有感測器數據
        updateData();
        
        // 處理足球機器人控制邏輯
        soccer_ProcessData(&moduleData);
        
        HAL_Delay(MAIN_LOOP_DELAY_MS);
    }
}
```

## � 調試配置

### 模塊化 DEBUG 宏

本專案使用模塊化的 DEBUG 宏系統，可為每個模塊單獨控制調試輸出。

#### 啟用/禁用 DEBUG 輸出

在 [CMakeLists.txt](CMakeLists.txt#L68-L76) 中取消註解相應的 DEBUG 宏：

```cmake
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # 取消註解以下行來啟用各模塊的調試輸出
    DEBUG_MPU6050        # MPU6050 傳感器數據和初始化
    DEBUG_MPU6050_DMP    # MPU6050 姿態 (Roll/Pitch/Yaw)
    DEBUG_IR             # IR 感測器眼睛數據
    DEBUG_I2C            # I2C 通訊錯誤和狀態
    DEBUG_MOTORS         # 馬達測試 PWM 值
)
```

#### DEBUG 宏對照表

| DEBUG 宏 | 模塊 | 輸出內容 | 函數 |
|------------|------|----------|------|
| `DEBUG_MPU6050` | MPU6050 | 傳感器數據 (Ax/Ay/Az/Gx/Gy/Gz/T)<br>初始化消息 | `dataUart_PrintMPU6050Data()`<br>`dataUart_PrintInitMessage()` |
| `DEBUG_MPU6050_DMP` | MPU6050 DMP | 姿態數據 (Roll/Pitch/Yaw)<br>初始化消息 | `dataUart_PrintMPU6050Attitude()`<br>`dataUart_PrintInitMessage()` |
| `DEBUG_IR` | IR 感測器 | 眼睛數據 (Eye/Val)<br>IR 數據解析 | `dataUart_PrintIRData()`<br>`ParseAndDisplayIRData()` |
| `DEBUG_I2C` | I2C 通訊 | I2C 錯誤信息<br>I2C 超時<br>RX 回調計數<br>設備發現 | `dataUart_PrintI2CError()`<br>`dataUart_PrintI2CStatus()`<br>`dataUart_PrintDeviceFound()`<br>`DisplayRawHexData()` |
| `DEBUG_MOTORS` | 馬達 | 測試標題<br>PWM 值 | `dataUart_PrintMotorTest()`<br>`dataUart_SendFormattedPWM()` |
| `DEBUG_SOCCER` | 足球控制 | 狀態信息 (未實現狀態機)<br>足球角度和偏航角 | `dataUart_PrintSoccerState()` |

#### 輸出範例

**DEBUG_MPU6050**:
```
MPU6050 Ready
Ax=0.12 Ay=-0.03 Az=0.98 Gx=0.5 Gy=-0.2 Gz=0.1 T=25.3
```

**DEBUG_MPU6050_DMP**:
```
MPU6050: Complementary Filter Ready
Roll=2.3 Pitch=-1.5 Yaw=45.2
```

**DEBUG_IR**:
```
Eye:3 Val:1024 | S0:[100,200,300,400,500,600,700] S1:[150,250,350,450,550,650,750]
```

**DEBUG_I2C**:
```
Device found at 0x68
RxCallback: 50
I2C_Error: Code=0x4, Slave=1
```

**DEBUG_MOTORS**:
```
=== Testing Motor 0 ===
PWM: 400 (40.0%)
PWM: 420 (42.0%)
```

#### 最佳實踐

1. **啟動時啟用所有 DEBUG**：確認所有模塊正常運作
2. **優化時禁用不需要的 DEBUG**：減少 UART 負載
3. **Release 版本禁用所有 DEBUG**：節省 Flash 和 RAM
4. **具體問題排查**：只啟用相關模塊的 DEBUG

### data_uart 模塊

所有調試打印函數集中在 [data_uart.c](Libraries/Uart/data_uart.c) 中，提供以下優點：

- ✅ **集中管理**：所有 UART 打印邏輯在一個模塊
- ✅ **統一格式**：一致的輸出格式
- ✅ **易於維護**：修改格式只需一處
- ✅ **安全檢查**：所有函數包含 NULL 檢查和緩衝區溢位保護

---

## �🛠️ 開發指南

### 新增 I2C 模組

使用通用 I2C 狀態機框架，大幅簡化開發。詳細指南請參考 [I2C_COMMON_USAGE.md](Doc/I2C_COMMON_USAGE.md)

**快速步驟**:

1. **創建模組結構體**（包含 `I2C_Module_t`）
2. **實現數據處理回調**（只需處理 `processBuffer` 中的數據）
3. **配置從機設備**（設置地址、緩衝區、txSize）
4. **初始化**（調用 `I2C_Module_Init()`）
5. **處理**（在主迴圈調用 `YourModule_Process()`）
6. **註冊回調**（在 `stm32h7xx_it.c` 添加回調派發）

**完整範例請參考**: 
- [ir.c](Libraries/Module/ir.c) - 直接讀模式
- [mpu6050.c](Libraries/Module/mpu6050.c) - 寫後讀模式

### DMA 緩衝區配置

**重要**: 所有 DMA 緩衝區必須位於非快取記憶體區域！

在 `const.h` 中定義:
```c
// 在 0x30000000 的 MPU 非快取區域
#define YOUR_MODULE_TXBUF_PTR ((uint8_t *)(0x30000000 + offset))
#define YOUR_MODULE_RXBUF_PTR ((uint8_t *)(0x30000000 + offset + 32))
```

建議:
- 每個緩衝區對齊到 32-byte 邊界
- 總計不超過 256 bytes (MPU Region 1 大小)
- 當前使用: 128 bytes / 256 bytes

### 修改馬達數量

1. 更新 `motors.h` 中的 `MOTOR_COUNT`
2. 在 `motors.c` 中添加 `mtrs[]` 陣列項目
3. 在 STM32CubeMX 中配置對應的 TIM 通道

### 調整 PWM 參數

修改 `motors.h`:
```c
#define PWM_MAX_VALUE 1000      // 調整 PWM 週期
#define PWM_STARTUP_MIN 400     // 調整最小啟動 PWM
```

### 除錯技巧

1. **UART 輸出**: 使用 `dataUart_SendString()` 輸出除錯訊息
2. **LED 指示**: 使用 LED_2/3/4 顯示狀態
3. **I2C 狀態**: 檢查 `i2cMaster.state` 變數
4. **馬達狀態**: 使用 `mtr_GetDirection()` 和 `mtr_GetSpeed()`

### 常見問題

#### Q1: I2C 通訊失敗 (TIMEOUT)
- 檢查從機地址是否正確 (需左移 1 bit)
- 確認上拉電阻已安裝 (SDA/SCL)
- 檢查 DMA 配置是否正確

#### Q2: 馬達不轉
- 確認 PWM 已啟動: `HAL_TIM_PWM_Start()`
- 檢查 PWM 值是否超過 `PWM_STARTUP_MIN`
- 量測 H 橋輸入訊號

#### Q3: 資料接收不完整
- 確認 DMA 緩衝區位置 (需在 SRAM_D2)
- 檢查 `bufferSize` 是否正確
- 確認 CPU 快取設定 (MPU_Config)

## 📖 相關文件

- [DEBUG_SYSTEM.md](Doc/DEBUG_SYSTEM.md) - 🆕 調試系統使用指南
- [I2C_COMMON_USAGE.md](Doc/I2C_COMMON_USAGE.md) - 通用 I2C 狀態機使用指南
- [I2C_Communication_Setup_Guide.md](Doc/I2C_Communication_Setup_Guide.md) - I2C 硬體與軟體設定詳解
- [I2C_MASTER_USAGE.md](Doc/I2C_MASTER_USAGE.md) - ⚠️ 已棄用，參考 I2C_COMMON_USAGE.md
- [STM32H750 資料手冊](https://www.st.com/resource/en/datasheet/stm32h750xb.pdf)
- [STM32H7 參考手冊](https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-stm32h743753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)

## 🔍 系統配置細節

### 系統時鐘
- **來源**: HSI (64 MHz)
- **PLL 配置**:
  - M = 4 (VCO 輸入 = 16 MHz)
  - N = 30 (VCO 輸出 = 480 MHz)
  - P = 2 (SYSCLK = 240 MHz)
- **AHB**: 120 MHz (除以 2)
- **APB1**: 60 MHz
- **APB2/3/4**: 120 MHz

### 記憶體對應
- **SRAM_D1**: 0x24000000 (128 KB)
- **SRAM_D2**: 0x30000000 (288 KB) - DMA 緩衝區
- **SRAM_D3**: 0x38000000 (64 KB)
- **FLASH**: 0x08000000 (128 KB)

### DMA 配置
- **I2C3_RX**: DMA1 Stream 0, 優先權: 高
- **I2C3_TX**: DMA1 Stream 1, 優先權: 高
- **記憶體**: 增量模式
- **週邊**: 固定模式

### DMA 快取一致性管理

#### 🔍 問題背景

STM32H7 使用 Cortex-M7 核心，配備 **16KB D-Cache**（資料快取），快取行大小為 **32 bytes**。當 DMA 和 CPU 同時訪問記憶體時，會產生快取一致性問題：

**問題場景**:
```
1. CPU 讀取資料 → 資料被載入到 D-Cache
2. DMA 寫入新資料到 RAM → CPU 的快取未更新
3. CPU 再次讀取 → 從快取讀到舊資料 ❌
```

#### ✅ 本專案的解決方案

本專案使用 **MPU (Memory Protection Unit)** 將 DMA 緩衝區設定為 **Non-Cacheable**：

```c
void MPU_Config(void) {
  // Region 1: D2 SRAM DMA 緩衝區 (0x30000000)
  MPU_InitStruct.BaseAddress = 0x30000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_256B;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;  // 🔑 關鍵設定
  MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  // ...
}
```

**優點**:
- ✅ **完全避免快取一致性問題** - CPU 和 DMA 都直接訪問 RAM
- ✅ **無需手動快取管理** - 不需要 `SCB_InvalidateDCache_by_Addr()`
- ✅ **無需對齊限制** - 不受 32-byte 快取行大小影響
- ✅ **程式碼簡化** - 移除複雜的快取失效操作

**權衡**:
- ⚠️ **效能略降** - 每次訪問都走 RAM（約 10-20 cycles vs 1 cycle 快取命中）
- 📊 **實際影響微小** - IR 感測器每 20ms 更新一次，16 bytes 讀取僅需 ~0.3μs

#### 🔬 技術細節

**記憶體架構**:
```
┌─────────────┐
│ CPU Core    │
│  (480 MHz)  │
└──────┬──────┘
       │
┌──────▼──────┐     ┌─────────────┐
│   D-Cache   │     │     DMA     │
│   (16 KB)   │     │ Controller  │
└──────┬──────┘     └──────┬──────┘
       │                   │
       └────────┬──────────┘
                │
       ┌────────▼────────┐
       │   D2 SRAM       │ ◄── Non-Cacheable (0x30000000)
       │   (288 KB)      │     CPU 和 DMA 都直接訪問
       └─────────────────┘
```

**Non-Cacheable 記憶體訪問**:
- CPU 讀/寫操作直接訪問 RAM，不經過 D-Cache
- DMA 控制器直接寫入 RAM
- 保證所有主機看到的資料一致

#### 📚 替代方案比較

| 方案 | 優點 | 缺點 | 適用場景 |
|------|------|------|----------|
| **Non-Cacheable (本專案)** | 簡單、可靠 | CPU 訪問較慢 | DMA 緩衝區、低頻訪問 |
| **手動快取失效** | CPU 訪問快 | 需對齊、易出錯 | 高頻 CPU 訪問 |
| **快取清空+失效** | 雙向 DMA | 複雜、開銷大 | TX/RX 雙向緩衝 |

#### ⚡ 效能分析

對於本專案的 IR 感測器應用：
```
更新頻率: 20 ms/次
資料大小: 16 bytes
讀取時間: ~16 × 10 cycles = 160 cycles @ 480MHz = 0.33 μs
影響比例: 0.33μs / 20ms = 0.00165%
```

**結論**: Non-cacheable 效能影響可忽略不計。

#### 🎯 關鍵程式碼位置

- **MPU 配置**: [Core/Src/main.c](Core/Src/main.c) - `MPU_Config()`
- **DMA 緩衝區定義**: [Libraries/const.h](Libraries/const.h)
- **I2C 回調**: [Libraries/I2C/i2c_master.c](Libraries/I2C/i2c_master.c) - `I2C_Master_RxCallback()`

#### 📖 延伸閱讀

- [AN4839: STM32H7 Cache Maintenance](https://www.st.com/resource/en/application_note/an4839-level-1-cache-on-stm32f7-series-and-stm32h7-series-stmicroelectronics.pdf)
- [STM32H7 Series Programming Manual - MPU](https://www.st.com/resource/en/programming_manual/pm0253-stm32f7-series-and-stm32h7-series-cortexm7-processor-programming-manual-stmicroelectronics.pdf)

## 📝 授權

Copyright (c) 2025 STMicroelectronics.
本軟體遵循 STMicroelectronics 授權條款。

## 🤝 貢獻

歡迎提交 Issue 和 Pull Request！

## 📧 聯絡方式

如有問題請聯繫專案維護者。

---

**版本**: 2.4.0  
**最後更新**: 2026年1月29日  
**平台**: STM32H750XX  
**開發工具**: STM32CubeIDE / VS Code + CMake
