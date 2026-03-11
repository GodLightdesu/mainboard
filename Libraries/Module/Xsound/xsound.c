#include "xsound.h"
#include "const.h"
#include "dataPrint.h"

static Xsound_t Xsound = {0};

/* 使用 const.h 中預定義的 RAM_D2 位址的 DMA 緩衝區 */
static uint8_t *rxBuffer = XSOUND_RXBUF_PTR;
static uint8_t processBuffer[XSOUND_BUFFER_SIZE];

/* 輔助函數：從字節陣列中提取浮點數 (little-endian) */
static float extract_float(const uint8_t *buffer, uint8_t offset)
{
  union
  {
    float f;
    uint8_t bytes[4];
  } converter;

  converter.bytes[0] = buffer[offset];
  converter.bytes[1] = buffer[offset + 1];
  converter.bytes[2] = buffer[offset + 2];
  converter.bytes[3] = buffer[offset + 3];

  return converter.f;
}

/* 資料處理回調函數 - 由通用 I2C 模組呼叫 */
static void Xsound_DataProcessCallback(I2C_Module_t *module, uint8_t slaveId)
{
  if (!Xsound.slave.enabled || Xsound.slave.processBuffer == NULL)
    return;

  /* 解析距離資料：[distance0(4 bytes), distance1(4 bytes), distance2(4 bytes), distance3(4 bytes)] */
  for (uint8_t i = 0; i < XSOUND_NUM_DISTANCES; i++)
  {
    Xsound.distances[i] = extract_float(Xsound.slave.processBuffer, i * 4);
  }

  Xsound.dataReady = true;

  /* Debug output */
  #ifdef DEBUG_XS
  dataUart_PrintXsoundData(&Xsound);
  #endif
}

void Xsound_Init(I2C_HandleTypeDef *hi2c)
{
  if (hi2c == NULL)
    return;

  /* 初始化 Xsound 結構 */
  memset(&Xsound, 0, sizeof(Xsound_t));
  Xsound.dataReady = false;

  /* 設定從設備 */
  Xsound.slave.address = XSOUND_ADDR;
  Xsound.slave.txBuffer = NULL; // 直接讀取，無需 TX
  Xsound.slave.rxBuffer = rxBuffer;
  Xsound.slave.processBuffer = processBuffer;
  Xsound.slave.bufferSize = XSOUND_BUFFER_SIZE;
  Xsound.slave.txSize = 0; // 0 = 直接讀取，不寫入暫存器
  Xsound.slave.enabled = true;

  /* 清空緩衝區 */
  if (rxBuffer)
    memset(rxBuffer, 0, XSOUND_BUFFER_SIZE);
  memset(processBuffer, 0, XSOUND_BUFFER_SIZE);

  /* 使用資料處理回調函數初始化通用 I2C 模組 */
  I2C_Module_Init(
      &Xsound.i2cModule,
      hi2c,
      &Xsound.slave,
      1, // 僅 1 個從設備
      XSOUND_SAMPLE_PERIOD_MS,
      Xsound_DataProcessCallback);
}

void Xsound_Process(void)
{
  /* 委派給通用 I2C 模組狀態機 */
  I2C_Module_Process(&Xsound.i2cModule);
}

void Xsound_RxCallback(I2C_HandleTypeDef *hi2c)
{
  /* 委派給通用 I2C 模組回調 */
  I2C_Module_RxCallback(&Xsound.i2cModule, hi2c);
}

void Xsound_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
  /* 委派給通用 I2C 模組回調 */
  I2C_Module_ErrorCallback(&Xsound.i2cModule, hi2c);
}

const Xsound_t *Xsound_GetData(void)
{
  return &Xsound;
}

void Xsound_ClearDataReady(void)
{
  Xsound.dataReady = false;
}