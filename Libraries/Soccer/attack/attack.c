#include "attack.h"

// PID控制器实例
static PID_Controller_t yawCorrectPID;  // 追球时偏航修正PID
static AttackState_t attackState = ATTACK_STATE_IDLE;

static void chaseMove(const ModuleData_t *data);
static void boundMove(const ModuleData_t *data);

void AttackInit(void) {
  // 初始化PID控制器
  PID_Init(&yawCorrectPID, ATK_CORR_kp, ATK_CORR_ki, ATK_CORR_kd,
           INTEGRAL_LIMIT, -BASE_SPEED, BASE_SPEED);
  attackState = ATTACK_STATE_IDLE;
}

AttackState_t getAttackState(void) {
  return attackState;
}

static AttackState_t updateAttackState(const ModuleData_t *data) {
  AttackState_t currentState = attackState;
  AttackState_t nextState = currentState;
  
  // Extract sensor data
  float yaw = data->mpuData->euler.yaw;
  bool ballDetected = (data->irData->ballAngle >= 0);
  bool yawAligned = (-YAW_THRESHOLD <= yaw && yaw <= YAW_THRESHOLD);
  bool outOfBounds = Grayscale_IsOnWhiteLine();
  
  // Check for out of bounds condition first - highest priority
  if (outOfBounds && currentState != ATTACK_STATE_OUT_OF_BOUNDS) {
    nextState = ATTACK_STATE_OUT_OF_BOUNDS;
    attackState = nextState;
    return nextState;
  }
  
  // State machine transitions based on current state
  switch (currentState) {
  case ATTACK_STATE_IDLE:
    // Transition to SEARCH if mode is active (handled by caller setting state or initial state)
    // If we rely on SoccerMode to switch us, we should start in SEARCH if active.
    // However, if we just init, we might be IDLE.
    // Let's assume if we are called, we should be doing something.
    if (Soccer_IsSystemStarted()) {
      nextState = ATTACK_STATE_SEARCH_BALL;
    }
    break;

  // return to center to search for ball
  case ATTACK_STATE_SEARCH_BALL:
    if (!yawAligned) {
      nextState = ATTACK_STATE_ALIGN_YAW;
    } else if (ballDetected) {
      nextState = ATTACK_STATE_CHASE_BALL;
    }
    // keep searching if not aligned and no ball detected
    break;

  case ATTACK_STATE_CHASE_BALL:
    if (!ballDetected) {
      nextState = ATTACK_STATE_SEARCH_BALL;
    } else if (!yawAligned) {
      nextState = ATTACK_STATE_ALIGN_YAW;
    }
    // keep chasing if ball is detected and yaw is aligned
    break;

  case ATTACK_STATE_ALIGN_YAW:
    if (yawAligned) {
      if (ballDetected) {
        nextState = ATTACK_STATE_CHASE_BALL;
      } else {
        nextState = ATTACK_STATE_SEARCH_BALL;
      }
    }
    // keep correcting if not aligned
    break;

  case ATTACK_STATE_OUT_OF_BOUNDS:
    if (!outOfBounds) {
      // If we just got back in bounds, decide where to go based on ball detection and yaw
      if (ballDetected) {
        nextState = ATTACK_STATE_CHASE_BALL;
      } else if (!yawAligned) {
        nextState = ATTACK_STATE_ALIGN_YAW;
      } else {
        nextState = ATTACK_STATE_SEARCH_BALL;
      }
    }
    // keep doing boundary correction if still out of bounds
    break;
  
  default:
    // Invalid state - reset to IDLE for safety
    nextState = ATTACK_STATE_IDLE;
    break;
  }
  
  attackState = nextState;
  return nextState;
}

void AttackMode(const ModuleData_t *data) {
  // Update state based on sensor data
  AttackState_t currentState = updateAttackState(data);

  // State machine execution
  switch (currentState) {
  case ATTACK_STATE_IDLE:
    mtrs_StopAll();
    break;

  case ATTACK_STATE_SEARCH_BALL: {
    mtrs_StopAll();
    break;
  }

  case ATTACK_STATE_CHASE_BALL: {
    chaseMove(data);
    break;
  }

  case ATTACK_STATE_ALIGN_YAW: {
    compassCar(0, data->mpuData->euler.yaw);
    break;
  }
  
  case ATTACK_STATE_OUT_OF_BOUNDS: {
    boundMove(data);
    break;
  }
    
  default:
    mtrs_StopAll();
    break;
  }
}

