#ifndef ATTACK_H
#define ATTACK_H

#include "soccer.h"
#include "PID.h"

#define BALL_CHASE_SPEED_BONUS 25       /**< Additional speed when chasing ball */

/* Corridor Correction PID - 用於追球時的偏航修正 */
#define CORR_kp 0.25f      // 稍微提高修正力度
#define CORR_ki 0.f        // 降低積分避免振盪
#define CORR_kd 2.0f       // 增加阻尼穩定性

#define POSSIBLE_MAX_BALL_VALUE 2500 /**< Maximum possible value from IR sensor when ball is very close, used for angle correction calculation */
#define BALL_CLOSE_VALUE_THRESHOLD 1000 /**< Threshold for considering the ball "close" and applying additional speed bonus */
#define BALL_CLOSE_SPEED_BONUS 10 /**< Additional speed bonus when ball is close */

/* Angle correction thresholds for ball chasing */
#define ANGLE_CORR_MIN_THRESHOLD 10.0f   /**< Minimum angle for correction (degrees) */
#define ANGLE_CORR_MAX_THRESHOLD 350.0f  /**< Maximum angle for correction (degrees) */
#define ANGLE_HALF_CIRCLE 180.0f         /**< Half circle angle (degrees) */

#define CAM_ANGLE_MAX 30.0f   /**< Maximum angle from camera for correction (degrees) */

typedef enum {
  ATTACK_STATE_IDLE = 0,      /**< Robot stopped, waiting for commands */
  ATTACK_STATE_SEARCH_BALL,   /**< Rotating to search for ball */
  ATTACK_STATE_CHASE_BALL,    /**< Moving towards detected ball */
  ATTACK_STATE_ALIGN_YAW,     /**< Correcting yaw orientation */
  ATTACK_STATE_OUT_OF_BOUNDS, /**< Detected out of playing field */
  ATTACK_STATE_RETURN_TO_CENTER, /**< Return to center if lost ball for too long */
} AttackState_t;

void AttackInit(void);
void AttackMode(const ModuleData_t *data);
AttackState_t getAttackState(void);

#endif /* ATTACK_H */