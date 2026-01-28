#include "soccer.h"
#include "mpu6050.h"

static State_t state;

State_t getState() { return state; }

void updateData() {
  /* Process IR state machine */
  IR_Process();

  MPU6050_Process();
  
  /* Update DMP attitude estimation */
  MPU6050_DMP_Update();
}