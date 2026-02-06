#ifndef MOTORS_H
#define MOTORS_H

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
#define MAX_SPEED 100
#define PWM_MAX_VALUE (uint16_t)(1000 - 1)   /**< 10kHz PWM @ 10MHz timer clock */
#define PWM_STARTUP_MIN 400                  /**< Min PWM for motor startup (~40% duty, ~4.8V @ 12V supply) */

/* Mathematical constants */
#ifndef PI
#define PI 3.14159265358979323846f
#endif


#include "tim.h"
#include "arm_math.h"
#include "stdint.h"
typedef enum {
  FAST_DECAY = 0,
  SLOW_DECAY
} DecayMode_t;

typedef enum {
  STOP = 0,
  FORWARD,
  BACKWARD,
  BRAKE
} MtrDir_t;

typedef enum {
  MTR0 = 0, /**< Front Left */
  MTR1,     /**< Front Right */
  MTR2,     /**< Rear Left */
  MTR3      /**< Rear Right */
} MtrID_t;

typedef struct {
  TIM_HandleTypeDef *htim;      /**< Timer handle */
  const uint32_t channel_fi;    /**< Forward Input PWM channel */
  const uint32_t channel_bi;    /**< Backward Input PWM channel */
  DecayMode_t decay_mode;       /**< Motor decay mode (fast or slow) */
  uint8_t speed;                /**< Current speed percentage (0-100) */
  MtrDir_t direction;           /**< Current motor direction */
} Mtr;

void Mtrs_Init(void);
/* Motor basic control functions */
void mtr_Forward(MtrID_t mtr_id, uint8_t speed);
void mtr_Backward(MtrID_t mtr_id, uint8_t speed);
void mtr_Brake(MtrID_t mtr_id);
void mtr_Stop(MtrID_t mtr_id);
void mtr_SetDecayMode(MtrID_t mtr_id, DecayMode_t decay_mode);

/* Motor status and utility functions */
MtrDir_t mtr_GetDirection(MtrID_t mtr_id);
uint8_t mtr_GetSpeed(MtrID_t mtr_id);
uint32_t spd_Map(uint8_t speed);

/* Motor advanced control functions */
void mtrs_Set4Speed(int spd0, int spd1, int spd2, int spd3);
void polarMove(float angle_deg, uint8_t speed_percent);
void polarMoveWthCorr(float angle_deg, uint8_t speed_percent, int yaw_corr);
void mtrs_StopAll(void);

/* Test and calibration functions */
void mtr_FindMinimumStartupPWM(MtrID_t mtr_id);
void mtr_TestAllMotors(void);
void mtr_TestAcceleration(MtrID_t mtr_id, uint8_t step, uint8_t max_speed);

// void mtrs_setSpeed();

#endif // MOTORS_H