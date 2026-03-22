#include "dataPrint.h"

/* Include module headers */
#include "MPU6050DMP.h"
#include "grayscale.h"
#include "button.h"
#include "xsound.h"
#include "cam.h"
#include "ir.h"

/* Static UART handle pointer */
static UART_HandleTypeDef *dataUart_huart = NULL;

int __io_putchar(int ch)
{
#ifdef DEBUG_GENERAL
  if (dataUart_huart == NULL) return -1;
  // Timeout for single character
  HAL_UART_Transmit(dataUart_huart, (uint8_t *)&ch, 1, UART_CHAR_TIMEOUT_MS);
  return ch;
#else
  (void)ch;
  return ch;
#endif
}

int _write(int file, char *ptr, int len)
{
#ifdef DEBUG_GENERAL
  (void)file;  // Unused parameter
  
  if (dataUart_huart == NULL) return -1;
  
  // Send entire buffer at once with timeout
  // Timeout allows ~480 bytes at 9600 baud (960 bytes/sec)
  HAL_StatusTypeDef status = HAL_UART_Transmit(dataUart_huart, (uint8_t *)ptr, len, UART_BUFFER_TIMEOUT_MS);
  
  return (status == HAL_OK) ? len : -1;
#else
  (void)file;
  (void)ptr;
  return len;
#endif
}

void dataUart_Init(UART_HandleTypeDef *huart) {
  dataUart_huart = huart;
}

void dataUart_SendString(const char *str) {
#ifdef DEBUG_GENERAL
  if (str == NULL) return;
  printf("%s", str);
#else
  (void)str;
#endif
}

void dataUart_PrintInitMessage(const char *moduleName) {
#if defined(DEBUG_GENERAL) || defined(DEBUG_MPU6050) || defined(DEBUG_MPU6050_DMP) || defined(DEBUG_IR) || defined(DEBUG_I2C) || defined(DEBUG_MOTORS)
  if (moduleName == NULL) return;
  printf("%s Ready\r\n", moduleName);
#else
  (void)moduleName;
#endif
}

void dataUart_PrintInitError(const char *errorMsg, int statusCode) {
#if defined(DEBUG_GENERAL) || defined(DEBUG_MPU6050) || defined(DEBUG_MPU6050_DMP) || defined(DEBUG_IR) || defined(DEBUG_I2C) || defined(DEBUG_MOTORS)
  if (errorMsg == NULL) return;
  printf("INIT ERROR: %s (Status=0x%X)\r\n", errorMsg, statusCode);
#else
  (void)errorMsg;
  (void)statusCode;
#endif
}

// IR debug functions
HAL_StatusTypeDef ParseAndDisplayIRData(const uint8_t *data, uint16_t size) {
#ifdef DEBUG_IR
  if (data == NULL || size == 0) return HAL_ERROR;
  if (size % 2 != 0 || size > 32) return HAL_ERROR;
  
  printf("Decimal: ");
  for (uint16_t i = 0; (i + 1) < size; i += 2) {
    const uint16_t value = (uint16_t)((data[i+1] << 8) | data[i]);
    printf("%u ", value);
  }
  printf("\r\n");
  return HAL_OK;
#else
  (void)data;
  (void)size;
  return HAL_OK;
#endif
}

void dataUart_PrintIRData(const IR_t *IR_Module) {
#ifdef DEBUG_IR
  if (IR_Module == NULL) return;
  printf("Eye:%d Val:%d ballAngle:%f\r\n", 
         IR_Module->maxEye, IR_Module->maxValue, IR_Module->ballAngle);
#else
  (void)IR_Module;
#endif
}

// Xsound debug functions
void dataUart_PrintXsoundData(const Xsound_t *xsound) {
#ifdef DEBUG_XS
  if (xsound == NULL) return;
  printf("Xsound: Front=%.2f Right=%.2f Left=%.2f Back=%.2f\r\n", 
         xsound->distances[3], xsound->distances[1], 
         xsound->distances[2], xsound->distances[0]);
#else
  (void)xsound;
#endif
}

// Grayscale debug functions
void dataUart_PrintGrayscaleData(const Grayscale_t *grayscale) {
#ifdef DEBUG_GRAYSCALE
  if (grayscale == NULL) return;
  grayscaleCombine();
  printf("Grayscale: ");
  for (int i = 0; i < GRAYSCALE_NUM; i++) {
    printf("%u ", grayscale->values[i]);
  }
  printf("\r\n");
#else
  (void)grayscale;
#endif
}


