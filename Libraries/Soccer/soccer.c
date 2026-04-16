#include "soccer.h"
#include "attack.h"
#include "defense.h"

#define LED_GRAYSCALE_SCAN LED_3

#define MODE_SWITCH_TIMEOUT_MS 5000 /**< Time threshold for switching back to defensive mode if ball is lost (milliseconds) */

#define ENABLE_MANUAL_GRAYSCALE_CALIBRATION 1
#if ENABLE_MANUAL_GRAYSCALE_CALIBRATION
static const uint16_t GS_MIN[GRAYSCALE_NUM] = {17278, 22417, 12909, 21134, 17383, 11456};
static const uint16_t GS_MAX[GRAYSCALE_NUM] = {22068, 26879, 17802, 25297, 23238, 15228};
static const uint16_t GS_THD[GRAYSCALE_NUM] = {18236, 23309, 13887, 21966, 18554, 12210};
#endif

// 系统状态变量
static volatile bool systemStarted = false;
static volatile bool emergencyStop = false;
// 标志位：请求切换grayscale扫描状态
static volatile bool gsToggleRequested = false;
static Mode_t mode = MODE_DEFENSIVE;
static uint32_t systemStartTime = 0;
static uint32_t lastBallDetectionTime = 0;
static uint32_t grayscaleLedLastBlink = 0;

// 车辆旋转对齐PID
static PID_Controller_t yawAlignPID;

// 按钮控制回调函数
static void SoccerButtonControl(uint8_t btn_index, BtnEvent_t event) {
  switch (btn_index) {
  case 0: // 按钮1 - 启动
    if (event == BTN_EVENT_CLICK) {
      // 长按启动系统
      if (!systemStarted) {
        systemStarted = true;
        emergencyStop = false;
        systemStartTime = HAL_GetTick();
        #ifdef DEBUG_BUTTON
        printf("System Started!\r\n");
        #endif
      }
    } break;
  case 1: // 按钮2 - 紧急停止
    if (event == BTN_EVENT_CLICK) {
      // 单击紧急停止
      emergencyStop = true;
      systemStarted = false;
      mtrs_StopAll();
      #ifdef DEBUG_BUTTON
      printf("Emergency Stop Activated!\r\n");
      #endif
    } break;
  case 2: // 按钮3 - Reset Yaw
    if (event == BTN_EVENT_CLICK) {
      // 单击重置陀螺仪偏航角（如果系统已启动）
      // if (systemStarted) {
      MPU6050_DMP_ResetYaw();
      // }
    } break;
  case 3: // 按钮4 - Grayscale scan start/stop
    if (event == BTN_EVENT_CLICK) {
      // Defer heavy grayscale operations to non-interrupt context
      gsToggleRequested = true;
    } break;
  }
}

void SoccerInit(Mode_t initMode) {
  // 设置按钮回调
  Button_SetSystemControlCallback(0, SoccerButtonControl);  // 按钮1：启动
  Button_SetSystemControlCallback(1, SoccerButtonControl);  // 按钮2：紧急停止
  Button_SetSystemControlCallback(2, SoccerButtonControl);  // 按钮3：reset yaw
  Button_SetSystemControlCallback(3, SoccerButtonControl);  // 按钮4：grayscale scan

  // For testing, start system immediately.
  // systemStarted = true;
  // reset all states and flags
  systemStarted = false;
  emergencyStop = false;
  gsToggleRequested = false;

  // 初始化模式
  mode = initMode;
  AttackInit();
  DefenseInit();

  PID_Init(&yawAlignPID, YAW_PID_kp, YAW_PID_ki, YAW_PID_kd,
           INTEGRAL_LIMIT, -60, 60);

#if ENABLE_MANUAL_GRAYSCALE_CALIBRATION
  if (Grayscale_ApplyCalibration(GS_MIN, GS_MAX, GS_THD)) {
    printf("[Grayscale] Manual calibration restored.\r\n");
  } else {
    printf("[Grayscale] Manual calibration invalid, please re-scan.\r\n");
  }
#endif
}

