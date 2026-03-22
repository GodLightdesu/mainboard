#ifndef GRAYSCALE_H
#define GRAYSCALE_H

#define GRAYSCALE_NUM 6

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "const.h"
#include "adc.h"

typedef struct Grayscale_t {
  uint16_t values[GRAYSCALE_NUM]; // Array of 6 grayscale sensor values
} Grayscale_t;

typedef struct GrayscaleLineInfo_t {
  uint16_t minValues[GRAYSCALE_NUM];
  uint16_t maxValues[GRAYSCALE_NUM];
  uint16_t thresholds[GRAYSCALE_NUM];
  uint8_t strengths[GRAYSCALE_NUM];   // 0~100, whiteness strength per sensor
  uint8_t onLineMask;                 // bit i = 1 means sensor i on white line
  bool isScanning;
  bool isCalibrated;
  bool isOnWhiteLine;
} GrayscaleLineInfo_t;

void grayscaleInit(void);
void grayscaleCombine(void);
uint16_t getGrayscaleValue(int index);
const Grayscale_t* Grayscale_GetData(void);

void Grayscale_ReorderValues(const uint16_t *src, uint16_t *dst);

void Grayscale_Process(void);
void Grayscale_StartScan(void);
void Grayscale_StopScan(void);
bool Grayscale_ApplyCalibration(const uint16_t *minValues,
                                const uint16_t *maxValues,
                                const uint16_t *thresholds);
void Grayscale_PrintCalibrationData(void);
bool Grayscale_IsScanning(void);
bool Grayscale_IsCalibrated(void);
bool Grayscale_IsOnWhiteLine(void);
bool Grayscale_IsSensorOnWhiteLine(uint8_t index);
uint8_t Grayscale_GetOnLineMask(void);
const GrayscaleLineInfo_t* Grayscale_GetLineInfo(void);

#endif /* GRAYSCALE_H */