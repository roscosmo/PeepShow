/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32u5xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define BTN_B_Pin GPIO_PIN_2
#define BTN_B_GPIO_Port GPIOC
#define BTN_B_EXTI_IRQn EXTI2_IRQn
#define E5_RST_Pin GPIO_PIN_3
#define E5_RST_GPIO_Port GPIOC
#define BTN_A_Pin GPIO_PIN_1
#define BTN_A_GPIO_Port GPIOA
#define BTN_A_EXTI_IRQn EXTI1_IRQn
#define VLT_E5_Pin GPIO_PIN_4
#define VLT_E5_GPIO_Port GPIOA
#define E5_BOOT_Pin GPIO_PIN_4
#define E5_BOOT_GPIO_Port GPIOC
#define LIS2DUX_INT2_Pin GPIO_PIN_13
#define LIS2DUX_INT2_GPIO_Port GPIOB
#define LIS2DUX_INT2_EXTI_IRQn EXTI13_IRQn
#define LIS2DUX_INT1_Pin GPIO_PIN_14
#define LIS2DUX_INT1_GPIO_Port GPIOB
#define LIS2DUX_INT1_EXTI_IRQn EXTI14_IRQn
#define ADP5360_INT_Pin GPIO_PIN_15
#define ADP5360_INT_GPIO_Port GPIOB
#define ADP5360_INT_EXTI_IRQn EXTI15_IRQn
#define TMAG5273_INT_Pin GPIO_PIN_6
#define TMAG5273_INT_GPIO_Port GPIOC
#define TMAG5273_INT_EXTI_IRQn EXTI6_IRQn
#define SD_MODE_Pin GPIO_PIN_9
#define SD_MODE_GPIO_Port GPIOC
#define VLT_LCD_Pin GPIO_PIN_11
#define VLT_LCD_GPIO_Port GPIOC
#define SPI3_CS_Pin GPIO_PIN_2
#define SPI3_CS_GPIO_Port GPIOD
#define BTN_BOOT_Pin GPIO_PIN_3
#define BTN_BOOT_GPIO_Port GPIOH
#define BTN_BOOT_EXTI_IRQn EXTI3_IRQn
#define BTN_L_Pin GPIO_PIN_8
#define BTN_L_GPIO_Port GPIOB
#define BTN_L_EXTI_IRQn EXTI8_IRQn
#define BTN_R_Pin GPIO_PIN_9
#define BTN_R_GPIO_Port GPIOB
#define BTN_R_EXTI_IRQn EXTI9_IRQn

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
