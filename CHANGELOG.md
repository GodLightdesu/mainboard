# 更新日誌

## [2.9.0] - 2026-03-12

### 📟 Printf 調試系統全面優化

#### 標準 Printf 重定向實現
- **批次傳輸模式**：覆寫 `_write()` 函數實現高效 UART 傳輸
  - **主要路徑**：printf() 調用 _write() 批次傳輸整個字符串
  - 性能提升 **10 倍**：100 字節從 ~1040ms 降至 ~104ms
  - UART 調用次數從 100 次降至 1 次
  - 超時設置：500ms（支持 ~480 字節長訊息）
- **字符模式備用**：覆寫 `__io_putchar()` 提供兼容性
  - **備用路徑**：putchar() 等函數使用
  - 超時設置：50ms（單字符傳輸）
  - 不影響 printf() 性能
- **自動重定向**：所有 printf() 調用自動重定向到 UART4
  - 波特率：9600 bps
  - 格式：8N1

#### 調用鏈優化
- **printf() 路徑**：`printf()` → `_write()` → `HAL_UART_Transmit()` (批次)
- **putchar() 路徑**：`putchar()` → `__io_putchar()` → `HAL_UART_Transmit()` (單字符)
- **syscalls.c weak 實現被覆寫**：不再使用逐字符循環的低效實現

#### 代碼架構優化
- **集中化管理**：將 printf 重定向從 main.c 移至 dataPrint.c
  - `int _write(int file, char *ptr, int len)` → dataPrint.c
  - `int __io_putchar(int ch)` → dataPrint.c
  - 更好的模組化設計
  - 統一管理調試輸出
- **靜態 UART 句柄**：`static UART_HandleTypeDef *dataUart_huart`
  - 通過 `dataUart_Init(&huart4)` 初始化
  - NULL 檢查防止未初始化使用
  - 靈活支持不同 UART 周邊

#### 全面 DEBUG 宏保護
- **現有 DEBUG 宏更新**：
  - `DEBUG_GENERAL`：添加系統重置、IWDG 重置檢測訊息保護
  - `DEBUG_MPU6050_DMP`：添加 MPU6050 初始化重試訊息保護
  - `DEBUG_SOCCER`：狀態機調試（已有）
  - `DEBUG_BUTTON`：按鍵事件與系統啟動訊息（已有）
- **所有 printf 調用檢查**：
  - ✅ soccer.c：4 處 printf（已保護）
  - ✅ main.c：4 處 printf（已保護）
  - ✅ dataPrint.c：所有函數內部 printf（已保護）
  - ✅ button.c：使用 dataUart_SendString（已保護）
  - ✅ 無未保護的 printf 調用

#### dataPrint 模塊優化
- **統一 printf 實現**：所有 dataUart_Print* 函數內部使用 printf()
  - 移除手動 snprintf + HAL_UART_Transmit 模式
  - 簡化代碼邏輯
  - 自動受益於批次傳輸優化
- **參數消除優化**：添加 #else 區塊消除未使用參數警告
  ```c
  #ifdef DEBUG_XXX
    printf(...);
  #else
    (void)param;  // 消除編譯警告
  #endif
  ```
- **完整函數列表**：
  - `dataUart_SendString()`：通用字符串輸出（DEBUG_GENERAL）
  - `dataUart_PrintInitMessage()`：模塊初始化訊息
  - `dataUart_PrintIRData()`：IR 傳感器數據（DEBUG_IR）
  - `dataUart_PrintXsoundData()`：Xsound 超聲波數據（DEBUG_XS）
  - `dataUart_PrintMotorTest()`：馬達測試（DEBUG_MOTORS）
  - `dataUart_PrintI2CError()`：I2C 錯誤（DEBUG_I2C）
  - `dataUart_PrintSoccerState()`：足球機器人狀態（DEBUG_SOCCER）
  - `dataUart_PrintButtonEvent()`：按鍵事件（DEBUG_BUTTON）

#### 使用方式靈活化
- **方式 1：直接使用 printf**（推薦）
  ```c
  #ifdef DEBUG_MY_MODULE
    printf("Value: %d\r\n", value);
  #endif
  ```
  - 標準 C 語法
  - 靈活的格式化
  - 編譯器優化支持
- **方式 2：使用包裝函數**
  ```c
  dataUart_PrintButtonEvent(0, "BTN1", "CLICK");
  ```
  - 統一格式
  - 自動 NULL 檢查
  - 無需手動 #ifdef

