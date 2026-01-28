#ifndef SOCCER
#define SOCCER

#include <stdint.h>
#include <stdbool.h>

#include "ir.h"
#include "mpu6050_dmp.h"

typedef enum {
  NTH_DO = 0,
  ATTACK = 1,
  DEFENCE = 2,
  OUT_OF_BOUND = 3,
  COMPASS_CAR = 4
} State_t;

void updateData();
State_t getState();

#endif