#include "grayscale.h"

#define GRAYSCALE_THRESHOLD_RATIO_PERCENT 50U
// 最小有效讀數差值。若綠地/白線差值很大(如2700)，可提高(100~500)抗雜訊。
// 若設10也常校正失敗，表示感測器讀值無縮放或硬體異常。
#define GRAYSCALE_MIN_VALID_RANGE         10U

/*
0: ADC1_INP19 (A1)  | Left back
1: ADC1_INP4  (A2)  | Front
2: ADC1_INP8  (A3)  | Left front
3: ADC1_INP10 (A4)  | Back
4: ADC1_INP11 (A5)  | Right back
5: ADC3_INP1  (A6)  | Right front
*/

// DMA buffer uses MPU-configured RAM_D2 memory, defined in const.h
static uint16_t* const adcBuffer1 = GRAYSCALE_RXBUF_PTR;

/* Grayscale data structure for external access */
static Grayscale_t grayscaleData = {0};
static GrayscaleLineInfo_t grayscaleLineInfo = {0};

void Grayscale_ReorderValues(const uint16_t *src, uint16_t *dst) {
  if (src == NULL || dst == NULL) { return; }

  //          0         1            2       3        4          5
  // Order: front, right front, right back, back, left back, left front
  dst[0] = src[1];
  dst[1] = src[5];
  dst[2] = src[4];
  dst[3] = src[3];
  dst[4] = src[0];
  dst[5] = src[2];
}

static void Grayscale_ResetScanRange(void) {
  for (int i = 0; i < GRAYSCALE_NUM; i++) {
    grayscaleLineInfo.minValues[i] = 0xFFFFU;
    grayscaleLineInfo.maxValues[i] = 0U;
    grayscaleLineInfo.thresholds[i] = 0U;
    grayscaleLineInfo.strengths[i] = 0U;
  }
  grayscaleLineInfo.onLineMask = 0U;
  grayscaleLineInfo.isOnWhiteLine = false;
}

static void Grayscale_UpdateScanRange(const Grayscale_t* data) {
  for (int i = 0; i < GRAYSCALE_NUM; i++) {
    const uint16_t value = data->values[i];
    if (value < grayscaleLineInfo.minValues[i]) {
      grayscaleLineInfo.minValues[i] = value;
    }
    if (value > grayscaleLineInfo.maxValues[i]) {
      grayscaleLineInfo.maxValues[i] = value;
    }
  }
}

static void Grayscale_EvaluateLineState(const Grayscale_t* data) {
  bool onWhite = false;
  uint8_t onLineMask = 0U;

  for (int i = 0; i < GRAYSCALE_NUM; i++) {
    const uint16_t value = data->values[i];
    const uint16_t minValue = grayscaleLineInfo.minValues[i];
    const uint16_t maxValue = grayscaleLineInfo.maxValues[i];

    uint32_t range = maxValue - minValue;
    if (range > GRAYSCALE_MIN_VALID_RANGE) {
      // 计算当前值在范围内的百分比作为强度 (0 - 100)
      int32_t currentStrength = ((int32_t)value - (int32_t)minValue) * 100 / (int32_t)range;
      
      // 限制强度值在 0 - 100 之间，以防数值漂移超出扫描期间记录的最值
      if (currentStrength < 0) { currentStrength = 0; }
      if (currentStrength > 100) { currentStrength = 100; }
      
      grayscaleLineInfo.strengths[i] = (uint8_t)currentStrength;

      // 判断是否在白线上：根据计算得到的阈值进行判断
      // 此处假设传感器在白线上的读数高于绿地（如果硬件响应相反，即白线读数更低，请改为 value <= thresholds[i]）
      if (value >= grayscaleLineInfo.thresholds[i]) {
        onLineMask |= (1U << i);
        onWhite = true;
      }
    } else {
      grayscaleLineInfo.strengths[i] = 0U;
    }
  }

  grayscaleLineInfo.onLineMask = onLineMask;
  grayscaleLineInfo.isOnWhiteLine = onWhite;
}

