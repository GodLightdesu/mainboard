#include "grayscale.h"

/*
0: ADC1_INP19 (A1)
1: ADC1_INP4  (A2)
2: ADC1_INP8  (A3)
3: ADC1_INP10 (A4)
4: ADC1_INP11 (A5)
5: ADC3_INP1  (A6)
*/

// DMA buffer uses MPU-configured RAM_D2 memory, defined in const.h
static uint16_t* const adcBuffer1 = GRAYSCALE_RXBUF_PTR;

/* Grayscale data structure for external access */
static Grayscale_t grayscaleData = {0};

void grayscaleInit(void) {
  HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET_LINEARITY ,ADC_SINGLE_ENDED);
  HAL_ADCEx_Calibration_Start(&hadc3,ADC_CALIB_OFFSET_LINEARITY ,ADC_SINGLE_ENDED);

  // ADC1 uses DMA (normal memory)
  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adcBuffer1, GRAYSCALE_NUM - 1);
  
  // ADC3 uses polling mode (no BDMA needed)
  // HAL_ADC_Start(&hadc3);
}

void grayscaleCombine(void) {
  // 将ADC1的5个值复制到grayscaleData的前5个位置
  for (int i = 0; i < GRAYSCALE_NUM - 1; i++) {
    grayscaleData.values[i] = adcBuffer1[i];
  }
  
  // 从ADC3直接读取值（轮询模式）
  // HAL_ADC_PollForConversion(&hadc3, 1);  // Wait max 1ms
  // grayscaleData.values[GRAYSCALE_NUM - 1] = HAL_ADC_GetValue(&hadc3);
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