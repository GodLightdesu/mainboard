#ifndef ATTACK_H
#define ATTACK_H

#include "soccer.h"
#include "PID.h"

#define BALL_CHASE_SPEED_BONUS 25 /**< Additional speed when chasing ball */
#define BOUND_MOVE_SPEED 70       /**< Speed for boundary correction movement */

/* Corridor Correction PID - 用於追球時的偏航修正 */
#define ATK_CORR_kp 0.25f      // 稍微提高修正力度
#define ATK_CORR_ki 0.f        // 降低積分避免振盪
#define ATK_CORR_kd 2.0f       // 增加阻尼穩定性

#define BALL_BIG_RADIUS 1200.0f // R = Max eye value * tan(theta)
#define POSSIBLE_MAX_BALL_VALUE 2000 /**< Maximum possible value from IR sensor when ball is very close, used for angle correction calculation */
#define BALL_CLOSE_VALUE_THRESHOLD 1500 /**< Threshold for considering the ball "close" and applying additional speed bonus */
#define BALL_CLOSE_SPEED_BONUS 10 /**< Additional speed bonus when ball is close */

/* Angle correction thresholds for ball chasing */
#define ANGLE_CORR_MIN_THRESHOLD 20.0f   /**< Minimum angle for correction (degrees) */
#define ANGLE_CORR_MAX_THRESHOLD 340.0f  /**< Maximum angle for correction (degrees) */
#define ANGLE_HALF_CIRCLE 180.0f         /**< Half circle angle (degrees) */

#define CAM_ANGLE_MAX 40.0f   /**< Maximum angle from camera for correction (degrees) */

#define GS_STR_MIN_THRESHOLD 20

typedef enum {
  ATTACK_STATE_IDLE = 0,      /**< Robot stopped, waiting for commands */
  ATTACK_STATE_SEARCH_BALL,   /**< Return to center if lost ball for too long */
  ATTACK_STATE_CHASE_BALL,    /**< Moving towards detected ball */
  ATTACK_STATE_ALIGN_YAW,     /**< Correcting yaw orientation */
  ATTACK_STATE_OUT_OF_BOUNDS, /**< Detected out of playing field */
} AttackState_t;

void AttackInit(void);
void AttackMode(const ModuleData_t *data);
AttackState_t getAttackState(void);

#endif /* ATTACK_H */