// Chase ball with angle correction
static void chaseMove(const ModuleData_t *data) {
  float target_yaw = 0.0f;
  float yaw = data->mpuData->euler.yaw;
  float ballAngle = data->irData->ballAngle;
  float moveAngle = ballAngle;

  const float maxValue = (float)data->irData->maxValue;
  float camAngle = data->camData->received_angle;
  // -30 to 30 degrees (cam Angle)
  if (camAngle > CAM_ANGLE_MAX) camAngle = CAM_ANGLE_MAX;
  if (camAngle < -CAM_ANGLE_MAX) camAngle = -CAM_ANGLE_MAX;

  int spd = BASE_SPEED + BALL_CHASE_SPEED_BONUS;  
  
  // Ball is not directly in front
  if ((ANGLE_CORR_MIN_THRESHOLD < ballAngle && ballAngle <= ANGLE_HALF_CIRCLE) ||
      (ANGLE_HALF_CIRCLE < ballAngle && ballAngle <= ANGLE_CORR_MAX_THRESHOLD)) {
    float angleDrift = 0.0f;
    arm_atan2_f32(BALL_BIG_RADIUS, POSSIBLE_MAX_BALL_VALUE - maxValue, &angleDrift);
    angleDrift *= RAD_TO_DEG;
    
    moveAngle = (ballAngle <= ANGLE_HALF_CIRCLE) ? (ballAngle + angleDrift) : (ballAngle - angleDrift);
  }
  // Ball is roughly in front
  else if (ANGLE_CORR_MAX_THRESHOLD < ballAngle || ballAngle <= ANGLE_CORR_MIN_THRESHOLD) {
    // TODO: maybe need pid to slow down when very close to prevent out of bound
    spd += BALL_CLOSE_SPEED_BONUS;
    
    // If ball is close (large max value), use cam angle for finer control
    if (data->irData->maxValue > BALL_CLOSE_VALUE_THRESHOLD) {
      moveAngle = camAngle; // Directly use camera angle for movement direction
      target_yaw = camAngle;  // Use camera angle for correction when ball is close

      // Increase gains for more aggressive correction when close
      // max angle (40) -> 0.5 * 40 = 20 yaw correction
      // mid angle (20) -> 0.5 * 20 = 10 yaw correction
      // which are more than enough to quickly align to the ball
      PID_SetGains(&yawCorrectPID, 0.5f, ATK_CORR_ki, ATK_CORR_kd);
    } else {
      target_yaw = 0.0f; // When ball is in front but not close, just try to go straight and let PID handle minor corrections
    }
  }

  int yawCorr = (int)PID_Compute(&yawCorrectPID, target_yaw, yaw);
  polarMoveWthCorr(moveAngle, spd, yawCorr);
  PID_SetGains(&yawCorrectPID, ATK_CORR_kp, ATK_CORR_ki, ATK_CORR_kd); // Reset to default gains for next cycle
  
  #ifdef DEBUG_ATTACK
  static uint32_t lastDebug = 0;
  const uint32_t currentTime = HAL_GetTick();
  if (TIME_DIFF(currentTime, lastDebug) >= DEBUG_PRINT_INTERVAL_MS) {
    printf("Chase: move=%.1f spd=%d yawCorr=%d camAngle=%.1f target_yaw=%.1f\r\n", moveAngle, spd, yawCorr, camAngle, target_yaw);
    lastDebug = currentTime;
  }
  #endif
}

// Boundary correction movement based on grayscale sensors
static void boundMove(const ModuleData_t *data) {
  float yaw = data->mpuData->euler.yaw;
  const GrayscaleLineInfo_t *gsInfo = Grayscale_GetLineInfo();
  float vec_x = 0.0f;
  float vec_y = 0.0f;

  for (int i = 0; i < GRAYSCALE_NUM; i++) {
    if (gsInfo->strengths[i] > GS_STR_MIN_THRESHOLD) {
      float vector_angle = (i * (360.0f / GRAYSCALE_NUM)) + 180.0f;
      float rad = vector_angle * DEG_TO_RAD;
      vec_x += (float)gsInfo->strengths[i] * arm_cos_f32(rad);
      vec_y += (float)gsInfo->strengths[i] * arm_sin_f32(rad);
    }
  }

  if (vec_x == 0.0f && vec_y == 0.0f) {
    mtrs_StopAll();
  } else {
    float moveAngle;
    arm_atan2_f32(vec_y, vec_x, &moveAngle);
    moveAngle *= RAD_TO_DEG;
    if (moveAngle < 0.0f) moveAngle += 360.0f;
    
    int yawCorr = (int)PID_Compute(&yawCorrectPID, 0.0f, yaw);
    polarMoveWthCorr(moveAngle, BOUND_MOVE_SPEED, yawCorr);

    #ifdef DEBUG_ATTACK
    static uint32_t lastDebug = 0;
    const uint32_t currentTime = HAL_GetTick();
    if (TIME_DIFF(currentTime, lastDebug) >= DEBUG_PRINT_INTERVAL_MS) {
      printf("Bound: move=%.1f vec=(%.1f, %.1f) yawCorr=%d\r\n", moveAngle, vec_x, vec_y, yawCorr);
      lastDebug = currentTime;
    }
    #endif
  }
}