void dataUart_PrintGrayscaleLineInfo(const GrayscaleLineInfo_t *lineInfo) {
#ifdef DEBUG_GRAYSCALE
  
  if (Grayscale_IsCalibrated())
  {
    const GrayscaleLineInfo_t *gsInfo = Grayscale_GetLineInfo();
    const Grayscale_t *gsData = Grayscale_GetData();
    printf("GS all=%d | onLine : %d,%d,%d,%d,%d,%d | S0~S5: %u,%u,%u,%u,%u,%u | raw S0~S5: %u,%u,%u,%u,%u,%u\r\n",
      Grayscale_IsOnWhiteLine(),
      Grayscale_IsSensorOnWhiteLine(0),
      Grayscale_IsSensorOnWhiteLine(1),
      Grayscale_IsSensorOnWhiteLine(2),
      Grayscale_IsSensorOnWhiteLine(3),
      Grayscale_IsSensorOnWhiteLine(4),
      Grayscale_IsSensorOnWhiteLine(5),
      gsInfo->strengths[0],
      gsInfo->strengths[1],
      gsInfo->strengths[2],
      gsInfo->strengths[3],
      gsInfo->strengths[4],
      gsInfo->strengths[5],
      gsData->values[0],
      gsData->values[1],
      gsData->values[2],
      gsData->values[3],
      gsData->values[4],
      gsData->values[5]
    );
  }
#else
  (void)lineInfo;
#endif
}

// Cam debug functions
void dataUart_PrintCamData(const Cam_t *cam) {
#ifdef DEBUG_CAM
  if (cam == NULL) return;
  printf("Cam Angle: %.2f\r\n", cam->received_angle);
#else
  (void)cam;
#endif
}

// MPU6050 debug functions
void dataUart_PrintMPU6050Euler(const MPU6050_DMP_t *mpuData) {
#ifdef DEBUG_MPU6050_DMP
  if (mpuData == NULL) return;
  printf("Euler: Roll=%.2f Pitch=%.2f Yaw=%.2f\r\n",
         mpuData->euler.roll, mpuData->euler.pitch, mpuData->euler.yaw);
#else
  (void)mpuData;
#endif
}

// Motor debug functions
void dataUart_PrintMotorTest(int motorId) {
#ifdef DEBUG_MOTORS
  printf("\r\n=== Testing Motor %d ===\r\n", motorId);
#else
  (void)motorId;
#endif
}

void dataUart_SendFormattedPWM(uint16_t pwm, float duty_percent) {
#ifdef DEBUG_MOTORS
  printf("PWM: %4u (%.1f%%)\r\n", pwm, duty_percent);
#else
  (void)pwm;
  (void)duty_percent;
#endif
}

// I2C debug functions
HAL_StatusTypeDef DisplayRawHexData(const uint8_t *data, uint16_t size) {
#ifdef DEBUG_I2C
  if (data == NULL || size == 0) return HAL_ERROR;
  
  printf("Raw: ");
  for (uint16_t i = 0; i < size; i++) {
    printf("%02x ", data[i]);
  }
  printf("\r\n");
  return HAL_OK;
#else
  (void)data;
  (void)size;
  return HAL_OK;
#endif
}

void dataUart_PrintI2CError(const char *errorType, int errorCode, int slaveId) {
#ifdef DEBUG_I2C
  if (errorType == NULL) return;
  
  if (slaveId >= 0) {
    printf("%s: Code=0x%X, Slave=%d\r\n", errorType, errorCode, slaveId);
  } else {
    printf("%s: Code=0x%X\r\n", errorType, errorCode);
  }
#else
  (void)errorType;
  (void)errorCode;
  (void)slaveId;
#endif
}

void dataUart_PrintI2CStatus(const char *message) {
#ifdef DEBUG_I2C
  if (message == NULL) return;
  printf("%s\r\n", message);
#else
  (void)message;
#endif
}

void dataUart_PrintDeviceFound(uint16_t addr) {
#ifdef DEBUG_I2C
  printf("Device found at 0x%02X\r\n", addr);
#else
  (void)addr;
#endif
}

// Soccer debug functions
void dataUart_PrintSoccerState(const char *stateName, float ballAngle, float ballDistance, float yawAngle) {
#ifdef DEBUG_SOCCER
  if (stateName == NULL) return;
  printf("Soccer State: %s | Ball: %.1f° %.1f | Yaw: %.1f°\r\n", 
         stateName, ballAngle, ballDistance, yawAngle);
#else
  (void)stateName;
  (void)ballAngle;
  (void)ballDistance;
  (void)yawAngle;
#endif
}

// Button debug functions
void dataUart_PrintButtonEvent(uint8_t btnIndex, const char *buttonName, const char *eventName) {
#ifdef DEBUG_BUTTON
  if (buttonName == NULL || eventName == NULL) return;
  printf("[%s] Event: %s\r\n", buttonName, eventName);
#else
  (void)btnIndex;
  (void)buttonName;
  (void)eventName;
#endif
}

void dataUart_PrintButtonState(uint8_t btnIndex, const char *buttonName, const char *stateName) {
#ifdef DEBUG_BUTTON
  if (buttonName == NULL || stateName == NULL) return;
  printf("[%s] State: %s\r\n", buttonName, stateName);
#else
  (void)btnIndex;
  (void)buttonName;
  (void)stateName;
#endif
}