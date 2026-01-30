#include "button.h"
#include "stm32h7xx_hal.h"
#include <string.h>

// 添加 data_uart 头文件用于调试输出
#ifdef DEBUG_BUTTON
#include "data_uart.h"
#endif

static const BtnPin_t btns_Pin[MAX_BUTTONS] = {
  {GPIOD, BTN_1_Pin}, // Button 1 - 启动按钮
  {GPIOD, BTN_2_Pin}, // Button 2 - 紧急停止按钮
  {GPIOD, BTN_3_Pin}, // Button 3
  {GPIOD, BTN_4_Pin}, // Button 4
  {GPIOD, BTN_5_Pin}, // Button 5
  {GPIOD, BTN_6_Pin}, // Button 6
  {GPIOC, BTN_7_Pin}, // Button 7
  {GPIOC, BTN_8_Pin}  // Button 8
};

// 按钮名称（用于调试）
static const char* btn_names[MAX_BUTTONS] = {
  "Button1", "Button2", "Button3", "Button4",
  "Button5", "Button6", "Button7", "Button8"
};

static Button_t buttons[MAX_BUTTONS];

// ==================== 状态处理函数声明 ====================
static void HandleIdleState(uint8_t btnIndex, Button_t* btn, uint32_t currentTime);
static void HandleDebouncePressState(uint8_t btnIndex, Button_t* btn, uint32_t currentTime);   // Handles debouncing when button press is detected
static void HandlePressedState(uint8_t btnIndex, Button_t* btn, uint32_t currentTime);         // Handles the pressed state, checks for long press
static void HandleWaitReleaseState(uint8_t btnIndex, Button_t* btn, uint32_t currentTime);     // Waits for button release after press
static void HandleWaitDoubleState(uint8_t btnIndex, Button_t* btn, uint32_t currentTime);      // Waits for potential double click
static void HandleLongPressState(uint8_t btnIndex, Button_t* btn, uint32_t currentTime);       // Handles long press state
static void HandleReleaseDebounceState(uint8_t btnIndex, Button_t* btn, uint32_t currentTime); // Handles debouncing when button is released

static StateHandler_t state_handlers[] = {
  HandleIdleState,            // BTN_STATE_IDLE
  HandleDebouncePressState,   // BTN_STATE_DEBOUNCE_PRESS
  HandlePressedState,         // BTN_STATE_PRESSED
  HandleWaitReleaseState,     // BTN_STATE_WAIT_RELEASE
  HandleWaitDoubleState,      // BTN_STATE_WAIT_DOUBLE
  HandleLongPressState,       // BTN_STATE_LONG_PRESS
  HandleReleaseDebounceState  // BTN_STATE_RELEASE_DEBOUNCE
};

// 状态名称字符串（用于调试）
static const char* state_names[] = {
  "IDLE",
  "DEBOUNCE_PRESS",
  "PRESSED",
  "WAIT_RELEASE",
  "WAIT_DOUBLE",
  "LONG_PRESS",
  "RELEASE_DEBOUNCE"
};

// 事件名称字符串（用于调试）
static const char* event_names[] = {
    "NONE",
    "CLICK",
    "DOUBLE_CLICK",
    "LONG_PRESS_START",
    "LONG_PRESS_HOLD",
    "LONG_PRESS_END"
};

// ==================== 辅助函数 ====================
static bool ReadRawState(Button_t* btn) {
    return (HAL_GPIO_ReadPin(btn->pin.port, btn->pin.pin) == GPIO_PIN_RESET);
}

static bool ProcessDebounce(Button_t* btn, uint32_t currentTime) {
  bool changed = false;
  
  // 读取当前原始状态
  btn->rawState = ReadRawState(btn);
  
  // 状态发生变化，记录变化时间
  if (btn->rawState != btn->lastRawState) {
    btn->debounceTimer = currentTime;
    btn->lastRawState = btn->rawState;
    
    #ifdef DEBUG_BUTTON
    if (btn->buttonName) {
        char msg[64];
        snprintf(msg, sizeof(msg), "[%s] Raw state changed to: %s\r\n", 
                btn->buttonName, btn->rawState ? "PRESSED" : "RELEASED");
        dataUart_SendString(msg);
    }
    #endif
  }
  
  // 检查消抖是否完成（使用绝对时间差）
  if ((currentTime - btn->debounceTimer) >= BTN_DEBOUNCE_TIME_MS) {
    if (btn->stableState != btn->rawState) {
      btn->stableState = btn->rawState;
      changed = true;
      
      #ifdef DEBUG_BUTTON
      if (btn->buttonName) {
          char msg[64];
          snprintf(msg, sizeof(msg), "[%s] Stable state changed to: %s\r\n", 
                  btn->buttonName, btn->stableState ? "PRESSED" : "RELEASED");
          dataUart_SendString(msg);
      }
      #endif
    }
  }
  
  return changed;
}