#### 性能指標
| 指標 | 批次模式 (_write) | 字符模式 (__io_putchar) |
|------|------------------|------------------------|
| 100 字節傳輸時間 | ~104ms | ~1040ms |
| UART 調用次數 | 1 次 | 100 次 |
| CPU 開銷 | 低 | 高 |
| 超時設置 | 500ms | 50ms |
| 使用函數 | **printf()** ← 主要 | putchar() ← 備用 |

#### 資源使用
- **Flash 使用**：
  - 禁用所有 DEBUG：~2.1 KB（僅 printf 庫和重定向函數）
  - 啟用所有 DEBUG：~3-5 KB
- **RAM 使用**：
  - printf 棧：~200-500 bytes（動態）
  - 靜態變量：4 bytes (`dataUart_huart`)
  - 無額外緩衝區
- **UART 傳輸計算**：
  - 波特率：9600 bps ≈ 960 bytes/sec
  - 500ms 超時 ≈ 最大 480 bytes

#### 文檔更新
- **完全重寫** [Doc/DEBUG_SYSTEM.md](Doc/DEBUG_SYSTEM.md)：
  - Printf 重定向實現原理
  - 性能對比與優勢說明
  - DEBUG 宏完整列表與作用域
  - 兩種使用方式詳細說明
  - 最佳實踐與性能考慮
  - 常見問題解答
  - 資源使用分析

#### 技術亮點
- ✅ **10 倍性能提升**：批次傳輸 vs 字符傳輸
- ✅ **標準兼容**：使用標準 printf，不依賴自定義實現
- ✅ **模組化設計**：集中管理，易於維護
- ✅ **完全可控**：所有輸出受 DEBUG 宏控制
- ✅ **資源優化**：Release 版本最小開銷
- ✅ **靈活使用**：支持 printf 和包裝函數兩種方式

---

## [2.8.0] - 2026-02-15

### 🚀 系統 Bootloader 跳轉功能

#### 按鈕控制啟動模式
- **BTN_4 按鈕控制**：在系統啟動時決定運行模式
  - **不按 BTN_4** + 上電/重啟 → 從 Flash 運行正常應用程序
  - **按住 BTN_4** + 上電/重啟 → 自動跳轉到 STM32 系統 Bootloader
- **早期檢測機制**：在 `main()` 最開始檢查按鈕狀態
  - 在 MPU 配置前執行
  - 在 HAL_Init() 前執行
  - 在任何外設初始化前執行
  - 手動配置 GPIO 避免依賴 HAL 庫

#### 完整的系統狀態清理
- **JumpToBootloader() 函數**：實現健全的 Bootloader 跳轉
  - 禁用所有中斷（包括 SysTick）
  - 禁用 GPIOD 時鐘（用於按鈕檢測的唯一外設）
  - **禁用並清理 DCache**：確保數據一致性
  - **禁用並無效化 ICache**：清除指令緩存
  - **禁用 MPU**：移除記憶體保護配置
  - **清除所有 NVIC 中斷**：
    - 清除中斷使能寄存器 (ICER)
    - 清除中斷掛起寄存器 (ICPR)
    - 重置中斷優先級寄存器 (IP)
  - **重置向量表**：VTOR 設為 0x00000000
  - **設置堆疊指標**：使用 Bootloader 的初始 SP
  - **重新啟用中斷**：Bootloader 需要中斷支持

#### STM32H750 專用配置
- **Bootloader 地址**：0x1FF09800（STM32H750 系統 Bootloader）
- **記憶體映射**：
  - 應用程序：0x08000000 (Flash Bank 1)
  - 系統記憶體：0x1FF00000 ~ 0x1FF1FFFF
  - Bootloader 入口：0x1FF09800

#### 運行時重啟功能
- **Button 4 長按**：`SoccerButtonControl()` 處理長按事件
  - 發送重啟訊息到 UART
  - 延遲 100ms 確保訊息發送完成
  - 調用 `NVIC_SystemReset()` 執行系統重啟
  - 重啟時可配合按住 BTN_4 從 Flash 啟動

#### 技術細節
- **原子性保證**：使用 `__disable_irq()` 確保檢查和跳轉不被中斷
- **緩存一致性**：STM32H7 的 DCache 和 ICache 必須正確清理
- **暫存器直接操作**：
  - 使用 `RCC->AHB4ENR` 手動控制時鐘
  - 使用 `GPIOD->MODER` 和 `GPIOD->PUPDR` 配置 GPIO
  - 使用 `GPIOD->IDR` 讀取按鈕狀態
