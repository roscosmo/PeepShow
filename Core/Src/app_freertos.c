/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.c
  * Description        : FreeRTOS applicative file
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

/* Includes ------------------------------------------------------------------*/
#include "app_freertos.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
typedef StaticTask_t osStaticThreadDef_t;
typedef StaticQueue_t osStaticMessageQDef_t;
typedef StaticTimer_t osStaticTimerDef_t;
typedef StaticSemaphore_t osStaticMutexDef_t;
typedef StaticSemaphore_t osStaticSemaphoreDef_t;
typedef StaticEventGroup_t osStaticEventGroupDef_t;
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for tskDisplay */
osThreadId_t tskDisplayHandle;
uint32_t tskDisplayStack[ 1024 ];
osStaticThreadDef_t tskDisplayTCB;
const osThreadAttr_t tskDisplay_attributes = {
  .name = "tskDisplay",
  .stack_mem = &tskDisplayStack[0],
  .stack_size = sizeof(tskDisplayStack),
  .cb_mem = &tskDisplayTCB,
  .cb_size = sizeof(tskDisplayTCB),
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for tskUI */
osThreadId_t tskUIHandle;
uint32_t tskUIStack[ 768 ];
osStaticThreadDef_t tskUITCB;
const osThreadAttr_t tskUI_attributes = {
  .name = "tskUI",
  .stack_mem = &tskUIStack[0],
  .stack_size = sizeof(tskUIStack),
  .cb_mem = &tskUITCB,
  .cb_size = sizeof(tskUITCB),
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for tskInput */
osThreadId_t tskInputHandle;
uint32_t tskInputStack[ 384 ];
osStaticThreadDef_t tskInputTCB;
const osThreadAttr_t tskInput_attributes = {
  .name = "tskInput",
  .stack_mem = &tskInputStack[0],
  .stack_size = sizeof(tskInputStack),
  .cb_mem = &tskInputTCB,
  .cb_size = sizeof(tskInputTCB),
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for tskPower */
osThreadId_t tskPowerHandle;
uint32_t tskPowerStack[ 384 ];
osStaticThreadDef_t tskPowerTCB;
const osThreadAttr_t tskPower_attributes = {
  .name = "tskPower",
  .stack_mem = &tskPowerStack[0],
  .stack_size = sizeof(tskPowerStack),
  .cb_mem = &tskPowerTCB,
  .cb_size = sizeof(tskPowerTCB),
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for tskSensor */
osThreadId_t tskSensorHandle;
uint32_t tskSensorStack[ 384 ];
osStaticThreadDef_t tskSensorTCB;
const osThreadAttr_t tskSensor_attributes = {
  .name = "tskSensor",
  .stack_mem = &tskSensorStack[0],
  .stack_size = sizeof(tskSensorStack),
  .cb_mem = &tskSensorTCB,
  .cb_size = sizeof(tskSensorTCB),
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for tskStorage */
osThreadId_t tskStorageHandle;
uint32_t tskStorageStack[ 1024 ];
osStaticThreadDef_t tskStorageTCB;
const osThreadAttr_t tskStorage_attributes = {
  .name = "tskStorage",
  .stack_mem = &tskStorageStack[0],
  .stack_size = sizeof(tskStorageStack),
  .cb_mem = &tskStorageTCB,
  .cb_size = sizeof(tskStorageTCB),
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for tskGame */
osThreadId_t tskGameHandle;
uint32_t tskGameStack[ 768 ];
osStaticThreadDef_t tskGameTCB;
const osThreadAttr_t tskGame_attributes = {
  .name = "tskGame",
  .stack_mem = &tskGameStack[0],
  .stack_size = sizeof(tskGameStack),
  .cb_mem = &tskGameTCB,
  .cb_size = sizeof(tskGameTCB),
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for tskAudio */
osThreadId_t tskAudioHandle;
uint32_t tskAudioStack[ 512 ];
osStaticThreadDef_t tskAudioTCB;
const osThreadAttr_t tskAudio_attributes = {
  .name = "tskAudio",
  .stack_mem = &tskAudioStack[0],
  .stack_size = sizeof(tskAudioStack),
  .cb_mem = &tskAudioTCB,
  .cb_size = sizeof(tskAudioTCB),
  .priority = (osPriority_t) osPriorityRealtime,
};
/* Definitions for mtxLog */
osMutexId_t mtxLogHandle;
osStaticMutexDef_t mtxLogCB;
const osMutexAttr_t mtxLog_attributes = {
  .name = "mtxLog",
  .cb_mem = &mtxLogCB,
  .cb_size = sizeof(mtxLogCB),
};
/* Definitions for mtxSettings */
osMutexId_t mtxSettingsHandle;
osStaticMutexDef_t mtxSettingsCB;
const osMutexAttr_t mtxSettings_attributes = {
  .name = "mtxSettings",
  .cb_mem = &mtxSettingsCB,
  .cb_size = sizeof(mtxSettingsCB),
};
/* Definitions for tmrInactivity */
osTimerId_t tmrInactivityHandle;
osStaticTimerDef_t tmrInactivityCB;
const osTimerAttr_t tmrInactivity_attributes = {
  .name = "tmrInactivity",
  .cb_mem = &tmrInactivityCB,
  .cb_size = sizeof(tmrInactivityCB),
};
/* Definitions for tmrUITick */
osTimerId_t tmrUITickHandle;
osStaticTimerDef_t tmrUITickCB;
const osTimerAttr_t tmrUITick_attributes = {
  .name = "tmrUITick",
  .cb_mem = &tmrUITickCB,
  .cb_size = sizeof(tmrUITickCB),
};
/* Definitions for qUIEvents */
osMessageQueueId_t qUIEventsHandle;
uint8_t qUIEventsBuf[ 16 * sizeof( 16 ) ];
osStaticMessageQDef_t qUIEventsCB;
const osMessageQueueAttr_t qUIEvents_attributes = {
  .name = "qUIEvents",
  .cb_mem = &qUIEventsCB,
  .cb_size = sizeof(qUIEventsCB),
  .mq_mem = &qUIEventsBuf,
  .mq_size = sizeof(qUIEventsBuf)
};
/* Definitions for qGameEvents */
osMessageQueueId_t qGameEventsHandle;
uint8_t qGameEventsBuf[ 16 * sizeof( 16 ) ];
osStaticMessageQDef_t qGameEventsCB;
const osMessageQueueAttr_t qGameEvents_attributes = {
  .name = "qGameEvents",
  .cb_mem = &qGameEventsCB,
  .cb_size = sizeof(qGameEventsCB),
  .mq_mem = &qGameEventsBuf,
  .mq_size = sizeof(qGameEventsBuf)
};
/* Definitions for qSysEvents */
osMessageQueueId_t qSysEventsHandle;
uint8_t qSysEventsBuf[ 16 * sizeof( 16 ) ];
osStaticMessageQDef_t qSysEventsCB;
const osMessageQueueAttr_t qSysEvents_attributes = {
  .name = "qSysEvents",
  .cb_mem = &qSysEventsCB,
  .cb_size = sizeof(qSysEventsCB),
  .mq_mem = &qSysEventsBuf,
  .mq_size = sizeof(qSysEventsBuf)
};
/* Definitions for qDisplayCmd */
osMessageQueueId_t qDisplayCmdHandle;
uint8_t qDisplayCmdBuf[ 16 * sizeof( 16 ) ];
osStaticMessageQDef_t qDisplayCmdCB;
const osMessageQueueAttr_t qDisplayCmd_attributes = {
  .name = "qDisplayCmd",
  .cb_mem = &qDisplayCmdCB,
  .cb_size = sizeof(qDisplayCmdCB),
  .mq_mem = &qDisplayCmdBuf,
  .mq_size = sizeof(qDisplayCmdBuf)
};
/* Definitions for qAudioCmd */
osMessageQueueId_t qAudioCmdHandle;
uint8_t qAudioCmdBuf[ 16 * sizeof( 16 ) ];
osStaticMessageQDef_t qAudioCmdCB;
const osMessageQueueAttr_t qAudioCmd_attributes = {
  .name = "qAudioCmd",
  .cb_mem = &qAudioCmdCB,
  .cb_size = sizeof(qAudioCmdCB),
  .mq_mem = &qAudioCmdBuf,
  .mq_size = sizeof(qAudioCmdBuf)
};
/* Definitions for qStorageReq */
osMessageQueueId_t qStorageReqHandle;
uint8_t qStorageReqBuf[ 24 * sizeof( 8 ) ];
osStaticMessageQDef_t qStorageReqCB;
const osMessageQueueAttr_t qStorageReq_attributes = {
  .name = "qStorageReq",
  .cb_mem = &qStorageReqCB,
  .cb_size = sizeof(qStorageReqCB),
  .mq_mem = &qStorageReqBuf,
  .mq_size = sizeof(qStorageReqBuf)
};
/* Definitions for qSensorReq */
osMessageQueueId_t qSensorReqHandle;
uint8_t qSensorReqBuf[ 16 * sizeof( 8 ) ];
osStaticMessageQDef_t qSensorReqCB;
const osMessageQueueAttr_t qSensorReq_attributes = {
  .name = "qSensorReq",
  .cb_mem = &qSensorReqCB,
  .cb_size = sizeof(qSensorReqCB),
  .mq_mem = &qSensorReqBuf,
  .mq_size = sizeof(qSensorReqBuf)
};
/* Definitions for semWake */
osSemaphoreId_t semWakeHandle;
osStaticSemaphoreDef_t semWakeCB;
const osSemaphoreAttr_t semWake_attributes = {
  .name = "semWake",
  .cb_mem = &semWakeCB,
  .cb_size = sizeof(semWakeCB),
};
/* Definitions for egMode */
osEventFlagsId_t egModeHandle;
osStaticEventGroupDef_t egModeCB;
const osEventFlagsAttr_t egMode_attributes = {
  .name = "egMode",
  .cb_mem = &egModeCB,
  .cb_size = sizeof(egModeCB),
};
/* Definitions for egPower */
osEventFlagsId_t egPowerHandle;
osStaticEventGroupDef_t egPowerCB;
const osEventFlagsAttr_t egPower_attributes = {
  .name = "egPower",
  .cb_mem = &egPowerCB,
  .cb_size = sizeof(egPowerCB),
};
/* Definitions for egDebug */
osEventFlagsId_t egDebugHandle;
osStaticEventGroupDef_t egDebugCB;
const osEventFlagsAttr_t egDebug_attributes = {
  .name = "egDebug",
  .cb_mem = &egDebugCB,
  .cb_size = sizeof(egDebugCB),
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

/* USER CODE BEGIN 5 */
void vApplicationMallocFailedHook(void)
{
   /* vApplicationMallocFailedHook() will only be called if
   configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h. It is a hook
   function that will get called if a call to pvPortMalloc() fails.
   pvPortMalloc() is called internally by the kernel whenever a task, queue,
   timer or semaphore is created. It is also called by various parts of the
   demo application. If heap_1.c or heap_2.c are used, then the size of the
   heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
   FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
   to query the size of free heap space that remains (although it does not
   provide information on how the remaining heap might be fragmented). */
}
/* USER CODE END 5 */

/* USER CODE BEGIN 4 */
void vApplicationStackOverflowHook(xTaskHandle xTask, char *pcTaskName)
{
   /* Run time stack overflow checking is performed if
   configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2. This hook function is
   called if a stack overflow is detected. */
}
/* USER CODE END 4 */

/* USER CODE BEGIN PREPOSTSLEEP */
__weak void PreSleepProcessing(uint32_t ulExpectedIdleTime)
{
/* place for user code */
}

__weak void PostSleepProcessing(uint32_t ulExpectedIdleTime)
{
/* place for user code */
}
/* USER CODE END PREPOSTSLEEP */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */
  /* creation of mtxLog */
  mtxLogHandle = osMutexNew(&mtxLog_attributes);

  /* creation of mtxSettings */
  mtxSettingsHandle = osMutexNew(&mtxSettings_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */
  /* creation of semWake */
  semWakeHandle = osSemaphoreNew(1, 1, &semWake_attributes);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */
  /* creation of tmrInactivity */
  tmrInactivityHandle = osTimerNew(tmrInactivityCb, osTimerOnce, NULL, &tmrInactivity_attributes);

  /* creation of tmrUITick */
  tmrUITickHandle = osTimerNew(UITickTimerCb, osTimerPeriodic, NULL, &tmrUITick_attributes);

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */
  /* creation of qUIEvents */
  qUIEventsHandle = osMessageQueueNew (16, sizeof(16), &qUIEvents_attributes);
  /* creation of qGameEvents */
  qGameEventsHandle = osMessageQueueNew (16, sizeof(16), &qGameEvents_attributes);
  /* creation of qSysEvents */
  qSysEventsHandle = osMessageQueueNew (16, sizeof(16), &qSysEvents_attributes);
  /* creation of qDisplayCmd */
  qDisplayCmdHandle = osMessageQueueNew (16, sizeof(16), &qDisplayCmd_attributes);
  /* creation of qAudioCmd */
  qAudioCmdHandle = osMessageQueueNew (16, sizeof(16), &qAudioCmd_attributes);
  /* creation of qStorageReq */
  qStorageReqHandle = osMessageQueueNew (24, sizeof(8), &qStorageReq_attributes);
  /* creation of qSensorReq */
  qSensorReqHandle = osMessageQueueNew (16, sizeof(8), &qSensorReq_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */
  /* creation of tskDisplay */
  tskDisplayHandle = osThreadNew(StartTaskDisplay, NULL, &tskDisplay_attributes);

  /* creation of tskUI */
  tskUIHandle = osThreadNew(StartTaskUI, NULL, &tskUI_attributes);

  /* creation of tskInput */
  tskInputHandle = osThreadNew(StartTaskInput, NULL, &tskInput_attributes);

  /* creation of tskPower */
  tskPowerHandle = osThreadNew(StartTaskPower, NULL, &tskPower_attributes);

  /* creation of tskSensor */
  tskSensorHandle = osThreadNew(StartTaskSensor, NULL, &tskSensor_attributes);

  /* creation of tskStorage */
  tskStorageHandle = osThreadNew(StartTaskFileSystem, NULL, &tskStorage_attributes);

  /* creation of tskGame */
  tskGameHandle = osThreadNew(StartTaskGame, NULL, &tskGame_attributes);

  /* creation of tskAudio */
  tskAudioHandle = osThreadNew(StartTaskAudio, NULL, &tskAudio_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* creation of egMode */
  egModeHandle = osEventFlagsNew(&egMode_attributes);

  /* creation of egPower */
  egPowerHandle = osEventFlagsNew(&egPower_attributes);

  /* creation of egDebug */
  egDebugHandle = osEventFlagsNew(&egDebug_attributes);

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}
/* USER CODE BEGIN Header_StartTaskDisplay */
/**
* @brief Function implementing the tskDisplay thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskDisplay */
void StartTaskDisplay(void *argument)
{
  /* USER CODE BEGIN tskDisplay */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END tskDisplay */
}

/* USER CODE BEGIN Header_StartTaskUI */
/**
* @brief Function implementing the tskUI thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskUI */
void StartTaskUI(void *argument)
{
  /* USER CODE BEGIN tskUI */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END tskUI */
}

/* USER CODE BEGIN Header_StartTaskInput */
/**
* @brief Function implementing the tskInput thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskInput */
void StartTaskInput(void *argument)
{
  /* USER CODE BEGIN tskInput */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END tskInput */
}

/* USER CODE BEGIN Header_StartTaskPower */
/**
* @brief Function implementing the tskPower thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskPower */
void StartTaskPower(void *argument)
{
  /* USER CODE BEGIN tskPower */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END tskPower */
}

/* USER CODE BEGIN Header_StartTaskSensor */
/**
* @brief Function implementing the tskSensor thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskSensor */
void StartTaskSensor(void *argument)
{
  /* USER CODE BEGIN tskSensor */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END tskSensor */
}

/* USER CODE BEGIN Header_StartTaskFileSystem */
/**
* @brief Function implementing the tskStorage thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskFileSystem */
void StartTaskFileSystem(void *argument)
{
  /* USER CODE BEGIN tskStorage */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END tskStorage */
}

/* USER CODE BEGIN Header_StartTaskGame */
/**
* @brief Function implementing the tskGame thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskGame */
void StartTaskGame(void *argument)
{
  /* USER CODE BEGIN tskGame */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END tskGame */
}

/* USER CODE BEGIN Header_StartTaskAudio */
/**
* @brief Function implementing the tskAudio thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskAudio */
void StartTaskAudio(void *argument)
{
  /* USER CODE BEGIN tskAudio */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END tskAudio */
}

/* tmrInactivityCb function */
void tmrInactivityCb(void *argument)
{
  /* USER CODE BEGIN tmrInactivityCb */

  /* USER CODE END tmrInactivityCb */
}

/* UITickTimerCb function */
void UITickTimerCb(void *argument)
{
  /* USER CODE BEGIN UITickTimerCb */

  /* USER CODE END UITickTimerCb */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