static void SendSystemControlEvent(uint8_t btnIndex, Button_t* btn, BtnEvent_t event) {
  // 调用系统控制回调
  if (btn->controlCallback != NULL) {
    btn->controlCallback(btnIndex, event);
  }
  
  // 也保存到事件缓冲区（兼容旧代码）
  btn->pendingEvent = event;
  btn->eventReady = true;
  
  #ifdef DEBUG_BUTTON
  if (btn->buttonName) {
      dataUart_PrintButtonEvent(btnIndex, btn->buttonName, event_names[event]);
  }
  #endif
}

static void ChangeState(uint8_t btnIndex, Button_t* btn, BtnState_t new_state, uint32_t currentTime) {
  #ifdef DEBUG_BUTTON
  if (btn->buttonName && btn->state != new_state) {
      dataUart_PrintButtonState(btnIndex, btn->buttonName, state_names[new_state]);
  }
  #endif
  (void)btnIndex; // 防止未使用警告
  btn->prevState = btn->state;
  btn->state = new_state;
  btn->timer = currentTime;
}

// ==================== 状态处理函数实现 ====================
static void HandleIdleState(uint8_t btnIndex, Button_t* btn, uint32_t currentTime) {
  if (btn->stableState) {
    ChangeState(btnIndex, btn, BTN_STATE_DEBOUNCE_PRESS, currentTime);
  }
}

static void HandleDebouncePressState(uint8_t btnIndex, Button_t* btn, uint32_t currentTime) {
  if (btn->stableState) {
    if ((currentTime - btn->timer) >= BTN_DEBOUNCE_TIME_MS) {
      ChangeState(btnIndex, btn, BTN_STATE_PRESSED, currentTime);
      btn->pressTime = currentTime;
      if (btn->clickCount == 0) {
        btn->clickCount = 1;
      }
      
      #ifdef DEBUG_BUTTON
      if (btn->buttonName) {
        char msg[64];
        snprintf(msg, sizeof(msg), "[%s] Press detected, clickCount: %d\r\n", 
                btn->buttonName, btn->clickCount);
        dataUart_SendString(msg);
      }
      #endif
    }
  } else {
    ChangeState(btnIndex, btn, BTN_STATE_IDLE, currentTime);
    btn->clickCount = 0;
  }
}

static void HandlePressedState(uint8_t btnIndex, Button_t* btn, uint32_t currentTime) {
  uint32_t press_duration = currentTime - btn->pressTime;
  // uint32_t now = HAL_GetTick();  // 重新获取当前时间
  // uint32_t press_duration = now - btn->pressTime;  // 使用最新的时间

  if (!btn->stableState) {
    ChangeState(btnIndex, btn, BTN_STATE_RELEASE_DEBOUNCE, currentTime);
    btn->releaseTime = currentTime;
  } else if (press_duration >= BTN_LONG_PRESS_TIME_MS) {
    // 长按达到阈值，发送长按开始事件（用于启动）
    ChangeState(btnIndex, btn, BTN_STATE_LONG_PRESS, currentTime);
    SendSystemControlEvent(btnIndex, btn, BTN_EVENT_LONG_PRESS_START);
    btn->lastHoldReportTime = currentTime;
  }
}

static void HandleWaitReleaseState(uint8_t btnIndex, Button_t* btn, uint32_t currentTime) {
  if (!btn->stableState) {
    uint32_t release_duration = currentTime - btn->releaseTime;
    
    if (release_duration < BTN_DOUBLE_CLICK_TIME_MS) {
      ChangeState(btnIndex, btn, BTN_STATE_WAIT_DOUBLE, currentTime);
      
      #ifdef DEBUG_BUTTON
      if (btn->buttonName) {
          char msg[64];
          snprintf(msg, sizeof(msg), "[%s] Waiting for double click...\r\n", 
                  btn->buttonName);
          dataUart_SendString(msg);
      }
      #endif
    } else {
      ChangeState(btnIndex, btn, BTN_STATE_IDLE, currentTime);
      SendSystemControlEvent(btnIndex, btn, BTN_EVENT_CLICK);
      btn->clickCount = 0;
    }
  }
}

