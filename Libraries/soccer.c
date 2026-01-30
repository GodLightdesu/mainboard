#include "soccer.h"

static State_t state;

State_t getState() { return state; }

void updateData() {
  /* Process IR state machine */
  IR_Process();

  /* Process MPU6050 state machine */
  MPU6050_Process();

  MPU6050_DMP_Update();
  
}

void soccer_ProcessData(const ModuleData_t* data) {  
  // Get Euler angles from DMP
  EulerAngles_t euler;
  if (MPU6050_DMP_GetEulerAngles(&euler)) {
    float yaw = euler.yaw;
    // float roll = euler.roll;
    // float pitch = euler.pitch;

    // for (MtrID_t mtr_id = MTR0; mtr_id < 4; ++mtr_id) {
    //   mtr_SetDecayMode(mtr_id, SLOW_DECAY);
    // }
    int8_t spd = 30;
    if (yaw > 10) {
      mtrs_Set4Speed(-spd, -spd, -spd, -spd);
    } else if (yaw < -10) {
      mtrs_Set4Speed(spd, spd, spd, spd);
    } else {
      // chase ball
      if (IR_IsDataReady() && data->irData->ballAngle >= 0) {
        polarMove(data->irData->ballAngle, spd + 10);
      } else {
        mtrs_StopAll();
      }
    }
  }
}