void updateData(void) {
  // Handle deferred grayscale scan toggle in main context
  if (gsToggleRequested) {
    gsToggleRequested = false;  // 重置请求标志
    if (!Grayscale_IsScanning()) {
      Grayscale_StartScan();
      // 开始扫描时关闭LED
      LED_Set(LED_GRAYSCALE_SCAN, false);
      grayscaleLedLastBlink = HAL_GetTick();

      #ifdef DEBUG_GRAYSCALE
      printf("[Grayscale] Scan started (pass white line and green ground).\r\n");
      #endif
    } else {
      Grayscale_StopScan();

      if (Grayscale_IsCalibrated()) {
        // 扫描完成后根据校准结果设置LED状态 (On)
        LED_Set(LED_GRAYSCALE_SCAN, true);

        Grayscale_PrintCalibrationData();
        #ifdef DEBUG_GRAYSCALE
        printf("[Grayscale] Scan done. onWhite=%d\r\n", Grayscale_IsOnWhiteLine());
        #endif
      } else {
        // 扫描完成但未成功校准，保持LED关闭并提示重新扫描
        LED_Set(LED_GRAYSCALE_SCAN, false);

        #ifdef DEBUG_GRAYSCALE
        printf("[Grayscale] Scan failed: range too small, rescan with white+green sweep.\r\n");
        #endif
      }
    }
  }

  // 如果正在扫描，闪烁LED表示状态
  if (Grayscale_IsScanning()) {
    LED_HeartbeatTick(LED_GRAYSCALE_SCAN,
                      &grayscaleLedLastBlink,
                      HAL_GetTick(),
                      LED_HEARTBEAT_MS);
  }

  /* Grayscale must keep updating even before system start,
     so scan/calibration via button can work in IDLE state. */
  Grayscale_Process();

  // 检查紧急停止 or 系统未启动，其他传感器可跳过
  if (emergencyStop || !systemStarted) { return; }
  
  /* Update MPU6050 DMP data (blocking) */
  int result = MPU6050DMP_updateData();
  if (result != 0) { MPU6050DMP_HandleI2CError(); }
  
  // IR sensor update (non-blocking, uses internal state machine and flags)
  IR_Process();
  
  // Xsound sensor update (non-blocking, uses internal state machine and flags)
  Xsound_Process();
}

void SoccerProcess(const ModuleData_t *data) {
  // 检查紧急停止 or 系统未启动，直接返回
  if (emergencyStop || !systemStarted) {
    mtrs_StopAll();
    return;
  }
  
  // Soccer_UpdateMode(data);

  // 根据当前模式执行对应的处理函数
  switch (mode) {
  case MODE_OFFENSIVE:
    AttackMode(data);
    break;
  case MODE_DEFENSIVE:
    DefenseMode(data);
    break;
  default:
    // 无效模式，安全起见停止所有动作
    mtrs_StopAll();
    break;
  }

  #ifdef DEBUG_SOCCER
  static uint32_t lastDebug = 0;
  const uint32_t currentTime = HAL_GetTick();
  if (TIME_DIFF(currentTime, lastDebug) >= DEBUG_PRINT_INTERVAL_MS) {
    int mpuReady = MPU6050_DMP_IsDataReady();
    float yaw = data->mpuData->euler.yaw;
    int irReady = IR_IsDataReady();
    float ballAngle = data->irData->ballAngle;
    int currentState = (mode == MODE_OFFENSIVE) ? getAttackState() : getDefenseState();
    printf("IR: rdy=%d ang=%.1f max=%d eye=%d | Yaw=%.1f MPU=%d | State=%d | Mode=%s\r\n",
           irReady, ballAngle, data->irData->maxValue, 
           data->irData->maxEye, yaw, mpuReady, currentState, mode == MODE_OFFENSIVE ? "OFFENSIVE" : "DEFENSIVE");
    lastDebug = currentTime;
  }
  #endif

  // Clear data ready flags after processing (independent flags)
  if (IR_IsDataReady()) { IR_ClearDataReady(); }
  if (MPU6050_DMP_IsDataReady()) { MPU6050_DMP_ClearReady(); }
}

