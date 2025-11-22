# STM32 I2C 通訊完整配置指南
## F103C8T6 (主機) ↔️ G474CBTX (從機)

---

## 📌 目錄
1. [硬體連接](#硬體連接)
2. [CubeMX 配置 - 從機 (G474CBTX)](#cubemx-配置---從機-g474cbtx)
3. [CubeMX 配置 - 主機 (F103C8T6)](#cubemx-配置---主機-f103c8t6)
4. [從機程式碼實作](#從機程式碼實作)
5. [主機程式碼實作](#主機程式碼實作)
6. [測試與除錯](#測試與除錯)

---

## 🔌 硬體連接

### 1. I2C 引腳連接

| F103C8T6 (主機) | G474CBTX (從機) | 說明 |
|----------------|----------------|------|
| **PB6** (I2C1_SCL) | **PB8** (I2C1_SCL) | 時鐘線 |
| **PB7** (I2C1_SDA) | **PB7** (I2C1_SDA) | 數據線 |
| **GND** | **GND** | 共地 (必須) |
| 3.3V (可選) | 3.3V | 共電源 (建議) |

### 2. 上拉電阻配置

**重要**: I2C 是開漏輸出,**必須**在 SCL 和 SDA 線上添加上拉電阻!

```
        VDD (3.3V)
           |
        [4.7kΩ]
           |
主機 ------+------ 從機  (SCL)

        VDD (3.3V)
           |
        [4.7kΩ]
           |
主機 ------+------ 從機  (SDA)
```

**電阻值選擇**:
- 標準速度 (100 kHz): 4.7kΩ ~ 10kΩ
- 快速模式 (400 kHz): 2.2kΩ ~ 4.7kΩ

**注意**: 某些開發板 (如 Blue Pill) 已內建上拉電阻,請先檢查板子規格。

---

## ⚙️ CubeMX 配置 - 從機 (G474CBTX)

### 步驟 1: 開啟專案
1. 啟動 STM32CubeMX
2. 開啟 `IR/IR.ioc` 或創建新專案選擇 **STM32G474CBTx**

### 步驟 2: I2C 配置

#### Pinout & Configuration
1. **Connectivity** → 選擇 **I2C1**
2. **Mode**: 選擇 `I2C`
3. **I2C Speed**: 選擇 `Standard Mode` (100 kHz)

#### Configuration 頁籤
進入 **I2C1 Configuration**:

**Parameter Settings**:
- **I2C Speed Mode**: `Standard Mode`
- **Own Address**: `0x02` (7-bit 地址,實際傳輸地址為 0x04)
  - 或使用 `0x01` ~ `0x7F` 之間任意未使用的地址
- **Primary slave address length**: `7-bit`

**GPIO Settings** (應自動設定):
- **PB7**: `I2C1_SDA`
- **PB8**: `I2C1_SCL`
- 兩者模式應為 **Alternate Function Open Drain**

**NVIC Settings** (重要!):
- ✅ **I2C1 event interrupt**: Enabled
- ✅ **I2C1 error interrupt**: Enabled

### 步驟 3: 生成程式碼
1. **Project Manager** → 設定專案名稱和路徑
2. **Toolchain/IDE**: 選擇 `Makefile` 或 `CMake`
3. 點擊 **GENERATE CODE**

---

## ⚙️ CubeMX 配置 - 主機 (F103C8T6)

### 步驟 1: 開啟專案
1. 啟動 STM32CubeMX
2. 開啟 `F103C8T6/F103C8T6.ioc` 或創建新專案選擇 **STM32F103C8Tx**

### 步驟 2: I2C 配置

#### Pinout & Configuration
1. **Connectivity** → 選擇 **I2C1**
2. **Mode**: 選擇 `I2C`

#### Configuration 頁籤
進入 **I2C1 Configuration**:

**Parameter Settings**:
- **I2C Speed Mode**: `Standard Mode` (100 kHz)
  - 或 `Fast Mode` (400 kHz) 如果兩邊都支持
- **Clock Speed**: `100000` (Hz)
- **Own Address**: `0x00` (主機地址,通常不重要)

**GPIO Settings** (應自動設定):
- **PB6**: `I2C1_SCL`
- **PB7**: `I2C1_SDA`
- 兩者模式應為 **Alternate Function Open Drain**

**NVIC Settings** (可選,用於中斷模式):
- ☐ **I2C1 event interrupt**: 可啟用(用於非阻塞模式)
- ☐ **I2C1 error interrupt**: 可啟用(用於錯誤處理)

### 步驟 3: 時鐘配置
進入 **Clock Configuration**:
- 確保 APB1 時鐘頻率正確 (通常 36 MHz)
- I2C1 使用 APB1 時鐘

### 步驟 4: 生成程式碼
1. **Project Manager** → 設定專案名稱和路徑
2. **Toolchain/IDE**: 選擇 `Makefile` 或 `CMake`
3. 點擊 **GENERATE CODE**

---

## 💻 從機程式碼實作 (G474CBTX)

### 文件結構
```
Core/
├── Inc/
│   ├── i2c_slave.h    (創建此文件)
│   └── main.h
└── Src/
    ├── i2c_slave.c    (創建此文件)
    └── main.c
```

### i2c_slave.h
```c
#ifndef __I2C_SLAVE_H
#define __I2C_SLAVE_H

#include "stm32g4xx_hal.h"

// 從機地址 (7-bit)
#define I2C_SLAVE_ADDRESS  0x02

// 數據緩衝區大小
#define I2C_BUFFER_SIZE    16

// 初始化從機
void I2C_Slave_Init(I2C_HandleTypeDef *hi2c);

// 更新發送數據 (可在主程式中調用)
void I2C_Slave_UpdateTxData(uint8_t *data, uint16_t size);

// 獲取接收數據 (可在主程式中調用)
uint8_t* I2C_Slave_GetRxData(void);
uint16_t I2C_Slave_GetRxSize(void);

#endif /* __I2C_SLAVE_H */
```

### i2c_slave.c
```c
#include "i2c_slave.h"
#include <string.h>

// 全域變數
static I2C_HandleTypeDef *g_hi2c;
static uint8_t TxBuffer[I2C_BUFFER_SIZE];
static uint8_t RxBuffer[I2C_BUFFER_SIZE];
static uint16_t RxSize = 0;

/**
 * @brief 初始化 I2C 從機
 */
void I2C_Slave_Init(I2C_HandleTypeDef *hi2c)
{
    g_hi2c = hi2c;
    
    // 清空緩衝區
    memset(TxBuffer, 0, I2C_BUFFER_SIZE);
    memset(RxBuffer, 0, I2C_BUFFER_SIZE);
    
    // 初始化一些測試數據
    TxBuffer[0] = 0xAA;  // 標識碼
    TxBuffer[1] = 0x01;  // 數據1
    TxBuffer[2] = 0x02;  // 數據2
    TxBuffer[3] = 0x03;  // 數據3
    
    // 啟動從機監聽模式
    HAL_I2C_EnableListen_IT(g_hi2c);
}

/**
 * @brief 更新發送緩衝區數據
 */
void I2C_Slave_UpdateTxData(uint8_t *data, uint16_t size)
{
    if (size > I2C_BUFFER_SIZE) {
        size = I2C_BUFFER_SIZE;
    }
    memcpy(TxBuffer, data, size);
}

/**
 * @brief 獲取接收數據指標
 */
uint8_t* I2C_Slave_GetRxData(void)
{
    return RxBuffer;
}

/**
 * @brief 獲取接收數據大小
 */
uint16_t I2C_Slave_GetRxSize(void)
{
    return RxSize;
}

/**
 * @brief 地址匹配回調 - 主機呼叫從機地址時觸發
 */
void HAL_I2C_AddrCallback(I2C_HandleTypeDef *hi2c, uint8_t TransferDirection, uint16_t AddrMatchCode)
{
    if (TransferDirection == I2C_DIRECTION_TRANSMIT) {
        // 主機要寫入數據到從機
        HAL_I2C_Slave_Seq_Receive_IT(hi2c, RxBuffer, I2C_BUFFER_SIZE, I2C_FIRST_AND_LAST_FRAME);
    } else {
        // 主機要讀取從機數據
        HAL_I2C_Slave_Seq_Transmit_IT(hi2c, TxBuffer, I2C_BUFFER_SIZE, I2C_FIRST_AND_LAST_FRAME);
    }
}

/**
 * @brief 監聽完成回調 - 一次傳輸完成
 */
void HAL_I2C_ListenCpltCallback(I2C_HandleTypeDef *hi2c)
{
    // 重新啟動監聽
    HAL_I2C_EnableListen_IT(hi2c);
}

/**
 * @brief 從機發送完成回調
 */
void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    // 數據已發送給主機
    // 可在此更新下次發送的數據
}

/**
 * @brief 從機接收完成回調
 */
void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    // 數據已從主機接收
    RxSize = I2C_BUFFER_SIZE;
    
    // 處理接收到的數據
    // 例如: 解析命令, 更新狀態等
}

/**
 * @brief I2C 錯誤回調
 */
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    // 發生錯誤,重新啟動監聽
    HAL_I2C_EnableListen_IT(hi2c);
}
```

### main.c (從機)
```c
/* USER CODE BEGIN Includes */
#include "i2c_slave.h"
/* USER CODE END Includes */

/* USER CODE BEGIN 0 */
extern I2C_HandleTypeDef hi2c1;
/* USER CODE END 0 */

int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_I2C1_Init();

  /* USER CODE BEGIN 2 */
  // 初始化 I2C 從機
  I2C_Slave_Init(&hi2c1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    
    // 檢查是否有新數據
    if (I2C_Slave_GetRxSize() > 0) {
        uint8_t *rxData = I2C_Slave_GetRxData();
        
        // 處理接收到的數據
        // 例如: 根據命令執行動作
        
        // 可選: 更新回應數據
        uint8_t response[4] = {0xBB, rxData[0], rxData[1], 0xFF};
        I2C_Slave_UpdateTxData(response, 4);
    }
    
    HAL_Delay(10);
  }
  /* USER CODE END 3 */
}
```

---

## 💻 主機程式碼實作 (F103C8T6)

### 文件結構
```
Core/
├── Inc/
│   ├── i2c_master.h   (創建此文件)
│   └── main.h
└── Src/
    ├── i2c_master.c   (創建此文件)
    └── main.c
```

### i2c_master.h
```c
#ifndef __I2C_MASTER_H
#define __I2C_MASTER_H

#include "stm32f1xx_hal.h"

// 從機地址 (7-bit 地址左移一位)
#define SLAVE_ADDRESS_G474  (0x02 << 1)  // 0x04

// I2C 超時設定
#define I2C_TIMEOUT  100  // ms

// 初始化主機
void I2C_Master_Init(I2C_HandleTypeDef *hi2c);

// 寫入數據到從機
HAL_StatusTypeDef I2C_Master_Write(uint8_t *data, uint16_t size);

// 從從機讀取數據
HAL_StatusTypeDef I2C_Master_Read(uint8_t *data, uint16_t size);

// 寫入後讀取 (Write-Then-Read)
HAL_StatusTypeDef I2C_Master_WriteRead(uint8_t *txData, uint16_t txSize, 
                                        uint8_t *rxData, uint16_t rxSize);

// 檢查從機是否就緒
HAL_StatusTypeDef I2C_Master_IsDeviceReady(void);

#endif /* __I2C_MASTER_H */
```

### i2c_master.c
```c
#include "i2c_master.h"

static I2C_HandleTypeDef *g_hi2c;

/**
 * @brief 初始化 I2C 主機
 */
void I2C_Master_Init(I2C_HandleTypeDef *hi2c)
{
    g_hi2c = hi2c;
}

/**
 * @brief 寫入數據到從機
 */
HAL_StatusTypeDef I2C_Master_Write(uint8_t *data, uint16_t size)
{
    return HAL_I2C_Master_Transmit(g_hi2c, SLAVE_ADDRESS_G474, data, size, I2C_TIMEOUT);
}

/**
 * @brief 從從機讀取數據
 */
HAL_StatusTypeDef I2C_Master_Read(uint8_t *data, uint16_t size)
{
    return HAL_I2C_Master_Receive(g_hi2c, SLAVE_ADDRESS_G474, data, size, I2C_TIMEOUT);
}

/**
 * @brief 先寫入命令,再讀取數據
 */
HAL_StatusTypeDef I2C_Master_WriteRead(uint8_t *txData, uint16_t txSize, 
                                        uint8_t *rxData, uint16_t rxSize)
{
    HAL_StatusTypeDef status;
    
    // 發送命令
    status = HAL_I2C_Master_Transmit(g_hi2c, SLAVE_ADDRESS_G474, txData, txSize, I2C_TIMEOUT);
    if (status != HAL_OK) {
        return status;
    }
    
    // 短暫延遲
    HAL_Delay(1);
    
    // 讀取回應
    status = HAL_I2C_Master_Receive(g_hi2c, SLAVE_ADDRESS_G474, rxData, rxSize, I2C_TIMEOUT);
    return status;
}

/**
 * @brief 檢查從機是否就緒
 */
HAL_StatusTypeDef I2C_Master_IsDeviceReady(void)
{
    // 嘗試 3 次,每次超時 10ms
    return HAL_I2C_IsDeviceReady(g_hi2c, SLAVE_ADDRESS_G474, 3, 10);
}
```

### main.c (主機)
```c
/* USER CODE BEGIN Includes */
#include "i2c_master.h"
#include <stdio.h>
/* USER CODE END Includes */

/* USER CODE BEGIN 0 */
extern I2C_HandleTypeDef hi2c1;
/* USER CODE END 0 */

int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART2_UART_Init();  // 用於除錯輸出

  /* USER CODE BEGIN 2 */
  I2C_Master_Init(&hi2c1);
  
  uint8_t txData[4] = {0x01, 0x02, 0x03, 0x04};
  uint8_t rxData[16] = {0};
  HAL_StatusTypeDef status;
  
  // 檢查從機是否就緒
  status = I2C_Master_IsDeviceReady();
  if (status == HAL_OK) {
      printf("Slave device found!\r\n");
  } else {
      printf("Slave device not found!\r\n");
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    
    // 測試1: 讀取從機數據
    status = I2C_Master_Read(rxData, 4);
    if (status == HAL_OK) {
        printf("Read: 0x%02X 0x%02X 0x%02X 0x%02X\r\n", 
               rxData[0], rxData[1], rxData[2], rxData[3]);
    }
    
    HAL_Delay(1000);
    
    // 測試2: 寫入數據到從機
    status = I2C_Master_Write(txData, 4);
    if (status == HAL_OK) {
        printf("Write successful\r\n");
    }
    
    HAL_Delay(1000);
    
    // 測試3: 寫入後讀取
    status = I2C_Master_WriteRead(txData, 2, rxData, 4);
    if (status == HAL_OK) {
        printf("Write-Read: 0x%02X 0x%02X 0x%02X 0x%02X\r\n", 
               rxData[0], rxData[1], rxData[2], rxData[3]);
    }
    
    HAL_Delay(1000);
  }
  /* USER CODE END 3 */
}
```

---

## 🧪 測試與除錯

### 測試步驟

#### 1. 硬體檢查
- ✅ 確認 SCL, SDA, GND 連接正確
- ✅ 確認上拉電阻已安裝 (4.7kΩ)
- ✅ 使用三用電表測量 SCL/SDA 靜態電壓應為 3.3V

#### 2. 軟體編譯
```bash
# 編譯從機 (G474)
cd IR/build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make

# 編譯主機 (F103)
cd F103C8T6/build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make
```

#### 3. 燒錄順序
1. **先燒錄從機** (G474CBTX)
2. **再燒錄主機** (F103C8T6)
3. 重啟兩塊板子

#### 4. 基本測試
使用邏輯分析儀或示波器觀察:
- SCL: 應有時鐘脈衝
- SDA: 應有數據變化
- 起始位: SDA 下降沿 (SCL 高電位時)
- 停止位: SDA 上升沿 (SCL 高電位時)

### 常見問題排查

#### ❌ 問題 1: HAL_I2C_Master_Transmit 返回 HAL_TIMEOUT
**可能原因**:
- 從機未啟動或未進入監聽模式
- I2C 地址錯誤
- 缺少上拉電阻
- SCL/SDA 接錯

**解決方法**:
```c
// 檢查從機是否就緒
if (HAL_I2C_IsDeviceReady(&hi2c1, SLAVE_ADDRESS_G474, 3, 100) == HAL_OK) {
    printf("Device ready\r\n");
} else {
    printf("Device not ready\r\n");
}
```

#### ❌ 問題 2: 從機不觸發中斷
**可能原因**:
- NVIC 中斷未啟用
- 從機地址不匹配
- 沒有調用 `HAL_I2C_EnableListen_IT()`

**解決方法**:
1. 檢查 CubeMX NVIC 設定
2. 確認 `stm32xxxx_it.c` 中有中斷處理函數:
```c
void I2C1_EV_IRQHandler(void)
{
  HAL_I2C_EV_IRQHandler(&hi2c1);
}

void I2C1_ER_IRQHandler(void)
{
  HAL_I2C_ER_IRQHandler(&hi2c1);
}
```

#### ❌ 問題 3: 數據傳輸錯誤或亂碼
**可能原因**:
- 時鐘速度不匹配
- 上拉電阻值不適當
- 連線太長 (超過 30cm)
- 電磁干擾

**解決方法**:
- 降低 I2C 速度到 100 kHz
- 縮短連線長度
- 使用雙絞線
- 添加去耦電容 (0.1µF)

#### ❌ 問題 4: 只能讀取或只能寫入
**可能原因**:
- 從機地址回調處理不完整
- 緩衝區大小不匹配

**解決方法**:
檢查 `HAL_I2C_AddrCallback()` 實作

### 除錯技巧

#### 1. 使用 UART 輸出除錯訊息
```c
// 在 main.c 中加入
#include <stdio.h>

// 重定向 printf 到 UART
int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, HAL_MAX_DELAY);
    return len;
}
```

#### 2. LED 指示狀態
```c
// 從機接收到數據時閃爍 LED
void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
}
```

#### 3. I2C 掃描工具
```c
// 掃描 I2C 總線上的所有設備
void I2C_Scan(void)
{
    printf("Scanning I2C bus:\r\n");
    for (uint8_t addr = 1; addr < 128; addr++) {
        if (HAL_I2C_IsDeviceReady(&hi2c1, addr << 1, 1, 10) == HAL_OK) {
            printf("Device found at 0x%02X\r\n", addr);
        }
    }
    printf("Scan complete\r\n");
}
```

---

## 📚 進階主題

### 使用 DMA 模式
```c
// 從機使用 DMA 接收
HAL_I2C_Slave_Receive_DMA(&hi2c1, RxBuffer, BUFFER_SIZE);

// 主機使用 DMA 發送
HAL_I2C_Master_Transmit_DMA(&hi2c1, SLAVE_ADDR, TxBuffer, SIZE);
```

### 多從機系統
```c
#define SLAVE1_ADDR  (0x02 << 1)
#define SLAVE2_ADDR  (0x03 << 1)
#define SLAVE3_ADDR  (0x04 << 1)

// 輪詢讀取所有從機
HAL_I2C_Master_Receive(&hi2c1, SLAVE1_ADDR, data1, size, 100);
HAL_I2C_Master_Receive(&hi2c1, SLAVE2_ADDR, data2, size, 100);
HAL_I2C_Master_Receive(&hi2c1, SLAVE3_ADDR, data3, size, 100);
```

### 實現暫存器讀寫協議
```c
// 從機實作類似 I2C EEPROM 的暫存器訊問
typedef struct {
    uint8_t reg_addr;
    uint8_t data[16];
} I2C_Register_t;
```

---

## 📖 參考資料

- [STM32 HAL Driver Documentation](https://www.st.com/en/embedded-software/stm32cube-mcu-packages.html)
- [I2C 通訊協議介紹](https://en.wikipedia.org/wiki/I%C2%B2C)
- [AN4235: I2C Timing Configuration Tool](https://www.st.com/resource/en/application_note/an4235-i2c-timing-configuration-tool-for-stm32f3xxxx-and-stm32f0xxxx-microcontrollers-stmicroelectronics.pdf)

---

**更新日期**: 2025-10-18
**作者**: GitHub Copilot
**測試平台**: STM32F103C8T6 + STM32G474CBTX
