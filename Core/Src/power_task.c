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
void vPortSetupTimerInterrupt(void);

static const uint32_t kInactivityMsDefault = 15000U;
static const uint32_t kInactivityMsMin = 1000U;
static const uint32_t kPll1M = 1U;
static const uint32_t kPll1N = 20U;
static const uint32_t kPll1P = 2U;
static const uint32_t kPll1Q = 2U;
static const uint32_t kPll1R = 2U;
static const uint32_t kFlashLatencyCruise = FLASH_LATENCY_0;
static const uint32_t kFlashLatencyMid = FLASH_LATENCY_2;
static const uint32_t kFlashLatencyTurbo = FLASH_LATENCY_4;

static uint8_t s_audio_ref = 0U;
static uint8_t s_debug_ref = 0U;
static uint8_t s_stream_ref = 0U;
static volatile uint8_t s_sleep_pending = 0U;
static volatile uint8_t s_allow_game_sleep = 1U;
static volatile uint8_t s_sleep_enabled = 1U;
static volatile uint32_t s_inactivity_ms = kInactivityMsDefault;
static volatile uint8_t s_wake_ignore = 0U;
static volatile uint32_t s_wake_ignore_until = 0U;
static volatile uint8_t s_in_game = 0U;
static volatile power_perf_mode_t s_perf_active = POWER_PERF_MODE_CRUISE;
static volatile power_perf_mode_t s_game_perf_mode = POWER_PERF_MODE_CRUISE;

static power_perf_mode_t power_task_sanitize_perf_mode(power_perf_mode_t mode)
{
  switch (mode)
  {
    case POWER_PERF_MODE_CRUISE:
    case POWER_PERF_MODE_MID:
    case POWER_PERF_MODE_TURBO:
      return mode;
    default:
      return POWER_PERF_MODE_CRUISE;
  }
}

static uint8_t power_task_enable_msi_cruise(void)
{
  RCC_OscInitTypeDef osc = {0};
  osc.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  osc.MSIState = RCC_MSI_ON;
  osc.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  osc.MSIClockRange = RCC_MSIRANGE_1;
  return (HAL_RCC_OscConfig(&osc) == HAL_OK) ? 1U : 0U;
}

static uint8_t power_task_enable_pll1(void)
{
  RCC_OscInitTypeDef osc = {0};
  osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  osc.HSIState = RCC_HSI_ON;
  osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  osc.PLL.PLLState = RCC_PLL_ON;
  osc.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  osc.PLL.PLLM = kPll1M;
  osc.PLL.PLLMBOOST = RCC_PLLMBOOST_DIV1;
  osc.PLL.PLLN = kPll1N;
  osc.PLL.PLLP = kPll1P;
  osc.PLL.PLLQ = kPll1Q;
  osc.PLL.PLLR = kPll1R;
  osc.PLL.PLLRGE = RCC_PLLVCIRANGE_1;
  osc.PLL.PLLFRACN = 0U;
  return (HAL_RCC_OscConfig(&osc) == HAL_OK) ? 1U : 0U;
}

static void power_task_disable_pll1(void)
{
  RCC_OscInitTypeDef osc = {0};
  osc.OscillatorType = RCC_OSCILLATORTYPE_NONE;
  osc.PLL.PLLState = RCC_PLL_OFF;
  (void)HAL_RCC_OscConfig(&osc);
}

static uint8_t power_task_switch_sysclk(uint32_t sysclk_source,
                                        uint32_t ahb_div,
                                        uint32_t flash_latency)
{
  RCC_ClkInitTypeDef clk = {0};
  clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_PCLK3;
  clk.SYSCLKSource = sysclk_source;
  clk.AHBCLKDivider = ahb_div;
  clk.APB1CLKDivider = RCC_HCLK_DIV1;
  clk.APB2CLKDivider = RCC_HCLK_DIV1;
  clk.APB3CLKDivider = RCC_HCLK_DIV8;
  if (HAL_RCC_ClockConfig(&clk, flash_latency) != HAL_OK)
  {
    return 0U;
  }

  vPortSetupTimerInterrupt();
  return 1U;
}

static uint8_t power_task_apply_cruise(void)
{
  if (power_task_enable_msi_cruise() == 0U)
  {
    return 0U;
  }

  if (power_task_switch_sysclk(RCC_SYSCLKSOURCE_MSI,
                               RCC_SYSCLK_DIV1,
                               kFlashLatencyCruise) == 0U)
  {
    return 0U;
  }

  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE3) != HAL_OK)
  {
    return 0U;
  }

  power_task_disable_pll1();
  return 1U;
}

static uint8_t power_task_apply_pll1(uint32_t ahb_div, uint32_t flash_latency)
{
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    return 0U;
  }

  if (power_task_enable_pll1() == 0U)
  {
    return 0U;
  }

  if (power_task_switch_sysclk(RCC_SYSCLKSOURCE_PLLCLK,
                               ahb_div,
                               flash_latency) == 0U)
  {
    return 0U;
  }

  return 1U;
}

