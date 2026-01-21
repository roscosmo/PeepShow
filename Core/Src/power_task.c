#include "power_task.h"

#include "app_freertos.h"
#include "audio_task.h"
#include "display_task.h"
#include "settings.h"
#include "storage_task.h"
#include "sleep_face.h"
#include "rtos_isr_bridge.h"

#include "cmsis_os2.h"
#include "main.h"
#include "stm32u5xx_hal_uart_ex.h"

extern UART_HandleTypeDef hlpuart1;
extern RTC_HandleTypeDef hrtc;
extern OSPI_HandleTypeDef hospi1;

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
static const uint32_t kSleepfaceHoldMs = 50U;
static const uint32_t kSleepfaceInputHoldMs = 50U;
static const uint32_t kSleepfaceWakeHoldMs = 500U;
static const uint32_t kSleepfaceLvcoIntervalS = 60U;
static const uint32_t kSleepfaceIntervalDefaultS = 1U;

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
static volatile uint8_t s_sleepface_active = 0U;
static volatile uint8_t s_rtc_alarm_pending = 0U;
static volatile uint8_t s_rtc_set_pending = 0U;
static uint8_t s_rtc_settings_applied = 0U;
static volatile uint8_t s_sleepface_interval_dirty = 0U;
static volatile uint32_t s_sleepface_interval_s = kSleepfaceIntervalDefaultS;
static uint32_t s_sleepface_hold_until = 0U;
static uint32_t s_sleepface_wake_hold_until = 0U;
static uint32_t s_lvco_accum_s = 0U;
static power_rtc_datetime_t s_rtc_set_value = {0};
static volatile uint8_t s_sleepface_minimal_clocks = 0U;
static volatile uint8_t s_restore_full_clocks = 0U;

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

static uint8_t power_task_apply_perf_mode(power_perf_mode_t mode);

static uint32_t power_task_sanitize_sleepface_interval(uint32_t interval_s)
{
  static const uint32_t kIntervals[] = { 1U, 2U, 5U, 10U, 30U, 60U };
  for (uint32_t i = 0U; i < (uint32_t)(sizeof(kIntervals) / sizeof(kIntervals[0])); ++i)
  {
    if (interval_s == kIntervals[i])
    {
      return interval_s;
    }
  }
  return kSleepfaceIntervalDefaultS;
}

static uint8_t power_task_is_leap_year(uint16_t year)
{
  return ((year % 4U) == 0U) ? 1U : 0U;
}

static uint8_t power_task_days_in_month(uint16_t year, uint8_t month)
{
  static const uint8_t kDays[12] = { 31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U };
  if ((month == 0U) || (month > 12U))
  {
    return 31U;
  }

  uint8_t days = kDays[month - 1U];
  if ((month == 2U) && (power_task_is_leap_year(year) != 0U))
  {
    days = 29U;
  }
  return days;
}

static uint8_t power_task_weekday_from_date(uint16_t year, uint8_t month, uint8_t day)
{
  static const uint8_t kMonthTable[12] = { 0U, 3U, 2U, 5U, 0U, 3U, 5U, 1U, 4U, 6U, 2U, 4U };
  uint16_t y = year;
  if (month < 3U)
  {
    y = (uint16_t)(y - 1U);
  }

  uint32_t w = (uint32_t)(y + (y / 4U) - (y / 100U) + (y / 400U) + kMonthTable[month - 1U] + day);
  uint8_t dow = (uint8_t)(w % 7U); // 0=Sunday, 1=Monday, ...
  return (dow == 0U) ? RTC_WEEKDAY_SUNDAY : dow;
}

static uint8_t power_task_rtc_get_datetime(power_rtc_datetime_t *out)
{
  if (out == NULL)
  {
    return 0U;
  }

  RTC_TimeTypeDef t = {0};
  RTC_DateTypeDef d = {0};
  if (HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN) != HAL_OK)
  {
    return 0U;
  }
  if (HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN) != HAL_OK)
  {
    return 0U;
  }

  out->hours = (uint8_t)t.Hours;
  out->minutes = (uint8_t)t.Minutes;
  out->seconds = (uint8_t)t.Seconds;
  out->day = (uint8_t)d.Date;
  out->month = (uint8_t)d.Month;
  out->year = (uint16_t)(2000U + (uint16_t)d.Year);
  return 1U;
}

