#include "power_task.h"

#include "app_freertos.h"
#include "audio_task.h"
#include "display_task.h"
#include "storage_task.h"

#include "cmsis_os2.h"
#include "main.h"
#include "stm32u5xx_hal_uart_ex.h"

extern UART_HandleTypeDef hlpuart1;

void SystemClock_Config(void);
void PeriphCommonClock_Config(void);

static const uint32_t kInactivityMs = 15000U;

static uint8_t s_audio_ref = 0U;
static uint8_t s_debug_ref = 0U;
static uint8_t s_stream_ref = 0U;
static volatile uint8_t s_sleep_pending = 0U;
static volatile uint8_t s_allow_game_sleep = 1U;

static void power_task_update_debug_flag(void)
{
  if (egDebugHandle == NULL)
  {
    return;
  }

  if (s_debug_ref > 0U)
  {
    (void)osEventFlagsSet(egDebugHandle, APP_DEBUG_MODE);
  }
  else
  {
    (void)osEventFlagsClear(egDebugHandle, APP_DEBUG_MODE);
  }
}

static uint8_t power_task_mode_allows_sleep(void)
{
  if (s_allow_game_sleep != 0U)
  {
    return 1U;
  }

  if (egModeHandle == NULL)
  {
    return 1U;
  }

  uint32_t flags = osEventFlagsGet(egModeHandle);
  if ((int32_t)flags < 0)
  {
    return 1U;
  }

  return ((flags & APP_MODE_GAME) == 0U) ? 1U : 0U;
}

static uint8_t power_task_quiesce_active(void)
{
  if (egPowerHandle == NULL)
  {
    return 0U;
  }

  uint32_t flags = osEventFlagsGet(egPowerHandle);
  if ((int32_t)flags < 0)
  {
    return 0U;
  }

  return ((flags & POWER_QUIESCE_REQ_FLAG) != 0U) ? 1U : 0U;
}

static void power_task_quiesce_set(uint8_t enable)
{
  if (egPowerHandle == NULL)
  {
    return;
  }

  if (enable != 0U)
  {
    (void)osEventFlagsClear(egPowerHandle, POWER_QUIESCE_ACK_MASK);
    (void)osEventFlagsSet(egPowerHandle, POWER_QUIESCE_REQ_FLAG);
  }
  else
  {
    (void)osEventFlagsClear(egPowerHandle, POWER_QUIESCE_REQ_FLAG | POWER_QUIESCE_ACK_MASK);
  }
}

static uint8_t power_task_quiesce_ready(void)
{
  if (egPowerHandle == NULL)
  {
    return 1U;
  }

  uint32_t flags = osEventFlagsGet(egPowerHandle);
  if ((int32_t)flags < 0)
  {
    return 0U;
  }

  return ((flags & POWER_QUIESCE_ACK_MASK) == POWER_QUIESCE_ACK_MASK) ? 1U : 0U;
}

static void power_task_configure_lpuart_wakeup(void)
{
  UART_WakeUpTypeDef wake = {0};
  wake.WakeUpEvent = UART_WAKEUP_ON_READDATA_NONEMPTY;
  (void)HAL_UARTEx_StopModeWakeUpSourceConfig(&hlpuart1, wake);
  __HAL_UART_ENABLE_IT(&hlpuart1, UART_IT_RXNE);
  (void)HAL_UARTEx_EnableStopMode(&hlpuart1);
}

static void power_task_enter_stop2(void)
{
  power_task_configure_lpuart_wakeup();
  HAL_SuspendTick();
  (void)HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);
  HAL_ResumeTick();
  SystemClock_Config();
  PeriphCommonClock_Config();
  power_task_configure_lpuart_wakeup();
}

static void power_task_try_sleep(void)
{
  if (s_sleep_pending == 0U)
  {
    return;
  }

  if (power_task_mode_allows_sleep() == 0U)
  {
    s_sleep_pending = 0U;
    power_task_activity_ping();
    return;
  }

  if (audio_is_active() != 0U)
  {
    return;
  }

  if (power_task_quiesce_active() == 0U)
  {
    power_task_quiesce_set(1U);
  }

  if (power_task_quiesce_ready() == 0U)
  {
    return;
  }

  if (display_is_busy())
  {
    return;
  }

  if (storage_is_busy() != 0U)
  {
    return;
  }

  s_sleep_pending = 0U;
  power_task_enter_stop2();
  power_task_quiesce_set(0U);
  power_task_activity_ping();
}

