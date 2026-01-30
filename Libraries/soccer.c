#include "soccer.h"

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
    if (event == BTN_EVENT_DOUBLE_CLICK) {
      // 双击紧急停止
      emergencyStop = true;
      systemStarted = false;
      state = STATE_IDLE;
      mtrs_StopAll();
    } break;
  }
}

void SoccerInit(void) {
  // 设置按钮回调
  Button_SetSystemControlCallback(0, SoccerButtonControl);  // 按钮1：启动
  Button_SetSystemControlCallback(1, SoccerButtonControl);  // 按钮2：紧急停止
}

void updateData() {
  // 检查紧急停止 or 系统未启动，no read data
  if (emergencyStop || !systemStarted) {
    return;
  }
    
  /* Process IR state machine */
  IR_Process();

  /* Process MPU6050 state machine */
  MPU6050_Process();

  MPU6050_DMP_Update();
}

void soccer_ProcessData(const ModuleData_t *data) {
  // 检查紧急停止 or 系统未启动，直接返回
  if (emergencyStop || !systemStarted) {
    mtrs_StopAll();
    return;
  }

  // Get Euler angles from DMP
  EulerAngles_t euler;
  if (MPU6050_DMP_GetEulerAngles(&euler)) {
    float yaw = euler.yaw;
    // float roll = euler.roll;
    // float pitch = euler.pitch;

    // for (MtrID_t mtr_id = MTR0; mtr_id < 4; ++mtr_id) {
    //   mtr_SetDecayMode(mtr_id, SLOW_DECAY);
    // }
    int8_t spd = 30;
    if (yaw > 10) {
      mtrs_Set4Speed(-spd, -spd, -spd, -spd);
    } else if (yaw < -10) {
      mtrs_Set4Speed(spd, spd, spd, spd);
    } else {
      // chase ball
      if (IR_IsDataReady() && data->irData->ballAngle >= 0) {
        polarMove(data->irData->ballAngle, spd + 10);
      } else {
        mtrs_StopAll();
      }
    }
  }
}

State_t getState() { return state; }

// 添加等待启动的函数
void Soccer_WaitForStart(void) {
#ifdef DEBUG_BUTTON
  char msg[64];
  snprintf(msg, sizeof(msg), "Waiting for system start...\r\n");
  dataUart_SendString(msg);
#endif
  
  // 等待系统启动
  while (!systemStarted) {
    // 闪烁LED表示等待状态
    static uint32_t lastBlink = 0;
    if (HAL_GetTick() - lastBlink > 500) {
      HAL_GPIO_TogglePin(GPIOD, LED_4_Pin);
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