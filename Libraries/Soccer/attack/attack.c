#include "attack.h"
#include "motors.h"

// PID控制器实例
static PID_Controller_t yawCorrectPID;  // 追球时偏航修正PID
static AttackState_t attackState = ATTACK_STATE_IDLE;

static void chaseMove(const ModuleData_t *data);
static void ATK_returnMove(const ModuleData_t *data);

void AttackInit(void) {
  // 初始化PID控制器
  PID_Init(&yawCorrectPID, ATK_CORR_kp, ATK_CORR_ki, ATK_CORR_kd,
           INTEGRAL_LIMIT, -BASE_SPEED, BASE_SPEED);
  attackState = ATTACK_STATE_IDLE;
}

AttackState_t getAttackState(void) {
  return attackState;
}

void AttackMode(const ModuleData_t *data) {
  float yaw = data->mpuData->euler.yaw;
  bool ballDetected = (data->irData->ballAngle >= 0);
  bool yawAligned = (-YAW_THRESHOLD <= yaw && yaw <= YAW_THRESHOLD);
  bool outOfBounds = Grayscale_IsOnWhiteLine();

  if (outOfBounds) {
    boundMove(data, &yawCorrectPID, ATK_BOUND_MOVE_SPEED);
    return;
  } else if (ballDetected) {
    chaseMove(data);
    return;
  } else if (!yawAligned) {
    compassCar(0, yaw);
    return;
  } else {
    ATK_returnMove(data);
    return;
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
    int yawCorr = (int)PID_Compute(&yawCorrectPID, target_yaw, yaw);
    polarMoveWthCorr(moveAngle, spd, yawCorr);
    PID_SetGains(&yawCorrectPID, ATK_CORR_kp, ATK_CORR_ki, ATK_CORR_kd); // Reset to default gains for next cycle
  }
  // Ball is roughly in front
  else if (ANGLE_CORR_MAX_THRESHOLD < ballAngle || ballAngle <= ANGLE_CORR_MIN_THRESHOLD) {
    // TODO: maybe need pid to slow down when very close to prevent out of bounds
    spd += BALL_CLOSE_SPEED_BONUS;
    
    // If ball is close (large max value), use cam angle for finer control
    if (data->irData->maxValue > BALL_CLOSE_VALUE_THRESHOLD) {
      // moveAngle = camAngle; // Directly use camera angle for movement direction
      target_yaw = camAngle;  // Use camera angle for correction when ball is close

      // Increase gains for more aggressive correction when close
      // max angle (40) -> 0.7 * 40 = 28 yaw correction
      // mid angle (20) -> 0.7 * 20 = 14 yaw correction
      // which are more than enough to quickly align to the ball
      PID_SetGains(&yawCorrectPID, 1.0f, ATK_CORR_ki, ATK_CORR_kd);
    }
    // for test
    // mtrs_StopAll();
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

void ATK_returnMove(const ModuleData_t *data) {
  float yaw = data->mpuData->euler.yaw;
  int yawCorr = (int)PID_Compute(&yawCorrectPID, 0.0f, yaw);
  bool nearRight = (data->xsoundData->distances[1] < 70);
  bool nearLeft = (data->xsoundData->distances[2] < 70);
  bool tooFarBack = (data->xsoundData->distances[0] > 90);
  bool tooClose = (data->xsoundData->distances[0] < 30);
  
  if (tooFarBack) {
    if (nearRight) {
      polarMoveWthCorr(225.0f, 65, yawCorr);
    }
    else if (nearLeft) {
      polarMoveWthCorr(135.0f, 65, yawCorr);
    }
    else {
      polarMoveWthCorr(180.0f, 65, yawCorr);
    }
  } else if (tooClose) {
    polarMoveWthCorr(0.0f, 65, yawCorr);
  }
  else {
    mtrs_StopAll();
  }
}
