#ifndef LED_H
#define LED_H

#include <stdbool.h>
#include <stdint.h>

#include "gpio.h"
#include "const.h"

typedef enum {
  LED_1 = 0,
  LED_2 = 1,
  LED_3 = 2,
  LED_4 = 3,
  LED_COUNT
} LED_t;

void LED_SetAll(bool on);
void LED_Set(LED_t ledId, bool on);

void LED_Toggle(LED_t ledId);

void LED_HeartbeatTick(LED_t ledId,
                       uint32_t *lastToggleTime,
                       uint32_t currentTime,
                       uint32_t intervalMs);

#endif /* LED_H */
