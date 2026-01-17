#include "motors.h"
#include "data_uart.h"  /* For UART output */

static Mtr mtrs[MOTOR_COUNT] = {
  { &htim1, TIM_CHANNEL_4, TIM_CHANNEL_3, FAST_DECAY, 0, STOP },  // Motor 0 (Front Left)
  { &htim2, TIM_CHANNEL_4, TIM_CHANNEL_3, FAST_DECAY, 0, STOP },  // Motor 1 (Front Right)
  { &htim3, TIM_CHANNEL_4, TIM_CHANNEL_3, FAST_DECAY, 0, STOP },  // Motor 2 (Rear Left)
  { &htim4, TIM_CHANNEL_4, TIM_CHANNEL_3, FAST_DECAY, 0, STOP }   // Motor 3 (Rear Right)
};

void Mtrs_Init(void) {
  for (uint8_t i = 0; i < MOTOR_COUNT; i++) {
    HAL_TIM_PWM_Start(mtrs[i].htim, mtrs[i].channel_fi);
    HAL_TIM_PWM_Start(mtrs[i].htim, mtrs[i].channel_bi);
  }
}

/* Motor basic control functions */
void mtr_Forward(MtrID_t mtr_id, uint8_t speed) {
  if (mtr_id >= MOTOR_COUNT) {
    return;
  }

  Mtr *mtr = &mtrs[mtr_id];
  speed = (speed > MAX_SPEED) ? MAX_SPEED : speed;
  uint16_t pulse = (speed == 0) ? 0 : spd_Map(speed);
  
  if (mtr->decay_mode == FAST_DECAY) {
    /* Fast Decay: FI=PWM, BI=0 (brake during OFF time) */
    __HAL_TIM_SET_COMPARE(mtr->htim, mtr->channel_fi, pulse);
    __HAL_TIM_SET_COMPARE(mtr->htim, mtr->channel_bi, 0);
  } else { // SLOW_DECAY
    /* Slow Decay: FI=PWM, BI=complementary PWM (recirculate during OFF time)
     * BI inverted to allow freewheeling through low-side FETs */
    __HAL_TIM_SET_COMPARE(mtr->htim, mtr->channel_fi, pulse);
    __HAL_TIM_SET_COMPARE(mtr->htim, mtr->channel_bi, PWM_MAX_VALUE - pulse);
  }
  
  mtr->speed = speed;
  mtr->direction = FORWARD;
}

void mtr_Backward(MtrID_t mtr_id, uint8_t speed) {
  if (mtr_id >= MOTOR_COUNT) {
    return;
  }

  Mtr *mtr = &mtrs[mtr_id];
  speed = (speed > MAX_SPEED) ? MAX_SPEED : speed;
  uint16_t pulse = (speed == 0) ? 0 : spd_Map(speed);

  if (mtr->decay_mode == FAST_DECAY) {
    /* Fast Decay: FI=0, BI=PWM (brake during OFF time) */
    __HAL_TIM_SET_COMPARE(mtr->htim, mtr->channel_fi, 0);
    __HAL_TIM_SET_COMPARE(mtr->htim, mtr->channel_bi, pulse);
  } else { // SLOW_DECAY
    /* Slow Decay: FI=complementary PWM, BI=PWM (recirculate during OFF time)
     * FI inverted to allow freewheeling through low-side FETs */
    __HAL_TIM_SET_COMPARE(mtr->htim, mtr->channel_fi, PWM_MAX_VALUE - pulse);
    __HAL_TIM_SET_COMPARE(mtr->htim, mtr->channel_bi, pulse);
  }
  
  mtr->speed = speed;
  mtr->direction = BACKWARD;
}

void mtr_Brake(MtrID_t mtr_id) {
  if (mtr_id >= MOTOR_COUNT) {
    return;
  }

  Mtr *mtr = &mtrs[mtr_id];

  __HAL_TIM_SET_COMPARE(mtr->htim, mtr->channel_fi, PWM_MAX_VALUE);
  __HAL_TIM_SET_COMPARE(mtr->htim, mtr->channel_bi, PWM_MAX_VALUE);
  
  mtr->speed = 0;
  mtr->direction = BRAKE;
}