void grayscaleInit(void) {
  HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET_LINEARITY ,ADC_SINGLE_ENDED);
  HAL_ADCEx_Calibration_Start(&hadc3,ADC_CALIB_OFFSET_LINEARITY ,ADC_SINGLE_ENDED);

  // ADC1 uses DMA (normal memory)
  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adcBuffer1, GRAYSCALE_NUM - 1);
  
  // ADC3 uses polling mode (no BDMA needed)
  HAL_ADC_Start(&hadc3);

  Grayscale_ResetScanRange();
  grayscaleLineInfo.isScanning = false;
  grayscaleLineInfo.isCalibrated = false;
}

void grayscaleCombine(void) {
  uint16_t rawValues[GRAYSCALE_NUM] = {0};

  // 将ADC1的5个值复制到临时缓冲区
  for (int i = 0; i < GRAYSCALE_NUM - 1; i++) {
    rawValues[i] = adcBuffer1[i];
  }

  // 从ADC3直接读取值（轮询模式）
  HAL_ADC_PollForConversion(&hadc3, 100);  // Wait max 1ms
  rawValues[GRAYSCALE_NUM - 1] = HAL_ADC_GetValue(&hadc3);

  // 按模块约定的顺序重排
  Grayscale_ReorderValues(rawValues, grayscaleData.values);
}

uint16_t getGrayscaleValue(int index) {
  if (index < 0 || index >= GRAYSCALE_NUM) {
    return -1; // 返回-1表示索引无效
  }
  grayscaleCombine(); // 每次读取前先更新grayscaleData数组
  return grayscaleData.values[index];
}

/**
 * @brief Get pointer to grayscale data structure
 * @note Call grayscaleCombine() before accessing to ensure data is updated
 * @return Pointer to grayscale data
 */
const Grayscale_t* Grayscale_GetData(void) {
  grayscaleCombine(); // Update data before returning
  return &grayscaleData;
}

void Grayscale_Process(void) {
  // 每次处理前先更新数据，确保grayscaleData是最新的
  grayscaleCombine();

  if (grayscaleLineInfo.isScanning) {
    Grayscale_UpdateScanRange(&grayscaleData);
  }

  if (grayscaleLineInfo.isCalibrated) {
    Grayscale_EvaluateLineState(&grayscaleData);
  } else {
    grayscaleLineInfo.isOnWhiteLine = false;
    grayscaleLineInfo.onLineMask = 0U;
    for (int i = 0; i < GRAYSCALE_NUM; i++) {
      grayscaleLineInfo.strengths[i] = 0U;
    }
  }
}

void Grayscale_StartScan(void) {
  grayscaleCombine();
  Grayscale_ResetScanRange();

  for (int i = 0; i < GRAYSCALE_NUM; i++) {
    const uint16_t value = grayscaleData.values[i];
    grayscaleLineInfo.minValues[i] = value;
    grayscaleLineInfo.maxValues[i] = value;
  }

  grayscaleLineInfo.isScanning = true;
  grayscaleLineInfo.isCalibrated = false;
}

void Grayscale_StopScan(void) {
  bool hasValidRange = false; // 标志至少有一个传感器范围有效

  // 如果当前不是在扫描状态，直接返回
  if (!grayscaleLineInfo.isScanning) { return; }
  grayscaleLineInfo.isScanning = false;

  for (int i = 0; i < GRAYSCALE_NUM; i++) {
    const uint16_t minValue = grayscaleLineInfo.minValues[i];
    const uint16_t maxValue = grayscaleLineInfo.maxValues[i];
    const uint16_t range = (uint16_t)(maxValue - minValue);
    
    // 检查扫描范围是否有效（排除噪声）
    if (range > (uint16_t)GRAYSCALE_MIN_VALID_RANGE) {
      hasValidRange = true;
      // 根据最大最小值与设置的比例百分比，计算该传感器的白线阈值
      grayscaleLineInfo.thresholds[i] = minValue + (range * GRAYSCALE_THRESHOLD_RATIO_PERCENT) / 100;
    } else {
      // 范围无效，阈值置 0 或保留默认处理
      grayscaleLineInfo.thresholds[i] = 0U;
    }
  }

  grayscaleLineInfo.isCalibrated = hasValidRange;
  Grayscale_Process();
}

