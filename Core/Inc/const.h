#ifndef CONST_H
#define CONST_H

/* Application configuration constants */
#define POWER_ON_DELAY_CYCLES   1000000U  /* Capacitor stabilization delay */
#define LED_HEARTBEAT_MS        100U      /* LED toggle interval (100ms = 5Hz blink) */
#define IR_SAMPLE_PERIOD_MS     20U       /* IR sensor data request interval (50Hz) */
#define MAIN_LOOP_DELAY_MS      1U        /* Small delay to prevent CPU hogging */

#define DMA_BUFFER_ADDRESS 0x30000000 /**< DMA buffer base address */

/* IR sensor */
#define IR_BUFFER_SIZE 16       /**< Buffer size: Vref + 7 sensors, 2 bytes each */
#define IR_SLAVE1_RXBUF_PTR ((uint8_t *)DMA_BUFFER_ADDRESS)
#define IR_SLAVE2_RXBUF_PTR ((uint8_t *)DMA_BUFFER_ADDRESS + IR_BUFFER_SIZE)

#endif // CONST_H