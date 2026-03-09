#ifndef GRAYSCALE_H
#define GRAYSCALE_H

#define GRAYSCALE_NUM 6
#include "stdint.h"

void grayscaleInit(void);
void grayscaleCombine(void);
uint16_t getGrayscaleValue(int index);

#endif /* GRAYSCALE_H */