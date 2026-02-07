#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief PID Controller structure
 * Supports P, PI, PD, and full PID control modes
 */
typedef struct {
  // PID gains
  float kp;           /**< Proportional gain */
  float ki;           /**< Integral gain */
  float kd;           /**< Derivative gain */
  
  // Internal state
  float integral;     /**< Accumulated integral term */
  float prev_error;   /**< Previous error for derivative calculation */
  
  // Anti-windup limits
  float integral_limit;  /**< Maximum integral accumulation */
  
  // Output limits
  int output_min;     /**< Minimum output value */
  int output_max;     /**< Maximum output value */
  
  // Control flags
  bool auto_reset_integral;  /**< Auto reset integral when crossing setpoint */
} PID_Controller_t;

/**
 * @brief Initialize a PID controller
 * 
 * @param pid Pointer to PID controller structure
 * @param kp Proportional gain
 * @param ki Integral gain
 * @param kd Derivative gain
 * @param integral_limit Maximum integral accumulation (anti-windup)
 * @param output_min Minimum output value
 * @param output_max Maximum output value
 */
void PID_Init(PID_Controller_t *pid, float kp, float ki, float kd,
              float integral_limit, int output_min, int output_max);

/**
 * @brief Compute PID output
 * 
 * @param pid Pointer to PID controller structure
 * @param setpoint Target value
 * @param measured Current measured value
 * @return int PID output value (clamped to output_min/max)
 */
int PID_Compute(PID_Controller_t *pid, float setpoint, float measured);

/**
 * @brief Reset PID controller state
 * Clears integral and previous error
 * 
 * @param pid Pointer to PID controller structure
 */
void PID_Reset(PID_Controller_t *pid);

/**
 * @brief Update PID gains
 * 
 * @param pid Pointer to PID controller structure
 * @param kp Proportional gain
 * @param ki Integral gain
 * @param kd Derivative gain
 */
void PID_SetGains(PID_Controller_t *pid, float kp, float ki, float kd);

/**
 * @brief Enable/disable auto integral reset when crossing setpoint
 * 
 * @param pid Pointer to PID controller structure
 * @param enable true to enable, false to disable
 */
void PID_SetAutoResetIntegral(PID_Controller_t *pid, bool enable);

#endif // PID_CONTROLLER_H