void power_task_run(void)
{
  app_sys_event_t sys_event = 0U;

#if DEBUGGING
  if (s_debug_ref == 0U)
  {
    s_debug_ref = 1U;
  }
#endif

  power_task_update_debug_flag();
  power_task_activity_ping();

  for (;;)
  {
    uint32_t timeout = (s_sleep_pending != 0U) ? 20U : osWaitForever;
    if (osMessageQueueGet(qSysEventsHandle, &sys_event, NULL, timeout) == osOK)
    {
      g_sys_event_count++;
      switch (sys_event)
      {
        case APP_SYS_EVENT_ENTER_GAME:
          if (egModeHandle != NULL)
          {
            (void)osEventFlagsClear(egModeHandle, APP_MODE_UI);
            (void)osEventFlagsSet(egModeHandle, APP_MODE_GAME);
          }
          break;
        case APP_SYS_EVENT_EXIT_GAME:
          if (egModeHandle != NULL)
          {
            (void)osEventFlagsClear(egModeHandle, APP_MODE_GAME);
            (void)osEventFlagsSet(egModeHandle, APP_MODE_UI);
          }
          break;
        case APP_SYS_EVENT_AUDIO_ON:
          if (s_audio_ref < 0xFFU)
          {
            s_audio_ref++;
          }
          break;
        case APP_SYS_EVENT_AUDIO_OFF:
          if (s_audio_ref > 0U)
          {
            s_audio_ref--;
          }
          break;
        case APP_SYS_EVENT_DEBUG_ON:
          if (s_debug_ref < 0xFFU)
          {
            s_debug_ref++;
          }
          break;
        case APP_SYS_EVENT_DEBUG_OFF:
          if (s_debug_ref > 0U)
          {
            s_debug_ref--;
          }
          break;
        case APP_SYS_EVENT_STREAM_ON:
          if (s_stream_ref < 0xFFU)
          {
            s_stream_ref++;
          }
          break;
        case APP_SYS_EVENT_STREAM_OFF:
          if (s_stream_ref > 0U)
          {
            s_stream_ref--;
          }
          break;
        case APP_SYS_EVENT_INACTIVITY:
          s_sleep_pending = 1U;
          break;
        default:
          break;
      }

      power_task_update_debug_flag();
    }

    power_task_try_sleep();
  }
}

void power_task_activity_ping(void)
{
  s_sleep_pending = 0U;
  if (tmrInactivityHandle != NULL)
  {
    (void)osTimerStart(tmrInactivityHandle, kInactivityMs);
  }
  if (power_task_quiesce_active() != 0U)
  {
    power_task_quiesce_set(0U);
  }
}

void power_task_request_sleep(void)
{
  s_sleep_pending = 1U;
  if (qSysEventsHandle != NULL)
  {
    app_sys_event_t evt = APP_SYS_EVENT_INACTIVITY;
    (void)osMessageQueuePut(qSysEventsHandle, &evt, 0U, 0U);
  }
}

void power_task_set_game_sleep_allowed(uint8_t allow)
{
  s_allow_game_sleep = (allow != 0U) ? 1U : 0U;
}

uint8_t power_task_get_game_sleep_allowed(void)
{
  return s_allow_game_sleep;
}

uint8_t power_task_is_quiescing(void)
{
  return power_task_quiesce_active();
}

void power_task_quiesce_ack(uint32_t ack_flag)
{
  if (egPowerHandle == NULL)
  {
    return;
  }

  (void)osEventFlagsSet(egPowerHandle, ack_flag);
}

void power_task_quiesce_clear(uint32_t ack_flag)
{
  if (egPowerHandle == NULL)
  {
    return;
  }

  (void)osEventFlagsClear(egPowerHandle, ack_flag);
}