- **防止問題**：
  - ✅ 避免 IWDG 看門狗在跳轉過程中觸發
  - ✅ 避免 MPU 限制 Bootloader 訪問記憶體
  - ✅ 避免緩存數據污染 Bootloader
  - ✅ 避免中斷干擾跳轉過程
  - ✅ 避免錯誤的向量表配置

#### 應用場景
- **韌體更新**：透過 USB DFU 或 UART 更新韌體
- **系統恢復**：在應用程序損壞時進入恢復模式
- **工廠模式**：生產測試和校準
- **快速重載**：開發時快速測試新韌體

#### 文檔
- 新增 [Doc/BOOTLOADER_USAGE.md](Doc/BOOTLOADER_USAGE.md) 詳細使用指南
- 包含完整的技術說明和實現細節

---

## [2.7.0] - 2026-02-06

### 🐕 看門狗保護系統

#### 獨立看門狗 (IWDG) 整合
- **IWDG配置**：使用LSI 32kHz時鐘源
  - IWDG 時鐘 125 Hz (LSI ÷ 256)
  - 計數器週期 8 ms 每個計數 (1/125Hz)
  - Reload_value: 625
  - 超時時間 5.0 秒 主超時時間 ((625 + 1) × 8 ms = 5,008 ms)
  - 視窗限制 無限制 視窗值 (4025 default) > 重載值 (625)
- **自動刷新**：在主循環中定期刷新看門狗
  - `HAL_IWDG_Refresh(&hiwdg1)` 防止系統重置
- **等待啟動保護**：在 `Soccer_WaitForStart()` 中持續刷新看門狗
  - 防止系統等待按鍵啟動時被看門狗重置
  - 每10ms循環檢查系統啟動狀態並刷新看門狗

#### LED 指示系統
- **等待狀態指示**：LED4 (GPIOB) 閃爍表示等待系統啟動
  - 500ms 閃爍週期
  - 視覺化系統狀態
- **系統啟動確認**：系統啟動後 LED4 保持點亮

### 📝 代碼改進
- **防護機制**：確保長時間等待操作不會觸發看門狗重置
- **可靠性提升**：系統異常時自動重置恢復正常運行
- **狀態監控**：LED指示提供系統狀態的視覺反饋

### 🎯 MPU6050 DMP 姿態估計
- **DMP (Digital Motion Processor) 整合**：使用 MPU6050 內建 DMP 進行姿態計算
  - 互補濾波器實現 (98% 陀螺儀 + 2% 加速度計)
  - 陀螺儀零點自動校準 (100 個樣本平均)
  - 硬體加速四元數計算
- **姿態輸出**：
  - Euler 角度 (Roll/Pitch/Yaw) 實時計算
  - 四元數 (Quaternion) 輸出
  - 防漂移機制：Yaw 角度重置功能
- **ARM CMSIS DSP 優化**：
  - 使用 `arm_sin_f32()`, `arm_cos_f32()` 硬體加速三角函數
  - 使用 `arm_atan2_f32()` 加速反正切計算
  - 使用 `arm_sqrt_f32()` 加速平方根計算

---

## [2.6.0] - 2026-01-30

### 🎮 按鍵控制模組新增

#### 完整的按鍵狀態機
- **狀態機設計**：實現完整的按鍵狀態機，支持消抖、單擊、雙擊、長按等功能
  - `BTN_STATE_IDLE`: 空閒狀態
  - `BTN_STATE_DEBOUNCE_PRESS`: 按下消抖
  - `BTN_STATE_PRESSED`: 已按下
  - `BTN_STATE_WAIT_RELEASE`: 等待釋放
  - `BTN_STATE_WAIT_DOUBLE`: 等待雙擊
  - `BTN_STATE_LONG_PRESS`: 長按
  - `BTN_STATE_RELEASE_DEBOUNCE`: 釋放消抖

#### 按鍵事件支持
- **事件類型**：支持多種按鍵事件
  - `BTN_EVENT_CLICK`: 單擊（用於一般控制）
  - `BTN_EVENT_DOUBLE_CLICK`: 雙擊（用於緊急停止）
  - `BTN_EVENT_LONG_PRESS_START`: 長按開始（用於啟動）
  - `BTN_EVENT_LONG_PRESS_HOLD`: 長按保持
  - `BTN_EVENT_LONG_PRESS_END`: 長按結束