static uint8_t power_task_rtc_is_default(const power_rtc_datetime_t *dt)
{
  if (dt == NULL)
  {
    return 1U;
  }

  if ((dt->year == 2000U) &&
      (dt->month == 1U) &&
      (dt->day == 1U) &&
      (dt->hours == 0U) &&
      (dt->minutes == 0U) &&
      (dt->seconds == 0U))
  {
    return 1U;
  }

  return 0U;
}

static void power_task_apply_rtc_settings_once(void)
{
  if (s_rtc_settings_applied != 0U)
  {
    return;
  }

  if (!settings_is_loaded())
  {
    return;
  }

  power_rtc_datetime_t now = {0};
  uint8_t apply = 1U;
  if (power_task_rtc_get_datetime(&now) != 0U)
  {
    apply = power_task_rtc_is_default(&now);
  }

  if (apply != 0U)
  {
    settings_rtc_datetime_t stored = {0};
    if (settings_get(SETTINGS_KEY_RTC_DATETIME, &stored))
    {
      power_rtc_datetime_t dt = {0};
      dt.hours = stored.hours;
      dt.minutes = stored.minutes;
      dt.seconds = stored.seconds;
      dt.day = stored.day;
      dt.month = stored.month;
      dt.year = stored.year;
      power_task_request_rtc_set(&dt);
    }
  }

  s_rtc_settings_applied = 1U;
}

static void power_task_rtc_add_seconds(power_rtc_datetime_t *dt, uint32_t delta_s)
{
  if (dt == NULL)
  {
    return;
  }

  uint32_t total = (uint32_t)dt->seconds +
                   ((uint32_t)dt->minutes * 60U) +
                   ((uint32_t)dt->hours * 3600U) +
                   delta_s;
  uint32_t days = total / 86400U;
  total = total % 86400U;

  dt->hours = (uint8_t)(total / 3600U);
  total %= 3600U;
  dt->minutes = (uint8_t)(total / 60U);
  dt->seconds = (uint8_t)(total % 60U);

  while (days > 0U)
  {
    uint8_t dim = power_task_days_in_month(dt->year, dt->month);
    if (dt->day < dim)
    {
      dt->day++;
    }
    else
    {
      dt->day = 1U;
      if (dt->month < 12U)
      {
        dt->month++;
      }
      else
      {
        dt->month = 1U;
        dt->year++;
      }
    }
    days--;
  }
}

static void power_task_rtc_disable_alarm(void)
{
  (void)HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A);
}

static uint8_t power_task_rtc_schedule_alarm(uint32_t interval_s)
{
  uint32_t interval = power_task_sanitize_sleepface_interval(interval_s);
  power_rtc_datetime_t now = {0};
  if (power_task_rtc_get_datetime(&now) == 0U)
  {
    return 0U;
  }

  power_rtc_datetime_t alarm_dt = now;
  power_task_rtc_add_seconds(&alarm_dt, interval);

  RTC_AlarmTypeDef alarm = {0};
  alarm.Alarm = RTC_ALARM_A;
  alarm.AlarmMask = RTC_ALARMMASK_NONE;
  alarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;
  alarm.AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_DATE;
  alarm.AlarmDateWeekDay = alarm_dt.day;
  alarm.AlarmTime.Hours = alarm_dt.hours;
  alarm.AlarmTime.Minutes = alarm_dt.minutes;
  alarm.AlarmTime.Seconds = alarm_dt.seconds;
  alarm.AlarmTime.SubSeconds = 0U;
  alarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  alarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;

  (void)HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A);
  return (HAL_RTC_SetAlarm_IT(&hrtc, &alarm, RTC_FORMAT_BIN) == HAL_OK) ? 1U : 0U;
}

