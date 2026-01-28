#ifndef SOCCER
#define SOCCER

#include "ir.h"
#include "mpu6050.h"
#include "mpu6050_dmp.h"
#include <stdint.h>
#include <stdbool.h>

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

void updateData();
State_t getState();

/**
 * @brief Process sensor data and update robot state machine
 * @param data Pointer to structure containing all module data
 */
void soccer_ProcessData(const ModuleData_t* data);

#endif