static void HandleWaitDoubleState(uint8_t btnIndex, Button_t* btn, uint32_t currentTime) {
  uint32_t wait_duration = currentTime - btn->releaseTime;
  
  if (wait_duration >= BTN_DOUBLE_CLICK_TIME_MS) {
    ChangeState(btnIndex, btn, BTN_STATE_IDLE, currentTime);
    SendSystemControlEvent(btnIndex, btn, BTN_EVENT_CLICK);
    btn->clickCount = 0;
  } else if (btn->stableState) {
    ChangeState(btnIndex, btn, BTN_STATE_DEBOUNCE_PRESS, currentTime);
    btn->clickCount = 2;
    
    #ifdef DEBUG_BUTTON
    if (btn->buttonName) {
      char msg[64];
      snprintf(msg, sizeof(msg), "[%s] Second click detected, clickCount: %d\r\n", 
              btn->buttonName, btn->clickCount);
      dataUart_SendString(msg);
    }
    #endif
  }
}

static void HandleLongPressState(uint8_t btnIndex, Button_t* btn, uint32_t currentTime) {
  // 检查是否释放
  if (!btn->stableState) {
    ChangeState(btnIndex, btn, BTN_STATE_RELEASE_DEBOUNCE, currentTime);
    SendSystemControlEvent(btnIndex, btn, BTN_EVENT_LONG_PRESS_END);
    btn->lastHoldReportTime = 0;
  } 
  // 长按持续报告（可选）
  else if ((currentTime - btn->lastHoldReportTime) >= 500) {
    SendSystemControlEvent(btnIndex, btn, BTN_EVENT_LONG_PRESS_HOLD);
    btn->lastHoldReportTime = currentTime;
  }
}

static void HandleReleaseDebounceState(uint8_t btnIndex, Button_t* btn, uint32_t currentTime) {
  if (!btn->stableState) {
    if ((currentTime - btn->timer) >= BTN_DEBOUNCE_TIME_MS) {
      if (btn->clickCount == 2) {
        // 双击事件（用于紧急停止）
        ChangeState(btnIndex, btn, BTN_STATE_IDLE, currentTime);
        SendSystemControlEvent(btnIndex, btn, BTN_EVENT_DOUBLE_CLICK);
        btn->clickCount = 0;
      } else {
        ChangeState(btnIndex, btn, BTN_STATE_WAIT_RELEASE, currentTime);
      }
    }
  } else {
    ChangeState(btnIndex, btn, BTN_STATE_PRESSED, currentTime);
  }
}

// ==================== 公有函数实现 ====================
void Button_Init(void) {
  memset(buttons, 0, sizeof(buttons));
  
  #ifdef DEBUG_BUTTON
  // 发送初始化消息
  dataUart_PrintInitMessage("Button Module");
  #endif
  
  for (int i = 0; i < MAX_BUTTONS; ++i) {
    buttons[i].pin = btns_Pin[i];
    buttons[i].state = BTN_STATE_IDLE;
    buttons[i].prevState = BTN_STATE_IDLE;
    buttons[i].stableState = false;
    buttons[i].rawState = false;
    buttons[i].lastRawState = false;
    buttons[i].eventReady = false;
    buttons[i].pendingEvent = BTN_EVENT_NONE;
    buttons[i].clickCount = 0;
    buttons[i].timer = 0;
    buttons[i].pressTime = 0;
    buttons[i].releaseTime = 0;
    buttons[i].debounceTimer = 0;
    buttons[i].controlCallback = NULL;
    buttons[i].lastHoldReportTime = 0;
    buttons[i].buttonName = btn_names[i];
    
    #ifdef DEBUG_BUTTON
    char msg[64];
    snprintf(msg, sizeof(msg), "[Button] %s initialized at GPIO Port: %c, Pin: %d\r\n", 
            btn_names[i], 
            (buttons[i].pin.port == GPIOD) ? 'D' : 
            (buttons[i].pin.port == GPIOC) ? 'C' : '?',
            buttons[i].pin.pin);
    dataUart_SendString(msg);
    #endif
  }
}