static uint8_t power_task_rtc_set_datetime(const power_rtc_datetime_t *dt)
{
  if (dt == NULL)
  {
    return 0U;
  }

  power_rtc_datetime_t v = *dt;
  if (v.hours > 23U)
  {
    v.hours = 23U;
  }
  if (v.minutes > 59U)
  {
    v.minutes = 59U;
  }
  if (v.seconds > 59U)
  {
    v.seconds = 59U;
  }
  if (v.month == 0U)
  {
    v.month = 1U;
  }
  if (v.month > 12U)
  {
    v.month = 12U;
  }
  uint8_t dim = power_task_days_in_month(v.year, v.month);
  if (v.day == 0U)
  {
    v.day = 1U;
  }
  if (v.day > dim)
  {
    v.day = dim;
  }

  RTC_TimeTypeDef t = {0};
  RTC_DateTypeDef d = {0};
  t.Hours = v.hours;
  t.Minutes = v.minutes;
  t.Seconds = v.seconds;
  t.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  t.StoreOperation = RTC_STOREOPERATION_RESET;

  d.Date = v.day;
  d.Month = v.month;
  d.Year = (uint8_t)((v.year >= 2000U) ? (v.year - 2000U) : (v.year % 100U));
  d.WeekDay = power_task_weekday_from_date(v.year, v.month, v.day);

  if (HAL_RTC_SetTime(&hrtc, &t, RTC_FORMAT_BIN) != HAL_OK)
  {
    return 0U;
  }
  if (HAL_RTC_SetDate(&hrtc, &d, RTC_FORMAT_BIN) != HAL_OK)
  {
    return 0U;
  }

  return 1U;
}

static void power_task_sleepface_tick(void)
{
  if (s_sleepface_active == 0U)
  {
    return;
  }

  power_rtc_datetime_t now = {0};
  if (power_task_rtc_get_datetime(&now) != 0U)
  {
    sleep_face_render(&now);
    app_display_cmd_t cmd = APP_DISPLAY_CMD_INVALIDATE;
    (void)osMessageQueuePut(qDisplayCmdHandle, &cmd, 0U, 0U);
  }

  s_sleepface_hold_until = osKernelGetTickCount() + kSleepfaceHoldMs;
  s_sleep_pending = 1U;

  uint32_t interval = power_task_sanitize_sleepface_interval(s_sleepface_interval_s);
  s_lvco_accum_s += interval;
  if (s_lvco_accum_s >= kSleepfaceLvcoIntervalS)
  {
    s_lvco_accum_s -= kSleepfaceLvcoIntervalS;
    if (qSensorReqHandle != NULL)
    {
      app_sensor_req_t req = APP_SENSOR_REQ_LVCO_TICK;
      (void)osMessageQueuePut(qSensorReqHandle, &req, 0U, 0U);
    }
  }

  (void)power_task_rtc_schedule_alarm(interval);
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

static void power_task_restore_full_clocks(void);

static void power_task_ospi_reinit(void)
{
  OSPIM_CfgTypeDef mgr = {0};
  HAL_OSPI_DLYB_CfgTypeDef dlyb = {0};

  (void)HAL_OSPI_DeInit(&hospi1);

  hospi1.Instance = OCTOSPI1;
  hospi1.Init.FifoThreshold = 1;
  hospi1.Init.DualQuad = HAL_OSPI_DUALQUAD_DISABLE;
  hospi1.Init.MemoryType = HAL_OSPI_MEMTYPE_MICRON;
  hospi1.Init.DeviceSize = 24;
  hospi1.Init.ChipSelectHighTime = 2;
  hospi1.Init.FreeRunningClock = HAL_OSPI_FREERUNCLK_DISABLE;
  hospi1.Init.ClockMode = HAL_OSPI_CLOCK_MODE_0;
  hospi1.Init.WrapSize = HAL_OSPI_WRAP_NOT_SUPPORTED;
  hospi1.Init.ClockPrescaler = 8;
  hospi1.Init.SampleShifting = HAL_OSPI_SAMPLE_SHIFTING_NONE;
  hospi1.Init.DelayHoldQuarterCycle = HAL_OSPI_DHQC_DISABLE;
  hospi1.Init.ChipSelectBoundary = 0;
  hospi1.Init.DelayBlockBypass = HAL_OSPI_DELAY_BLOCK_BYPASSED;
  hospi1.Init.MaxTran = 0;
  hospi1.Init.Refresh = 0;
  (void)HAL_OSPI_Init(&hospi1);

  mgr.ClkPort = 1;
  mgr.NCSPort = 2;
  mgr.IOLowPort = HAL_OSPIM_IOPORT_1_LOW;
  (void)HAL_OSPIM_Config(&hospi1, &mgr, HAL_OSPI_TIMEOUT_DEFAULT_VALUE);

  dlyb.Units = 0;
  dlyb.PhaseSel = 0;
  (void)HAL_OSPI_DLYB_SetConfig(&hospi1, &dlyb);
}

static void power_task_restore_full_clocks(void)
{
  SystemClock_Config();
  PeriphCommonClock_Config();
  power_task_ospi_reinit();
  s_perf_active = POWER_PERF_MODE_CRUISE;
  if ((s_in_game != 0U) && (s_sleepface_active == 0U) && (s_rtc_alarm_pending == 0U))
  {
    (void)power_task_apply_perf_mode(s_game_perf_mode);
  }
  s_sleepface_minimal_clocks = 0U;
}

static void power_task_restore_sleepface_clocks(void)
{
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE3) != HAL_OK)
  {
    return;
  }

  RCC_OscInitTypeDef osc = {0};
  osc.OscillatorType = RCC_OSCILLATORTYPE_MSI | RCC_OSCILLATORTYPE_MSIK | RCC_OSCILLATORTYPE_HSI;
  osc.MSIState = RCC_MSI_ON;
  osc.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  osc.MSIClockRange = RCC_MSIRANGE_1;
  osc.MSIKState = RCC_MSIK_ON;
  osc.MSIKClockRange = RCC_MSIKRANGE_4;
  osc.HSIState = RCC_HSI_OFF;
  osc.PLL.PLLState = RCC_PLL_NONE;
  (void)HAL_RCC_OscConfig(&osc);

  RCC_ClkInitTypeDef clk = {0};
  clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_PCLK3;
  clk.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clk.APB1CLKDivider = RCC_HCLK_DIV1;
  clk.APB2CLKDivider = RCC_HCLK_DIV1;
  clk.APB3CLKDivider = RCC_HCLK_DIV8;
  (void)HAL_RCC_ClockConfig(&clk, kFlashLatencyCruise);

  vPortSetupTimerInterrupt();
  s_perf_active = POWER_PERF_MODE_CRUISE;
  s_sleepface_minimal_clocks = 1U;
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

