#include "motors.h"

/* Motor configuration array - indexed by MtrID_t */
const Mtr motors[MOTOR_COUNT] = {
  {&htim1, TIM_CHANNEL_4, TIM_CHANNEL_3}, /* MTR0 - Front Left */
  {&htim2, TIM_CHANNEL_4, TIM_CHANNEL_3}, /* MTR1 - Front Right */
  {&htim3, TIM_CHANNEL_4, TIM_CHANNEL_3}, /* MTR2 - Rear Left */
  {&htim4, TIM_CHANNEL_4, TIM_CHANNEL_3}, /* MTR3 - Rear Right */
};

/* Private state tracking - volatile for potential ISR access */
static volatile MtrDirection_t motor_direction[MOTOR_COUNT] = {
  MOTOR_STOP, MOTOR_STOP, MOTOR_STOP, MOTOR_STOP
};
static volatile float motor_speed[MOTOR_COUNT] = {0.0f, 0.0f, 0.0f, 0.0f};

void Mtrs_Init(void) {
  for (MtrID_t mtrID = 0; mtrID < MOTOR_COUNT; mtrID++) {
    /* Start PWM channels for H-bridge control */
    HAL_TIM_PWM_Start(motors[mtrID].htim, motors[mtrID].channel_fi);
    HAL_TIM_PWM_Start(motors[mtrID].htim, motors[mtrID].channel_bi);

    /* Set initial speed to 0 (coast stop) */
    __HAL_TIM_SET_COMPARE(motors[mtrID].htim, motors[mtrID].channel_fi, 0);
    __HAL_TIM_SET_COMPARE(motors[mtrID].htim, motors[mtrID].channel_bi, 0);
    
    /* Initialize state tracking */
    motor_direction[mtrID] = MOTOR_STOP;
    motor_speed[mtrID] = 0.0f;
  }
}

// Set motor speed with direction (positive speed = forward)
void Mtr_SetSpeed(MtrID_t mtrID, MtrDirection_t direction, float speed_percent) {
  /* Validate motor ID */
  if (mtrID >= MOTOR_COUNT) { 
    return; 
  }

  /* Clamp speed to valid range [0, 100] */
  if (speed_percent < 0.0f) {
    speed_percent = 0.0f;
  } else if (speed_percent > 100.0f) {
    speed_percent = 100.0f;
  }

  /* Calculate PWM duty cycle */
  const uint32_t pwm_value = (uint32_t)((speed_percent / 100.0f) * PWM_MAX_VALUE);
  
  /* Update state tracking */
  motor_direction[mtrID] = direction;
  motor_speed[mtrID] = speed_percent;

  /* Set H-bridge control signals based on direction */
  switch (direction) {
  case MOTOR_FORWARD:
    /* FI=PWM, BI=0 -> Forward rotation */
    __HAL_TIM_SET_COMPARE(motors[mtrID].htim, motors[mtrID].channel_fi, pwm_value);
    __HAL_TIM_SET_COMPARE(motors[mtrID].htim, motors[mtrID].channel_bi, 0);
    break;
    
  case MOTOR_BACKWARD:
    /* FI=0, BI=PWM -> Backward rotation */
    __HAL_TIM_SET_COMPARE(motors[mtrID].htim, motors[mtrID].channel_fi, 0);
    __HAL_TIM_SET_COMPARE(motors[mtrID].htim, motors[mtrID].channel_bi, pwm_value);
    break;
    
  case MOTOR_BRAKE:
    /* FI=PWM, BI=PWM -> Active braking */
    __HAL_TIM_SET_COMPARE(motors[mtrID].htim, motors[mtrID].channel_fi, pwm_value);
    __HAL_TIM_SET_COMPARE(motors[mtrID].htim, motors[mtrID].channel_bi, pwm_value);
    break;
    
  case MOTOR_STOP:
  default:
    /* FI=0, BI=0 -> Coast stop */
    __HAL_TIM_SET_COMPARE(motors[mtrID].htim, motors[mtrID].channel_fi, 0);
    __HAL_TIM_SET_COMPARE(motors[mtrID].htim, motors[mtrID].channel_bi, 0);
    motor_speed[mtrID] = 0.0f;
    break;
  }
}