void mtr_Stop(MtrID_t mtr_id) {
  if (mtr_id >= MOTOR_COUNT) {
    return;
  }

  Mtr *mtr = &mtrs[mtr_id];

  __HAL_TIM_SET_COMPARE(mtr->htim, mtr->channel_fi, 0);
  __HAL_TIM_SET_COMPARE(mtr->htim, mtr->channel_bi, 0);
  
  mtr->speed = 0;
  mtr->direction = STOP;
}

void mtr_SetDecayMode(MtrID_t mtr_id, DecayMode_t decay_mode) {
  if (mtr_id >= MOTOR_COUNT) {
    return;
  }

  mtrs[mtr_id].decay_mode = decay_mode;
}

/* Motor status and utility functions */
MtrDir_t mtr_GetDirection(MtrID_t mtr_id) {
  if (mtr_id >= MOTOR_COUNT) {
    return STOP;
  }
  return mtrs[mtr_id].direction;
}

uint8_t mtr_GetSpeed(MtrID_t mtr_id) {
  if (mtr_id >= MOTOR_COUNT) {
    return 0;
  }
  return mtrs[mtr_id].speed;
}

/* Linear remapping: 1-100% speed → PWM_STARTUP_MIN to PWM_MAX_VALUE
 * This ensures motor always gets sufficient voltage to overcome static friction
 * Formula: pulse = PWM_STARTUP_MIN + (speed/100) * (PWM_MAX_VALUE - PWM_STARTUP_MIN) */
uint32_t spd_Map(uint8_t speed) {
  if (speed == 0) {
    return 0;
  }
  if (speed > MAX_SPEED) {
    speed = MAX_SPEED;
  }
  return PWM_STARTUP_MIN + ((uint32_t)speed * (PWM_MAX_VALUE - PWM_STARTUP_MIN)) / MAX_SPEED;
}

/* Motor advanced control functions */
void mtrs_Set4Speed(int spd0, int spd1, int spd2, int spd3) {
  const int spds[MOTOR_COUNT] = {spd0, spd1, spd2, spd3};
  
  for (uint8_t i = 0; i < MOTOR_COUNT; i++) {
    int speed = spds[i];
    
    /* Clamp to valid range [-255, 255] */
    if (speed > 255) speed = 255;
    if (speed < -255) speed = -255;
    
    if (speed > 0) {
      mtr_Forward((MtrID_t)i, (uint8_t)speed);
    } else if (speed < 0) {
      mtr_Backward((MtrID_t)i, (uint8_t)(-speed));
    } else {  /* speed == 0 */
      mtr_Stop((MtrID_t)i);
    }
  }
}

void polarMove(float angle_deg, uint8_t speed_percent) {
  /* Validate and clamp speed */
  if (speed_percent > MAX_SPEED) {
    speed_percent = MAX_SPEED;
  }
  
  /* Convert angle to radians */
  const float angle_rad = angle_deg * (M_PI / 180.0f);
  const float phase_offset = M_PI / 4.0f;  /* 45 degrees for mecanum wheels */

  /* Calculate with deadzone and rounding */
  const float DEADZONE = 0.5f; /* Smaller deadzone since we're rounding */

  /* Calculate motor speeds using mecanum kinematics */
  float calcA = speed_percent * sinf(angle_rad + phase_offset);
  float calcB = speed_percent * sinf(angle_rad - phase_offset);
  
  /* Apply deadzone and rounding */
  int spdA = (fabsf(calcA) < DEADZONE) ? 0 : (int)roundf(calcA);
  int spdB = (fabsf(calcB) < DEADZONE) ? 0 : (int)roundf(calcB);
  
  /* Set motor speeds: FL, FR, RL, RR */
  mtrs_Set4Speed(-spdB, spdA, spdB, -spdA);
}

/* ============================================================================
 * Test and Calibration Functions
 * ========================================================================= */