#### 硬體抽象層
- **GPIO 配置**：支持最多 8 個按鍵，配置在 GPIOD 和 GPIOC
  - Button 1 (GPIOD): 啟動按鈕
  - Button 2 (GPIOD): 緊急停止按鈕
  - Button 3-8: 擴展按鈕

#### 系統控制整合
- **回調機制**：提供系統控制回調函數 `SystemControlCallback_t`
  - 足球機器人控制系統已整合按鍵控制
  - Button 1: 啟動足球機器人
  - Button 2: 緊急停止

#### 調試系統整合
- **DEBUG_BUTTON 宏**：完整的按鍵調試支持
  - 按鍵事件和狀態變化日誌
  - 原始和穩定狀態監控
  - 按鍵初始化和命名

#### API 介面
- **初始化函數**：`Button_Init()` 初始化所有按鍵
- **掃描函數**：`Button_Scan()` 定期掃描按鍵狀態（10ms 間隔）
- **事件查詢**：`Button_GetEvent()` 獲取按鍵事件
- **狀態查詢**：`Button_GetState()` 和 `Button_IsPressed()` 查詢按鍵狀態
- **回調設置**：`Button_SetSystemControlCallback()` 設置系統控制回調

#### 定時器整合
- **TIM7 中斷**：按鍵掃描整合到 TIM7 定時器中斷
  - 10ms 掃描間隔
  - 與現有系統定時器兼容

#### 記憶體使用
- **Flash 使用**：約 3KB（包含調試功能）
- **RAM 使用**：約 1KB（8 個按鍵狀態結構）

---

## [2.5.0] - 2026-01-29

### 🚀 ARM CMSIS DSP 效能優化

#### 硬體加速數學函數
- **ARM DSP 函數庫整合**：全面替換標準 C 數學函數為 ARM CMSIS DSP 硬體加速版本
  - `sinf()` → `arm_sin_f32()` - 正弦函數硬體加速
  - `cosf()` → `arm_cos_f32()` - 餘弦函數硬體加速
  - `sqrtf()` → `arm_sqrt_f32()` - 平方根硬體加速
  - `atan2f()` → `arm_atan2_f32()` - 反正切函數硬體加速
- **DSP 常數標準化**：統一使用 DSP 函數庫定義的常數
  - `M_PI` → `PI` - 使用 ARM DSP 定義的圓周率常數

#### 優化模組
- **MPU6050_DMP** (`mpu6050_dmp.c`)：DMP 姿態估計,四元數到歐拉角轉換和互補濾波器計算
- **IR 感測器** (`ir.c`)：足球角度計算
- **馬達控制** (`motors.c`)：輪子速度計算和麥克納姆輪運動學

#### 效能提升
- **硬體加速**：利用 STM32H7 Cortex-M7 FPU 和 DSP 指令集
- **計算精度**：維持單精度浮點精度
- **記憶體使用**：無顯著變化 (FLASH: 82KB)
- **代碼一致性**：統一變數宣告風格和 DSP 函數使用模式

#### 編譯依賴
- **DSP 函數庫**：自動包含 ARM CMSIS DSP 源碼
- **標頭檔**：`arm_math.h` 提供 DSP 函數和常數定義
- **建置系統**：CMake 自動配置 ARM_MATH_CM7 宏定義

---

## [2.4.0] - 2026-01-28

### 🔧 模組封裝重構

#### 靜態變數封裝
- **所有模組變數私有化**：將全局變數改為靜態變數
  - `mpu6050.c`: `mpu6050_data` → 靜態
  - `ir.c`: `ir_data` → 靜態
  - `motors.c`: `motors_data` → 靜態
- **Getter 函數**：提供 const 指針訪問器
  - `MPU6050_GetData()`: 返回 `const MPU6050_Data_t*`
  - `IR_GetData()`: 返回 `const IR_Data_t*`
  - `Motors_GetData()`: 返回 `const Motors_Data_t*`

#### 記憶體優化
- **消除循環依賴**：移除標頭檔間的交叉引用
- **減少堆疊使用**：靜態變數避免重複分配
- **編譯時初始化**：靜態變數在編譯時初始化

### ⚡ 效能優化

