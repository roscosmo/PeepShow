/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.h
  * Description        : FreeRTOS applicative header file
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
#ifndef __APP_FREERTOS_H__
#define __APP_FREERTOS_H__

#ifdef __cplusplus
extern "C" {
#endif
/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os2.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Exported macro -------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */
extern osThreadId_t tskDisplayHandle;
extern osThreadId_t tskUIHandle;
extern osThreadId_t tskInputHandle;
extern osThreadId_t tskPowerHandle;
extern osThreadId_t tskSensorHandle;
extern osThreadId_t tskStorageHandle;
extern osThreadId_t tskGameHandle;
extern osThreadId_t tskAudioHandle;
extern osMutexId_t mtxLogHandle;
extern osMutexId_t mtxSettingsHandle;
extern osTimerId_t tmrInactivityHandle;
extern osTimerId_t tmrUITickHandle;
extern osMessageQueueId_t qUIEventsHandle;
extern osMessageQueueId_t qGameEventsHandle;
extern osMessageQueueId_t qSysEventsHandle;
extern osMessageQueueId_t qDisplayCmdHandle;
extern osMessageQueueId_t qAudioCmdHandle;
extern osMessageQueueId_t qStorageReqHandle;
extern osMessageQueueId_t qSensorReqHandle;
extern osSemaphoreId_t semWakeHandle;
extern osEventFlagsId_t egModeHandle;
extern osEventFlagsId_t egPowerHandle;
extern osEventFlagsId_t egDebugHandle;

/* Exported function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartTaskDisplay(void *argument);
void StartTaskUI(void *argument);
void StartTaskInput(void *argument);
void StartTaskPower(void *argument);
void StartTaskSensor(void *argument);
void StartTaskFileSystem(void *argument);
void StartTaskGame(void *argument);
void StartTaskAudio(void *argument);
void tmrInactivityCb(void *argument);
void UITickTimerCb(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationMallocFailedHook(void);
void vApplicationStackOverflowHook(xTaskHandle xTask, char *pcTaskName);
/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

#ifdef __cplusplus
}
#endif
#endif /* __APP_FREERTOS_H__ */
