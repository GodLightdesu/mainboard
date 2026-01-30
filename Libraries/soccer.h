#ifndef SOCCER
#define SOCCER

#include "ir.h"
#include "button.h"
#include "data_uart.h"
#include "motors.h"
#include "MPU6050.h"
#include "MPU6050_DMP.h"
#include <stdbool.h>
#include <stdint.h>

/* Soccer control constants */
#define YAW_CORRECTION_THRESHOLD 10.0f  /**< Yaw angle threshold for compass correction (degrees) */
#define BASE_SPEED 40                   /**< Base motor speed */
#define BALL_CHASE_SPEED_BONUS 10       /**< Additional speed when chasing ball */

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
  const IR_t* irData;
  const MPU6050_t* mpuData;
  const MPU6050_DMP_t* dmpData;
  // Add new module pointers here as needed
} ModuleData_t;

void SoccerInit(void);
void updateData();
void soccer_ProcessData(const ModuleData_t *data);

State_t getState();

void Soccer_WaitForStart(void);
void Soccer_StartSystem(void);
void Soccer_EmergencyStop(void);
void Soccer_ResetEmergencyStop(void);
bool Soccer_IsSystemStarted(void);
bool Soccer_IsEmergencyStop(void);

#endif