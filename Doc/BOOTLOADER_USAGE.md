# STM32H750 Bootloader 跳轉使用指南

本文檔詳細說明 STM32H750 主控板的系統 Bootloader 跳轉功能，包括實現原理、使用方法和技術細節。

---

## 📋 目錄

- [功能概述](#功能概述)
- [使用方法](#使用方法)
- [實現原理](#實現原理)
- [技術細節](#技術細節)
- [應用場景](#應用場景)
- [故障排除](#故障排除)

---

## 🎯 功能概述

本系統實現了一個健全的 STM32 系統 Bootloader 跳轉機制，允許用戶在啟動時選擇：
- 運行正常應用程序（從 Flash）
- 進入系統 Bootloader（用於固件更新、恢復等）

### 主要特點

✅ **按鈕控制啟動模式** - 通過 BTN_4 (GPIOD Pin 2) 選擇啟動方式  
✅ **最早期檢測** - 在任何系統初始化前檢查按鈕狀態  
✅ **完整系統重置** - 正確清理所有硬件狀態  
✅ **緩存處理** - 清理 DCache 和 ICache  
✅ **MPU 管理** - 禁用內存保護單元  
✅ **中斷清理** - 清除所有中斷使能和掛起  
✅ **運行時重啟** - 支持通過長按按鈕重啟系統  

---

## 🔧 使用方法

### 1. 啟動時進入 Bootloader

**步驟：**
1. 確保 BTN_4（按鈕4）**沒有按下**
2. 給系統上電或按下復位鍵
3. 系統會自動跳轉到 STM32 系統 Bootloader
4. 此時可以通過 USB、UART 等接口更新固件

**硬件連接：**
- BTN_4: GPIOD Pin 2
- 按鈕配置：按下 = 低電平，未按下 = 高電平（內部上拉）

### 2. 啟動時運行應用程序

**步驟：**
1. **按住 BTN_4**（按鈕4）
2. 給系統上電或按下復位鍵
3. 保持按住直到 LED 開始閃爍（約 1 秒）
4. 系統會從 Flash 運行正常應用程序

### 3. 運行時重啟系統

**步驟：**
1. 在系統運行時，**長按 BTN_4** 約 3 秒
2. 系統會發送 "System Reset - Running from Flash..." 到 UART
3. 系統執行 `NVIC_SystemReset()` 重啟
4. 如果在重啟時按住按鈕，會從 Flash 啟動
5. 如果重啟時不按按鈕，會進入 Bootloader

### 4. 固件更新流程

**完整流程：**

```text
┌─────────────────────────────────────────────────────┐
│  1. 讓系統進入 Bootloader                           │
│     - 按住 BTN_4，然後重啟                          │
├─────────────────────────────────────────────────────┤
│  2. 通過 Bootloader 接口更新固件                    │
│     - USB DFU: STM32CubeProgrammer (USB 模式)      │
│     - UART: STM32CubeProgrammer (UART 模式)        │
│     - 其他: CAN, I2C, SPI (視 Bootloader 配置)     │
├─────────────────────────────────────────────────────┤
│  3. 更新完成後重新上電                              │
│     - 不按 BTN_4 重新上電                           │
│     - 系統從 Flash 運行新固件                       │
└─────────────────────────────────────────────────────┘
```

---

## 🏗️ 實現原理

### 系統啟動流程

```text
上電/復位
    │
    ├─> Power-on delay (電容穩定)
    │
    ├─> 啟用 GPIOD 時鐘 (RCC->AHB4ENR)
    │
    ├─> 配置 BTN_4 為輸入 + 上拉
    │
    ├─> 延遲穩定 (10000 cycles)
    │
    ├─> 讀取 BTN_4 狀態 (GPIOD->IDR)
    │
    ├─> BTN_4 = 1 (未按下)?
    │   │
    │   ├─ NO ──> JumpToBootloader()
    │   │         │
    │   │         ├─> 禁用中斷
    │   │         ├─> 禁用 SysTick
    │   │         ├─> 關閉 GPIOD 時鐘
    │   │         ├─> 禁用 DCache & ICache
    │   │         ├─> 禁用 MPU
    │   │         ├─> 清除所有 NVIC 中斷
    │   │         ├─> 重置 VTOR
    │   │         ├─> 設置 Bootloader SP
    │   │         ├─> 啟用中斷
    │   │         └─> 跳轉到 0x1FF09800
    │   │
    │   └─ YES ───> 繼續正常啟動
    │               │
    │               ├─> MPU_Config()
    │               ├─> HAL_Init()
    │               ├─> SystemClock_Config()
    │               ├─> 外設初始化
    │               └─> 主循環
    │
    └─> 運行
```

### JumpToBootloader() 函數詳解

#### 步驟 1: 禁用中斷系統
```c
__disable_irq();        // 設置 PRIMASK，屏蔽所有可屏蔽中斷
SysTick->CTRL = 0;      // 禁用 SysTick 計數器和中斷
SysTick->LOAD = 0;      // 清除重載值
SysTick->VAL = 0;       // 清除當前值
```
**原因：** 防止在跳轉過程中觸發中斷導致系統崩潰。

#### 步驟 2: 關閉外設時鐘
```c
RCC->AHB4ENR &= ~RCC_AHB4ENR_GPIODEN;  // 關閉 GPIOD 時鐘
```
**原因：** 清理我們啟用的唯一外設，讓系統回到接近復位狀態。

#### 步驟 3: 禁用緩存系統
```c
#if (__DCACHE_PRESENT == 1U)
SCB_DisableDCache();    // 禁用數據緩存，會自動清理（write-back）髒數據
#endif

#if (__ICACHE_PRESENT == 1U)
SCB_DisableICache();    // 禁用指令緩存
SCB_InvalidateICache(); // 無效化所有緩存行
#endif
```
**原因：**
- **DCache**: STM32H7 有 16KB 數據緩存，可能有未寫回內存的髒數據
- **ICache**: STM32H7 有 16KB 指令緩存，可能緩存了應用程序指令
- Bootloader 期望直接從內存讀取數據和指令，不是緩存

#### 步驟 4: 禁用內存保護
```c
HAL_MPU_Disable();      // 禁用 MPU
```
**原因：** 
- 應用程序在 `MPU_Config()` 中配置了 MPU 保護區域
- Bootloader 不知道這些配置，訪問受保護區域可能觸發 MemManage 異常
- 必須禁用以確保 Bootloader 可以自由訪問內存

#### 步驟 5: 清除 NVIC 中斷
```c
for (uint32_t i = 0; i < 8; i++) {
    NVIC->ICER[i] = 0xFFFFFFFF;  // 禁用所有中斷
    NVIC->ICPR[i] = 0xFFFFFFFF;  // 清除所有掛起中斷
}

for (uint32_t i = 0; i < 8; i++) {
    NVIC->IP[i] = 0x00000000;    // 重置所有中斷優先級
}
```
**原因：**
- STM32H7 有 150+ 個中斷，分佈在 8 個 32-bit 寄存器
- **ICER**: 清除中斷使能
- **ICPR**: 清除掛起標志
- **IP**: 重置優先級配置

#### 步驟 6: 重置向量表
```c
SCB->VTOR = 0x00000000;  // 重置向量表偏移
```
**原因：**
- 應用程序可能設置 VTOR 指向 Flash 中的向量表
- Bootloader 有自己的向量表在 0x1FF09800
- 重置為 0 確保異常和中斷處理正確

#### 步驟 7: 設置堆疊並跳轉
```c
// 讀取 Bootloader 的初始堆疊指針
__set_MSP(*(uint32_t *)BOOTLOADER_ADDRESS);

// 讀取 Bootloader 的 Reset Handler 地址
SysMemBootJump = (void (*)(void)) (*((uint32_t *)(BOOTLOADER_ADDRESS + 4)));

// 重新啟用中斷（Bootloader 需要）
__enable_irq();

// 跳轉！
SysMemBootJump();
```
**原因：**
- ARM Cortex-M 向量表格式：
  - 地址 0x0: 初始堆疊指針 (MSP)
  - 地址 0x4: Reset Handler 函數地址
- 必須正確設置 SP，否則堆疊操作會崩潰
- Bootloader 需要中斷支持（USB、UART 等）

---

## 🔍 技術細節

### STM32H750 內存布局

```text
地址空間布局：

0x00000000 ┌─────────────────────┐
           │ 可能映射到 Flash     │ (BOOT0=0, BOOT1=X)
           │ 或系統內存           │ (BOOT0=1, BOOT1=0)
           └─────────────────────┘
           
0x08000000 ┌─────────────────────┐
           │  Flash Bank 1       │ ← 應用程序位置
           │  (128 KB)           │   向量表: 0x08000000
           │                     │   Reset: 0x08000004
0x08020000 ├─────────────────────┤
           │  Flash Bank 2       │
           │  (預留/未使用)      │
           └─────────────────────┘

0x1FF00000 ┌─────────────────────┐
           │  系統內存區域       │ ← STM32 固件
           │  (128 KB)           │
0x1FF09800 │  ▼ Bootloader       │ ← 跳轉目標
           │    入口點           │   向量表: 0x1FF09800
           │                     │   Reset: 0x1FF09804
0x1FF1FFFF └─────────────────────┘

0x20000000 ┌─────────────────────┐
           │  DTCM RAM           │
           │  (128 KB)           │
           └─────────────────────┘

0x24000000 ┌─────────────────────┐
           │  AXI SRAM (D1)      │
           │  (512 KB)           │
           └─────────────────────┘

0x30000000 ┌─────────────────────┐
           │  AHB SRAM (D2)      │
           │  (288 KB)           │
           └─────────────────────┘
```

### GPIO 寄存器操作

#### 啟用時鐘
```c
RCC->AHB4ENR |= RCC_AHB4ENR_GPIODEN;
//             └─ bit 3: GPIOD 時鐘使能
```

#### 配置為輸入模式
```c
GPIOD->MODER &= ~(0x3U << (2 * 2));
//                └─ Pin 2: bits [5:4]
//                   00 = 輸入模式
```

#### 配置上拉電阻
```c
GPIOD->PUPDR &= ~(0x3U << (2 * 2));  // 先清零
GPIOD->PUPDR |= (0x1U << (2 * 2));   // 01 = 上拉
//               └─ Pin 2: bits [5:4]
//                  00 = 無上下拉
//                  01 = 上拉
//                  10 = 下拉
```

#### 讀取輸入
```c
if ((GPIOD->IDR & GPIO_PIN_2) == 0) {
//   └─ Input Data Register, bit 2
//       0 = 低電平 (按鈕按下)
//       1 = 高電平 (按鈕未按下，被上拉)
}
```

### 緩存系統

#### STM32H7 緩存配置
- **L1 DCache**: 16 KB, 4-way set associative
- **L1 ICache**: 16 KB, 2-way set associative
- **Cache line size**: 32 bytes

#### 為什麼需要清理緩存？

**場景 1: 數據緩存 (DCache)**
```text
應用程序寫入變量 → DCache 暫存 → 未寫回 SRAM
                                  ↓
                          跳轉到 Bootloader
                                  ↓
                   Bootloader 讀取 SRAM → 讀到舊數據 ✗
```

**解決：** `SCB_DisableDCache()` 會自動執行 Clean（寫回所有髒數據）

**場景 2: 指令緩存 (ICache)**
```text
應用程序執行 → ICache 緩存指令
                    ↓
            跳轉到 Bootloader
                    ↓
        CPU 從緩存讀指令 → 執行應用程序指令 ✗
```

**解決：** `SCB_InvalidateICache()` 無效化所有緩存行，強制從內存重新抓取

### 中斷系統

#### NVIC 寄存器
- **ISER[8]**: Interrupt Set-Enable Registers (設置使能)
- **ICER[8]**: Interrupt Clear-Enable Registers (清除使能) ← 我們用這個
- **ISPR[8]**: Interrupt Set-Pending Registers (設置掛起)
- **ICPR[8]**: Interrupt Clear-Pending Registers (清除掛起) ← 我們用這個
- **IP[240]**: Interrupt Priority Registers (優先級) ← 我們重置這個

#### 為什麼要清除中斷？
```text
應用程序配置：
- UART4 中斷使能，優先級 5
- TIM7 中斷使能，優先級 3
- I2C3 事件中斷使能，優先級 4
            ↓
    跳轉到 Bootloader
            ↓
Bootloader 不知道這些配置
            ↓
中斷觸發 → 跳轉到應用程序的中斷處理 → 崩潰 ✗
```

**解決：** 清除所有 NVIC 設置，讓 Bootloader 重新配置

### MPU (Memory Protection Unit)

#### 為什麼要禁用 MPU？

應用程序的 MPU 配置（見 `MPU_Config()`）：
```c
// Region 0: 全局 4GB，大部分區域禁止訪問
MPU_InitStruct.BaseAddress = 0x0;
MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
//                                 ↑ 禁止訪問

// Region 1: 僅允許訪問 0x30000000 的 256 bytes
MPU_InitStruct.BaseAddress = 0x30000000;
MPU_InitStruct.Size = MPU_REGION_SIZE_256B;
```

**問題：**
```text
Bootloader 可能需要訪問：
- 0x1FF00000 ~ 0x1FF1FFFF: 系統內存 ✗ 被限制
- 0x20000000 ~ 0x2001FFFF: DTCM RAM ✗ 被限制
- 0x40000000 ~ 0x5FFFFFFF: 外設 ✗ 被限制
```

**解決：** `HAL_MPU_Disable()` 禁用 MPU，移除所有限制

---

## 🎯 應用場景

### 1. 固件更新（USB DFU）

**流程：**
```bash
# 1. 讓設備進入 Bootloader
#    不按 BTN_4，按復位鍵

# 2. 使用 STM32CubeProgrammer 通過 USB 更新
STM32_Programmer_CLI -c port=USB1 -w firmware.bin 0x08000000

# 3. 按住 BTN_4 重新上電，運行新固件
```

### 2. 固件更新（UART）

**流程：**
```bash
# 1. 連接 UART（通常是 USART1）
#    TX -> RX, RX -> TX, GND -> GND

# 2. 讓設備進入 Bootloader
#    不按 BTN_4，按復位鍵

# 3. 使用 STM32CubeProgrammer 通過 UART 更新
STM32_Programmer_CLI -c port=COM3 br=115200 -w firmware.bin 0x08000000

# 4. 按住 BTN_4 重新上電，運行新固件
```

### 3. 系統恢復模式

**場景：** 應用程序崩潰或損壞

**操作：**
1. 不按 BTN_4，重新上電
2. 進入 Bootloader
3. 重新燒錄正確的固件
4. 按住 BTN_4 重新上電

### 4. 工廠測試模式

**場景：** 生產測試和校準

**配置：**
```c
// 在 main() 開始添加測試模式檢測
if ((GPIOD->IDR & GPIO_PIN_2) != 0) {
    // BTN_4 未按下
    if (/* 檢測其他測試條件 */) {
        // 進入測試模式而非 Bootloader
        FactoryTestMode();
    } else {
        JumpToBootloader();
    }
}
```

### 5. 開發調試

**場景：** 快速測試新固件

**工作流：**
```bash
# 1. 編譯新固件
cmake --build build

# 2. 燒錄
STM32_Programmer_CLI --connect port=swd --download build/mainboard.elf -rst

# 3. 如果需要通過 Bootloader 測試
#    不按 BTN_4 重啟，進入 Bootloader
#    然後通過 USB/UART 重新燒錄
```

---

## 🔧 故障排除

### 問題 1: 跳轉後系統無響應

**症狀：**
- 按 BTN_4 重啟後，系統凍結
- 沒有 USB 設備枚舉

**可能原因：**
1. **MPU 未禁用** → Bootloader 觸發 MemManage 異常
2. **緩存未清理** → Bootloader 讀到錯誤數據
3. **中斷未清除** → 中斷處理指向錯誤地址

**檢查：**
```c
// 確認 JumpToBootloader() 包含：
HAL_MPU_Disable();             // ← 必須有
SCB_DisableDCache();           // ← 必須有
SCB_InvalidateICache();        // ← 必須有
for (i=0; i<8; i++) {
    NVIC->ICER[i] = 0xFFFFFFFF; // ← 必須有
}
```

### 問題 2: 按鈕檢測不正確

**症狀：**
- 按住按鈕卻跳轉到 Bootloader
- 不按按鈕卻運行應用程序

**可能原因：**
1. **按鈕接線錯誤** → 檢查 BTN_4 是否接到 PD2
2. **上拉配置錯誤** → 檢查 PUPDR 設置
3. **讀取時間太早** → GPIO 未穩定

**檢查：**
```c
// 確認有足夠的穩定延遲
RCC->AHB4ENR |= RCC_AHB4ENR_GPIODEN;
for(volatile uint32_t i = 0; i < 1000; i++);  // 時鐘穩定

// 配置 GPIO...

for(volatile uint32_t i = 0; i < 10000; i++); // GPIO 穩定 ← 必須有
if ((GPIOD->IDR & GPIO_PIN_2) != 0) { ... }
```

### 問題 3: IWDG 看門狗重啟循環

**症狀：**
- 跳轉到 Bootloader 後立即重啟
- 循環進入應用程序和 Bootloader

**原因：**
- IWDG 在跳轉前已啟動
- Bootloader 沒有刷新看門狗
- 超時後系統重啟 → 重複

**解決：**
```c
// 在 main() 中，按鈕檢查在 MX_IWDG1_Init() 之前
int main(void) {
    // ← 按鈕檢查在這裡（早期）✓
    
    MPU_Config();
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    // ...
    MX_IWDG1_Init();  // ← IWDG 初始化在後面
}
```

### 問題 4: USB DFU 無法連接

**症狀：**
- 進入 Bootloader 後，電腦無法識別 USB 設備

**可能原因：**
1. **USB 引腳未配置** → STM32H750 Bootloader 使用 USB OTG_FS
2. **BOOT0 引腳狀態** → 某些型號需要 BOOT0=1
3. **USB 驅動問題** → Windows 需要安裝 STM32 Bootloader 驅動

**檢查硬件：**
- USB_DP (PA12) 連接正確
- USB_DM (PA11) 連接正確
- VBUS 有 5V 電源

**檢查軟件：**
```bash
# Windows: 設備管理器應該看到
# "STM32 BOOTLOADER" 或 "DFU in FS Mode"

# 安裝驅動：
# 使用 STM32CubeProgrammer 自帶的驅動程序
```

### 問題 5: 長按重啟無效

**症狀：**
- 長按 BTN_4 沒有重啟系統

**可能原因：**
1. **回調未註冊** → 檢查 `SoccerInit()` 中的回調設置
2. **TIM7 未啟動** → 按鈕掃描依賴 TIM7 中斷
3. **長按時間不夠** → 默認 3 秒

**檢查：**
```c
// 確認 SoccerInit() 中有：
Button_SetSystemControlCallback(3, SoccerButtonControl); // Button 4

// 確認 main() 中有：
HAL_TIM_Base_Start_IT(&htim7);

// 確認 stm32h7xx_it.c 中有：
void TIM7_IRQHandler(void) {
    HAL_TIM_IRQHandler(&htim7);
}

// 確認 HAL_TIM_PeriodElapsedCallback 調用：
Button_Scan();
```

---

## 📊 測試檢查清單

### 啟動測試

- [ ] 不按 BTN_4 上電 → 進入 Bootloader
- [ ] 按住 BTN_4 上電 → 運行應用程序
- [ ] 通過 ST-Link 復位 → 按按鈕狀態進入對應模式
- [ ] LED 指示正常（等待時閃爍，啟動後常亮）

### Bootloader 功能測試

- [ ] USB 設備正確枚舉（Windows: DFU 設備，Linux: lsusb）
- [ ] 可以通過 USB 讀取設備信息
- [ ] 可以通過 USB 下載固件
- [ ] 可以通過 UART 連接（115200 baud）
- [ ] 可以通過 UART 下載固件

### 應用程序測試

- [ ] 從 Bootloader 更新後應用程序正常運行
- [ ] 所有外設工作正常（I2C、UART、PWM 等）
- [ ] MPU6050 數據正常
- [ ] IR 感測器數據正常
- [ ] 馬達控制正常
- [ ] IWDG 看門狗正常（沒有意外重啟）

### 重啟測試

- [ ] 長按 BTN_4 可以重啟系統
- [ ] 重啟時按住 BTN_4 進入應用程序
- [ ] 重啟時不按 BTN_4 進入 Bootloader
- [ ] UART 輸出 "System Reset" 訊息

### 邊界測試

- [ ] 連續快速切換啟動模式 10 次
- [ ] 在 Bootloader 中保持 1 分鐘不超時
- [ ] 固件更新過程中不會死機
- [ ] 按鈕抖動不影響檢測

---

## 📚 參考資料

### STM32 官方文檔

- **AN2606**: STM32 microcontroller system memory boot mode
  - Bootloader 接口說明
  - 各型號 Bootloader 地址
  
- **RM0433**: STM32H742/743/753 Reference Manual
  - NVIC 章節
  - MPU 章節
  - 緩存系統章節
  
- **PM0253**: STM32 Cortex-M7 Programming Manual
  - 中斷處理
  - 緩存操作
  
### 開發工具

- **STM32CubeProgrammer**: 
  - 下載: https://www.st.com/stm32cubeprog
  - 支持 USB DFU、UART、SWD 等接口
  
- **DfuSe**: USB DFU 工具（舊版）
  - Windows USB DFU 工具
  
### 相關代碼

- `Core/Src/main.c`: `JumpToBootloader()` 實現
- `Libraries/soccer.c`: 按鈕長按重啟處理
- `Libraries/Button/button.c`: 按鈕掃描和事件處理

---

## 🔖 版本歷史

- **v2.8.0** (2026-02-15): 初始版本
  - 實現按鈕控制 Bootloader 跳轉
  - 完整的系統狀態清理
  - 運行時重啟功能
  - 本文檔創建

---

## ✍️ 作者

STM32H750 主控板開發團隊

最後更新：2026-02-15
