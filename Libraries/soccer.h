#ifndef SOCCER_H
#define SOCCER_H

#include "ir.h"
#include "button.h"
#include "dataPrint.h"
#include "motors.h"
#include "MPU6050DMP.h"
#include "PID.h"
#include <stdbool.h>
#include <stdint.h>
#include "iwdg.h"

/* Soccer control constants */
#define REAL_MAX_SPEED 65               /**< Maximum motor speed */
#define BASE_SPEED 35                   /**< Base motor speed */
#define BALL_CHASE_SPEED_BONUS 25       /**< Additional speed when chasing ball */
/* Yaw Alignment PID - 用於原地旋轉對齊 */
#define YAW_PID_kp 0.25f    // 提高響應速度
#define YAW_PID_ki 0.0f     // 降低積分避免超調
#define YAW_PID_kd 2.0f     // 增加阻尼穩定性

/* Corridor Correction PID - 用於追球時的偏航修正 */
#define CORR_kp 0.25f      // 稍微提高修正力度
#define CORR_ki 0.f        // 降低積分避免振盪
#define CORR_kd 2.0f       // 增加阻尼穩定性

#define YAW_THRESHOLD 10.0f             /**< Yaw angle threshold for stopping correction (degrees) */
#define INTEGRAL_LIMIT 500.0f           /**< Maximum integral accumulation for anti-windup */

#define POSSIBLE_MAX_BALL_VALUE 2500 /**< Maximum possible value from IR sensor when ball is very close, used for angle correction calculation */

typedef enum {
  STATE_IDLE = 0,        /**< Robot stopped, waiting for commands */
  STATE_SEARCH_BALL,     /**< Rotating to search for ball */
  STATE_CHASE_BALL,      /**< Moving towards detected ball */
  STATE_ALIGN_YAW,       /**< Correcting yaw orientation */
  STATE_OUT_OF_BOUNDS    /**< Detected out of playing field */
} State_t;

/**
 * @brief Structure containing pointers to all module data
 * This allows easy extension when adding new modules
 */
typedef struct {
  const IR_t *irData;
  const MPU6050_DMP_t *mpuData;
  // Add new module pointers here as needed
} ModuleData_t;

void SoccerInit(void);
void updateData();
void soccer_ProcessData(const ModuleData_t *data);

State_t getState();
void setState(State_t newState);
State_t updateState(const ModuleData_t *data);

void Soccer_WaitForStart(void);
void Soccer_StartSystem(void);
void Soccer_EmergencyStop(void);
void Soccer_ResetEmergencyStop(void);
bool Soccer_IsSystemStarted(void);
bool Soccer_IsEmergencyStop(void);

#endif