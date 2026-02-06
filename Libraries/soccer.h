#ifndef SOCCER_H
#define SOCCER_H

#include "ir.h"
#include "button.h"
#include "dataPrint.h"
#include "motors.h"
#include "MPU6050DMP.h"
#include <stdbool.h>
#include <stdint.h>
#include "iwdg.h"

/* Soccer control constants */
#define REAL_MAX_SPEED 55               /**< Maximum motor speed */
#define BASE_SPEED 25                   /**< Base motor speed */
#define BALL_CHASE_SPEED_BONUS 10       /**< Additional speed when chasing ball */
#define YAW_PID_KD 0.3                /**< Derivative gain for yaw PID control */
#define CORR_KD 0.30f                   /**< Derivative gain for corridor correction PID */
#define YAW_THRESHOLD 10.0f             /**< Yaw angle threshold for stopping correction (degrees) */

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
void SoccerPIDCompCar(float kd, float yaw, float target_yaw);
int SoccerPIDCompCorr(float kd, float yaw, float target_yaw);

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