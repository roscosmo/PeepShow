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
#include "debug_uart.h"
#include "display_task.h"
#include "ui_task.h"
#include "game_task.h"
#include "sensor_task.h"
#include "audio_task.h"
#include "settings.h"
#include "storage_task.h"
#include "power_task.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
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
static const uint32_t kButtonDebounceMs = 20U;
static uint32_t s_btn_last_tick[APP_BUTTON_COUNT];

volatile uint32_t g_input_task_started = 0U;
volatile uint32_t g_input_last_flags = 0U;
volatile int32_t g_input_wait_result = 0;
volatile uint32_t g_exti_callback_count = 0U;
volatile uint32_t g_exti_last_pin = 0U;
volatile uint32_t g_exti_last_flags = 0U;
volatile uint32_t g_exti_last_kernel_state = 0U;
volatile int32_t g_exti_last_qput_result = 0;
volatile uint32_t g_exti_qput_attempts = 0U;
volatile uint32_t g_exti_qput_errors = 0U;
volatile uint32_t g_exti_last_event_id = 0U;
volatile uint32_t g_exti_last_event_pressed = 0U;
volatile uint32_t g_input_event_count = 0U;
volatile uint32_t g_input_debounce_drops = 0U;
volatile uint32_t g_input_invalid_count = 0U;
volatile uint32_t g_input_ui_drop_count = 0U;
volatile uint32_t g_input_game_drop_count = 0U;
volatile uint32_t g_ui_event_count = 0U;
volatile uint32_t g_sys_event_count = 0U;

