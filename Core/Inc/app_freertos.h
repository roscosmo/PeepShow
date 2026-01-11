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
typedef enum
{
  APP_BUTTON_A = 0U,
  APP_BUTTON_B = 1U,
  APP_BUTTON_L = 2U,
  APP_BUTTON_R = 3U,
  APP_BUTTON_BOOT = 4U,
  APP_BUTTON_COUNT = 5U
} app_button_id_t;

typedef struct
{
  uint8_t button_id;
  uint8_t pressed;
  uint16_t reserved;
} app_input_event_t;
typedef uint32_t app_ui_event_t;
typedef uint32_t app_sys_event_t;
typedef uint32_t app_display_cmd_t;
typedef uint32_t app_audio_cmd_t;
typedef uint32_t app_storage_req_t;
typedef uint32_t app_sensor_req_t;
typedef uint32_t app_game_event_t;

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
extern volatile uint32_t g_input_task_started;
extern volatile uint32_t g_input_last_flags;
extern volatile int32_t g_input_wait_result;
extern volatile uint32_t g_exti_callback_count;
extern volatile uint32_t g_exti_last_pin;
extern volatile uint32_t g_exti_last_flags;
extern volatile uint32_t g_exti_last_kernel_state;
extern volatile int32_t g_exti_last_qput_result;
extern volatile uint32_t g_exti_qput_attempts;
extern volatile uint32_t g_exti_qput_errors;
extern volatile uint32_t g_exti_last_event_id;
extern volatile uint32_t g_exti_last_event_pressed;
extern volatile uint32_t g_input_event_count;
extern volatile uint32_t g_input_debounce_drops;
extern volatile uint32_t g_input_invalid_count;
extern volatile uint32_t g_input_ui_drop_count;
extern volatile uint32_t g_input_game_drop_count;
extern volatile uint32_t g_ui_event_count;
extern volatile uint32_t g_sys_event_count;

/* USER CODE END EC */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DEBUGGING 1
#define APP_INPUT_FLAG_BTN_A    (1UL << 0U)
#define APP_INPUT_FLAG_BTN_B    (1UL << 1U)
#define APP_INPUT_FLAG_BTN_L    (1UL << 2U)
#define APP_INPUT_FLAG_BTN_R    (1UL << 3U)
#define APP_INPUT_FLAG_BTN_BOOT (1UL << 4U)
#define APP_INPUT_FLAG_ALL      (APP_INPUT_FLAG_BTN_A | APP_INPUT_FLAG_BTN_B | \
                                 APP_INPUT_FLAG_BTN_L | APP_INPUT_FLAG_BTN_R | \
                                 APP_INPUT_FLAG_BTN_BOOT)

#define APP_MODE_UI             (1UL << 0U)
#define APP_MODE_GAME           (1UL << 1U)

#define APP_SYS_EVENT_BOOT_BUTTON (1UL)
#define APP_SYS_EVENT_ENTER_GAME  (1UL << 1U)
#define APP_SYS_EVENT_EXIT_GAME   (1UL << 2U)
#define APP_DISPLAY_CMD_TOGGLE    (1UL)
#define APP_DISPLAY_CMD_RENDER_DEMO (2UL)
#define APP_DISPLAY_CMD_INVALIDATE (3UL)
#define APP_UI_DEMO_BUTTON APP_BUTTON_R
#define APP_GAME_DEMO_BUTTON APP_BUTTON_R

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
extern osMessageQueueId_t qInputHandle;
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