// TODO: 根据传感器数据更新模式（如果需要）
// if no ball for long time, switch mode
// defensive -> offensive: go to center and look for ball
// offensive -> defensive: go back to defensive position (white line in front of the goal)l;.,
// can use a timer to track how long since last ball detection, and if exceeds threshold, toggle mode
void Soccer_UpdateMode(const ModuleData_t *data) {
  if (data->irData->ballAngle >= 0.0f) {
    lastBallDetectionTime = HAL_GetTick();
  } else {
    uint32_t timeSinceLastBall = HAL_GetTick() - lastBallDetectionTime;
    if (timeSinceLastBall > MODE_SWITCH_TIMEOUT_MS) {
      // 超过阈值且当前模式为防守，切换到进攻
      if (mode == MODE_DEFENSIVE) {
        mode = MODE_OFFENSIVE;
        #ifdef DEBUG_SOCCER
        printf("Mode switched to OFFENSIVE due to no ball detected for %lu ms\r\n", timeSinceLastBall);
        #endif
      }
      // 超过阈值且当前模式为进攻，切换到防守
      else if (mode == MODE_OFFENSIVE) {
        mode = MODE_DEFENSIVE;
        #ifdef DEBUG_SOCCER
        printf("Mode switched to DEFENSIVE due to no ball detected for %lu ms\r\n", timeSinceLastBall);
        #endif
      }
    }
  }
}

// 添加等待启动的函数
void Soccer_WaitForStart(void) {
#ifdef DEBUG_BUTTON
  printf("Waiting for system start...\r\n");
#endif
  
  // 等待系统启动
  while (!systemStarted) {
    HAL_IWDG_Refresh(&hiwdg1);  // Refresh watchdog to prevent reset
    updateData();               // Keep grayscale scan/calibration logic running
    // 闪烁LED表示等待状态
    static uint32_t lastBlink = 0;
    LED_HeartbeatTick(LED_4, &lastBlink, HAL_GetTick(), LED_WAIT_BLINK_MS);
    HAL_Delay(10);
  }
  
#ifdef DEBUG_BUTTON
  printf("System started successfully!\r\n");
#endif
  
  LED_Set(LED_4, true);
}

// 外部控制API
void Soccer_StartSystem(void) {
  if (!systemStarted) {
    systemStarted = true;
    emergencyStop = false;
    systemStartTime = HAL_GetTick();
  }
}

void Soccer_EmergencyStop(void) {
  emergencyStop = true;
  systemStarted = false;
  mtrs_StopAll();
}

void Soccer_ResetEmergencyStop(void) {
  emergencyStop = false;
}

bool Soccer_IsSystemStarted(void) {
  return systemStarted;
}

bool Soccer_IsEmergencyStop(void) { return emergencyStop; }

void compassCar(float target_yaw, float yaw) {
  int pidOutput = (int)PID_Compute(&yawAlignPID, target_yaw, yaw);
  if (pidOutput >= 0) pidOutput = (pidOutput < 35) ? 35 : pidOutput;
  else pidOutput = (pidOutput > -35) ? -35 : pidOutput;
  mtrs_Set4Speed(pidOutput, pidOutput, pidOutput, pidOutput);
}
/* Compass Car test */
// static PID_Controller_t yawAlignPID;    // 车辆旋转对齐PID
// PID_Init(&yawAlignPID, YAW_PID_kp, YAW_PID_ki, YAW_PID_kd,
// INTEGRAL_LIMIT, -45, 45);
// float target_yaw = 0;  // 目标偏航角（与球对齐）
// float yaw = moduleData.mpuData->euler.yaw;;  // 当前偏航角
// int pidOutput = (int)PID_Compute(&yawAlignPID, target_yaw, yaw);
// if (pidOutput >= 0) pidOutput = (pidOutput < BASE_SPEED) ? BASE_SPEED : pidOutput;
// else pidOutput = (pidOutput > -BASE_SPEED) ? -BASE_SPEED : pidOutput;
// mtrs_Set4Speed(pidOutput, pidOutput, pidOutput, pidOutput);

/*
L     F     R
  F       F 
B           B

      B
*/
// Boundary correction movement based on grayscale sensors
void boundMove(const ModuleData_t *data, PID_Controller_t *yawPid, int moveSpeed) {
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

    int yawCorr = (int)PID_Compute(yawPid, 0.0f, yaw);
    polarMoveWthCorr(moveAngle, moveSpeed, yawCorr);

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