/* USER CODE END Variables */
/* Definitions for tskDisplay */
osThreadId_t tskDisplayHandle;
const osThreadAttr_t tskDisplay_attributes = {
  .name = "tskDisplay",
  .priority = (osPriority_t) osPriorityHigh,
  .stack_size = 1024 * 4
};
/* Definitions for tskUI */
osThreadId_t tskUIHandle;
const osThreadAttr_t tskUI_attributes = {
  .name = "tskUI",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 768 * 4
};
/* Definitions for tskInput */
osThreadId_t tskInputHandle;
const osThreadAttr_t tskInput_attributes = {
  .name = "tskInput",
  .priority = (osPriority_t) osPriorityAboveNormal,
  .stack_size = 384 * 4
};
/* Definitions for tskPower */
osThreadId_t tskPowerHandle;
const osThreadAttr_t tskPower_attributes = {
  .name = "tskPower",
  .priority = (osPriority_t) osPriorityBelowNormal,
  .stack_size = 384 * 4
};
/* Definitions for tskSensor */
osThreadId_t tskSensorHandle;
const osThreadAttr_t tskSensor_attributes = {
  .name = "tskSensor",
  .priority = (osPriority_t) osPriorityBelowNormal,
  .stack_size = 384 * 4
};
/* Definitions for tskStorage */
osThreadId_t tskStorageHandle;
const osThreadAttr_t tskStorage_attributes = {
  .name = "tskStorage",
  .priority = (osPriority_t) osPriorityLow,
  .stack_size = 1024 * 4
};
/* Definitions for tskGame */
osThreadId_t tskGameHandle;
const osThreadAttr_t tskGame_attributes = {
  .name = "tskGame",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 768 * 4
};
/* Definitions for tskAudio */
osThreadId_t tskAudioHandle;
const osThreadAttr_t tskAudio_attributes = {
  .name = "tskAudio",
  .priority = (osPriority_t) osPriorityRealtime,
  .stack_size = 512 * 4
};
/* Definitions for mtxLog */
osMutexId_t mtxLogHandle;
const osMutexAttr_t mtxLog_attributes = {
  .name = "mtxLog"
};
/* Definitions for mtxSettings */
osMutexId_t mtxSettingsHandle;
const osMutexAttr_t mtxSettings_attributes = {
  .name = "mtxSettings"
};
/* Definitions for tmrInactivity */
osTimerId_t tmrInactivityHandle;
const osTimerAttr_t tmrInactivity_attributes = {
  .name = "tmrInactivity"
};
/* Definitions for tmrUITick */
osTimerId_t tmrUITickHandle;
const osTimerAttr_t tmrUITick_attributes = {
  .name = "tmrUITick"
};
/* Definitions for qUIEvents */
osMessageQueueId_t qUIEventsHandle;
const osMessageQueueAttr_t qUIEvents_attributes = {
  .name = "qUIEvents"
};
/* Definitions for qGameEvents */
osMessageQueueId_t qGameEventsHandle;
const osMessageQueueAttr_t qGameEvents_attributes = {
  .name = "qGameEvents"
};
/* Definitions for qSysEvents */
osMessageQueueId_t qSysEventsHandle;
const osMessageQueueAttr_t qSysEvents_attributes = {
  .name = "qSysEvents"
};
/* Definitions for qDisplayCmd */
osMessageQueueId_t qDisplayCmdHandle;
const osMessageQueueAttr_t qDisplayCmd_attributes = {
  .name = "qDisplayCmd"
};
/* Definitions for qAudioCmd */
osMessageQueueId_t qAudioCmdHandle;
const osMessageQueueAttr_t qAudioCmd_attributes = {
  .name = "qAudioCmd"
};
/* Definitions for qStorageReq */
osMessageQueueId_t qStorageReqHandle;
const osMessageQueueAttr_t qStorageReq_attributes = {
  .name = "qStorageReq"
};
/* Definitions for qSensorReq */
osMessageQueueId_t qSensorReqHandle;
const osMessageQueueAttr_t qSensorReq_attributes = {
  .name = "qSensorReq"
};
/* Definitions for qInput */
osMessageQueueId_t qInputHandle;
const osMessageQueueAttr_t qInput_attributes = {
  .name = "qInput"
};
/* Definitions for semWake */
osSemaphoreId_t semWakeHandle;
const osSemaphoreAttr_t semWake_attributes = {
  .name = "semWake"
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
static void app_input_process_event(const app_input_event_t *evt);

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
  settings_init();

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
  /* creation of qInput */
  qInputHandle = osMessageQueueNew (16, sizeof(4), &qInput_attributes);

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
  if (egModeHandle != NULL)
  {
    (void)osEventFlagsSet(egModeHandle, APP_MODE_UI);
  }
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
  (void)argument;
  display_task_run();
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
  (void)argument;
  ui_task_run();
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
  app_input_event_t evt = {0};
  (void)argument;
  g_input_task_started = 1U;
  debug_uart_printf("tskInput start\r\n");

  /* Infinite loop */
  for(;;)
  {
    osStatus_t status = osMessageQueueGet(qInputHandle, &evt, NULL, osWaitForever);
    g_input_wait_result = (int32_t)status;
    if (status != osOK)
    {
      debug_uart_printf("qInput get err: 0x%08lx\r\n", (unsigned long)status);
      continue;
    }
    app_input_process_event(&evt);
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
  (void)argument;
  power_task_run();
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
  (void)argument;
  sensor_task_run();
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
  (void)argument;
  storage_task_run();
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
  (void)argument;
  game_task_run();
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
  (void)argument;
  audio_task_run();
  /* USER CODE END tskAudio */
}

/* tmrInactivityCb function */
void tmrInactivityCb(void *argument)
{
  /* USER CODE BEGIN tmrInactivityCb */
  (void)argument;
  power_task_request_sleep();

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
static void app_input_process_event(const app_input_event_t *evt)
{
  if (evt == NULL)
  {
    return;
  }

  if (evt->button_id >= (uint8_t)APP_BUTTON_COUNT)
  {
    g_input_invalid_count++;
    return;
  }

  uint32_t now = osKernelGetTickCount();
  uint32_t last = s_btn_last_tick[evt->button_id];
  if ((now - last) < kButtonDebounceMs)
  {
    g_input_debounce_drops++;
    return;
  }
  s_btn_last_tick[evt->button_id] = now;
  power_task_activity_ping();

  g_input_last_flags = ((uint32_t)evt->button_id) | ((uint32_t)evt->pressed << 8);
  debug_uart_printf("input id=%lu state=%lu\r\n",
                    (unsigned long)evt->button_id,
                    (unsigned long)evt->pressed);

  uint32_t mode_flags = 0U;
  if (egModeHandle != NULL)
  {
    uint32_t flags = osEventFlagsGet(egModeHandle);
    if ((int32_t)flags >= 0)
    {
      mode_flags = flags;
    }
  }

  if (evt->button_id == (uint8_t)APP_BUTTON_BOOT)
  {
    if (evt->pressed != 0U)
    {
      app_ui_event_t ui_event = g_input_last_flags;
      if (osMessageQueuePut(qUIEventsHandle, &ui_event, 0U, 0U) == osOK)
      {
        g_input_event_count++;
      }
      else
      {
        g_input_ui_drop_count++;
      }
    }
    return;
  }

  if ((mode_flags & APP_MODE_GAME) != 0U)
  {
    app_game_event_t game_event = g_input_last_flags;
    if (osMessageQueuePut(qGameEventsHandle, &game_event, 0U, 0U) == osOK)
    {
      g_input_event_count++;
    }
    else
    {
      g_input_game_drop_count++;
    }
  }
  else
  {
    app_ui_event_t ui_event = g_input_last_flags;
    if (osMessageQueuePut(qUIEventsHandle, &ui_event, 0U, 0U) == osOK)
    {
      g_input_event_count++;
    }
    else
    {
      g_input_ui_drop_count++;
    }
  }
}

/* USER CODE END Application */

