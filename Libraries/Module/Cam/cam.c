#include "cam.h"
#include "const.h"

#define CAM_PACKET_HEADER 0xFFu
#define CAM_PACKET_FOOTER 0xFEu

static Cam_t cam;
static UART_HandleTypeDef *cam_uart = NULL;

static void cam_start_rx_dma(void) {
	if (cam_uart == NULL) {
		return;
	}

  // Error may due to no connection or wrong wiring, so we just try to restart it without halting the system
	if (HAL_UART_Receive_DMA(cam_uart, CAM_RXBUF_PTR, 1) != HAL_OK) {
		// Error_Handler();
	}
}

static void cam_parse_byte(uint8_t byte) {
	switch (cam.state) {
	case WAIT_HEADER:
		if (byte == CAM_PACKET_HEADER) {
			cam.rx_index = 0;
			cam.state = WAIT_DATA;
		}
		break;

	case WAIT_DATA:
		cam.rx_buffer[cam.rx_index++] = byte;
		if (cam.rx_index >= sizeof(cam.rx_buffer)) {
			cam.state = WAIT_FOOTER;
		}
		break;

	case WAIT_FOOTER:
		if (byte == CAM_PACKET_FOOTER) {
			memcpy(&cam.received_angle, cam.rx_buffer, sizeof(cam.received_angle));
			cam.dataReady = true;
		}
		cam.state = WAIT_HEADER;
		cam.rx_index = 0;
		break;

	default:
		cam.state = WAIT_HEADER;
		cam.rx_index = 0;
		break;
	}
}

void cam_init(UART_HandleTypeDef *huart) {
	cam_uart = huart;
	memset(&cam, 0, sizeof(cam));
	cam.state = WAIT_HEADER;
	cam_start_rx_dma();
}

bool cam_data_ready(void) {
	bool ready;
	__disable_irq();
	ready = cam.dataReady;
	__enable_irq();
	return ready;
}

float cam_get_angle(void) {
	float angle;
	__disable_irq();
	angle = cam.received_angle;
	cam.dataReady = false;
	__enable_irq();
	return angle;
}

const Cam_t* Cam_GetData(void) {
	return &cam;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	if ((cam_uart != NULL) && (huart->Instance == cam_uart->Instance)) {
		cam_parse_byte(*CAM_RXBUF_PTR);
		cam_start_rx_dma();
	}
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
	if ((cam_uart != NULL) && (huart->Instance == cam_uart->Instance)) {
		cam.state = WAIT_HEADER;
		cam.rx_index = 0;
		cam_start_rx_dma();
	}
}