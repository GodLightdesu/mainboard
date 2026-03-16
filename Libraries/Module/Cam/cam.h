#ifndef CAM_H
#define CAM_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "usart.h"

// States for the UART parser
typedef enum { WAIT_HEADER, WAIT_DATA, WAIT_FOOTER } parser_state_t;

typedef struct Cam_t {
  bool dataReady;
  float received_angle;
  uint8_t rx_buffer[4];
  uint8_t rx_index;
  parser_state_t state;
} Cam_t;

void cam_init(UART_HandleTypeDef *huart);
bool cam_data_ready(void);
float cam_get_angle(void);
const Cam_t* Cam_GetData(void);

#endif // CAM_H