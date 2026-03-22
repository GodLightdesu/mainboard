#include "defense.h"

static PID_Controller_t yawCorrectPID;  // 追球时偏航修正PID
static DefenseState_t defenseState = DEFENSE_STATE_IDLE;

static void boundMove(const ModuleData_t *data, int yawCorr);
static void blockMove(const ModuleData_t *data, int yawCorr);

void DefenseInit(void) {
  // 初始化PID控制器
  PID_Init(&yawCorrectPID, DEF_CORR_kp, DEF_CORR_ki, DEF_CORR_kd,
           INTEGRAL_LIMIT, -BASE_SPEED, BASE_SPEED);
  defenseState = DEFENSE_STATE_IDLE;
}

static DefenseState_t updateDefenseState(const ModuleData_t *data) {
  DefenseState_t currentState = defenseState;
  DefenseState_t nextState = currentState;
  
  // Extract sensor data
  float yaw = data->mpuData->euler.yaw;
  bool ballDetected = (data->irData->ballAngle >= 0);
  bool yawAligned = (-YAW_THRESHOLD <= yaw && yaw <= YAW_THRESHOLD);
  bool outOfBounds = Grayscale_IsOnWhiteLine();

  switch (currentState) {
  case DEFENSE_STATE_IDLE:
    if (outOfBounds) {
      nextState = DEFENSE_STATE_OUT_OF_BOUNDS;
    } else if (ballDetected) {
      nextState = DEFENSE_STATE_BLOCK_SHOT;
    } else if (!yawAligned) {
      nextState = DEFENSE_STATE_ALIGN_YAW;
    }
    break;

  case DEFENSE_STATE_RETURN_GOAL:
    break;

  case DEFENSE_STATE_BLOCK_SHOT:
    if (outOfBounds) {
      nextState = DEFENSE_STATE_OUT_OF_BOUNDS;
    } else if (!ballDetected) {
      if (!yawAligned) {
        nextState = DEFENSE_STATE_ALIGN_YAW;
      } else {
        nextState = DEFENSE_STATE_IDLE;
      }
    }
    break;

  case DEFENSE_STATE_OUT_OF_BOUNDS:
    if (!outOfBounds) {
      if (ballDetected) {
        nextState = DEFENSE_STATE_BLOCK_SHOT;
      } else if (!yawAligned) {
        nextState = DEFENSE_STATE_ALIGN_YAW;
      } else {
        nextState = DEFENSE_STATE_IDLE;
      }
    }
    break;

  case DEFENSE_STATE_ALIGN_YAW:
    if (outOfBounds) {
      nextState = DEFENSE_STATE_OUT_OF_BOUNDS;
    } else if (ballDetected) {
      nextState = DEFENSE_STATE_BLOCK_SHOT;
    } else if (yawAligned) {
      nextState = DEFENSE_STATE_IDLE;
    }
    break;
  }

  defenseState = nextState;
  return defenseState;
}

void DefenseMode(const ModuleData_t *data) {
  // Update state based on sensor data
  DefenseState_t currentState = updateDefenseState(data);

  float yaw = data->mpuData->euler.yaw;
  int yawCorr = (int)PID_Compute(&yawCorrectPID, 0.0f, yaw);

  switch (currentState) {
  case DEFENSE_STATE_IDLE: {
    mtrs_StopAll();
    break;
  }
  case DEFENSE_STATE_BLOCK_SHOT: {
    blockMove(data, yawCorr);
    break;
  }
  case DEFENSE_STATE_RETURN_GOAL: {
    break;
  }
  case DEFENSE_STATE_OUT_OF_BOUNDS: {
    boundMove(data, yawCorr);
    break;
  }
  case DEFENSE_STATE_ALIGN_YAW: {
    compassCar(0, yaw);
    break;
  }
  default: {
    mtrs_StopAll();
    break;
  }
  }
}

DefenseState_t getDefenseState(void) { return defenseState; }

void boundMove(const ModuleData_t *data, int yawCorr) {
  if (Grayscale_IsSensorOnWhiteLine(0)) {
    polarMoveWthCorr(0.0f, 60, yawCorr);
  }
  // back touch white
  else if (Grayscale_IsSensorOnWhiteLine(3)) {
    polarMoveWthCorr(180.0f, 60, yawCorr);
  }
  // back and right back
  else if (Grayscale_IsSensorOnWhiteLine(3) && Grayscale_IsSensorOnWhiteLine(2)) {
    polarMoveWthCorr(135.0f, 50, yawCorr);
  }
  // back and left back
  else if (Grayscale_IsSensorOnWhiteLine(3) && Grayscale_IsSensorOnWhiteLine(4)) {
    polarMoveWthCorr(225.0f, 50, yawCorr);
  }
  // front and right front
  else if (Grayscale_IsSensorOnWhiteLine(0) && Grayscale_IsSensorOnWhiteLine(1)) {
    polarMoveWthCorr(0.0f, 50, yawCorr);
  }
  // front and left front
  else if (Grayscale_IsSensorOnWhiteLine(0) && Grayscale_IsSensorOnWhiteLine(5)) {
    polarMoveWthCorr(0.0f, 50, yawCorr);
  }
}

void blockMove(const ModuleData_t *data, int yawCorr) {
  float ballAngle = data->irData->ballAngle;
  float moveAngle = 0.0f;
  int spd = 60;

  if (ballAngle >= 0 && ballAngle < 180) {
    // Ball on right, move right
    moveAngle = 90.0f;
  } else if (ballAngle >= 180 && ballAngle < 360) {
    // Ball on left, move left
    moveAngle = 270.0f;
  }

  polarMoveWthCorr(moveAngle, spd, yawCorr);
}