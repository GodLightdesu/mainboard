#ifndef CONST_H
#define CONST_H

#define POWER_ON_DELAY_CYCLES   1000000U
#define LED_HEARTBEAT_MS        100U
#define IR_SAMPLE_PERIOD_MS     20U
#define MAIN_LOOP_DELAY_MS      10U

/* Debug and timing constants */
#define DEBUG_PRINT_INTERVAL_MS     300U
#define LONG_PRESS_HOLD_REPORT_MS   500U
#define LED_WAIT_BLINK_MS           500U
#define UART_CHAR_TIMEOUT_MS        50U
#define UART_BUFFER_TIMEOUT_MS      500U
#define MPU6050_INIT_RETRY_DELAY_MS 100U

/* I2C timing */
#define I2C_INIT_DELAY_MS       10U

#define DMA_BUFFER_ADDRESS 0x30000000
#define DMA_CACHE_LINE_SIZE 32  /**< STM32H7 D-Cache line size */

/* IR sensor - aligned to 32-byte boundaries */
#define IR_BUFFER_SIZE 16       /**< Buffer size: Vref + 7 sensors, 2 bytes each */
#define IR_SLAVE1_RXBUF_PTR ((uint8_t *)(DMA_BUFFER_ADDRESS))
#define IR_SLAVE2_RXBUF_PTR ((uint8_t *)(DMA_BUFFER_ADDRESS + DMA_CACHE_LINE_SIZE))

/* MPU6050 - aligned to 32-byte boundary */
#define MPU6050_BUFFER_SIZE 14  /**< Buffer size: accel(6) + temp(2) + gyro(6) */
#define MPU6050_RXBUF_PTR ((uint8_t *)(DMA_BUFFER_ADDRESS + (2 * DMA_CACHE_LINE_SIZE)))

/* Grayscale ADC - aligned to 32-byte boundary */
#define GRAYSCALE_BUFFER_SIZE 10  /**< Buffer size: 5 sensors * 2 bytes each */
#define GRAYSCALE_RXBUF_PTR ((uint16_t *)(DMA_BUFFER_ADDRESS + (3 * DMA_CACHE_LINE_SIZE)))

/* Xsound - aligned to 32-byte boundary */
#define XSOUND_SAMPLE_PERIOD_MS 50U
#define XSOUND_BUFFER_SIZE 16   /**< Buffer size: 4 float distance values * 4 bytes each */
#define XSOUND_RXBUF_PTR ((uint8_t *)(DMA_BUFFER_ADDRESS + (4 * DMA_CACHE_LINE_SIZE)))

/* Cam - aligned to 32-byte boundary */
#define CAM_RXBUF_PTR ((uint8_t *)(DMA_BUFFER_ADDRESS + (5 * DMA_CACHE_LINE_SIZE)))

/* Total DMA buffer layout (192 bytes, 6 cache lines):
0x30000000 - 0x3000001F (0-31):    IR_SLAVE1    (16 bytes, padded to 32)
0x30000020 - 0x3000003F (32-63):   IR_SLAVE2    (16 bytes, padded to 32)
0x30000040 - 0x3000005F (64-95):   MPU6050      (14 bytes, padded to 32)
0x30000060 - 0x3000007F (96-127):  GRAYSCALE    (10 bytes, padded to 32)
0x30000080 - 0x3000009F (128-159): XSOUND       (16 bytes, padded to 32)
0x300000A0 - 0x300000BF (160-191): CAM          (1 byte, padded to 32)
*/

/* Mathematical constants */
#ifndef PI
#define PI 3.14159265358979323846f
#endif
#define PI_DIV_4   0.785398163397448f     /**< PI/4 precomputed for phase offset calculations */
#define DEG_TO_RAD 0.017453292519943295f  /**< PI/180 for degree to radian conversion */
#define RAD_TO_DEG 57.295779513082320877f /**< 180/PI for radian to degree conversion */

/* Utility macros */
/**
 * @brief Calculate time difference handling 32-bit overflow
 * @note HAL_GetTick() overflows after ~49.7 days, this macro handles it correctly
 */
#define TIME_DIFF(now, prev) ((uint32_t)((now) - (prev)))

#endif