#ifndef CONST_H
#define CONST_H

#define POWER_ON_DELAY_CYCLES   1000000U
#define LED_HEARTBEAT_MS        100U
#define IR_SAMPLE_PERIOD_MS     20U
#define MAIN_LOOP_DELAY_MS      10U

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

/* Total DMA buffer layout (128 bytes, 4 cache lines):
0x30000000 - 0x3000001F (0-31):    IR_SLAVE1    (16 bytes, padded to 32)
0x30000020 - 0x3000003F (32-63):   IR_SLAVE2    (16 bytes, padded to 32)
0x30000040 - 0x3000005F (64-95):   MPU6050      (14 bytes, padded to 32)
0x30000060 - 0x3000007F (96-127):  GRAYSCALE    (10 bytes, padded to 32)
*/

#endif