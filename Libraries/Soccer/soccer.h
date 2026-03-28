#ifndef SOCCER_H
#define SOCCER_H

#include <stdbool.h>

#include "iwdg.h"
#include "arm_math.h"
#include "dataPrint.h"
#include "const.h"

#include "ir.h"
#include "led.h"
#include "cam.h"
#include "PID.h"
#include "button.h"
#include "motors.h"
#include "xsound.h"
#include "grayscale.h"
#include "MPU6050DMP.h"

/* Soccer control constants */
#define REAL_MAX_SPEED 70               /**< Maximum motor speed */
#define BASE_SPEED 35                   /**< Base motor speed */

/* Yaw Alignment PID - 用於原地旋轉對齊 */
#define YAW_PID_kp 0.4f     // 提高響應速度
#define YAW_PID_ki 0.0f     // 降低積分避免超調
#define YAW_PID_kd 2.0f     // 增加阻尼穩定性
#define YAW_THRESHOLD 7.0f             /**< Yaw angle threshold for stopping correction (degrees) */
#define INTEGRAL_LIMIT 500.0f           /**< Maximum integral accumulation for anti-windup */

#define BALL_BIG_RADIUS 1200.0f         /**< R = Max eye value * tan(theta) */
#define POSSIBLE_MAX_BALL_VALUE 2000    /**< Maximum possible value from IR sensor when ball is very close, used for angle correction calculation */
#define BALL_CLOSE_VALUE_THRESHOLD 1500 /**< Threshold for considering the ball "close" and applying additional speed bonus */
#define BALL_CLOSE_SPEED_BONUS 25       /**< Additional speed bonus when ball is close */

#define ANGLE_HALF_CIRCLE 180.0f        /**< Half circle angle (degrees) */

// Grayscale boundary correction
#define GS_STR_MIN_THRESHOLD 20

typedef enum {
  MODE_DEFENSIVE = 0, /**< Stay near own goal and defend */
  MODE_OFFENSIVE,     /**< Actively chase ball and attempt to score */
  MODE_NUM = 2        /**< Total number of modes */
} Mode_t;

/**
 * @brief Structure containing pointers to all module data
 * This allows easy extension when adding new modules
 */
typedef struct {
  const IR_t *irData;
  const Cam_t *camData;
  const Xsound_t *xsoundData;
  const MPU6050_DMP_t *mpuData;
  const Grayscale_t *grayscaleData;
  // Add new module pointers here as needed
} ModuleData_t;

void SoccerInit(Mode_t mode);

void updateData(void);
void updateMode(const ModuleData_t *data);  // TODO
void SoccerProcess(const ModuleData_t *data);

void Soccer_WaitForStart(void);
void Soccer_StartSystem(void);
void Soccer_EmergencyStop(void);
void Soccer_ResetEmergencyStop(void);
bool Soccer_IsSystemStarted(void);
bool Soccer_IsEmergencyStop(void);

void compassCar(float target_yaw, float yaw);
void boundMove(const ModuleData_t *data, PID_Controller_t *yawPid, int moveSpeed);

#endif