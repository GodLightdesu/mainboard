#ifndef CONST_H
#define CONST_H

#define POWER_ON_DELAY_CYCLES   1000000U
#define LED_HEARTBEAT_MS        100U
#define IR_SAMPLE_PERIOD_MS     20U
#define MAIN_LOOP_DELAY_MS      1U
#define IR_DETECTION_THRESHOLD  10U

#define DMA_BUFFER_ADDRESS 0x30000000

/* IR sensor */
#define IR_BUFFER_SIZE 16       /**< Buffer size: Vref + 7 sensors, 2 bytes each */
#define IR_SLAVE1_RXBUF_PTR ((uint8_t *)DMA_BUFFER_ADDRESS)
#define IR_SLAVE2_RXBUF_PTR ((uint8_t *)DMA_BUFFER_ADDRESS + IR_BUFFER_SIZE)

#endif