static uint8_t power_task_apply_perf_mode(power_perf_mode_t mode)
{
  power_perf_mode_t target = power_task_sanitize_perf_mode(mode);
  if (target == s_perf_active)
  {
    return 1U;
  }

  uint8_t ok = 0U;
  if (target == POWER_PERF_MODE_CRUISE)
  {
    ok = power_task_apply_cruise();
  }
  else if (target == POWER_PERF_MODE_MID)
  {
    ok = power_task_apply_pll1(RCC_SYSCLK_DIV2, kFlashLatencyMid);
  }
  else
  {
    ok = power_task_apply_pll1(RCC_SYSCLK_DIV1, kFlashLatencyTurbo);
  }

  if (ok != 0U)
  {
    s_perf_active = target;
  }

  return ok;
}

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
  s_perf_active = POWER_PERF_MODE_CRUISE;
  if (s_in_game != 0U)
  {
    (void)power_task_apply_perf_mode(s_game_perf_mode);
  }
  power_task_configure_lpuart_wakeup();
}

static void power_task_try_sleep(void)
{
  if (s_sleep_pending == 0U)
  {
    return;
  }
  if ((s_sleep_enabled == 0U) || (s_inactivity_ms == 0U))
  {
    s_sleep_pending = 0U;
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
  s_wake_ignore = 1U;
  s_wake_ignore_until = osKernelGetTickCount() + 250U;
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
          s_in_game = 1U;
          (void)power_task_apply_perf_mode(s_game_perf_mode);
          break;
        case APP_SYS_EVENT_EXIT_GAME:
          if (egModeHandle != NULL)
          {
            (void)osEventFlagsClear(egModeHandle, APP_MODE_GAME);
            (void)osEventFlagsSet(egModeHandle, APP_MODE_UI);
          }
          s_in_game = 0U;
          (void)power_task_apply_perf_mode(POWER_PERF_MODE_CRUISE);
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
        case APP_SYS_EVENT_PERF_MODE:
          if (s_in_game != 0U)
          {
            (void)power_task_apply_perf_mode(s_game_perf_mode);
          }
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
    if ((s_sleep_enabled != 0U) && (s_inactivity_ms > 0U))
    {
      (void)osTimerStart(tmrInactivityHandle, s_inactivity_ms);
    }
    else
    {
      (void)osTimerStop(tmrInactivityHandle);
    }
  }
  if (power_task_quiesce_active() != 0U)
  {
    power_task_quiesce_set(0U);
  }
}

void power_task_request_sleep(void)
{
  if ((s_sleep_enabled == 0U) || (s_inactivity_ms == 0U))
  {
    s_sleep_pending = 0U;
    return;
  }
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

void power_task_set_sleep_enabled(uint8_t enabled)
{
  s_sleep_enabled = (enabled != 0U) ? 1U : 0U;
  power_task_activity_ping();
}

uint8_t power_task_get_sleep_enabled(void)
{
  return s_sleep_enabled;
}

void power_task_set_inactivity_timeout_ms(uint32_t timeout_ms)
{
  uint32_t ms = timeout_ms;
  if (ms < kInactivityMsMin)
  {
    ms = kInactivityMsDefault;
  }
  s_inactivity_ms = ms;
  power_task_activity_ping();
}

uint32_t power_task_get_inactivity_timeout_ms(void)
{
  return s_inactivity_ms;
}

uint8_t power_task_consume_wake_press(uint32_t now_ms)
{
  if (s_wake_ignore == 0U)
  {
    return 0U;
  }

  if ((int32_t)(now_ms - s_wake_ignore_until) > 0)
  {
    s_wake_ignore = 0U;
    return 0U;
  }

  s_wake_ignore = 0U;
  return 1U;
}

void power_task_request_game_perf_mode(power_perf_mode_t mode)
{
  s_game_perf_mode = power_task_sanitize_perf_mode(mode);
  if (qSysEventsHandle != NULL)
  {
    app_sys_event_t evt = APP_SYS_EVENT_PERF_MODE;
    (void)osMessageQueuePut(qSysEventsHandle, &evt, 0U, 0U);
  }
}

void power_task_cycle_game_perf_mode(void)
{
  power_perf_mode_t mode = power_task_get_game_perf_mode();
  power_perf_mode_t next = POWER_PERF_MODE_CRUISE;

  switch (mode)
  {
    case POWER_PERF_MODE_CRUISE:
      next = POWER_PERF_MODE_MID;
      break;
    case POWER_PERF_MODE_MID:
      next = POWER_PERF_MODE_TURBO;
      break;
    default:
      next = POWER_PERF_MODE_CRUISE;
      break;
  }

  power_task_request_game_perf_mode(next);
}

power_perf_mode_t power_task_get_game_perf_mode(void)
{
  return s_game_perf_mode;
}

power_perf_mode_t power_task_get_perf_mode(void)
{
  return s_perf_active;
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
