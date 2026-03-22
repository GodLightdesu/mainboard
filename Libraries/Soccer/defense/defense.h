#ifndef DEFENSE_H
#define DEFENSE_H

/* Corridor Correction PID */
#define DEF_CORR_kp 0.25f      // 稍微提高修正力度
#define DEF_CORR_ki 0.f        // 降低積分避免振盪
#define DEF_CORR_kd 2.0f       // 增加阻尼穩定性

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