#### 指針快取機制
- **模組數據結構**：`ModuleData_t` 整合所有模組指針
  ```c
  typedef struct {
      const IR_t* irData;
      const MPU6050_t* mpuData;
      const MPU6050_DMP_t* dmpData;
  } ModuleData_t;
  ```
- **主循環快取**：在 `main.c` 中宣告靜態 `ModuleData_t moduleData`
- **減少函數調用**：避免每次循環重複調用 getter 函數

#### 記憶體使用優化
- **FLASH 使用率**：67.23% (之前約 75%)
- **RAM 使用率**：保持穩定
- **編譯時間**：無顯著變化

### 📝 文檔更新
- **README.md**: 更新版本至 2.4.0，新增狀態機和效能優化說明
- **CHANGELOG.md**: 本文件，記錄所有變更

---

## [2.3.0] - 2026-01-28

### 🔧 I2C 總線管理器重構

#### 全局 I2C 總線管理器
- **新增結構體**：`I2C_BusManager_t` 用於管理共享 I2C 總線
  - `hi2c`: I2C 周邊控制器指針
  - `owner`: 當前佔用總線的模組 (NULL 表示空閒)
  - `locked`: 總線鎖定狀態
- **支援多總線**：靜態陣列支援最多 4 條 I2C 總線管理

#### 新增 API 函數
- **`I2C_Bus_Init()`**: 初始化總線管理器並註冊到全局陣列
- **`I2C_Bus_GetManager()`**: 根據 I2C 控制器獲取對應的總線管理器
- **`I2C_Bus_TryAcquire()`**: 原子化嘗試獲取總線所有權
  - 使用 `__disable_irq()/__enable_irq()` 保證原子性
  - 如果總線被其他模組佔用，返回 false
  - 支援同一模組重入（已擁有總線時直接返回 true）
- **`I2C_Bus_Release()`**: 釋放總線所有權
  - 只有擁有者可以釋放總線
  - 原子化操作防止競爭條件

#### I2C_Module_ReadSlave() 改進
- **總線獲取檢查**：在開始 DMA 傳輸前嘗試獲取總線
- **失敗時釋放**：如果後續檢查失敗（狀態不對或硬體忙），自動釋放總線
- **防止衝突**：確保多模組不會同時訪問同一 I2C 硬體

#### 總線釋放時機
- **正常完成**：`I2C_STATE_PROCESSING` 結束後釋放
- **超時處理**：`I2C_STATE_READING` 超時進入錯誤狀態時釋放
- **錯誤回調**：`I2C_Module_ErrorCallback()` 中釋放
- **錯誤恢復**：`I2C_STATE_ERROR` 恢復後釋放
- **DMA 失敗**：DMA 啟動失敗時釋放

#### 多模組共享機制
- **真正輪流**：MPU6050 和 IR 模組可安全共享 hi2c3
- **公平調度**：先到先得，後到等待下次輪詢
- **自動重試**：佔用失敗的模組在下次輪詢時自動重試

#### 主程序更新
- **main.c**：在模組初始化前創建並初始化 `i2c3_bus` 管理器
  ```c
  I2C_BusManager_t i2c3_bus;
  I2C_Bus_Init(&i2c3_bus, &hi2c3);
  ```

#### 問題修復
- **重複重置修復**：移除超時處理中的重複 I2C 重置
  - 超時時不再執行 `HAL_I2C_DeInit/Init`
  - 統一在錯誤恢復階段執行重置
- **錯誤地址修復**：中止傳輸時使用正確的從機地址
  - 移除錯誤的 abort 調用（因為可能使用錯誤地址）
  - 依賴總線鎖機制防止衝突

#### 代碼優化與最佳實踐
- **優化總線管理器調用**：減少重複的 `I2C_Bus_GetManager()` 調用
  - 在 `I2C_Module_ReadSlave()` 開始時獲取管理器指針並重用
  - 避免多次查找同一總線管理器
- **統一錯誤處理**：使用 do-while(0) 模式替代 goto
  - 單一清理點釋放總線資源
  - 更清晰的錯誤處理流程
  - 避免 goto 語句提升代碼可讀性
- **防禦性編程**：初始化所有局部變量
  - `status` 變量初始化為 `HAL_ERROR` 防止未定義行為
  - 即使邏輯保證不會訪問未初始化值，仍提供安全保障
  - 提高代碼健壯性和維護性

---

## [2.2.0] - 2026-01-28

### 🔧 調試系統重構

