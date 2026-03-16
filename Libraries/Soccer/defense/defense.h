#ifndef DEFENSE_H
#define DEFENSE_H

#include "soccer.h"

typedef enum {
  DEFENSE_STATE_IDLE = 0,     /**< Robot stopped, waiting for commands */
  DEFENSE_STATE_TRACK_BALL,   /**< Moving left/right to block ball */
  DEFENSE_STATE_RETURN_GOAL,  /**< Moving back to goal area */
  DEFENSE_STATE_BLOCK_SHOT,   /**< Lunging forward to block */
  DEFENSE_STATE_OUT_OF_BOUNDS,/**< Detected out of playing field */
} DefenseState_t;

void DefenseInit(void);
void DefenseMode(const ModuleData_t *data);
DefenseState_t getDefenseState(void);

#endif /* DEFENSE_H */