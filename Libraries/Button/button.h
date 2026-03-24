#ifndef BUTTON_H
#define BUTTON_H

#define MAX_BUTTONS 8
#define BTN_DEBOUNCE_TIME_MS 10       // 消抖时间
#define BTN_LONG_PRESS_TIME_MS  500  // 长按时间阈值（用于启动）
#define BTN_DOUBLE_CLICK_TIME_MS 300  // 双击间隔时间阈值
#define BTN_SCAN_INTERVAL_MS    10    // 扫描间隔

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "const.h"
#include "gpio.h"
#include "tim.h"

// 添加 dataPrint 头文件用于调试输出
#ifdef DEBUG_BUTTON
#include "dataPrint.h"
#endif

// 按键状态枚举
typedef enum {
  BTN_STATE_IDLE = 0,         // 空闲状态
  BTN_STATE_DEBOUNCE_PRESS,   // 按下消抖
  BTN_STATE_PRESSED,          // 已按下
  BTN_STATE_WAIT_RELEASE,     // 等待释放
  BTN_STATE_WAIT_DOUBLE,      // 等待双击
  BTN_STATE_LONG_PRESS,       // 长按
  BTN_STATE_RELEASE_DEBOUNCE, // 释放消抖
  BTN_NUM_STATES              // 状态总数
} BtnState_t;

// 按键事件枚举
typedef enum {
  BTN_EVENT_NONE = 0,          // 无事件
  BTN_EVENT_CLICK,             // 单击（用于启动）
  BTN_EVENT_DOUBLE_CLICK,      // 双击（用于紧急停止）
  BTN_EVENT_LONG_PRESS_START,  // 长按开始
  BTN_EVENT_LONG_PRESS_HOLD,   // 长按保持
  BTN_EVENT_LONG_PRESS_END,    // 长按结束
  BTN_NUM_EVENTS               // 事件总数
} BtnEvent_t;

// 系统控制回调函数类型
typedef void (*SystemControlCallback_t)(uint8_t btnIndex, BtnEvent_t event);

typedef struct 
{
  GPIO_TypeDef* port;          // GPIO port
  uint16_t pin;                // GPIO pin
} BtnPin_t;

// Button data structure
typedef struct Button_t {
  BtnPin_t pin;

  // state machine
  BtnState_t state;            // current state
  BtnState_t prevState;        // previous state
  uint32_t timer;
  uint32_t pressTime;
  uint32_t releaseTime;
  uint8_t clickCount;

  // physical state
  bool rawState;
  bool stableState;
  bool lastRawState;
  uint32_t debounceTimer;

  // event flag
  BtnEvent_t pendingEvent;
  bool eventReady;
  
  // system control callback
  SystemControlCallback_t controlCallback;
  
  // for long press reporting
  uint32_t lastHoldReportTime;
  
  // button name for debugging
  const char* buttonName;
} Button_t;

// 状态处理函数类型
typedef void (*StateHandler_t)(uint8_t btnIndex, Button_t*, uint32_t);

// Function prototypes
void Button_Init(void);
void Button_Scan(void);

// Event handling
BtnEvent_t Button_GetEvent(uint8_t btnIndex);
BtnState_t Button_GetState(uint8_t btnIndex);
bool Button_IsPressed(uint8_t btnIndex);

// System control callbacks
void Button_SetSystemControlCallback(uint8_t btnIndex, SystemControlCallback_t callback);
void Button_ClearSystemControlCallback(uint8_t btnIndex);

// Debug functions
#ifdef DEBUG_BUTTON
void Button_SetButtonName(uint8_t btnIndex, const char* name);
#endif

#endif // BUTTON_H