#### 模塊化 DEBUG 宏系統
- **獨立 DEBUG 宏**：在 CMakeLists.txt 中為每個模塊定義單獨的 DEBUG 宏
  - `DEBUG_MPU6050`: 控制 MPU6050 傳感器數據和初始化訊息
  - `DEBUG_MPU6050_DMP`: 控制 MPU6050 姿態數據 (Roll/Pitch/Yaw)
  - `DEBUG_IR`: 控制 IR 感測器眼睛數據
  - `DEBUG_I2C`: 控制 I2C 通訊錯誤、狀態和設備發現訊息
  - `DEBUG_MOTORS`: 控制馬達測試 PWM 值
- **靈活控制**：可單獨啟用/禁用每個模塊的調試輸出
- **取代舊系統**：取代了 `IR_DEBUG`、`MPU6050_DEBUG`、`I2C_COMMON_DEBUG` 等舊宏

#### 集中式打印管理 (data_uart 模塊)
- **新增調試打印函數**：
  - `dataUart_PrintMPU6050Data()`: 打印 MPU6050 傳感器數據
  - `dataUart_PrintMPU6050Attitude()`: 打印姿態數據
  - `dataUart_PrintIRData()`: 打印 IR 眼睛數據
  - `dataUart_PrintI2CError()`: 打印 I2C 錯誤信息
  - `dataUart_PrintI2CStatus()`: 打印 I2C 狀態消息
  - `dataUart_PrintMotorTest()`: 打印馬達測試標題
  - `dataUart_PrintDeviceFound()`: 打印找到的設備地址
  - `dataUart_PrintInitMessage()`: 打印模塊初始化消息
- **所有模塊更新**：所有模塊現在使用 data_uart 打印函數
- **代碼簡化**：模塊不再直接呼叫 HAL_UART_Transmit
- **易於維護**：修改打印格式只需在 data_uart.c 中修改

#### 依賴關係修正
- **添加缺失的 include**：
  - `mpu6050_dmp.c`: 添加 `#include "data_uart.h"`
  - `i2c_common.h`: 添加 `#include "data_uart.h"`
  - `mpu6050.h`: 添加 `#include "data_uart.h"`
- **無循環依賴**：所有標頭檔依賴關係正確

### 📝 文檔更新
- 更新 README.md 反映最新架構
- 更新 I2C_COMMON_USAGE.md 增加調試配置指南
- 移除不再使用的舊 DEBUG 宏說明

---

## [2.1.2] - 2026-01-25

### ⚡ 效能優化

#### IR 資料處理簡化
- **合併原子操作**：減少中斷禁用區塊從 2 個到 1 個
- **簡化最大值搜尋**：從雙層迴圈改為單層迴圈
  - 直接遍歷所有 eyes（0-13），動態計算所屬 slave
  - 移除冗餘的 offset 計算和 continue 判斷
- **移除不必要變數**：刪除 `currentMask` 臨時變數
- **程式碼減少**：IR 資料處理從 51 行減少到 37 行
- **效能提升**：減少記憶體存取和分支判斷次數

---

## [2.1.1] - 2026-01-25

### 🔧 修正

#### 禁用從機跳過邏輯
- **自動跳過禁用的從機**：狀態機在 IDLE 狀態會自動搜尋下一個已啟用的從機
  - 防止卡在嘗試讀取禁用的從機
  - 使用 while 循環找到下一個 `enabled = true` 的從機
  - 最多搜尋一圈（`slaveCount` 次）避免無限循環
- **改善輪詢行為**：禁用任一從機後，其他從機仍能正常輪詢和輸出數據

#### IR 模組調試支持
- **新增 IR_DEBUG 宏**：控制 UART 調試輸出
  - 定義在 `ir.h` 頂部，方便啟用/禁用
  - 啟用時每次數據處理完成會輸出 `Eye:x Val:y`
  - 禁用時不會佔用 UART 帶寬

---

## [2.1.0] - 2026-01-25

### 🔐 線程安全性與健壯性大幅提升

#### 原子操作保護
- **所有狀態轉換使用 `__disable_irq()/__enable_irq()`**
  - 防止主循環與中斷之間的競態條件
  - 狀態讀取和寫入都受原子保護
  - 回調函數中的狀態檢查使用臨界區

#### 時間計算溢位安全
- **新增 `TIME_DIFF` 巨集**：處理 HAL_GetTick() 32-bit 溢位
  - 使用無符號減法特性計算時間差
  - 可靠運行長達 49.7 天後仍正常工作
  - 應用於所有超時檢查和輪詢間隔

