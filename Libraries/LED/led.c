#include "led.h"


static GPIO_TypeDef *const ledPorts[4] = {
  GPIOD, GPIOD, GPIOB, GPIOB
};

static const uint16_t ledPins[4] = {
  LED_1_Pin, LED_2_Pin, LED_3_Pin, LED_4_Pin
};

static bool LED_IsValidId(LED_t ledId) {
  return ledId < LED_COUNT;
}

void LED_Set(LED_t ledId, bool on) {
  if (!LED_IsValidId(ledId)) { return; }

  HAL_GPIO_WritePin(ledPorts[ledId], ledPins[ledId], on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void LED_Toggle(LED_t ledId) {
  if (!LED_IsValidId(ledId)) { return; }

  HAL_GPIO_TogglePin(ledPorts[ledId], ledPins[ledId]);
}

void LED_SetAll(bool on) {
  const GPIO_PinState state = on ? GPIO_PIN_SET : GPIO_PIN_RESET;
  HAL_GPIO_WritePin(GPIOD, LED_1_Pin | LED_2_Pin, state);
  HAL_GPIO_WritePin(GPIOB, LED_3_Pin | LED_4_Pin, state);
}

void LED_HeartbeatTick(LED_t ledId,
                       uint32_t *lastToggleTime,
                       uint32_t currentTime,
                       uint32_t intervalMs) {
  if ((lastToggleTime == NULL) || (intervalMs == 0U)) {
    return;
  }

  if (TIME_DIFF(currentTime, *lastToggleTime) >= intervalMs) {
    LED_Toggle(ledId);
    *lastToggleTime = currentTime;
  }
}
