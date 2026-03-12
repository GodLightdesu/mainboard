#include "soccer.h"

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
  // case 3: // 按钮4 - Reset All
  //   if (event == BTN_EVENT_LONG_PRESS_START) {
  //     // 长按重启系统（从flash运行）
  //     systemStarted = false;
  //     emergencyStop = false;
  //     state = STATE_IDLE;
  //     mtrs_StopAll();
  //     dataUart_SendString("System Reset - Running from Flash...\r\n");
  //     HAL_Delay(100);  // 让消息发送完成
  //     NVIC_SystemReset();  // 系统重启
  //   } break;
  }
}

void SoccerInit(void) {
  // 设置按钮回调
  Button_SetSystemControlCallback(0, SoccerButtonControl);  // 按钮1：启动
  Button_SetSystemControlCallback(1, SoccerButtonControl);  // 按钮2：紧急停止
  Button_SetSystemControlCallback(2, SoccerButtonControl);  // 按钮3：reset yaw
  // Button_SetSystemControlCallback(3, SoccerButtonControl);  // 按钮4：reset all
  
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
  State_t nextState = currentState;
  
  // Extract sensor data
  float yaw = data->mpuData->euler.yaw;
  float ballAngle = data->irData->ballAngle;
  bool ballDetected = (ballAngle >= 0);
  bool yawAligned = (-YAW_THRESHOLD <= yaw && yaw <= YAW_THRESHOLD);
  
  // State machine transitions based on current state
  switch (currentState) {
  case STATE_IDLE:
    // Can only transition when system starts (handled by button callback)
    // Stay in IDLE state until external trigger
    break;
    
  case STATE_SEARCH_BALL:
    if (ballDetected) {
      // Found ball - switch to chase
      nextState = STATE_CHASE_BALL;
    } else if (!yawAligned) {
      // Need to align yaw before continuing search
      nextState = STATE_ALIGN_YAW;
    }
    // Otherwise stay in SEARCH_BALL
    break;
    
  case STATE_CHASE_BALL:
    if (!ballDetected) {
      // Lost ball - decide next action
      if (yawAligned) {
        nextState = STATE_SEARCH_BALL;
      } else {
        nextState = STATE_ALIGN_YAW;
      }
    }
    // If ball still detected, stay in CHASE_BALL
    break;
    
  case STATE_ALIGN_YAW:
    if (yawAligned) {
      // Yaw aligned - go search for ball
      nextState = STATE_SEARCH_BALL;
    } else if (ballDetected) {
      // Ball detected during alignment - prioritize chasing
      nextState = STATE_CHASE_BALL;
    }
    // Otherwise stay in ALIGN_YAW until aligned
    break;
    
  case STATE_OUT_OF_BOUNDS:
    // TODO: Implement transition back to normal operation
    // For now, stay in OUT_OF_BOUNDS until manual intervention
    break;
    
  default:
    // Invalid state - reset to IDLE for safety
    nextState = STATE_IDLE;
    break;
  }
  
  setState(nextState);
  return nextState;
}

void soccer_ProcessData(const ModuleData_t *data) {
  // 检查紧急停止 or 系统未启动，直接返回
  if (emergencyStop || !systemStarted) {
    mtrs_StopAll();
    return;
  }

  // Common variables
  const float target_yaw = 0.0f;
  float yaw = data->mpuData->euler.yaw;
  float ballAngle = data->irData->ballAngle;
  
  // Update state based on sensor data
  State_t currentState = updateState(data);
  
  // State machine execution
  switch (currentState) {
    case STATE_IDLE:
      mtrs_StopAll();
      break;
      
    case STATE_SEARCH_BALL: {
      // Rotate slowly to search for ball
      // TODO: Implement search pattern (e.g., rotate in place)
      mtrs_StopAll();
      break;
    }
    
    case STATE_CHASE_BALL: {
      // Chase ball with angle correction
      float moveAngle = ballAngle;
      const float maxValue = (float)data->irData->maxValue;
      const float BallBigRadius = 600.0f; // R = Max eye value * tan(theta)
      
      int yawCorr = (int)PID_Compute(&yawCorrectPID, target_yaw, yaw);
      int spd = BASE_SPEED + BALL_CHASE_SPEED_BONUS;
      
      // Apply angle correction based on ball position (optimize: avoid redundant calls)
      if ((ANGLE_CORR_MIN_THRESHOLD < ballAngle && ballAngle <= ANGLE_HALF_CIRCLE) ||
          (ANGLE_HALF_CIRCLE < ballAngle && ballAngle <= ANGLE_CORR_MAX_THRESHOLD)) {
        float angleDrift;
        arm_atan2_f32(BallBigRadius, POSSIBLE_MAX_BALL_VALUE - maxValue, &angleDrift);
        angleDrift *= RAD_TO_DEG; // Convert to degrees once
        
        // Apply correction direction based on which side of 180° the ball is
        moveAngle = (ballAngle <= ANGLE_HALF_CIRCLE) ? 
                    (ballAngle + angleDrift) : (ballAngle - angleDrift);
      }
      // else: Ball is straight ahead or behind, no angle correction needed
      
      polarMoveWthCorr(moveAngle, spd, yawCorr);
      break;
    }
    
    case STATE_ALIGN_YAW: {
      // Rotate in place to align yaw to target
      int pidOutput = (int)PID_Compute(&yawAlignPID, target_yaw, yaw);
      mtrs_Set4Speed(pidOutput, pidOutput, pidOutput, pidOutput);
      break;
    }
    
    case STATE_OUT_OF_BOUNDS:
      // TODO: Implement out of bounds behavior
      mtrs_StopAll();
      break;
      
    default:
      mtrs_StopAll();
      break;
  }
  
  #ifdef DEBUG_SOCCER
  static uint32_t lastDebug = 0;
  const uint32_t currentTime = HAL_GetTick();
  if (TIME_DIFF(currentTime, lastDebug) >= DEBUG_PRINT_INTERVAL_MS) {
    bool irReady = IR_IsDataReady();
    bool mpuReady = MPU6050_DMP_IsDataReady();
    printf("IR: rdy=%d ang=%.1f max=%d eye=%d | Yaw=%.1f MPU=%d | State=%d\r\n",
           irReady, ballAngle, data->irData->maxValue, 
           data->irData->maxEye, yaw, mpuReady, currentState);
    lastDebug = currentTime;
  }
  #endif

  // Clear data ready flags after processing (independent flags)
  if (IR_IsDataReady()) { IR_ClearDataReady(); }
  if (MPU6050_DMP_IsDataReady()) { MPU6050_DMP_ClearReady(); }
}

// 添加等待启动的函数
void Soccer_WaitForStart(void) {
#ifdef DEBUG_BUTTON
  printf("Waiting for system start...\r\n");
#endif
  
  // 等待系统启动
  while (!systemStarted) {
    HAL_IWDG_Refresh(&hiwdg1);  // Refresh watchdog to prevent reset
    // 闪烁LED表示等待状态
    static uint32_t lastBlink = 0;
    const uint32_t currentTime = HAL_GetTick();
    if (TIME_DIFF(currentTime, lastBlink) >= LED_WAIT_BLINK_MS) {
      HAL_GPIO_TogglePin(GPIOB, LED_4_Pin);
      lastBlink = currentTime;
    }
    HAL_Delay(10);
  }
  
#ifdef DEBUG_BUTTON
  printf("System started successfully!\r\n");
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