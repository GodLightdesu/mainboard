#ifndef GRAYSCALE_H
#define GRAYSCALE_H

#define GRAYSCALE_NUM 6

#include <stdint.h>

#include "const.h"
#include "adc.h"

typedef struct Grayscale_t {
  uint16_t values[GRAYSCALE_NUM];  // Array of 6 grayscale sensor values
} Grayscale_t;

void grayscaleInit(void);
void grayscaleCombine(void);
uint16_t getGrayscaleValue(int index);
const Grayscale_t* Grayscale_GetData(void);

#endif /* GRAYSCALE_H */