void Mtr_Stop(MtrID_t mtrID) {
  if (mtrID >= MOTOR_COUNT) return;
  
  /* Coast stop: FI = 0, BI = 0 */
  __HAL_TIM_SET_COMPARE(motors[mtrID].htim, motors[mtrID].channel_fi, 0);
  __HAL_TIM_SET_COMPARE(motors[mtrID].htim, motors[mtrID].channel_bi, 0);
  
  /* Update state */
  motor_direction[mtrID] = MOTOR_STOP;
  motor_speed[mtrID] = 0.0f;
}

void Mtr_Brake(MtrID_t mtrID) {
  if (mtrID >= MOTOR_COUNT) return;
  
  /* Active brake: FI = MAX, BI = MAX */
  __HAL_TIM_SET_COMPARE(motors[mtrID].htim, motors[mtrID].channel_fi, PWM_MAX_VALUE);
  __HAL_TIM_SET_COMPARE(motors[mtrID].htim, motors[mtrID].channel_bi, PWM_MAX_VALUE);
  
  /* Update state */
  motor_direction[mtrID] = MOTOR_BRAKE;
  motor_speed[mtrID] = 100.0f;
}

void Mtrs_BrakeAll(void) {
  for (MtrID_t mtrID = 0; mtrID < MOTOR_COUNT; mtrID++) {
    Mtr_Brake(mtrID);
  }
}

// advanced functions
void mtrs_Set(float spd1, float spd2, float spd3, float spd4) {
  /* Helper to determine direction from signed speed */
  const float speeds[MOTOR_COUNT] = {spd1, spd2, spd3, spd4};
  
  for (MtrID_t i = 0; i < MOTOR_COUNT; i++) {
    MtrDirection_t dir;
    if (speeds[i] > 0) {
      dir = MOTOR_FORWARD;
    } else if (speeds[i] < 0) {
      dir = MOTOR_BACKWARD;
    } else {
      dir = MOTOR_STOP;
    }
    Mtr_SetSpeed(i, dir, fabsf(speeds[i]));
  }
}

// Mecanum wheel polar movement (angle in degrees, speed in percent)
void polar_Move(float angle_deg, float speed_percent) {
  /* Convert to radians and normalize */
  const float angle_rad = angle_deg * (M_PI / 180.0f);
  const float normalized_speed = speed_percent / 100.0f;
  
  /* Phase offset for mecanum kinematics */
  const float phase_offset = M_PI / 4.0f;
  
  /* Calculate individual motor speeds using mecanum kinematic equations */
  const float spd0 = normalized_speed * sinf(angle_rad + phase_offset) * 100.0f;
  const float spd1 = normalized_speed * sinf(angle_rad - phase_offset) * 100.0f;
  const float spd2 = normalized_speed * sinf(angle_rad - phase_offset) * 100.0f;
  const float spd3 = normalized_speed * sinf(angle_rad + phase_offset) * 100.0f;
  
  /* Apply to motors (with sign corrections for motor mounting) */
  mtrs_Set(-spd1, spd0, spd2, -spd3);
}

// Motor test function - smooth acceleration/deceleration
void mtrTest(void) {
  const float accel_step = 1.0f;       /* 1% per step */
  const uint32_t step_delay = 50;      /* 50ms per step = 5 seconds total */
  
  /* Smooth acceleration forward */
  for (float speed = 0.0f; speed <= 100.0f; speed += accel_step) {
    Mtr_SetSpeed(MTR1, MOTOR_FORWARD, speed);
    HAL_Delay(step_delay);
  }
  
  HAL_Delay(500);

  /* Smooth deceleration backward */
  for (float speed = 100.0f; speed >= 0.0f; speed -= accel_step) {
    Mtr_SetSpeed(MTR1, MOTOR_BACKWARD, speed);
    HAL_Delay(step_delay);
  }

  HAL_Delay(200);

  /* Test active braking */
  for (float speed = 0.0f; speed <= 100.0f; speed += accel_step) {
    Mtr_SetSpeed(MTR1, MOTOR_FORWARD, speed);
    HAL_Delay(step_delay);
  }
  HAL_Delay(500);
  Mtr_Brake(MTR1);
  HAL_Delay(500);
  Mtr_Stop(MTR1);
}