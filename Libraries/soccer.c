#include "soccer.h"
#include "button.h"
#include "MPU6050DMP.h"
#include "motors.h"

// 系统状态变量
static volatile bool systemStarted = false;
static volatile bool emergencyStop = false;
static State_t state = STATE_IDLE;
static uint32_t systemStartTime = 0;

// PID控制器实例
static PID_Controller_t yawAlignPID;    // 车辆旋转对齐PID
static PID_Controller_t yawCorrectPID;  // 追球时偏航修正PID

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
      if (systemStarted) {
        MPU6050_DMP_ResetYaw();
      }
    } break;
  case 3: // 按钮4 - Reset All
    if (event == BTN_EVENT_LONG_PRESS_START) {
      // 长按重启系统（从flash运行）
      dataUart_SendString("System Reset - Running from Flash...\r\n");
      HAL_Delay(100);  // 让消息发送完成
      NVIC_SystemReset();  // 系统重启
    } break;
  }
}

void SoccerInit(void) {
  // 设置按钮回调
  Button_SetSystemControlCallback(0, SoccerButtonControl);  // 按钮1：启动
  Button_SetSystemControlCallback(1, SoccerButtonControl);  // 按钮2：紧急停止
  Button_SetSystemControlCallback(2, SoccerButtonControl);  // 按钮3：reset yaw
  Button_SetSystemControlCallback(3, SoccerButtonControl);  // 按钮4：reset all
  
  // 初始化PID控制器
  PID_Init(&yawAlignPID, YAW_PID_kp, YAW_PID_ki, YAW_PID_kd, 
           INTEGRAL_LIMIT, -45, 45);
  PID_Init(&yawCorrectPID, CORR_kp, CORR_ki, CORR_kd,
           INTEGRAL_LIMIT, -BASE_SPEED, BASE_SPEED);
}

void updateData() {
  // 检查紧急停止 or 系统未启动，no read data
  if (emergencyStop || !systemStarted) { return; }
  
  /* Update MPU6050 DMP data (blocking) */
  int result = MPU6050DMP_updateData();
  if (result != 0) { MPU6050DMP_HandleI2CError(); }
  
  // IR sensor update (non-blocking, uses internal state machine and flags)
  IR_Process();
  
  // Xsound sensor update (non-blocking, uses internal state machine and flags)
  Xsound_Process();
}

void setState(State_t newState) { state = newState; }
State_t getState() { return state; }
State_t updateState(const ModuleData_t *data) {
  State_t currentState = getState();

  bool mpuReady = MPU6050_DMP_IsDataReady();
  // if (!mpuReady) {
  //   currentState = STATE_IDLE; // No valid data, go to idle
  //   setState(currentState);
  //   return currentState;
  // }

  float yaw = data->mpuData->euler.yaw;
  bool irReady = IR_IsDataReady();
  // Ball is detected by IR sensor
  if (data->irData->ballAngle >= 0) {
    currentState = STATE_CHASE_BALL;
  } else {  // No ball detected - decide whether to search or align based on yaw
    if (-YAW_THRESHOLD <= yaw && yaw <= YAW_THRESHOLD) {
      currentState = STATE_SEARCH_BALL;
    } else {
      currentState = STATE_ALIGN_YAW;
    }
  }
  
  setState(currentState);
  return currentState;
}

// TODO: using state machine to control behavior
void soccer_ProcessData(const ModuleData_t *data) {
  // 检查紧急停止 or 系统未启动，直接返回
  if (emergencyStop || !systemStarted) {
    mtrs_StopAll();
    return;
  }

  if (!MPU6050_DMP_IsDataReady()) {
    mtrs_StopAll();  // No valid data, stop motors for safety
    return;
  }

  const float target_yaw = 0.0f;
  float yaw = data->mpuData->euler.yaw;
  float ballAngle = data->irData->ballAngle;
  
  float moveAngle = ballAngle;
  float angleDrift = 0.0f;
  const float maxValue = (float)data->irData->maxValue;
  const float BallBigRadius = 600.0f; // R = Max eye value * tan(theta)

  if (ballAngle >=0) {
    int yawCorr = (int)PID_Compute(&yawCorrectPID, target_yaw, yaw);
    int spd = BASE_SPEED + BALL_CHASE_SPEED_BONUS;
    // TODO: angleDrift shd be small when ball is far, but can cause large angle correction when close.
    if (15 < ballAngle && ballAngle <= 180) {
      arm_atan2_f32(BallBigRadius, POSSIBLE_MAX_BALL_VALUE-maxValue, &angleDrift);
      moveAngle = ballAngle + angleDrift * 57.295779513f; // Convert to degrees
    } else if (180 < ballAngle && ballAngle <= 345) {
      arm_atan2_f32(BallBigRadius, POSSIBLE_MAX_BALL_VALUE-maxValue, &angleDrift);
      moveAngle = ballAngle - angleDrift * 57.295779513f; // Convert to degrees
    } else {    // Ball is straight ahead, no angle correction needed
      moveAngle = ballAngle;
    }
    polarMoveWthCorr(moveAngle, spd, yawCorr);
  } else if (yaw < -YAW_THRESHOLD || yaw > YAW_THRESHOLD) {
    int pidOutput = (int)PID_Compute(&yawAlignPID, target_yaw, yaw);
    mtrs_Set4Speed(pidOutput, pidOutput, pidOutput, pidOutput);
  } else {
    mtrs_StopAll();
  }
  
  #ifdef DEBUG_SOCCER
  static uint32_t lastDebug = 0;
  if (HAL_GetTick() - lastDebug > 300) {  // Every 300 ms
    bool irReady = IR_IsDataReady();
    bool mpuReady = MPU6050_DMP_IsDataReady();
    char msg[160];
    snprintf(msg, sizeof(msg), 
              "IR: rdy=%d ang=%.1f max=%d eye=%d | Yaw=%.1f MPU=%d | State=%d\r\n",
              irReady, ballAngle, data->irData->maxValue, 
              data->irData->maxEye, yaw, mpuReady, getState());
    dataUart_SendString(msg);
    lastDebug = HAL_GetTick();
  }
  #endif

  // Clear data ready flags after processing (independent flags)
  if (IR_IsDataReady()) { IR_ClearDataReady(); }
  if (MPU6050_DMP_IsDataReady()) { MPU6050_DMP_ClearReady(); }
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