static void power_task_quiesce_kick(void)
{
  /* Wake idle owners so they can observe quiesce and ACK. */
  if (qAudioCmdHandle != NULL)
  {
    app_audio_cmd_t cmd = 0U;
    (void)osMessageQueuePut(qAudioCmdHandle, &cmd, 0U, 0U);
  }
  if (qStorageReqHandle != NULL)
  {
    app_storage_req_t req = (app_storage_req_t)STORAGE_OP_NONE;
    (void)osMessageQueuePut(qStorageReqHandle, &req, 0U, 0U);
  }
}

static uint8_t power_task_wake_hold_active(void)
{
  if (s_sleepface_wake_hold_until == 0U)
  {
    return 0U;
  }

  uint32_t now = osKernelGetTickCount();
  if ((int32_t)(now - s_sleepface_wake_hold_until) < 0)
  {
    return 1U;
  }

  s_sleepface_wake_hold_until = 0U;
  return 0U;
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
  if (s_sleepface_active != 0U)
  {
    rtos_isr_bridge_set_nonwake_buttons_enabled(0U);
  }
  power_task_configure_lpuart_wakeup();
  HAL_SuspendTick();
  (void)HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);
  HAL_ResumeTick();
  if (s_sleepface_active != 0U)
  {
    power_task_restore_sleepface_clocks();
  }
  else
  {
    power_task_restore_full_clocks();
  }
  if (s_sleepface_active != 0U)
  {
    rtos_isr_bridge_set_nonwake_buttons_enabled(1U);
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

  if (power_task_wake_hold_active() != 0U)
  {
    s_sleep_pending = 0U;
    return;
  }

  if (s_sleepface_hold_until != 0U)
  {
    uint32_t now_ms = osKernelGetTickCount();
    if ((int32_t)(now_ms - s_sleepface_hold_until) < 0)
    {
      return;
    }
    s_sleepface_hold_until = 0U;
  }

  if (audio_is_active() != 0U)
  {
    return;
  }

  if (storage_is_busy() != 0U)
  {
    return;
  }

  if (power_task_quiesce_active() == 0U)
  {
    power_task_quiesce_set(1U);
    power_task_quiesce_kick();
  }

  if (power_task_quiesce_ready() == 0U)
  {
    return;
  }

  if (s_sleepface_active == 0U)
  {
    s_sleepface_active = 1U;
    s_lvco_accum_s = 0U;
    (void)power_task_rtc_schedule_alarm(s_sleepface_interval_s);
  }

  s_sleep_pending = 0U;
  power_task_enter_stop2();
  if (s_rtc_alarm_pending != 0U)
  {
    s_sleep_pending = 1U;
    power_task_quiesce_set(0U);
    return;
  }

  s_sleepface_hold_until = osKernelGetTickCount() + kSleepfaceInputHoldMs;
  s_sleep_pending = 1U;
  power_task_quiesce_set(0U);
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
    power_task_apply_rtc_settings_once();

    if (s_restore_full_clocks != 0U)
    {
      s_restore_full_clocks = 0U;
      power_task_restore_full_clocks();
    }

    if (power_task_wake_hold_active() != 0U)
    {
      s_sleep_pending = 0U;
      if (s_rtc_alarm_pending != 0U)
      {
        s_rtc_alarm_pending = 0U;
        power_task_rtc_disable_alarm();
      }
    }

    uint32_t timeout = (s_sleep_pending != 0U) ? 20U : osWaitForever;
    if (s_rtc_settings_applied == 0U)
    {
      if (timeout == osWaitForever)
      {
        timeout = 50U;
      }
    }
    if ((s_rtc_alarm_pending != 0U) || (s_rtc_set_pending != 0U) || (s_sleepface_interval_dirty != 0U))
    {
      timeout = 0U;
    }
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
          if (power_task_wake_hold_active() == 0U)
          {
            s_sleep_pending = 1U;
          }
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

    if (s_rtc_set_pending != 0U)
    {
      power_rtc_datetime_t dt = s_rtc_set_value;
      s_rtc_set_pending = 0U;
      power_task_rtc_disable_alarm();
      (void)power_task_rtc_set_datetime(&dt);
      if ((s_sleepface_active != 0U) && (s_sleep_enabled != 0U))
      {
        (void)power_task_rtc_schedule_alarm(s_sleepface_interval_s);
      }
    }

    if (s_sleepface_interval_dirty != 0U)
    {
      s_sleepface_interval_dirty = 0U;
      if ((s_sleepface_active != 0U) && (s_sleep_enabled != 0U))
      {
        (void)power_task_rtc_schedule_alarm(s_sleepface_interval_s);
      }
    }

    if (s_rtc_alarm_pending != 0U)
    {
      s_rtc_alarm_pending = 0U;
      if ((s_sleepface_active != 0U) && (s_sleep_enabled != 0U))
      {
        power_task_sleepface_tick();
      }
      else
      {
        power_task_rtc_disable_alarm();
      }
    }

    power_task_try_sleep();
  }
}

