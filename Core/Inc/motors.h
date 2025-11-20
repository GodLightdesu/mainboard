#ifndef MOTORS_H
#define MOTORS_H

#include "stdint.h"
#include "tim.h"

/*
Mtr Layout:
  0  1
  2  3

H-Bridge Control:
FI  BI  FO  BO    State
H   L   H   L     Forward
L   H   L   H     Backward
H   H   L   L     Brake
L   L   Open Open Stop

PWM Mapping:
Mtr  FI Channel  BI Channel
  0    TIM1_CH4   TIM1_CH3
  1    TIM2_CH4   TIM2_CH3
  2    TIM3_CH4   TIM3_CH3
  3    TIM4_CH4   TIM4_CH3
*/

#define MOTOR_COUNT 4
#define PWM_MAX_VALUE 2399  // Period value for 100 kHz PWM

typedef struct {
  TIM_HandleTypeDef *htim;
  uint32_t channel_fi; // Forward Input channel
  uint32_t channel_bi; // Backward Input channel
} Mtr;

// Mtr direction enum
typedef enum {
  MOTOR_STOP = 0, // L L - Coast stop
  MOTOR_FORWARD,  // H L - Forward
  MOTOR_BACKWARD, // L H - Backward
  MOTOR_BRAKE     // H H - Active brake
} MtrDirection_t;

typedef enum {
  MTR0 = 0,
  MTR1,
  MTR2,
  MTR3
} MtrID_t;

// Function prototypes
void Mtrs_Init(void);
void Mtr_SetSpeed(MtrID_t motor_id, MtrDirection_t direction, float speed_percent);
void Mtr_Stop(MtrID_t motor_id);
void Mtr_Brake(MtrID_t motor_id);
void Mtrs_BrakeAll(void);

// advanced functions
void polar_Move(float angle_deg, float speed_percent);

#endif // MOTORS_H