/**
 * @brief Find the minimum PWM value required for motor startup
 * @param mtr_id Motor ID to test
 * @note This function tests PWM values from 10% to 70% in 2% increments
 *       Observe the motor visually to determine when it starts rotating
 *       PWM values are sent via UART for logging
 */
void mtr_FindMinimumStartupPWM(MtrID_t mtr_id) {
  if (mtr_id >= MOTOR_COUNT) {
    return;
  }
  
  char buffer[64];
  snprintf(buffer, sizeof(buffer), "\r\n=== Testing Motor %d ===\r\n", mtr_id);
  dataUart_SendString(buffer);
  
  Mtr *mtr = &mtrs[mtr_id];
  const uint16_t test_start = 400;  // Start at 40% (400/1000)
  const uint16_t test_end = 700;    // End at 70% (700/1000)
  const uint16_t test_step = 20;    // Step by 2% (20/1000)
  
  for (uint16_t pwm = test_start; pwm <= test_end; pwm += test_step) {
    float duty_percent = (pwm * 100.0f) / PWM_MAX_VALUE;
    
    /* Send current test PWM via UART */
    snprintf(buffer, sizeof(buffer), "PWM: %4u (%.1f%%)\r\n", pwm, duty_percent);
    dataUart_SendString(buffer);
    
    /* Apply PWM directly without minimum threshold enforcement */
    if (mtr->decay_mode == FAST_DECAY) {
      __HAL_TIM_SET_COMPARE(mtr->htim, mtr->channel_fi, pwm);
      __HAL_TIM_SET_COMPARE(mtr->htim, mtr->channel_bi, 0);
    } else {
      __HAL_TIM_SET_COMPARE(mtr->htim, mtr->channel_fi, pwm);
      __HAL_TIM_SET_COMPARE(mtr->htim, mtr->channel_bi, PWM_MAX_VALUE - pwm);
    }
    
    HAL_Delay(800);  // Run for 0.8 seconds
    
    /* Stop motor */
    __HAL_TIM_SET_COMPARE(mtr->htim, mtr->channel_fi, 0);
    __HAL_TIM_SET_COMPARE(mtr->htim, mtr->channel_bi, 0);
    
    HAL_Delay(400);  // Rest for 0.4 seconds
  }
  
  dataUart_SendString("=== Test Complete ===\r\n\r\n");
  
  /* Ensure motor is stopped */
  mtr_Stop(mtr_id);
}

/**
 * @brief Test all motors sequentially to find minimum startup PWM
 * @note Tests each motor from front-left to rear-right
 *       Results are sent via UART - total time: ~2 minutes
 */
void mtr_TestAllMotors(void) {
  for (MtrID_t id = MTR0; id < MOTOR_COUNT; id++) {
    HAL_Delay(2000);  // 2 second delay between motors
    mtr_FindMinimumStartupPWM(id);
    HAL_Delay(1000);  // 1 second pause after each motor
  }
}

void mtr_TestAcceleration(MtrID_t mtr_id, uint8_t step, uint8_t max_speed) {
  if (mtr_id >= MOTOR_COUNT || step == 0) {
    return;
  }

  // Forward acceleration and deceleration
  for (uint8_t spd = 0; spd <= max_speed; spd += step) {
    mtr_Forward(mtr_id, spd);
    HAL_Delay(20);
  }
  HAL_Delay(500);
  for (int8_t spd = max_speed; spd >= 0; spd -= step) {
    mtr_Forward(mtr_id, spd);
    HAL_Delay(20);
  }
  HAL_Delay(500);

  // Backward acceleration and deceleration
  for (uint8_t spd = 0; spd <= max_speed; spd += step) {
    mtr_Backward(mtr_id, spd);
    HAL_Delay(20);
  }
  HAL_Delay(500);
  for (int8_t spd = max_speed; spd >= 0; spd -= step) {
    mtr_Backward(mtr_id, spd);
    HAL_Delay(20);
  }
  
  mtr_Stop(mtr_id);
}