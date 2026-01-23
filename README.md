# STM32H750 主控板專案

這是一個基於 STM32H750 微控制器的主控板韌體專案，用於控制多馬達機器人系統並整合 IR 感測器陣列。

## 🎉 最新更新 (v1.0.1 - 2026-01-19)

### 重要改進
- **I2C Master 最佳化**：改用單次檢查輪詢，搭配智慧重試機制（失敗 5ms、禁用 0ms）
- **DMA 快取同步**：添加 `SCB_InvalidateDCache` 解決 STM32H7 資料一致性問題
- **速度控制統一**：所有馬達函數統一使用 0-100 百分比，提升可讀性
- **更嚴格驗證**：改進緩衝區和資料大小檢查，提升系統穩定性

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
  - Flash: 128 KB
  - SRAM: 1 MB (含 DMA 緩衝區於 D2 域)
- **調試介面**: SWD (Serial Wire Debug)

## 🏗️ 系統架構

```
┌─────────────────────────────────────────┐
│         STM32H750 主控板                │
├─────────────────────────────────────────┤
│  ┌────────────┐  ┌────────────┐        │
│  │ I2C Master │  │   Motors   │        │
│  │  (I2C3)    │  │  (4xPWM)   │        │
│  └────────────┘  └────────────┘        │
│  ┌────────────┐  ┌────────────┐        │
│  │ IR Sensors │  │ Data UART  │        │
│  │  (2 Slave) │  │  (UART4)   │        │
│  └────────────┘  └────────────┘        │
└─────────────────────────────────────────┘
```

## ✨ 功能特性

### 1. **I2C 主機模組**
- 支援最多 4 個 I2C 從機裝置
- DMA 非阻塞式資料接收
- 順序輪詢模式 (Sequential Polling Mode)
- 可配置的輪詢間隔
- 完整的錯誤處理與超時保護
- 回調機制用於資料處理

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

### 4. **UART 資料輸出**
- 格式化資料傳輸
- 感測器數值顯示
- 調試訊息輸出
- PWM 監控資訊

## 📁 專案結構

```
mainboard/
├── Core/                      # STM32CubeMX 產生的核心檔案
│   ├── Inc/                   # HAL 標頭檔
│   │   ├── main.h
│   │   ├── i2c.h
│   │   ├── tim.h
│   │   ├── usart.h
│   │   └── ...
│   └── Src/                   # HAL 實作檔案
│       ├── main.c             # 主程式進入點
│       ├── i2c.c
│       ├── tim.c
│       └── ...
│
├── Libraries/                 # 自訂函式庫
│   ├── const.h                # 全域常數定義
│   │
│   ├── I2C/                   # I2C 主機函式庫
│   │   ├── i2c_master.h
│   │   └── i2c_master.c       # DMA-based I2C master 實作
│   │
│   ├── Module/                # 週邊模組
│   │   ├── motors.h
│   │   ├── motors.c           # 馬達控制實作
│   │   ├── ir.h
│   │   └── ir.c               # IR 感測器介面
│   │
│   └── Uart/                  # UART 通訊
│       ├── data_uart.h
│       └── data_uart.c        # 格式化資料輸出
│
├── Drivers/                   # STM32 HAL 驅動程式
│   ├── CMSIS/                 # ARM CMSIS 標頭檔
│   └── STM32H7xx_HAL_Driver/  # STM32H7 HAL 函式庫
│
├── cmake/                     # CMake 建構系統
│   ├── gcc-arm-none-eabi.cmake
│   └── stm32cubemx/
│
├── build/                     # 建構輸出目錄
│
├── CMakeLists.txt             # 主 CMake 設定
├── CMakePresets.json          # CMake 預設配置
├── mainboard.ioc              # STM32CubeMX 專案檔
├── startup_stm32h750xx.s      # 啟動程式碼
├── STM32H750XX_FLASH.ld       # 連結器腳本
│
├── I2C_Communication_Setup_Guide.md   # I2C 設定指南
├── I2C_MASTER_USAGE.md                # I2C 主機使用說明
└── README.md                          # 本文件
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
- **用途**: IR 感測器通訊
- **從機 1**: 0x30 (左偏)
- **從機 2**: 0x31 (右偏)
- **DMA**: DMA1 Stream 用於接收
- **緩衝區位置**: 0x30000000 (SRAM_D2)

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
| UART4  | 資料輸出       | 115200     |
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

### I2C Master 模組 (`i2c_master.c`)

#### 主要特性
- DMA 非阻塞式接收
- 雙緩衝機制 (RX Buffer + Process Buffer)
- 狀態機管理 (IDLE, READING, PROCESSING, ERROR)
- 自動超時處理
- 可選擇順序輪詢或手動控制

#### API 使用範例

```c
// 初始化
I2C_Master_t i2cMaster;
I2C_Master_Init(&i2cMaster, &hi2c3);

