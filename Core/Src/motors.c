#include "motors.h"

// Define motors with their respective timers and channels
Mtr motors[MOTOR_COUNT] = {
  {&htim1, TIM_CHANNEL_4, TIM_CHANNEL_3}, // Mtr 0
  {&htim2, TIM_CHANNEL_4, TIM_CHANNEL_3}, // Mtr 1
  {&htim3, TIM_CHANNEL_4, TIM_CHANNEL_3}, // Mtr 2
  {&htim4, TIM_CHANNEL_4, TIM_CHANNEL_3}, // Mtr 3
};

// Store current state for each motor
static MtrDirection_t motor_direction[MOTOR_COUNT] = {
  MOTOR_STOP, MOTOR_STOP, MOTOR_STOP, MOTOR_STOP
};
static float motor_speed[MOTOR_COUNT] = {0.0f, 0.0f, 0.0f, 0.0f};

void Mtrs_Init(void) {
  for (MtrID_t mtrID = 0; mtrID < MOTOR_COUNT; mtrID++) {
    // Start PWM channels
    HAL_TIM_PWM_Start(motors[mtrID].htim, motors[mtrID].channel_fi);
    HAL_TIM_PWM_Start(motors[mtrID].htim, motors[mtrID].channel_bi);

    // Set initial speed to 0 (stop)
    __HAL_TIM_SET_COMPARE(motors[mtrID].htim, motors[mtrID].channel_fi, 0);
    __HAL_TIM_SET_COMPARE(motors[mtrID].htim, motors[mtrID].channel_bi, 0);
  }
}

void Mtr_SetSpeed(MtrID_t mtrID, MtrDirection_t direction, float speed_percent){
  if (mtrID >= MOTOR_COUNT) { return; }

  // Clamp speed to valid range
  if (speed_percent < 0.0f) speed_percent = 0.0f;
  if (speed_percent > 100.0f) speed_percent = 100.0f;

  // Calculate PWM value
  uint32_t pwm_value = (uint32_t)((speed_percent / 100.0f) * PWM_MAX_VALUE);
  
  // Store current state
  motor_direction[mtrID] = direction;
  motor_speed[mtrID] = speed_percent;

  switch (direction) {
  case MOTOR_FORWARD:
    // FI = PWM, BI = 0 (Forward)
    __HAL_TIM_SET_COMPARE(motors[mtrID].htim, motors[mtrID].channel_fi, pwm_value);
    __HAL_TIM_SET_COMPARE(motors[mtrID].htim, motors[mtrID].channel_bi, 0);
    break;
    
  case MOTOR_BACKWARD:
    // FI = 0, BI = PWM (Backward)
    __HAL_TIM_SET_COMPARE(motors[mtrID].htim, motors[mtrID].channel_fi, 0);
    __HAL_TIM_SET_COMPARE(motors[mtrID].htim, motors[mtrID].channel_bi, pwm_value);
    break;
    
  case MOTOR_BRAKE:
    // FI = PWM, BI = PWM (Active brake)
    __HAL_TIM_SET_COMPARE(motors[mtrID].htim, motors[mtrID].channel_fi, pwm_value);
    __HAL_TIM_SET_COMPARE(motors[mtrID].htim, motors[mtrID].channel_bi, pwm_value);
    break;
    
  case MOTOR_STOP:
  default:
    // FI = 0, BI = 0 (Coast stop)
    __HAL_TIM_SET_COMPARE(motors[mtrID].htim, motors[mtrID].channel_fi, 0);
    __HAL_TIM_SET_COMPARE(motors[mtrID].htim, motors[mtrID].channel_bi, 0);
    motor_speed[mtrID] = 0.0f;
    break;
  }
  
}

void Mtr_Stop(MtrID_t mtrID) {
  if (mtrID >= MOTOR_COUNT) return;
  
  // Coast stop: FI = 0, BI = 0
  __HAL_TIM_SET_COMPARE(motors[mtrID].htim, motors[mtrID].channel_fi, 0);
  __HAL_TIM_SET_COMPARE(motors[mtrID].htim, motors[mtrID].channel_bi, 0);
  
  // Update state
  motor_direction[mtrID] = MOTOR_STOP;
  motor_speed[mtrID] = 0.0f;
}

void Mtr_Brake(MtrID_t mtrID) {
  if (mtrID >= MOTOR_COUNT) return;
  
  // Active brake: FI = MAX, BI = MAX
  __HAL_TIM_SET_COMPARE(motors[mtrID].htim, motors[mtrID].channel_fi, PWM_MAX_VALUE);
  __HAL_TIM_SET_COMPARE(motors[mtrID].htim, motors[mtrID].channel_bi, PWM_MAX_VALUE);
  
  // Update state
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
  Mtr_SetSpeed(MTR0, (spd1 > 0) ? MOTOR_FORWARD : (spd1 < 0) ? MOTOR_BACKWARD : MOTOR_STOP, fabsf(spd1));
  Mtr_SetSpeed(MTR1, (spd2 > 0) ? MOTOR_FORWARD : (spd2 < 0) ? MOTOR_BACKWARD : MOTOR_STOP, fabsf(spd2));
  Mtr_SetSpeed(MTR2, (spd3 > 0) ? MOTOR_FORWARD : (spd3 < 0) ? MOTOR_BACKWARD : MOTOR_STOP, fabsf(spd3));
  Mtr_SetSpeed(MTR3, (spd4 > 0) ? MOTOR_FORWARD : (spd4 < 0) ? MOTOR_BACKWARD : MOTOR_STOP, fabsf(spd4));
}

//TODO: implement proper polar movement calculations
void polar_Move(float angle_deg, float speed_percent) {}

// Mtr test
void mtrTest(void) {
  // Smooth acceleration: 5 seconds to reach full speed
  for (float speed = 0.0f; speed <= 100.0f; speed += 1.0f) {
    // Mtr_SetSpeed(MTR0, MOTOR_FORWARD, speed);
    Mtr_SetSpeed(MTR1, MOTOR_FORWARD, speed);
    // Mtr_SetSpeed(MTR2, MOTOR_FORWARD, speed);
    // Mtr_SetSpeed(MTR3, MOTOR_FORWARD, speed);
    HAL_Delay(50);  // 50ms per step = 5 seconds total (101 steps × 50ms)
  }
  
  HAL_Delay(100);

  // Smooth deceleration: 5 seconds
  for (float speed = 100.0f; speed >= 0.0f; speed -= 1.0f) {
    // Mtr_SetSpeed(MTR0, MOTOR_BACKWARD, speed);
    Mtr_SetSpeed(MTR1, MOTOR_BACKWARD, speed);
    // Mtr_SetSpeed(MTR2, MOTOR_BACKWARD, speed);
    // Mtr_SetSpeed(MTR3, MOTOR_BACKWARD, speed);
    HAL_Delay(50);  // 50ms per step = 5 seconds total
  }

  HAL_Delay(500);

  for (float speed = 0.0f; speed <= 100.0f; speed += 1.0f) {
    // Mtr_SetSpeed(MTR0, MOTOR_FORWARD, speed);
    Mtr_SetSpeed(MTR1, MOTOR_FORWARD, speed);
    // Mtr_SetSpeed(MTR2, MOTOR_FORWARD, speed);
    // Mtr_SetSpeed(MTR3, MOTOR_FORWARD, speed);
    HAL_Delay(50);  // 50ms per step = 5 seconds total
  }

  // test smooth braking
  Mtr_Brake(MTR1);

  HAL_Delay(1000);
}