bool Grayscale_ApplyCalibration(const uint16_t *minValues,
                                const uint16_t *maxValues,
                                const uint16_t *thresholds) {
  if ((minValues == NULL) || (maxValues == NULL)) {
    return false;
  }

  bool hasValidRange = false;

  for (int i = 0; i < GRAYSCALE_NUM; i++) {
    const uint16_t minValue = minValues[i];
    const uint16_t maxValue = maxValues[i];
    const uint16_t range = (uint16_t)(maxValue - minValue);

    grayscaleLineInfo.minValues[i] = minValue;
    grayscaleLineInfo.maxValues[i] = maxValue;

    if (range > (uint16_t)GRAYSCALE_MIN_VALID_RANGE) {
      hasValidRange = true;
      if (thresholds != NULL) {
        grayscaleLineInfo.thresholds[i] = thresholds[i];
      } else {
        grayscaleLineInfo.thresholds[i] =
            minValue + (range * GRAYSCALE_THRESHOLD_RATIO_PERCENT) / 100;
      }
    } else {
      grayscaleLineInfo.thresholds[i] = 0U;
    }
  }

  grayscaleLineInfo.isScanning = false;
  grayscaleLineInfo.isCalibrated = hasValidRange;
  Grayscale_Process();

  return hasValidRange;
}

void Grayscale_PrintCalibrationData(void) {
  if (!grayscaleLineInfo.isCalibrated) {
    printf("[Grayscale] No valid calibration to print.\r\n");
    return;
  }

  printf("[Grayscale] Copy these values for manual restore after reset:\r\n");

  printf("static const uint16_t GS_MIN[GRAYSCALE_NUM] = {");
  for (int i = 0; i < GRAYSCALE_NUM; i++) {
    printf("%u%s", grayscaleLineInfo.minValues[i], (i == GRAYSCALE_NUM - 1) ? "" : ", ");
  }
  printf("};\r\n");

  printf("static const uint16_t GS_MAX[GRAYSCALE_NUM] = {");
  for (int i = 0; i < GRAYSCALE_NUM; i++) {
    printf("%u%s", grayscaleLineInfo.maxValues[i], (i == GRAYSCALE_NUM - 1) ? "" : ", ");
  }
  printf("};\r\n");

  printf("static const uint16_t GS_THD[GRAYSCALE_NUM] = {");
  for (int i = 0; i < GRAYSCALE_NUM; i++) {
    printf("%u%s", grayscaleLineInfo.thresholds[i], (i == GRAYSCALE_NUM - 1) ? "" : ", ");
  }
  printf("};\r\n");
}

bool Grayscale_IsScanning(void) {
  return grayscaleLineInfo.isScanning;
}

bool Grayscale_IsCalibrated(void) {
  return grayscaleLineInfo.isCalibrated;
}

bool Grayscale_IsOnWhiteLine(void) {
  return grayscaleLineInfo.isOnWhiteLine;
}

bool Grayscale_IsSensorOnWhiteLine(uint8_t index) {
  if (index >= GRAYSCALE_NUM) {
    return false;
  }

  return ((grayscaleLineInfo.onLineMask >> index) & 0x01U) != 0U;
}

uint8_t Grayscale_GetOnLineMask(void) {
  return grayscaleLineInfo.onLineMask;
}

const GrayscaleLineInfo_t* Grayscale_GetLineInfo(void) {
  return &grayscaleLineInfo;
}