#### DMA 緩衝區結構改進
- **移除 `readReg` 改為 `txBuffer` 和 `txSize`**
  - 支持任意長度的 TX 數據（不限於單字節）
  - TX buffer 位於 DMA 可訪問的非快取記憶體
  - 避免結構體成員地址問題
- **記憶體對齊優化**
  - 所有 DMA 緩衝區對齊到 32-byte 邊界
  - 防止潛在的快取一致性問題
  - IR TX buffers 已移除（直接讀取不需要）

#### 狀態機完整性增強
- **READY_TO_READ 超時保護**：防止卡在中間狀態
- **錯誤處理改進**：所有錯誤路徑都有原子保護
- **狀態檢查驗證**：Process 函數中使用原子讀取當前狀態

#### 記憶體配置（256 bytes MPU region）
```
0x30000000 (0):    IR Slave 1 RX Buffer (16B/32B)
0x30000020 (32):   IR Slave 2 RX Buffer (16B/32B)  
0x30000040 (64):   MPU6050 TX Buffer (1B/32B)
0x30000060 (96):   MPU6050 RX Buffer (14B/32B)
Total: 128 bytes used, 128 bytes reserved
```

### 📊 代碼質量
- ✅ 無編譯警告或錯誤
- ✅ 無競態條件
- ✅ 溢位安全的時間計算
- ✅ 完整的中斷保護
- ✅ 健壯的錯誤恢復

---

## [2.0.1] - 2026-01-24

### 🔒 中斷安全性改進

#### 狀態機優化
- **新增 I2C_STATE_READY_TO_READ 狀態**：TX 完成後標記準備讀取，DMA 在主迴圈啟動
  - 避免在中斷上下文中啟動 DMA，防止 HAL 狀態未就緒
  - 確保 DMA 啟動有完整的錯誤處理和超時重置
- **數據複製移至中斷**：`RxCallback` 中立即執行 `memcpy()`
  - 防止下次 DMA 傳輸覆蓋 `rxBuffer`
  - 消除主迴圈和中斷之間的競態條件
- **處理回調在主迴圈**：數據處理保持在非中斷上下文
  - 允許回調中執行耗時操作（如 printf）
  - 分離時間關鍵操作（memcpy）和邏輯處理

#### 改進的狀態轉換
```
寫-讀模式:
  IDLE → WRITING → READY_TO_READ → READING → PROCESSING → IDLE

直接讀模式:
  IDLE → READING → PROCESSING → IDLE
```

#### 錯誤處理增強
- **ErrorCallback 計時器**：設置 `stateStartTime` 啟用 50ms 錯誤恢復
- **狀態檢查保護**：TxCallback/RxCallback 驗證當前狀態
- **超時計時器重置**：READY_TO_READ → READING 時重置計時器

### 📊 代碼變更
- `I2C_State_t`: 添加 `I2C_STATE_READY_TO_READ`
- `I2C_Module_TxCallback()`: 只設置狀態標誌
- `I2C_Module_RxCallback()`: 執行 memcpy 並設置 PROCESSING
- `I2C_Module_Process()`: 處理 READY_TO_READ，啟動 DMA
- Flash: 65552 bytes (50.01%)

---

## [2.0.0] - 2026-01-24

### 🏗️ 架構重構

#### 通用 I2C 狀態機
- **新增 i2c_common 模組**：提取可重用的 I2C 狀態機邏輯
  - `I2C_Module_t`: 通用 I2C 模組結構
  - `I2C_Module_Process()`: 統一的狀態機處理
  - `I2C_Module_RxCallback()`/`I2C_Module_TxCallback()`: DMA 回調處理
  - 支持順序輪詢、超時處理、錯誤恢復
- **回調驅動設計**：每個模組通過 `I2C_DataProcessCallback_t` 處理自己的數據
- **消除重複代碼**：狀態機邏輯只實現一次，所有模組共享

#### IR 模組重構
- **使用通用狀態機**：IR 模組現在使用 `i2c_common` 框架
- **簡化實現**：從 100+ 行狀態機代碼減少到回調函數
- **保持功能**：自動順序讀取兩個從設備，數據處理邏輯不變
- **直接讀取模式**：使用 `readReg = 0xFF` 跳過寄存器寫入

