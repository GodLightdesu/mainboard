#include "PID.h"

void PID_Init(PID_Controller_t *pid, float kp, float ki, float kd,
              float integral_limit, int output_min, int output_max) {
  pid->kp = kp;
  pid->ki = ki;
  pid->kd = kd;
  pid->integral = 0.0f;
  pid->prev_error = 0.0f;
  pid->integral_limit = integral_limit;
  pid->output_min = output_min;
  pid->output_max = output_max;
  pid->auto_reset_integral = true;  // Default: enabled
}

int PID_Compute(PID_Controller_t *pid, float setpoint, float measured) {
  // Calculate error
  float error = setpoint - measured;
  
  // Proportional term
  float p_term = pid->kp * error;
  
  // Integral term with anti-windup
  pid->integral += error;
  if (pid->integral > pid->integral_limit) {
    pid->integral = pid->integral_limit;
  } else if (pid->integral < -pid->integral_limit) {
    pid->integral = -pid->integral_limit;
  }
  float i_term = pid->ki * pid->integral;
  
  // Derivative term
  float derivative = error - pid->prev_error;
  float d_term = pid->kd * derivative;
  
  // Auto reset integral when crossing setpoint (optional)
  if (pid->auto_reset_integral) {
    if ((error > 0 && pid->prev_error < 0) || (error < 0 && pid->prev_error > 0)) {
      pid->integral = 0.0f;
      i_term = 0.0f;
    }
  }
  
  // Update previous error for next iteration
  pid->prev_error = error;
  
  // Calculate output
  float output = p_term + i_term + d_term;
  
  // Clamp output to limits
  int output_int = (int)output;
  if (output_int > pid->output_max) {
    output_int = pid->output_max;
  } else if (output_int < pid->output_min) {
    output_int = pid->output_min;
  }
  
  return output_int;
}

void PID_Reset(PID_Controller_t *pid) {
  pid->integral = 0.0f;
  pid->prev_error = 0.0f;
}

void PID_SetGains(PID_Controller_t *pid, float kp, float ki, float kd) {
  pid->kp = kp;
  pid->ki = ki;
  pid->kd = kd;
}

void PID_SetAutoResetIntegral(PID_Controller_t *pid, bool enable) {
  pid->auto_reset_integral = enable;
}