void power_task_activity_ping(void)
{
  uint8_t was_sleepface = s_sleepface_active;
  if (s_sleepface_active != 0U)
  {
    s_sleepface_active = 0U;
    s_lvco_accum_s = 0U;
    s_sleepface_wake_hold_until = osKernelGetTickCount() + kSleepfaceWakeHoldMs;
    s_rtc_alarm_pending = 0U;
    power_task_rtc_disable_alarm();
  }

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

  if ((was_sleepface != 0U) && (s_sleepface_minimal_clocks != 0U))
  {
    s_restore_full_clocks = 1U;
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

void power_task_set_sleepface_interval_s(uint32_t interval_s)
{
  uint32_t v = power_task_sanitize_sleepface_interval(interval_s);
  s_sleepface_interval_s = v;
  s_sleepface_interval_dirty = 1U;
}

uint32_t power_task_get_sleepface_interval_s(void)
{
  return s_sleepface_interval_s;
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

uint8_t power_task_is_sleepface_active(void)
{
  return s_sleepface_active;
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

void power_task_request_rtc_set(const power_rtc_datetime_t *dt)
{
  if (dt == NULL)
  {
    return;
  }

  settings_rtc_datetime_t stored = {0};
  stored.hours = dt->hours;
  stored.minutes = dt->minutes;
  stored.seconds = dt->seconds;
  stored.day = dt->day;
  stored.month = dt->month;
  stored.year = dt->year;
  (void)settings_set(SETTINGS_KEY_RTC_DATETIME, &stored);

  s_rtc_set_value = *dt;
  s_rtc_set_pending = 1U;
}

void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc_handle)
{
  (void)hrtc_handle;
  s_rtc_alarm_pending = 1U;
}