// 註冊從機
uint8_t rxBuf[16], procBuf[16];
I2C_Master_RegisterSlave(&i2cMaster, 0x30 << 1, rxBuf, procBuf, 16);

// 設定回調
I2C_Master_SetSlaveCallback(&i2cMaster, 0, MyCallback);

// 啟用順序輪詢模式
I2C_Master_EnableSequentialMode(&i2cMaster, 20);  // 20ms 間隔

// 主迴圈中處理
while(1) {
    I2C_Master_Process(&i2cMaster);
}
```

#### 中斷處理

在 `stm32h7xx_it.c` 中需要呼叫回調:

```c
void I2C3_EV_IRQHandler(void) {
    HAL_I2C_EV_IRQHandler(&hi2c3);
}

void I2C3_ER_IRQHandler(void) {
    HAL_I2C_ER_IRQHandler(&hi2c3);
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c) {
    if(hi2c->Instance == I2C3) {
        I2C_Master_RxCallback(&i2cMaster, hi2c);
    }
}
```

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

#### 資料格式

每個從機傳送 16 bytes:
```
[Vref_LSB, Vref_MSB, Eye0_LSB, Eye0_MSB, ..., Eye6_LSB, Eye6_MSB]
```

#### API 使用範例

```c
// 初始化
IR_Init(&i2cMaster);

// 手動讀取 (使用順序模式時不需要)
IR_ReadData(SLAVE_1);

// 檢查資料是否就緒
if(IR_IsDataReady(SLAVE_1)) {
    // 處理資料 (回調中自動執行)
    IR_ClearDataReady(SLAVE_1);
}

// 全域變數
extern uint8_t maxEye;      // 最大值的感測器編號 (0-6)
extern uint16_t maxValue;   // 最大感測器數值
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
    I2C_Master_Init(&i2cMaster, &hi2c3);
    dataUart_Init(&huart4);
    IR_Init(&i2cMaster);
    Mtrs_Init();
    
    // 4. 啟用 I2C 順序輪詢
    I2C_Master_EnableSequentialMode(&i2cMaster, 20);  // 20ms
    
    // 5. 主迴圈
    while(1) {
        // 狀態 LED 心跳
        HAL_GPIO_TogglePin(LED_2_GPIO_Port, LED_2_Pin);
        
        // 處理 I2C 通訊
        I2C_Master_Process(&i2cMaster);
        
        // 根據 IR 資料控制馬達 (範例)
        // if(maxValue > IR_DETECTION_THRESHOLD) {
        //     polarMove(GetAngleFromSensor(maxEye), 60);
        // }
        
        HAL_Delay(10);
    }
}
```

## 🛠️ 開發指南

### 新增 I2C 從機

1. 在 `const.h` 中定義 DMA 緩衝區地址
2. 註冊從機:
```c
uint8_t rxBuf[SIZE], procBuf[SIZE];
I2C_Master_RegisterSlave(&i2cMaster, ADDR, rxBuf, procBuf, SIZE);
```
3. 設定回調函數處理資料

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

- [I2C_Communication_Setup_Guide.md](I2C_Communication_Setup_Guide.md) - I2C 硬體與軟體設定詳解
- [I2C_MASTER_USAGE.md](I2C_MASTER_USAGE.md) - I2C Master 函式庫使用指南
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

**版本**: 1.0.0  
**最後更新**: 2025年1月  
**平台**: STM32H750XX  
**開發工具**: STM32CubeIDE / VS Code + CMake
