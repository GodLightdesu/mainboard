#include "soccer.h"
#include "button.h"
#include "MPU6050DMP.h"

// 系统状态变量
static volatile bool systemStarted = false;
static volatile bool emergencyStop = false;
static State_t state = STATE_IDLE;
static uint32_t systemStartTime = 0;

// 按钮控制回调函数
static void SoccerButtonControl(uint8_t btn_index, BtnEvent_t event) {
  switch (btn_index) {
  case 0: // 按钮1 - 启动
    if (event == BTN_EVENT_LONG_PRESS_START) {
      // 长按启动系统
      if (!systemStarted) {
        systemStarted = true;
        emergencyStop = false;
        state = STATE_SEARCH_BALL;
        systemStartTime = HAL_GetTick();
      }
    } break;
  case 1: // 按钮2 - 紧急停止
    if (event == BTN_EVENT_CLICK) {
      // 单击紧急停止
      emergencyStop = true;
      systemStarted = false;
      state = STATE_IDLE;
      mtrs_StopAll();
    } break;
  case 2: // 按钮3 - Reset Yaw
    if (event == BTN_EVENT_CLICK) {
      // 单击重置陀螺仪偏航角（如果系统已启动）
      // Note: The new MPU6050DMP API doesn't support ResetYaw function
      // You would need to implement this in the DMP library if needed
      if (systemStarted) {
        // MPU6050_DMP_ResetYaw(); // Not available in new API
      }
    } break;
  case 3: // 按钮4 - Reset All
    if (event == BTN_EVENT_LONG_PRESS_START) {
      
    } break;
  }
}

void SoccerInit(void) {
  // 设置按钮回调
  Button_SetSystemControlCallback(0, SoccerButtonControl);  // 按钮1：启动
  Button_SetSystemControlCallback(1, SoccerButtonControl);  // 按钮2：紧急停止
  Button_SetSystemControlCallback(2, SoccerButtonControl);  // 按钮3：reset yaw
  Button_SetSystemControlCallback(3, SoccerButtonControl);  // 按钮4：reset all
}

void updateData() {
  // 检查紧急停止 or 系统未启动，no read data
  if (emergencyStop || !systemStarted) {
    return;
  }

  // 輪流讀取 MPU6050 和 IR，避免 I2C 匯流排衝突
  static bool readMPU = true;
  
  if (HAL_I2C_GetState(&hi2c3) == HAL_I2C_STATE_READY) {
    if (readMPU) {
      /* Update MPU6050 DMP data (blocking) */
      int result = MPU6050DMP_updateData();
      if (result != 0) {
        // Update failed - handle I2C errors in MPU6050 library
        MPU6050DMP_HandleI2CError();
      }
      // Always switch to IR next time, regardless of success/failure
      readMPU = false;
    } else {
      /* Process IR state machine (non-blocking DMA) */
      IR_Process();
      readMPU = true;
    }
  }
}

void soccer_ProcessData(const ModuleData_t *data) {
  // 检查紧急停止 or 系统未启动，直接返回
  if (emergencyStop || !systemStarted) {
    mtrs_StopAll();
    return;
  }

  const float target_yaw = 0.0f;
  updateState(data);
  switch (getState()) {
  case STATE_IDLE: {
    mtrs_StopAll();
    return;
  }
  case STATE_SEARCH_BALL: {
    mtrs_StopAll();
    return;
  }
  case STATE_CHASE_BALL: {
    float ballAngle = data->irData->ballAngle;
    // Only process if both IR and MPU data are ready
    if (IR_IsDataReady() && MPU6050_DMP_IsDataReady() && ballAngle >= 0) {
      float yaw = data->mpuData->euler.yaw; // Get current yaw from DMP data
      // ver 2: use yaw correction PID
      int yawCorr = SoccerPIDCompCorr(CORR_KD, yaw, target_yaw);
      int spd = BASE_SPEED + BALL_CHASE_SPEED_BONUS;

      polarMoveWthCorr(ballAngle, (uint8_t)spd, yawCorr);
      // polarMove(ballAngle, BASE_SPEED + BALL_CHASE_SPEED_BONUS);
    } else {
      mtrs_StopAll();
    }
    break;
  }
  case STATE_ALIGN_YAW: {
    // Only adjust yaw if MPU data is ready
    if (MPU6050_DMP_IsDataReady()) {
      float yaw = data->mpuData->euler.yaw;
      // ver 1: use PID to adjust yaw
      SoccerPIDCompCar(YAW_PID_KD, yaw, target_yaw);
    } else {
      mtrs_StopAll();
    }
    break;
  }
  case STATE_OUT_OF_BOUNDS:{
    mtrs_StopAll();
    return;
  }
  }
}

