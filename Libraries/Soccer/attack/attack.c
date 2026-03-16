#include "attack.h"
#include "soccer.h"

// PID控制器实例
static PID_Controller_t yawCorrectPID;  // 追球时偏航修正PID
static AttackState_t attackState = ATTACK_STATE_IDLE;

static void chaseMove(const ModuleData_t *data);
static void boundMove(const ModuleData_t *data);

void AttackInit(void) {
  // 初始化PID控制器
  PID_Init(&yawCorrectPID, CORR_kp, CORR_ki, CORR_kd,
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
    
  case ATTACK_STATE_SEARCH_BALL:
    if (ballDetected) {
      // Found ball - switch to chase
      nextState = ATTACK_STATE_CHASE_BALL;
    } else if (!yawAligned) {
      // Need to align yaw before continuing search
      nextState = ATTACK_STATE_ALIGN_YAW;
    }
    break;
    
  case ATTACK_STATE_CHASE_BALL:
    if (!ballDetected) {
      // Lost ball - decide next action
      if (yawAligned) {
        nextState = ATTACK_STATE_SEARCH_BALL;
      } else {
        nextState = ATTACK_STATE_ALIGN_YAW;
      }
    }
    break;
    
  case ATTACK_STATE_ALIGN_YAW:
    if (yawAligned) {
      // Yaw aligned - go search for ball
      nextState = ATTACK_STATE_SEARCH_BALL;
    } else if (ballDetected) {
      // Ball detected during alignment - prioritize chasing
      nextState = ATTACK_STATE_CHASE_BALL;
    }
    break;
    
  case ATTACK_STATE_OUT_OF_BOUNDS:
    // Check if back in bounds
    if (!outOfBounds) {
      // Back in bounds - return to search mode
      nextState = ATTACK_STATE_SEARCH_BALL; // Or align yaw first?
    }
    break;

  case ATTACK_STATE_RETURN_TO_CENTER:
    // This state can be triggered if we want to implement a timeout for losing the ball
    // For now, we won't implement this logic, but it could be added here.
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

  case ATTACK_STATE_RETURN_TO_CENTER: {
    // For now, we won't implement this state, but it could involve moving towards a predefined "center" location.
    mtrs_StopAll();
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
  const float BallBigRadius = 800.0f; // R = Max eye value * tan(theta)
  float camAngle = data->camData->received_angle;
  // -30 to 30 degrees (cam Angle)
  if (camAngle > CAM_ANGLE_MAX) camAngle = CAM_ANGLE_MAX;
  if (camAngle < -CAM_ANGLE_MAX) camAngle = -CAM_ANGLE_MAX;

  int spd = BASE_SPEED + BALL_CHASE_SPEED_BONUS;  
  
  // Ball is not directly in front
  if ((ANGLE_CORR_MIN_THRESHOLD < ballAngle && ballAngle <= ANGLE_HALF_CIRCLE) ||
      (ANGLE_HALF_CIRCLE < ballAngle && ballAngle <= ANGLE_CORR_MAX_THRESHOLD)) {
    float angleDrift = 0.0f;
    arm_atan2_f32(BallBigRadius, POSSIBLE_MAX_BALL_VALUE - maxValue, &angleDrift);
    angleDrift *= RAD_TO_DEG;
    
    moveAngle = (ballAngle <= ANGLE_HALF_CIRCLE) ? (ballAngle + angleDrift) : (ballAngle - angleDrift);
  }
  // Ball is roughly in front
  else if (ANGLE_CORR_MAX_THRESHOLD < ballAngle || ballAngle <= ANGLE_CORR_MIN_THRESHOLD) {
    // If ball is close (large max value), use cam angle for finer control
    if (data->irData->maxValue > BALL_CLOSE_VALUE_THRESHOLD) {
      spd += BALL_CLOSE_SPEED_BONUS;
      // TODO: maybe need pid to slow down when very close to prevent out of bound

      // Use camera angle for correction when ball is close
      target_yaw = camAngle;
      // moveAngle = ballAngle; // Keep using IR angle for movement direction, but use camera for yaw correction
    } else {
      // Ball is far but in front, can use IR angle directly
      // moveAngle = ballAngle; // Already set above
    }
  }

  int yawCorr = (int)PID_Compute(&yawCorrectPID, target_yaw, yaw);
  polarMoveWthCorr(moveAngle, spd, yawCorr);

  #ifdef DEBUG_SOCCER
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
    if (gsInfo->strengths[i] > 50) {
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
    polarMoveWthCorr(moveAngle, BASE_SPEED, yawCorr);

    #ifdef DEBUG_SOCCER
    static uint32_t lastDebug = 0;
    const uint32_t currentTime = HAL_GetTick();
    if (TIME_DIFF(currentTime, lastDebug) >= DEBUG_PRINT_INTERVAL_MS) {
      printf("Bound: move=%.1f vec=(%.1f, %.1f) yawCorr=%d\r\n", moveAngle, vec_x, vec_y, yawCorr);
      lastDebug = currentTime;
    }
    #endif
  }
}
