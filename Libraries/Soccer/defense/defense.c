#include "defense.h"

static PID_Controller_t yawCorrectPID;  // 追球时偏航修正PID
static DefenseState_t defenseState = DEFENSE_STATE_IDLE;

static void blockMove(const ModuleData_t *data, int yawCorr);
static void returnMove(const ModuleData_t *data, int yawCorr);

void DefenseInit(void) {
  // 初始化PID控制器
  PID_Init(&yawCorrectPID, DEF_CORR_kp, DEF_CORR_ki, DEF_CORR_kd,
           INTEGRAL_LIMIT, -BASE_SPEED, BASE_SPEED);
  defenseState = DEFENSE_STATE_IDLE;
}

void DefenseMode(const ModuleData_t *data) {
  // Extract sensor data
  float yaw = data->mpuData->euler.yaw;
  bool ballDetected = (data->irData->ballAngle >= 0);
  bool yawAligned = (-YAW_THRESHOLD <= yaw && yaw <= YAW_THRESHOLD);
  bool outOfBounds = Grayscale_IsOnWhiteLine();

  int yawCorr = (int)PID_Compute(&yawCorrectPID, 0.0f, yaw);

  // State transitions based on sensor inputs
  if (outOfBounds) {
    boundMove(data, &yawCorrectPID, DEF_BOUND_MOVE_SPEED);
    return;
  } else if (ballDetected && data->irData->maxValue > DEF_BALL_THRESHOLD) {
    blockMove(data, yawCorr);
    return;
  } else if (!yawAligned) {
    compassCar(0, yaw);
    return;
  } else {
    returnMove(data, yawCorr);
    return;
  }
}

DefenseState_t getDefenseState(void) { return defenseState; }

void blockMove(const ModuleData_t *data, int yawCorr) {
  float target_yaw = 0.0f;
  float yaw = data->mpuData->euler.yaw;
  float ballAngle = data->irData->ballAngle;
  // ball in right
  if (ballAngle >= DEF_ANGLE_CORR_MIN_THRESHOLD && ballAngle <= 180) {
    polarMoveWthCorr(90.0f, DEF_BLOCK_SPEED, yawCorr);
  }
   // ball in left
  else if (ballAngle <= DEF_ANGLE_CORR_MAX_THRESHOLD && ballAngle > 180) {
    polarMoveWthCorr(270.0f, DEF_BLOCK_SPEED, yawCorr);
  }
}
void returnMove(const ModuleData_t *data, int yawCorr) {
  bool nearRight = (data->xsoundData->distances[1] < DEF_R_XS_THRESHOLD_CLOSE);
  bool nearLeft = (data->xsoundData->distances[2] < DEF_L_XS_THRESHOLD_CLOSE);
  bool tooFarBack = (data->xsoundData->distances[0] > DEF_XSOUND_THRESHOLD_TOO_FAR);
  bool farBack = (data->xsoundData->distances[0] > DEF_XSOUND_THRESHOLD_FAR && 
                data->xsoundData->distances[0] <= DEF_XSOUND_THRESHOLD_TOO_FAR);
  bool tooClose = (data->xsoundData->distances[0] < DEF_XSOUND_THRESHOLD_CLOSE);
  
  if (tooFarBack) {
    if (nearRight) {
      polarMoveWthCorr(225.0f, DEF_RETURN_SPD, yawCorr);
    }
    else if (nearLeft) {
      polarMoveWthCorr(135.0f, DEF_RETURN_SPD, yawCorr);
    }
    else {
      polarMoveWthCorr(180.0f, DEF_RETURN_SPD, yawCorr);
    }
  } else if (farBack) {
    if (nearRight) {
      polarMoveWthCorr(270.0f, DEF_RETURN_SPD, yawCorr);
    }
    else if (nearLeft) {
      polarMoveWthCorr(90.0f, DEF_RETURN_SPD, yawCorr);
    }
    else {
      polarMoveWthCorr(180.0f, DEF_RETURN_SPD, yawCorr);
    }
  } else if (tooClose) {
    polarMoveWthCorr(0.0f, DEF_RETURN_SPD, yawCorr);
  }
  else {
    mtrs_StopAll();
  }
}