void Button_Scan(void) {
  uint32_t currentTime = HAL_GetTick();
  
  for (int i = 0; i < MAX_BUTTONS; i++) {
    // 1. 处理消抖，获取稳定状态
    ProcessDebounce(&buttons[i], currentTime);
    
    // 2. 调用当前状态的处理函数
    if (buttons[i].state < BTN_NUM_STATES) {
      state_handlers[buttons[i].state](i, &buttons[i], currentTime);
    }
  }
}

BtnEvent_t Button_GetEvent(uint8_t btnIndex) {
  if (btnIndex >= MAX_BUTTONS) {
    return BTN_EVENT_NONE;
  }
  
  if (buttons[btnIndex].eventReady) {
    BtnEvent_t event = buttons[btnIndex].pendingEvent;
    buttons[btnIndex].eventReady = false;
    buttons[btnIndex].pendingEvent = BTN_EVENT_NONE;
    
    #ifdef DEBUG_BUTTON
    if (buttons[btnIndex].buttonName) {
        char msg[80];
        snprintf(msg, sizeof(msg), "[%s] Event retrieved: %s\r\n", 
                buttons[btnIndex].buttonName, event_names[event]);
        dataUart_SendString(msg);
    }
    #endif
    
    return event;
  }
  return BTN_EVENT_NONE;
}

BtnState_t Button_GetState(uint8_t btnIndex) {
  if (btnIndex >= MAX_BUTTONS) {
    return BTN_NUM_STATES;
  }
  
  #ifdef DEBUG_BUTTON
  if (buttons[btnIndex].buttonName) {
    char msg[64];
    snprintf(msg, sizeof(msg), "[%s] State queried: %s\r\n", 
            buttons[btnIndex].buttonName, state_names[buttons[btnIndex].state]);
    dataUart_SendString(msg);
  }
  #endif
  
  return buttons[btnIndex].state;
}

bool Button_IsPressed(uint8_t btnIndex) {
  if (btnIndex >= MAX_BUTTONS) { return false; }
  
  bool pressed = buttons[btnIndex].stableState;
  
  #ifdef DEBUG_BUTTON
  static bool last_pressed_state[MAX_BUTTONS] = {false};
  if (pressed != last_pressed_state[btnIndex]) {
    last_pressed_state[btnIndex] = pressed;
    if (buttons[btnIndex].buttonName) {
      char msg[64];
      snprintf(msg, sizeof(msg), "[%s] IsPressed query result: %s\r\n", 
              buttons[btnIndex].buttonName, pressed ? "PRESSED" : "RELEASED");
      dataUart_SendString(msg);
    }
  }
  #endif
  
  return pressed;
}

// ==================== 系统控制回调函数API ====================
void Button_SetSystemControlCallback(uint8_t btnIndex, SystemControlCallback_t callback) {
  if (btnIndex >= MAX_BUTTONS) { return; }
  
  buttons[btnIndex].controlCallback = callback;
  
  #ifdef DEBUG_BUTTON
  if (buttons[btnIndex].buttonName) {
    char msg[80];
    snprintf(msg, sizeof(msg), "[%s] System control callback set: %s\r\n", 
            buttons[btnIndex].buttonName, callback ? "Enabled" : "Disabled");
    dataUart_SendString(msg);
  }
  #endif
}

void Button_ClearSystemControlCallback(uint8_t btnIndex) {
  if (btnIndex >= MAX_BUTTONS) { return; }
  buttons[btnIndex].controlCallback = NULL;
}

// 定时器中断回调
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  if (htim->Instance == TIM7) {
    Button_Scan();  // 10ms扫描一次按键
  }
}

// ==================== 调试函数API ====================
#ifdef DEBUG_BUTTON
void Button_SetButtonName(uint8_t btnIndex, const char* name) {
  if (btnIndex >= MAX_BUTTONS || name == NULL) { return; }
  
  buttons[btnIndex].buttonName = name;
  
  char msg[64];
  snprintf(msg, sizeof(msg), "[Button] Button %d renamed to: %s\r\n", btnIndex, name);
  dataUart_SendString(msg);
}
#endif