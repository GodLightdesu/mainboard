#ifndef MOTORS_H
#define MOTORS_H

#include "stdint.h"
#include "math.h"
#include "tim.h"

/*
Motor Layout:
  0(FL)  1(FR)
  2(RL)  3(RR)

H-Bridge Control Logic:
  FI  BI  FO  BO    State
  H   L   H   L     Forward
  L   H   L   H     Backward
  H   H   L   L     Active Brake
  L   L   Open Open Coast Stop

PWM Channel Mapping:
  Motor  FI Channel  BI Channel
    0    TIM1_CH4    TIM1_CH3
    1    TIM2_CH4    TIM2_CH3
    2    TIM3_CH4    TIM3_CH3
    3    TIM4_CH4    TIM4_CH3
*/

#define MOTOR_COUNT 4
#define PWM_MAX_VALUE 1199  /**< PWM period for 100 kHz */

/** Motor configuration structure */
typedef struct {
  TIM_HandleTypeDef *htim;  /**< Timer handle */
  uint32_t channel_fi;      /**< Forward Input PWM channel */
  uint32_t channel_bi;      /**< Backward Input PWM channel */
} Mtr;

/** Motor direction control */
typedef enum {
  MOTOR_STOP = 0,   /**< Coast stop (FI=0, BI=0) */
  MOTOR_FORWARD,    /**< Forward rotation (FI=PWM, BI=0) */
  MOTOR_BACKWARD,   /**< Backward rotation (FI=0, BI=PWM) */
  MOTOR_BRAKE       /**< Active brake (FI=PWM, BI=PWM) */
} MtrDirection_t;

/** Motor identifiers */
typedef enum {
  MTR0 = 0,  /**< Front Left */
  MTR1,      /**< Front Right */
  MTR2,      /**< Rear Left */
  MTR3       /**< Rear Right */
} MtrID_t;

/* Basic motor control */

/**
 * @brief   Initialize all motors
 */
void Mtrs_Init(void);
void Mtr_SetSpeed(MtrID_t motor_id, MtrDirection_t direction, float speed_percent);
void Mtr_Stop(MtrID_t motor_id);
void Mtr_Brake(MtrID_t motor_id);
void Mtrs_BrakeAll(void);

// Advanced movement functions
void mtrs_Set(float spd1, float spd2, float spd3, float spd4);
void polar_Move(float angle_deg, float speed_percent);

// Test function
void mtrTest(void);

#endif // MOTORS_H