void SoccerPIDCompCar(float kd, float yaw, float target_yaw) {
  // Placeholder for PID control logic
  // Implement PID control based on yaw angle
  float error = target_yaw - yaw;     // >0: need turn right, <0: need turn left
  int spd = (int)(kd * error);

  // set a minimum and maximum speed threshold to ensure the robot can move
  if (spd > 0) {
    spd = spd < BASE_SPEED ? BASE_SPEED : spd; // Minimum speed when turning right
    spd = spd > REAL_MAX_SPEED ? REAL_MAX_SPEED : spd; // Max speed limit
  } else if (spd < 0) {
    spd = spd > -BASE_SPEED ? -BASE_SPEED : spd; // Minimum speed when turning left
    spd = spd < -REAL_MAX_SPEED ? -REAL_MAX_SPEED : spd; // Max speed limit
  }
  mtrs_Set4Speed(spd, spd, spd, spd);
}

// kd should be smaller (larger?) than yaw PID kd
int SoccerPIDCompCorr(float kd, float yaw, float target_yaw) {
  // Placeholder for PID control logic
  // Implement PID control based on corridor position
  float error = target_yaw - yaw;   // >0: need move right, <0: need move left
  int spd = (int)(kd * error);
  return spd;   // >0: rotate right, <0: rotate left
}

void setState(State_t newState) { state = newState; }
State_t getState() { return state; }
State_t updateState(const ModuleData_t *data) {
  State_t state = getState();
  // Check IR data first - highest priority
  if (IR_IsDataReady() && data->irData->ballAngle >= 0) {
    state = STATE_CHASE_BALL;
  } else if (MPU6050_DMP_IsDataReady()) {
    // Only check MPU data if it's ready
    float yaw = data->mpuData->euler.yaw;  // Get yaw angle from DMP
    // yaw: +right, -left
    if (-YAW_THRESHOLD < yaw && yaw < YAW_THRESHOLD) {
      state = STATE_SEARCH_BALL;
    } else {
      state = STATE_ALIGN_YAW;
    }
  }
  // If no data is ready, keep current state
  setState(state);
  return state;
}

// 添加等待启动的函数
void Soccer_WaitForStart(void) {
#ifdef DEBUG_BUTTON
  char msg[64];
  snprintf(msg, sizeof(msg), "Waiting for system start...\r\n");
  dataUart_SendString(msg);
#endif
  
  // 等待系统启动
  while (!systemStarted) {
    HAL_IWDG_Refresh(&hiwdg1);  // Refresh watchdog to prevent reset
    // 闪烁LED表示等待状态
    static uint32_t lastBlink = 0;
    if (HAL_GetTick() - lastBlink > 500) {
      HAL_GPIO_TogglePin(GPIOB, LED_4_Pin);
      lastBlink = HAL_GetTick();
    }
    HAL_Delay(10);
  }
  
#ifdef DEBUG_BUTTON
  snprintf(msg, sizeof(msg), "System started successfully!\r\n");
  dataUart_SendString(msg);
#endif
  
  HAL_GPIO_WritePin(GPIOD, LED_4_Pin, GPIO_PIN_SET);
}

// 外部控制API
void Soccer_StartSystem(void) {
  if (!systemStarted) {
    systemStarted = true;
    emergencyStop = false;
    state = STATE_SEARCH_BALL;
    systemStartTime = HAL_GetTick();
  }
}

void Soccer_EmergencyStop(void) {
  emergencyStop = true;
  systemStarted = false;
  state = STATE_IDLE;
  mtrs_StopAll();
}

void Soccer_ResetEmergencyStop(void) {
  emergencyStop = false;
}

bool Soccer_IsSystemStarted(void) {
  return systemStarted;
}

bool Soccer_IsEmergencyStop(void) {
  return emergencyStop;
}