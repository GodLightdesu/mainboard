#ifndef DEFENSE_H
#define DEFENSE_H

/* Corridor Correction PID */
#define DEF_CORR_kp 0.25f      // 稍微提高修正力度
#define DEF_CORR_ki 0.f        // 降低積分避免振盪
#define DEF_CORR_kd 2.0f       // 增加阻尼穩定性

// Defense movement speeds
#define DEF_BOUND_SPEED_STRAIGHT 50
#define DEF_BOUND_SPEED_DIAGONAL 50
#define DEF_BLOCK_SPEED 65
#define DEF_RETURN_SPD 65
#define DEF_BOUND_MOVE_SPEED 65       /**< Speed for boundary correction movement */

// Xsound thresholds for boundary avoidance
#define DEF_L_XS_THRESHOLD_CLOSE 70   // larger to more middle
#define DEF_R_XS_THRESHOLD_CLOSE 70   // larger to more middle
#define DEF_XSOUND_THRESHOLD_TOO_FAR 100
#define DEF_XSOUND_THRESHOLD_FAR 55
#define DEF_XSOUND_THRESHOLD_CLOSE 40

#define DEF_BALL_THRESHOLD 300

#define DEF_ANGLE_CORR_MIN_THRESHOLD 25.0f   /**< Minimum angle for correction (degrees) */
#define DEF_ANGLE_CORR_MAX_THRESHOLD 335.0f  /**< Maximum angle for correction (degrees) */

#include "soccer.h"

typedef enum {
  DEFENSE_STATE_IDLE = 0,    /**< Robot stopped, waiting for commands */
  DEFENSE_STATE_RETURN_GOAL, /**< Moving back to goal area */
  DEFENSE_STATE_BLOCK_SHOT,  /**< Lunging forward to block */
  DEFENSE_STATE_ALIGN_YAW,     /**< Correcting yaw orientation */
  DEFENSE_STATE_OUT_OF_BOUNDS,/**< Detected out of playing field */
} DefenseState_t;

void DefenseInit(void);
void DefenseMode(const ModuleData_t *data);
DefenseState_t getDefenseState(void);

#endif /* DEFENSE_H */