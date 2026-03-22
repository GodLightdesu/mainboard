/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "i2c.h"
#include "iwdg.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>

// Modules' headers
#include "ir.h"
#include "cam.h"
#include "motors.h"
#include "button.h"
#include "xsound.h"
#include "grayscale.h"
#include "MPU6050DMP.h"
#include "dataPrint.h"
#include "soccer.h"
#include "const.h"
#include "led.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  * @brief  Jump to system bootloader
  * @note   This function will jump to the STM32 system bootloader
  *         For STM32H750, bootloader is located at 0x1FF09800
  * @retval None
  */
void JumpToBootloader(void)
{
  /* Define bootloader address for STM32H750 */
  #define BOOTLOADER_ADDRESS 0x1FF09800
  
  void (*SysMemBootJump)(void);
  
  /* Disable all interrupts */
  __disable_irq();
  
  /* Disable SysTick */
  SysTick->CTRL = 0;
  SysTick->LOAD = 0;
  SysTick->VAL = 0;
  
  /* Disable GPIOD clock that was enabled for button check */
  RCC->AHB4ENR &= ~RCC_AHB4ENR_GPIODEN;
  
  /* Disable and clean data cache */
  #if (__DCACHE_PRESENT == 1U)
  SCB_DisableDCache();
  #endif
  
  /* Disable and invalidate instruction cache */
  #if (__ICACHE_PRESENT == 1U)
  SCB_DisableICache();
  SCB_InvalidateICache();
  #endif
  
  /* Disable MPU */
  HAL_MPU_Disable();
  
  /* Clear all interrupt enable registers & pending interrupts */
  for (uint32_t i = 0; i < 8; i++)
  {
    NVIC->ICER[i] = 0xFFFFFFFF;  /* Disable all interrupts */
    NVIC->ICPR[i] = 0xFFFFFFFF;  /* Clear all pending interrupts */
  }
  
  /* Reset all interrupt priorities */
  for (uint32_t i = 0; i < 8; i++)
  {
    NVIC->IP[i] = 0x00000000;
  }
  
  /* Reset vector table offset register */
  SCB->VTOR = 0x00000000;
  
  /* Set main stack pointer to bootloader's initial stack pointer */
  __set_MSP(*(uint32_t *)BOOTLOADER_ADDRESS);
  
  /* Get bootloader reset handler address */
  SysMemBootJump = (void (*)(void)) (*((uint32_t *)(BOOTLOADER_ADDRESS + 4)));
  
  /* Enable all interrupts before jumping */
  __enable_irq();
  
  /* Call the bootloader reset handler */
  SysMemBootJump();
  
  /* Jump is done successfully, this should not be reached */
  while (1);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  /* Power-on delay to allow capacitors to stabilize */
  for(volatile uint32_t i = 0; i < POWER_ON_DELAY_CYCLES; i++);
  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C2_Init();
  MX_I2C3_Init();
  MX_UART4_Init();
  MX_UART5_Init();
  MX_UART7_Init();
  MX_UART8_Init();
  MX_ADC1_Init();
  MX_ADC3_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_TIM7_Init();
  // MX_IWDG1_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
  /* Initialize status LEDs */
  LED_SetAll(true);

  /* Initialize application modules */
  dataUart_Init(&huart4); /* UART for data output */

  HAL_Delay(I2C_INIT_DELAY_MS); // wait 10ms

  grayscaleInit(); /* Initialize grayscale sensors */

  /* Initialize I2C bus manager for shared I2C3 peripheral */
  I2C_BusManager_t i2c3_bus;
  I2C_Bus_Init(&i2c3_bus, &hi2c3);

  /* Initialize IR sensor module with I2C peripheral */
  IR_Init(&hi2c3);

  /* Initialize Xsound sensor module with I2C peripheral */
  Xsound_Init(&hi2c3);

  /* Initialize Camera module with UART peripheral */
  cam_init(&huart8);

  /* Initialize MPU6050 with retry loop */
  uint16_t addr = 0x68;     // mpu6050
  int result = -1;
  while (result != 0) {
    if (I2C_Find(&MPU6050_I2C_HANDLE, addr)) {
      result = MPU6050_DMP_Init();
      if (result != 0) {
        #ifdef DEBUG_MPU6050_DMP
        printf("MPU6050 DMP Init failed (retrying), Status=0x%X\r\n", result);
        #endif
        HAL_Delay(MPU6050_INIT_RETRY_DELAY_MS);  // Wait before retry
      }
    } else {
      #ifdef DEBUG_MPU6050_DMP
      printf("MPU6050 not found (retrying)\r\n");
      #endif
      HAL_Delay(MPU6050_INIT_RETRY_DELAY_MS);  // Wait before retry
    }
    HAL_IWDG_Refresh(&hiwdg1);  // Refresh watchdog during init
  }
  #ifdef DEBUG_MPU6050_DMP
  printf("MPU6050 initialized successfully!\r\n");
  #endif

  /* Create module data struct once */
  static ModuleData_t moduleData;
  moduleData.irData = IR_GetData();
  moduleData.camData = Cam_GetData();
  moduleData.xsoundData = Xsound_GetData();
  moduleData.mpuData = MPU6050DMP_GetData();
  moduleData.grayscaleData = Grayscale_GetData();

  /* Initialize button module */
  Button_Init();
  #ifdef DEBUG_BUTTON
  Button_SetButtonName(0, "START_BTN");
  Button_SetButtonName(1, "STOP_BTN");
  Button_SetButtonName(2, "RESET_YAW_BTN");
  Button_SetButtonName(3, "GS_SCAN_BTN");
  #endif
  HAL_TIM_Base_Start_IT(&htim7);

  /* Initialize motor control */
  Mtrs_Init();

  /* Initialize soccer control logic */
  // SoccerInit(MODE_DEFENSIVE);
  SoccerInit(MODE_OFFENSIVE);

  /* Check for IWDG reset - auto-start system if reset occurred */
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDG1RST)) {
    /* IWDG reset detected - auto-start system */
    __HAL_RCC_CLEAR_RESET_FLAGS();
    dataUart_SendString("IWDG Reset Detected - Auto Starting System\r\n");
    Soccer_StartSystem();
    LED_Set(LED_4, true);
  } else {
    /* Normal startup - wait for button press */
    Soccer_WaitForStart();
  }

  /* Initialize timing variables */
  uint32_t lastLedToggleTime = HAL_GetTick();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    // Refresh watchdog to prevent reset
    HAL_IWDG_Refresh(&hiwdg1);

    // Heartbeat LED flash to indicate system is alive
    LED_HeartbeatTick(LED_2, &lastLedToggleTime, HAL_GetTick(), LED_HEARTBEAT_MS);

    /* Update all sensor data */
    updateData();

    // /* Process data in soccer module */
    SoccerProcess(&moduleData);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    /* Debug prints for sensor data - enable as needed */
    // dataUart_PrintIRData(moduleData.irData);
    // dataUart_PrintXsoundData(moduleData.xsoundData);
    // dataUart_PrintGrayscaleData(moduleData.grayscaleData);
    // dataUart_PrintGrayscaleLineInfo(Grayscale_GetLineInfo());
    // dataUart_PrintCamData(moduleData.camData);
    // dataUart_PrintMPU6050Euler(moduleData.mpuData);

    // i2c scan
    // uint16_t addrFound = 0;
    // I2C_HandleTypeDef testI2C = hi2c3;  // Use I2C3 for scanning
    // for (uint16_t addr = 1; addr < 128; addr++) {
    //   if (I2C_Find(&testI2C, addr)) {
    //     addrFound = addr;
    //   }
    // }
    // if (addrFound == 0) {
    //   printf("No I2C devices found on i2c3\r\n");
    // }

    // Test: Move right at half speed
    // polarMove(90, 50);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 60;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Initializes the peripherals clock
  */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInitStruct.PLL2.PLL2M = 4;
  PeriphClkInitStruct.PLL2.PLL2N = 10;
  PeriphClkInitStruct.PLL2.PLL2P = 2;
  PeriphClkInitStruct.PLL2.PLL2Q = 2;
  PeriphClkInitStruct.PLL2.PLL2R = 2;
  PeriphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_3;
  PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOMEDIUM;
  PeriphClkInitStruct.PLL2.PLL2FRACN = 0;
  PeriphClkInitStruct.AdcClockSelection = RCC_ADCCLKSOURCE_PLL2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Number = MPU_REGION_NUMBER1;
  MPU_InitStruct.BaseAddress = 0x30000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_256B;
  MPU_InitStruct.SubRegionDisable = 0x0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