#### MPU6050 模組重構
- **DMA 支持**：從阻塞式 I2C 升級為 DMA 自動讀取
- **自動輪詢**：每 50ms 自動讀取加速度計、陀螺儀和溫度數據
- **數據結構化**：`MPU6050_Data_t` 包含所有傳感器數據
- **寫-讀操作**：使用 `readReg = 0x3B` 先寫寄存器地址再讀取 14 字節
- **狀態機管理**：完全由通用狀態機處理，無需模組內部狀態管理

#### 移除舊架構
- **刪除 i2c_master**：完全移除 `Libraries/I2C/i2c_master.{h,c}`
- **簡化依賴**：模組直接使用 HAL I2C API 和通用狀態機
- **減少代碼**：總體代碼量減少，可維護性提升

### 📝 文件更新
- **新增 I2C_COMMON_USAGE.md**：通用 I2C 狀態機使用指南
  - 完整的 API 文檔
  - 創建新模組的步驟
  - IR 模組作為完整範例
  - 狀態機流程圖
- **更新 README.md**：反映新架構
- **更新 I2C_MASTER_USAGE.md**：標記為已棄用

### 🎯 改進重點
- ✅ **DRY 原則**：消除狀態機重複代碼
- ✅ **模組化**：每個 I2C 模組獨立管理
- ✅ **可擴展**：新增模組只需實現數據處理回調
- ✅ **一致性**：所有模組使用相同的狀態機邏輯
- ✅ **性能**：MPU6050 升級為 DMA，減少 CPU 負載

### 📊 代碼變更
- Flash: 64560 → 65456 bytes (+896 bytes for MPU6050 DMA)
- 新增文件: `i2c_common.{h,c}`, `I2C_COMMON_USAGE.md`
- 刪除文件: `i2c_master.{h,c}`
- 修改文件: `ir.{h,c}`, `mpu6050.{h,c}`, `soccer.{h,c}`, `stm32h7xx_it.c`

---

## [1.0.1] - 2026-01-19

### 🔧 修正

#### I2C Master 模組
- **最佳化順序輪詢**：改用單次檢查機制，提升非阻塞性能
  - 移除循環搜尋，每次 `Process()` 只檢查一個從機
  - 減少單次呼叫的執行時間，提高系統響應性
- **智慧重試機制**：
  - 成功讀取：正常間隔 (20ms)
  - 讀取失敗：快速重試 (5ms)
  - 從機禁用：立即跳過 (0ms)
- **DMA 快取同步**：添加 `SCB_InvalidateDCache_by_Addr()` 確保 CPU 讀取最新資料
  - 解決 STM32H7 D-Cache 與 DMA 記憶體一致性問題
  - 避免讀取到舊的快取資料

#### 馬達控制模組
- **統一速度範圍**：所有函數統一使用 0-100 百分比
  - `mtrs_Set4Speed()` 速度範圍從 ±255 修正為 ±100
  - 保持與 `mtr_Forward/Backward` 和 `polarMove` 一致
  - 提升程式碼可讀性和直觀性

#### IR 感測器模組
- **精確緩衝區檢查**：使用精確大小比對 (`size == IR_BUFFER_SIZE`)
  - 取代範圍檢查 (`size >= IR_BUFFER_SIZE`)
  - 提高資料驗證準確性

#### UART 模組
- **防止緩衝區溢出**：添加最大資料長度檢查 (32 bytes)
  - `ParseAndDisplayIRData()` 限制最多 16 個數值
  - 提升系統穩定性

### 📝 文件
- **新增 README.md**：完整的專案說明文件
  - 硬體規格和系統架構
  - 詳細的模組 API 說明和使用範例
  - 編譯、燒錄和除錯指南
  - PWM 原理和設定詳解
- **新增 CHANGELOG.md**：版本更新記錄

### 🎯 改進重點
- ✅ 更高效的非阻塞式 I2C 輪詢
- ✅ 解決 DMA 資料一致性問題
- ✅ 統一且直觀的速度控制介面
- ✅ 更嚴格的輸入驗證

## [1.0.0] - 2025-01-17

### ✨ 初始版本
- I2C Master 模組（支援 DMA 非阻塞式通訊）
- 4 馬達 PWM 控制系統 (10kHz, 0-100% 速度控制)
- IR 感測器陣列介面（2 從機，14 感測器）
- UART 資料輸出模組
- 極座標移動控制（麥克納姆輪運動學）
- 馬達測試與校準功能
