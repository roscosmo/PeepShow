#include "sensor_task.h"

#include "app_freertos.h"
#include "cmsis_os2.h"
#include "tmag5273.h"
#include "ADP5360.h"
#include "settings.h"
#include "power_task.h"

#include <math.h>
#include <string.h>

extern TMAGJoy *UI_GetJoy(void);

typedef struct
{
  uint32_t phase_start_ms;
  uint32_t last_sample_ms;
  uint32_t last_step_ms;
  uint32_t sample_count;
  double sum_x;
  double sum_y;
  double sum_x2;
  double sum_y2;
  float neutral_min_x;
  float neutral_max_x;
  float neutral_min_y;
  float neutral_max_y;
  float cx;
  float cy;
  float up_x;
  float up_y;
  float right_x;
  float right_y;
  float down_x;
  float down_y;
  float left_x;
  float left_y;
  float rot_deg;
  float rot_c;
  float rot_s;
  uint8_t invert_x;
  uint8_t invert_y;
  float min_x;
  float max_x;
  float min_y;
  float max_y;
  float raw_min_x;
  float raw_max_x;
  float raw_min_y;
  float raw_max_y;
  float sx;
  float sy;
  float neutral_mT;
  float thr_mT;
  uint8_t retry;
  uint8_t have_prev;
  TMAGJoy_Cal prev_cal;
  float prev_thr_x;
  float prev_thr_y;
  uint8_t prev_abs_dz_en;
  float prev_abs_dz_mT;
} sensor_joy_cal_t;

static sensor_joy_status_t s_status;
static sensor_joy_cal_t s_cal;
static uint8_t s_menu_nav_enabled = 0U;
static uint8_t s_monitor_enabled = 0U;
static sensor_power_status_t s_power_status;
static uint8_t s_power_stats_enabled = 0U;
static uint32_t s_power_last_ms = 0U;
static uint32_t s_lvco_last_ms = 0U;
static uint8_t s_lvco_low_count = 0U;
static uint8_t s_lvco_cut_latched = 0U;
static uint32_t s_settings_seq = 0U;

static const uint32_t kJoyCalSampleMs = 10U;
static const uint32_t kJoyCalNeutralMs = 1500U;
static const uint32_t kJoyCalDirMs = 2500U;
static const uint32_t kJoyCalSweepMs = 5000U;
static const uint32_t kJoyCalStepMs = 20U;
static const uint32_t kJoyCalMinNeutralSamples = 40U;
static const uint32_t kJoyCalMinDirSamples = 8U;
static const uint32_t kJoyCalMinSweepSamples = 50U;
static const float kJoyCalDirMinMt = 0.6f;
static const float kJoyCalDirMinScale = 1.6f;
static const float kJoyCalDirMinMax = 12.0f;

static const uint32_t kJoyMonitorTickMs = 100U;
static const uint32_t kJoyCalTickMs = 10U;
static const uint32_t kPowerStatsTickMs = 1000U;
static const uint32_t kBattCutoffPollMs = 60000U;
static const uint16_t kBattCutoffMv = 3500U;
static const uint8_t kBattCutoffDebounce = 2U;

static float s_menu_press_norm = 0.45f;
static float s_menu_release_norm = 0.25f;
static float s_menu_axis_ratio = 1.4f;
static uint8_t s_menu_wait_neutral = 0U;

static const float kJoyMenuPressMin = 0.25f;
static const float kJoyMenuPressMax = 0.90f;
static const float kJoyMenuReleaseMin = 0.10f;
static const float kJoyMenuReleaseMax = 0.80f;
static const float kJoyMenuAxisMin = 1.0f;
static const float kJoyMenuAxisMax = 2.5f;
static const float kJoyMenuPressStep = 0.05f;
static const float kJoyMenuReleaseStep = 0.05f;
static const float kJoyMenuAxisStep = 0.1f;
static const float kJoyMenuHystMin = 0.05f;

static bool sensor_is_ui_mode(void);

static float sensor_clampf(float v, float lo, float hi)
{
  if (v < lo)
  {
    return lo;
  }
  if (v > hi)
  {
    return hi;
  }
  return v;
}

static void sensor_joy_cal_reset(void)
{
  (void)memset(&s_cal, 0, sizeof(s_cal));
  s_cal.rot_c = 1.0f;
  s_cal.rot_s = 0.0f;
  s_cal.sx = 1.0f;
  s_cal.sy = 1.0f;
}

static void sensor_joy_menu_reset_state(void)
{
  s_menu_wait_neutral = 0U;
}

static void sensor_joy_menu_clamp(void)
{
  s_menu_press_norm = sensor_clampf(s_menu_press_norm, kJoyMenuPressMin, kJoyMenuPressMax);
  s_menu_release_norm = sensor_clampf(s_menu_release_norm, kJoyMenuReleaseMin, kJoyMenuReleaseMax);
  if (s_menu_release_norm > (s_menu_press_norm - kJoyMenuHystMin))
  {
    s_menu_release_norm = s_menu_press_norm - kJoyMenuHystMin;
  }
  if (s_menu_release_norm < kJoyMenuReleaseMin)
  {
    s_menu_release_norm = kJoyMenuReleaseMin;
  }
  if (s_menu_press_norm < (s_menu_release_norm + kJoyMenuHystMin))
  {
    s_menu_press_norm = s_menu_release_norm + kJoyMenuHystMin;
  }
  s_menu_press_norm = sensor_clampf(s_menu_press_norm, kJoyMenuPressMin, kJoyMenuPressMax);
  s_menu_axis_ratio = sensor_clampf(s_menu_axis_ratio, kJoyMenuAxisMin, kJoyMenuAxisMax);
}

static void sensor_joy_status_reset(void)
{
  (void)memset(&s_status, 0, sizeof(s_status));
  s_status.stage = SENSOR_JOY_STAGE_IDLE;
  s_status.progress = 0.0f;
  s_status.dir = TMAGJOY_NEUTRAL;
  s_status.sx_mT = 1.0f;
  s_status.sy_mT = 1.0f;
  sensor_joy_cal_reset();
}

static void sensor_power_status_reset(void)
{
  (void)memset(&s_power_status, 0, sizeof(s_power_status));
  s_power_status.valid = 0U;
  s_power_last_ms = 0U;
  s_lvco_last_ms = 0U;
  s_lvco_low_count = 0U;
  s_lvco_cut_latched = 0U;
}

static void sensor_power_set_enabled(uint8_t enable)
{
  s_power_stats_enabled = enable ? 1U : 0U;
  if (s_power_stats_enabled != 0U)
  {
    s_power_last_ms = 0U;
    s_power_status.valid = 0U;
  }
}

static void sensor_power_isofet_set(uint8_t on)
{
  ADP5360_func_t func = {0};
  if (ADP5360_get_chg_function(&func) != HAL_OK)
  {
    return;
  }

  // off_isofet is active-high, so on=1 means off_isofet=0 (conducting).
  uint8_t off_isofet = (on != 0U) ? 0U : 1U;
  if (func.off_isofet == off_isofet)
  {
    return;
  }

  func.off_isofet = off_isofet;
  (void)ADP5360_set_chg_function(&func);
}

static void sensor_power_lvco_update(uint32_t now_ms)
{
  if (s_lvco_cut_latched != 0U)
  {
    return;
  }

  if ((s_lvco_last_ms != 0U) && ((now_ms - s_lvco_last_ms) < kBattCutoffPollMs))
  {
    return;
  }

  s_lvco_last_ms = now_ms;

  ADP5360_pgood_t pgood = {0};
  if (ADP5360_get_pgood(&pgood, NULL) != HAL_OK)
  {
    s_lvco_low_count = 0U;
    return;
  }

  if (pgood.vbus_ok != 0U)
  {
    s_lvco_low_count = 0U;
    return;
  }

  uint16_t vbat_mV = 0U;
  if (ADP5360_get_vbat(&vbat_mV, NULL) != HAL_OK)
  {
    s_lvco_low_count = 0U;
    return;
  }

  if (vbat_mV <= kBattCutoffMv)
  {
    if (s_lvco_low_count < 0xFFU)
    {
      s_lvco_low_count++;
    }
    if (s_lvco_low_count >= kBattCutoffDebounce)
    {
      s_lvco_cut_latched = 1U;
      sensor_power_isofet_set(0U);
    }
  }
  else
  {
    s_lvco_low_count = 0U;
  }
}

static void sensor_power_update(uint32_t now_ms)
{
  if ((s_power_stats_enabled == 0U) || !sensor_is_ui_mode())
  {
    return;
  }

  if ((s_power_last_ms != 0U) && ((now_ms - s_power_last_ms) < kPowerStatsTickMs))
  {
    return;
  }

  s_power_last_ms = now_ms;

  uint16_t vbat_mV = 0U;
  uint16_t raw12 = 0U;
  uint8_t soc_percent = 0U;
  uint8_t raw7 = 0U;
  ADP5360_status1_t st1 = {0};
  ADP5360_status2_t st2 = {0};
  ADP5360_pgood_t pgood = {0};
  uint8_t raw_pgood = 0U;
  uint8_t fault_mask = 0U;
  uint8_t ok = 1U;

  if (ADP5360_get_vbat(&vbat_mV, &raw12) != HAL_OK)
  {
    ok = 0U;
  }
  if (ADP5360_get_soc(&soc_percent, &raw7) != HAL_OK)
  {
    ok = 0U;
  }
  if (ADP5360_get_status1(&st1) != HAL_OK)
  {
    ok = 0U;
  }
  if (ADP5360_get_status2(&st2) != HAL_OK)
  {
    ok = 0U;
  }
  (void)ADP5360_get_pgood(&pgood, &raw_pgood);
  (void)ADP5360_get_fault(&fault_mask);

  if (ok != 0U)
  {
    s_power_status.vbat_mV = vbat_mV;
    s_power_status.soc_percent = soc_percent;
    s_power_status.st1 = st1;
    s_power_status.st2 = st2;
    s_power_status.pgood = pgood;
    s_power_status.fault_mask = fault_mask;
    s_power_status.valid = 1U;
  }
  else
  {
    s_power_status.valid = 0U;
  }
}

static bool sensor_is_ui_mode(void)
{
  if (egModeHandle == NULL)
  {
    return false;
  }

  uint32_t flags = osEventFlagsGet(egModeHandle);
  if ((int32_t)flags < 0)
  {
    return false;
  }

  return ((flags & APP_MODE_UI) != 0U);
}

static void sensor_joy_refresh_status(TMAGJoy *joy, bool sample_raw)
{
  if (joy == NULL)
  {
    s_status.dir = TMAGJOY_NEUTRAL;
    s_status.nx = 0.0f;
    s_status.ny = 0.0f;
    s_status.r_abs_mT = 0.0f;
    s_status.sx_mT = 1.0f;
    s_status.sy_mT = 1.0f;
    s_status.thr_mT = 0.0f;
    s_status.deadzone_mT = 0.0f;
    s_status.deadzone_en = 0U;
    return;
  }

  TMAGJoy_Cal cal;
  TMAGJoy_GetCal(joy, &cal);
  s_status.sx_mT = cal.sx;
  s_status.sy_mT = cal.sy;

  float thr_x = 0.0f;
  float thr_y = 0.0f;
  TMAGJoy_GetThresholds(joy, &thr_x, &thr_y);
  float thr_mT = thr_x;
  if ((thr_y > 0.0f) && ((thr_mT <= 0.0f) || (thr_y < thr_mT)))
  {
    thr_mT = thr_y;
  }
  s_status.thr_mT = thr_mT;

  uint8_t dz_en = 0U;
  float dz_mT = 0.0f;
  TMAGJoy_GetAbsDeadzone(joy, &dz_en, &dz_mT);
  s_status.deadzone_en = dz_en;
  s_status.deadzone_mT = dz_mT;

  if (sample_raw)
  {
    TMAGJoy_ReadCalibratedRaw(joy, &s_status.nx, &s_status.ny, &s_status.r_abs_mT);
  }
}

static void sensor_apply_settings(TMAGJoy *joy, bool apply_cal)
{
  settings_data_t data;
  settings_get(&data);

  s_menu_press_norm = data.menu_press_norm;
  s_menu_release_norm = data.menu_release_norm;
  s_menu_axis_ratio = data.menu_axis_ratio;
  sensor_joy_menu_clamp();
  sensor_joy_menu_reset_state();

  if ((joy != NULL) && apply_cal && (data.joy.valid != 0U))
  {
    TMAGJoy_SetCenter(joy, data.joy.cx, data.joy.cy);
    TMAGJoy_SetSpan(joy, data.joy.sx, data.joy.sy);
    TMAGJoy_SetRotationDeg(joy, data.joy.rot_deg);
    TMAGJoy_SetInvert(joy, data.joy.invert_x, data.joy.invert_y);
    TMAGJoy_SetAbsDeadzone(joy, data.joy.abs_deadzone_en, data.joy.abs_deadzone_mT);
    (void)TMAG5273_set_x_threshold_mT(data.joy.thr_x_mT);
    (void)TMAG5273_set_y_threshold_mT(data.joy.thr_y_mT);
    joy->cfg.thr_x_mT = data.joy.thr_x_mT;
    joy->cfg.thr_y_mT = data.joy.thr_y_mT;
    sensor_joy_refresh_status(joy, false);
  }
}

static void sensor_store_joy_settings(TMAGJoy *joy, uint8_t valid)
{
  if (joy == NULL)
  {
    return;
  }

  settings_joy_cal_t cal;
  (void)memset(&cal, 0, sizeof(cal));

  TMAGJoy_Cal joy_cal;
  TMAGJoy_GetCal(joy, &joy_cal);
  cal.cx = joy_cal.cx;
  cal.cy = joy_cal.cy;
  cal.sx = joy_cal.sx;
  cal.sy = joy_cal.sy;
  cal.rot_deg = joy_cal.rot_deg;
  cal.invert_x = joy_cal.invert_x;
  cal.invert_y = joy_cal.invert_y;

  float thr_x = 0.0f;
  float thr_y = 0.0f;
  TMAGJoy_GetThresholds(joy, &thr_x, &thr_y);
  cal.thr_x_mT = thr_x;
  cal.thr_y_mT = thr_y;

  uint8_t dz_en = 0U;
  float dz_mT = 0.0f;
  TMAGJoy_GetAbsDeadzone(joy, &dz_en, &dz_mT);
  cal.abs_deadzone_en = dz_en;
  cal.abs_deadzone_mT = dz_mT;

  cal.valid = (valid != 0U) ? 1U : 0U;
  settings_set_joy_cal(&cal);
}

static float sensor_joy_cal_dir_min_mT(void)
{
  float min = kJoyCalDirMinMt;
  if (s_cal.neutral_mT > 0.0f)
  {
    float scaled = s_cal.neutral_mT * kJoyCalDirMinScale;
    if (scaled > min)
    {
      min = scaled;
    }
    if (min > kJoyCalDirMinMax)
    {
      min = kJoyCalDirMinMax;
    }
  }
  return min;
}

static uint32_t sensor_joy_cal_stage_duration(sensor_joy_stage_t stage)
{
  switch (stage)
  {
    case SENSOR_JOY_STAGE_NEUTRAL:
      return kJoyCalNeutralMs;
    case SENSOR_JOY_STAGE_UP:
    case SENSOR_JOY_STAGE_RIGHT:
    case SENSOR_JOY_STAGE_DOWN:
    case SENSOR_JOY_STAGE_LEFT:
      return kJoyCalDirMs;
    case SENSOR_JOY_STAGE_SWEEP:
      return kJoyCalSweepMs;
    default:
      return 0U;
  }
}

static uint32_t sensor_joy_cal_stage_min_samples(sensor_joy_stage_t stage)
{
  switch (stage)
  {
    case SENSOR_JOY_STAGE_NEUTRAL:
      return kJoyCalMinNeutralSamples;
    case SENSOR_JOY_STAGE_UP:
    case SENSOR_JOY_STAGE_RIGHT:
    case SENSOR_JOY_STAGE_DOWN:
    case SENSOR_JOY_STAGE_LEFT:
      return kJoyCalMinDirSamples;
    case SENSOR_JOY_STAGE_SWEEP:
      return kJoyCalMinSweepSamples;
    default:
      return 0U;
  }
}

static void sensor_joy_cal_begin_stage(sensor_joy_stage_t stage, uint32_t now_ms, uint8_t retry, TMAGJoy *joy)
{
  s_status.stage = stage;
  s_status.progress = 0.0f;
  s_cal.phase_start_ms = now_ms;
  s_cal.last_sample_ms = now_ms;
  s_cal.last_step_ms = now_ms;
  s_cal.sample_count = 0U;
  s_cal.sum_x = 0.0;
  s_cal.sum_y = 0.0;
  s_cal.sum_x2 = 0.0;
  s_cal.sum_y2 = 0.0;
  s_cal.retry = retry;

  if (stage == SENSOR_JOY_STAGE_NEUTRAL)
  {
    s_cal.neutral_min_x = 1e9f;
    s_cal.neutral_max_x = -1e9f;
    s_cal.neutral_min_y = 1e9f;
    s_cal.neutral_max_y = -1e9f;
  }
  else if (stage == SENSOR_JOY_STAGE_SWEEP)
  {
    s_cal.min_x = 1e9f;
    s_cal.max_x = -1e9f;
    s_cal.min_y = 1e9f;
    s_cal.max_y = -1e9f;
    s_cal.raw_min_x = 1e9f;
    s_cal.raw_max_x = -1e9f;
    s_cal.raw_min_y = 1e9f;
    s_cal.raw_max_y = -1e9f;
  }

  sensor_joy_refresh_status(joy, false);
}

static void sensor_joy_cal_snapshot(TMAGJoy *joy)
{
  if (joy == NULL)
  {
    return;
  }

  TMAGJoy_GetCal(joy, &s_cal.prev_cal);
  TMAGJoy_GetThresholds(joy, &s_cal.prev_thr_x, &s_cal.prev_thr_y);
  TMAGJoy_GetAbsDeadzone(joy, &s_cal.prev_abs_dz_en, &s_cal.prev_abs_dz_mT);
  s_cal.have_prev = 1U;
}

static void sensor_joy_cal_restore(TMAGJoy *joy)
{
  if ((joy == NULL) || (s_cal.have_prev == 0U))
  {
    return;
  }

  TMAGJoy_SetCenter(joy, s_cal.prev_cal.cx, s_cal.prev_cal.cy);
  TMAGJoy_SetSpan(joy, s_cal.prev_cal.sx, s_cal.prev_cal.sy);
  TMAGJoy_SetRotationDeg(joy, s_cal.prev_cal.rot_deg);
  TMAGJoy_SetInvert(joy, s_cal.prev_cal.invert_x, s_cal.prev_cal.invert_y);
  TMAGJoy_SetAbsDeadzone(joy, s_cal.prev_abs_dz_en, s_cal.prev_abs_dz_mT);
  (void)TMAG5273_set_x_threshold_mT(s_cal.prev_thr_x);
  (void)TMAG5273_set_y_threshold_mT(s_cal.prev_thr_y);
  joy->cfg.thr_x_mT = s_cal.prev_thr_x;
  joy->cfg.thr_y_mT = s_cal.prev_thr_y;
  s_cal.have_prev = 0U;

  sensor_joy_refresh_status(joy, false);
}

static void sensor_joy_cal_store_dir(sensor_joy_stage_t stage, float x, float y)
{
  switch (stage)
  {
    case SENSOR_JOY_STAGE_UP:
      s_cal.up_x = x;
      s_cal.up_y = y;
      break;
    case SENSOR_JOY_STAGE_RIGHT:
      s_cal.right_x = x;
      s_cal.right_y = y;
      break;
    case SENSOR_JOY_STAGE_DOWN:
      s_cal.down_x = x;
      s_cal.down_y = y;
      break;
    case SENSOR_JOY_STAGE_LEFT:
      s_cal.left_x = x;
      s_cal.left_y = y;
      break;
    default:
      break;
  }
}

static void sensor_joy_cal_compute_axes(void)
{
  const float cx = s_cal.cx;
  const float cy = s_cal.cy;

  const float vrx = s_cal.right_x - cx;
  const float vry = s_cal.right_y - cy;
  const float vlx = s_cal.left_x - cx;
  const float vly = s_cal.left_y - cy;
  const float vux = s_cal.up_x - cx;
  const float vuy = s_cal.up_y - cy;
  const float vdx = s_cal.down_x - cx;
  const float vdy = s_cal.down_y - cy;

  float rot = 0.0f;
  float best_err = 1e9f;
  uint8_t best_ix = 0U;
  uint8_t best_iy = 0U;

  const float eps = 1e-3f;
  struct { float x; float y; float mag; } v[4] =
  {
    { vrx, vry, sqrtf(vrx * vrx + vry * vry) },
    { vux, vuy, sqrtf(vux * vux + vuy * vuy) },
    { vlx, vly, sqrtf(vlx * vlx + vly * vly) },
    { vdx, vdy, sqrtf(vdx * vdx + vdy * vdy) }
  };
  const float tx[4] = { 1.0f, 0.0f, -1.0f, 0.0f };
  const float ty[4] = { 0.0f, 1.0f, 0.0f, -1.0f };

  for (uint8_t ix = 0U; ix < 2U; ++ix)
  {
    for (uint8_t iy = 0U; iy < 2U; ++iy)
    {
      float sum_c = 0.0f;
      float sum_d = 0.0f;
      uint8_t count = 0U;

      for (uint8_t i = 0U; i < 4U; ++i)
      {
        if (v[i].mag < eps)
        {
          continue;
        }
        const float vx = v[i].x / v[i].mag;
        const float vy = v[i].y / v[i].mag;
        const float tpx = ix ? -tx[i] : tx[i];
        const float tpy = iy ? -ty[i] : ty[i];
        sum_c += vx * tpy - vy * tpx;
        sum_d += vx * tpx + vy * tpy;
        count++;
      }

      if (count == 0U)
      {
        continue;
      }

      const float phi = atan2f(sum_c, sum_d);
      const float c = cosf(phi);
      const float s = sinf(phi);

      float err = 0.0f;
      for (uint8_t i = 0U; i < 4U; ++i)
      {
        if (v[i].mag < eps)
        {
          continue;
        }
        float rx = c * v[i].x - s * v[i].y;
        float ry = s * v[i].x + c * v[i].y;
        if (ix)
        {
          rx = -rx;
        }
        if (iy)
        {
          ry = -ry;
        }
        const float dx = rx / v[i].mag - tx[i];
        const float dy = ry / v[i].mag - ty[i];
        err += dx * dx + dy * dy;
      }

      if (err < best_err)
      {
        best_err = err;
        rot = phi;
        best_ix = ix;
        best_iy = iy;
      }
    }
  }

  s_cal.rot_deg = rot * 57.2957795f;
  s_cal.rot_c = cosf(rot);
  s_cal.rot_s = sinf(rot);
  s_cal.invert_x = best_ix;
  s_cal.invert_y = best_iy;
}

static void sensor_joy_start_calibration(TMAGJoy *joy, uint32_t now_ms)
{
  if (joy == NULL)
  {
    return;
  }

  sensor_joy_cal_snapshot(joy);
  sensor_joy_cal_begin_stage(SENSOR_JOY_STAGE_NEUTRAL, now_ms, 0U, joy);
}

static void sensor_joy_save(TMAGJoy *joy)
{
  if ((s_status.stage != SENSOR_JOY_STAGE_IDLE) &&
      (s_status.stage != SENSOR_JOY_STAGE_DONE))
  {
    sensor_joy_cal_restore(joy);
    sensor_joy_cal_reset();
    s_status.stage = SENSOR_JOY_STAGE_IDLE;
    s_status.progress = 0.0f;
    return;
  }

  if (s_status.stage == SENSOR_JOY_STAGE_DONE)
  {
    s_status.progress = 1.0f;
  }
}

static void sensor_joy_set_menu_nav(TMAGJoy *joy, uint8_t enable)
{
  s_menu_nav_enabled = enable ? 1U : 0U;
  sensor_joy_menu_reset_state();
  if (enable != 0U)
  {
    s_menu_wait_neutral = 1U;
  }
  if (joy != NULL)
  {
    TMAGJoy_MenuEnable(enable);
  }
}

static void sensor_joy_set_monitor(uint8_t enable)
{
  s_monitor_enabled = enable ? 1U : 0U;
}

static void sensor_joy_adjust_deadzone(TMAGJoy *joy, int32_t delta_px)
{
  if (joy == NULL)
  {
    return;
  }

  float max_span = (s_status.sx_mT > s_status.sy_mT) ? s_status.sx_mT : s_status.sy_mT;
  if (max_span < 1e-3f)
  {
    max_span = 1.0f;
  }

  uint16_t dz_px = (uint16_t)((s_status.deadzone_mT / max_span) * 50.0f + 0.5f);
  if (delta_px < 0)
  {
    if (dz_px >= 2U)
    {
      dz_px = (uint16_t)(dz_px - 2U);
    }
    else
    {
      dz_px = 0U;
    }
  }
  else
  {
    if (dz_px < 50U)
    {
      dz_px = (uint16_t)(dz_px + 2U);
      if (dz_px > 50U)
      {
        dz_px = 50U;
      }
    }
  }

  float dz_mT = ((float)dz_px / 50.0f) * max_span;
  if (dz_mT < 0.5f)
  {
    dz_mT = 0.5f;
  }

  TMAGJoy_SetAbsDeadzone(joy, 1U, dz_mT);
  sensor_joy_refresh_status(joy, true);

  settings_data_t data;
  settings_get(&data);
  sensor_store_joy_settings(joy, data.joy.valid);
}

static void sensor_joy_emit_menu_event(app_button_id_t button_id)
{
  if ((qInputHandle == NULL) || !sensor_is_ui_mode())
  {
    return;
  }

  if (button_id >= APP_BUTTON_COUNT)
  {
    return;
  }

  app_input_event_t evt = {0};
  evt.button_id = (uint8_t)button_id;
  evt.pressed = 1U;
  (void)osMessageQueuePut(qInputHandle, &evt, 0U, 0U);
}

static app_button_id_t sensor_joy_menu_dir(float nx, float ny)
{
  float ax = fabsf(nx);
  float ay = fabsf(ny);

  if ((ax < 1e-4f) && (ay < 1e-4f))
  {
    return APP_BUTTON_COUNT;
  }

  float ratio = s_menu_axis_ratio;
  if (ratio < kJoyMenuAxisMin)
  {
    ratio = kJoyMenuAxisMin;
  }

  if (ax >= (ay * ratio))
  {
    return (nx >= 0.0f) ? APP_BUTTON_JOY_RIGHT : APP_BUTTON_JOY_LEFT;
  }

  if (ay >= (ax * ratio))
  {
    return (ny >= 0.0f) ? APP_BUTTON_JOY_UP : APP_BUTTON_JOY_DOWN;
  }

  if (nx >= 0.0f)
  {
    return (ny >= 0.0f) ? APP_BUTTON_JOY_UPRIGHT : APP_BUTTON_JOY_DOWNRIGHT;
  }

  return (ny >= 0.0f) ? APP_BUTTON_JOY_UPLEFT : APP_BUTTON_JOY_DOWNLEFT;
}

static void sensor_joy_menu_poll(TMAGJoy *joy)
{
  if (joy == NULL)
  {
    return;
  }

  float nx = 0.0f;
  float ny = 0.0f;
  float r_abs = 0.0f;
  TMAGJoy_ReadCalibratedRaw(joy, &nx, &ny, &r_abs);

  float rN = sqrtf(nx * nx + ny * ny);
  if (rN > 1.0f)
  {
    rN = 1.0f;
  }

  uint8_t dz_en = 0U;
  float dz_mT = 0.0f;
  TMAGJoy_GetAbsDeadzone(joy, &dz_en, &dz_mT);
  if ((dz_en != 0U) && (r_abs < dz_mT))
  {
    rN = 0.0f;
  }

  if (s_menu_wait_neutral != 0U)
  {
    if (rN <= s_menu_release_norm)
    {
      s_menu_wait_neutral = 0U;
    }
    return;
  }

  if (rN < s_menu_press_norm)
  {
    return;
  }

  app_button_id_t button_id = sensor_joy_menu_dir(nx, ny);
  if (button_id != APP_BUTTON_COUNT)
  {
    s_menu_wait_neutral = 1U;
    sensor_joy_emit_menu_event(button_id);
  }
}

static void sensor_joy_handle_req(app_sensor_req_t req, TMAGJoy *joy, uint32_t now_ms)
{
  uint8_t menu_params_changed = 0U;

  if ((req & APP_SENSOR_REQ_JOY_CAL_NEUTRAL) != 0U)
  {
    sensor_joy_start_calibration(joy, now_ms);
  }
  if ((req & APP_SENSOR_REQ_JOY_CAL_EXTENTS) != 0U)
  {
    sensor_joy_start_calibration(joy, now_ms);
  }
  if ((req & APP_SENSOR_REQ_JOY_CAL_SAVE) != 0U)
  {
    sensor_joy_save(joy);
  }
  if ((req & APP_SENSOR_REQ_JOY_MENU_ON) != 0U)
  {
    sensor_joy_set_menu_nav(joy, 1U);
  }
  if ((req & APP_SENSOR_REQ_JOY_MENU_OFF) != 0U)
  {
    sensor_joy_set_menu_nav(joy, 0U);
  }
  if ((req & APP_SENSOR_REQ_JOY_MONITOR_ON) != 0U)
  {
    sensor_joy_set_monitor(1U);
  }
  if ((req & APP_SENSOR_REQ_JOY_MONITOR_OFF) != 0U)
  {
    sensor_joy_set_monitor(0U);
  }
  if ((req & APP_SENSOR_REQ_JOY_DZ_INC) != 0U)
  {
    sensor_joy_adjust_deadzone(joy, 2);
  }
  if ((req & APP_SENSOR_REQ_JOY_DZ_DEC) != 0U)
  {
    sensor_joy_adjust_deadzone(joy, -2);
  }
  if ((req & APP_SENSOR_REQ_JOY_MENU_PRESS_INC) != 0U)
  {
    s_menu_press_norm += kJoyMenuPressStep;
    sensor_joy_menu_clamp();
    sensor_joy_menu_reset_state();
    menu_params_changed = 1U;
  }
  if ((req & APP_SENSOR_REQ_JOY_MENU_PRESS_DEC) != 0U)
  {
    s_menu_press_norm -= kJoyMenuPressStep;
    sensor_joy_menu_clamp();
    sensor_joy_menu_reset_state();
    menu_params_changed = 1U;
  }
  if ((req & APP_SENSOR_REQ_JOY_MENU_RELEASE_INC) != 0U)
  {
    s_menu_release_norm += kJoyMenuReleaseStep;
    sensor_joy_menu_clamp();
    sensor_joy_menu_reset_state();
    menu_params_changed = 1U;
  }
  if ((req & APP_SENSOR_REQ_JOY_MENU_RELEASE_DEC) != 0U)
  {
    s_menu_release_norm -= kJoyMenuReleaseStep;
    sensor_joy_menu_clamp();
    sensor_joy_menu_reset_state();
    menu_params_changed = 1U;
  }
  if ((req & APP_SENSOR_REQ_JOY_MENU_RATIO_INC) != 0U)
  {
    s_menu_axis_ratio += kJoyMenuAxisStep;
    sensor_joy_menu_clamp();
    sensor_joy_menu_reset_state();
    menu_params_changed = 1U;
  }
  if ((req & APP_SENSOR_REQ_JOY_MENU_RATIO_DEC) != 0U)
  {
    s_menu_axis_ratio -= kJoyMenuAxisStep;
    sensor_joy_menu_clamp();
    sensor_joy_menu_reset_state();
    menu_params_changed = 1U;
  }
  if (menu_params_changed != 0U)
  {
    settings_set_menu_params(s_menu_press_norm, s_menu_release_norm, s_menu_axis_ratio);
  }
  if ((req & APP_SENSOR_REQ_POWER_STATS_ON) != 0U)
  {
    sensor_power_set_enabled(1U);
  }
  if ((req & APP_SENSOR_REQ_POWER_STATS_OFF) != 0U)
  {
    sensor_power_set_enabled(0U);
  }
}

static void sensor_joy_cal_step(TMAGJoy *joy, uint32_t now_ms)
{
  if (joy == NULL)
  {
    return;
  }

  if ((s_status.stage == SENSOR_JOY_STAGE_IDLE) ||
      (s_status.stage == SENSOR_JOY_STAGE_DONE))
  {
    return;
  }

  uint32_t duration = sensor_joy_cal_stage_duration(s_status.stage);
  if ((duration > 0U) && ((now_ms - s_cal.last_step_ms) >= kJoyCalStepMs))
  {
    s_cal.last_step_ms = now_ms;
    uint32_t elapsed = now_ms - s_cal.phase_start_ms;
    float p = (elapsed >= duration) ? 1.0f : ((float)elapsed / (float)duration);
    if (fabsf(p - s_status.progress) > 0.01f)
    {
      s_status.progress = p;
    }
  }

  if ((now_ms - s_cal.last_sample_ms) >= kJoyCalSampleMs)
  {
    s_cal.last_sample_ms += kJoyCalSampleMs;
    float x = 0.0f;
    float y = 0.0f;
    if (TMAG5273_read_mT(&x, &y, NULL) == 0)
    {
      if (s_status.stage == SENSOR_JOY_STAGE_NEUTRAL)
      {
        s_cal.sum_x += x;
        s_cal.sum_y += y;
        s_cal.sum_x2 += (double)x * (double)x;
        s_cal.sum_y2 += (double)y * (double)y;
        s_cal.sample_count++;
        if (x < s_cal.neutral_min_x)
        {
          s_cal.neutral_min_x = x;
        }
        if (x > s_cal.neutral_max_x)
        {
          s_cal.neutral_max_x = x;
        }
        if (y < s_cal.neutral_min_y)
        {
          s_cal.neutral_min_y = y;
        }
        if (y > s_cal.neutral_max_y)
        {
          s_cal.neutral_max_y = y;
        }
      }
      else
      {
        float dx = x - s_cal.cx;
        float dy = y - s_cal.cy;
        float r = sqrtf(dx * dx + dy * dy);
        float dir_min = sensor_joy_cal_dir_min_mT();
        if (r >= dir_min)
        {
          if (s_status.stage == SENSOR_JOY_STAGE_SWEEP)
          {
            float rx = s_cal.rot_c * dx - s_cal.rot_s * dy;
            float ry = s_cal.rot_s * dx + s_cal.rot_c * dy;
            if (s_cal.invert_x)
            {
              rx = -rx;
            }
            if (s_cal.invert_y)
            {
              ry = -ry;
            }
            if (rx < s_cal.min_x)
            {
              s_cal.min_x = rx;
            }
            if (rx > s_cal.max_x)
            {
              s_cal.max_x = rx;
            }
            if (ry < s_cal.min_y)
            {
              s_cal.min_y = ry;
            }
            if (ry > s_cal.max_y)
            {
              s_cal.max_y = ry;
            }
            if (dx < s_cal.raw_min_x)
            {
              s_cal.raw_min_x = dx;
            }
            if (dx > s_cal.raw_max_x)
            {
              s_cal.raw_max_x = dx;
            }
            if (dy < s_cal.raw_min_y)
            {
              s_cal.raw_min_y = dy;
            }
            if (dy > s_cal.raw_max_y)
            {
              s_cal.raw_max_y = dy;
            }
            s_cal.sample_count++;
          }
          else
          {
            s_cal.sum_x += x;
            s_cal.sum_y += y;
            s_cal.sample_count++;
          }
        }
      }
    }
  }

  if (duration == 0U)
  {
    return;
  }

  if ((now_ms - s_cal.phase_start_ms) < duration)
  {
    return;
  }

  uint32_t min_samples = sensor_joy_cal_stage_min_samples(s_status.stage);
  if (s_cal.sample_count < min_samples)
  {
    sensor_joy_cal_begin_stage(s_status.stage, now_ms, 1U, joy);
    return;
  }

  if (s_status.stage == SENSOR_JOY_STAGE_NEUTRAL)
  {
    float cx_avg = (float)(s_cal.sum_x / (double)s_cal.sample_count);
    float cy_avg = (float)(s_cal.sum_y / (double)s_cal.sample_count);
    float cx_mid = 0.5f * (s_cal.neutral_min_x + s_cal.neutral_max_x);
    float cy_mid = 0.5f * (s_cal.neutral_min_y + s_cal.neutral_max_y);
    s_cal.cx = 0.5f * (cx_avg + cx_mid);
    s_cal.cy = 0.5f * (cy_avg + cy_mid);

    {
      double n = (double)s_cal.sample_count;
      double ex2 = s_cal.sum_x2 / n;
      double ey2 = s_cal.sum_y2 / n;
      double var = (ex2 - (double)s_cal.cx * (double)s_cal.cx) +
                   (ey2 - (double)s_cal.cy * (double)s_cal.cy);
      float rms = (var > 0.0) ? sqrtf((float)var) : 0.0f;
      float dz = rms * 2.5f;
      if (dz < 1.5f)
      {
        dz = 1.5f;
      }
      if (dz > 12.0f)
      {
        dz = 12.0f;
      }
      s_cal.neutral_mT = dz;
      TMAGJoy_SetAbsDeadzone(joy, 1U, dz);
    }

    TMAGJoy_SetCenter(joy, s_cal.cx, s_cal.cy);
    sensor_joy_cal_begin_stage(SENSOR_JOY_STAGE_UP, now_ms, 0U, joy);
    return;
  }

  if ((s_status.stage == SENSOR_JOY_STAGE_UP) ||
      (s_status.stage == SENSOR_JOY_STAGE_RIGHT) ||
      (s_status.stage == SENSOR_JOY_STAGE_DOWN) ||
      (s_status.stage == SENSOR_JOY_STAGE_LEFT))
  {
    float avg_x = (float)(s_cal.sum_x / (double)s_cal.sample_count);
    float avg_y = (float)(s_cal.sum_y / (double)s_cal.sample_count);
    float avg_dx = avg_x - s_cal.cx;
    float avg_dy = avg_y - s_cal.cy;
    float avg_r = sqrtf(avg_dx * avg_dx + avg_dy * avg_dy);
    float dir_min = sensor_joy_cal_dir_min_mT();
    if (avg_r < dir_min)
    {
      sensor_joy_cal_begin_stage(s_status.stage, now_ms, 1U, joy);
      return;
    }

    sensor_joy_cal_store_dir(s_status.stage, avg_x, avg_y);

    switch (s_status.stage)
    {
      case SENSOR_JOY_STAGE_UP:
        sensor_joy_cal_begin_stage(SENSOR_JOY_STAGE_RIGHT, now_ms, 0U, joy);
        break;
      case SENSOR_JOY_STAGE_RIGHT:
        sensor_joy_cal_begin_stage(SENSOR_JOY_STAGE_DOWN, now_ms, 0U, joy);
        break;
      case SENSOR_JOY_STAGE_DOWN:
        sensor_joy_cal_begin_stage(SENSOR_JOY_STAGE_LEFT, now_ms, 0U, joy);
        break;
      case SENSOR_JOY_STAGE_LEFT:
        sensor_joy_cal_compute_axes();
        TMAGJoy_SetRotationDeg(joy, s_cal.rot_deg);
        TMAGJoy_SetInvert(joy, s_cal.invert_x, s_cal.invert_y);
        sensor_joy_cal_begin_stage(SENSOR_JOY_STAGE_SWEEP, now_ms, 0U, joy);
        break;
      default:
        break;
    }
    return;
  }

  if (s_status.stage == SENSOR_JOY_STAGE_SWEEP)
  {
    float sx_sweep = fmaxf(fabsf(s_cal.min_x), fabsf(s_cal.max_x));
    float sy_sweep = fmaxf(fabsf(s_cal.min_y), fabsf(s_cal.max_y));

    float base_sx = 0.0f;
    float base_sy = 0.0f;
    {
      float rdx = s_cal.right_x - s_cal.cx;
      float rdy = s_cal.right_y - s_cal.cy;
      float ldx = s_cal.left_x - s_cal.cx;
      float ldy = s_cal.left_y - s_cal.cy;
      float udx = s_cal.up_x - s_cal.cx;
      float udy = s_cal.up_y - s_cal.cy;
      float ddx = s_cal.down_x - s_cal.cx;
      float ddy = s_cal.down_y - s_cal.cy;

      float rxr = s_cal.rot_c * rdx - s_cal.rot_s * rdy;
      float rxl = s_cal.rot_c * ldx - s_cal.rot_s * ldy;
      float ryu = s_cal.rot_s * udx + s_cal.rot_c * udy;
      float ryd = s_cal.rot_s * ddx + s_cal.rot_c * ddy;
      if (s_cal.invert_x)
      {
        rxr = -rxr;
        rxl = -rxl;
      }
      if (s_cal.invert_y)
      {
        ryu = -ryu;
        ryd = -ryd;
      }

      base_sx = fmaxf(fabsf(rxr), fabsf(rxl));
      base_sy = fmaxf(fabsf(ryu), fabsf(ryd));
    }

    {
      float sx = fmaxf(sx_sweep, base_sx);
      float sy = fmaxf(sy_sweep, base_sy);
      s_cal.sx = (sx < 1e-3f) ? 1.0f : sx;
      s_cal.sy = (sy < 1e-3f) ? 1.0f : sy;
    }

    {
      float raw_sx = fmaxf(fabsf(s_cal.raw_min_x), fabsf(s_cal.raw_max_x));
      float raw_sy = fmaxf(fabsf(s_cal.raw_min_y), fabsf(s_cal.raw_max_y));
      float raw_min = fminf(raw_sx, raw_sy);
      float thr = raw_min * 0.25f;
      float min_thr = s_cal.neutral_mT * 1.2f;
      if (min_thr < 1.0f)
      {
        min_thr = 1.0f;
      }
      if (thr < min_thr)
      {
        thr = min_thr;
      }
      if (raw_min > 1e-3f)
      {
        float max_thr = raw_min * 0.60f;
        if (thr > max_thr)
        {
          thr = max_thr;
        }
      }
      if (thr < 1.0f)
      {
        thr = 1.0f;
      }
      s_cal.thr_mT = thr;
      (void)TMAG5273_set_x_threshold_mT(thr);
      (void)TMAG5273_set_y_threshold_mT(thr);
      joy->cfg.thr_x_mT = thr;
      joy->cfg.thr_y_mT = thr;
    }

    TMAGJoy_SetCenter(joy, s_cal.cx, s_cal.cy);
    TMAGJoy_SetRotationDeg(joy, s_cal.rot_deg);
    TMAGJoy_SetInvert(joy, s_cal.invert_x, s_cal.invert_y);
    TMAGJoy_SetSpan(joy, s_cal.sx, s_cal.sy);

    s_status.stage = SENSOR_JOY_STAGE_DONE;
    s_status.progress = 1.0f;
    s_cal.have_prev = 0U;
    sensor_joy_refresh_status(joy, false);
    sensor_store_joy_settings(joy, 1U);
  }
}

void sensor_task_run(void)
{
  TMAGJoy_InitOnce();
  TMAGJoy *joy = UI_GetJoy();
  sensor_joy_status_reset();
  sensor_joy_menu_clamp();
  sensor_joy_menu_reset_state();
  sensor_joy_set_menu_nav(joy, 0U);
  sensor_joy_set_monitor(0U);
  sensor_joy_refresh_status(joy, false);
  sensor_power_status_reset();
  sensor_power_set_enabled(0U);
  sensor_apply_settings(joy, true);
  s_settings_seq = settings_get_seq();

  for (;;)
  {
    if (power_task_is_quiescing() != 0U)
    {
      power_task_quiesce_ack(POWER_QUIESCE_ACK_SENSOR);
      osDelay(10U);
      continue;
    }
    power_task_quiesce_clear(POWER_QUIESCE_ACK_SENSOR);

    app_sensor_req_t req = 0U;
    uint32_t timeout = osWaitForever;
    uint32_t seq = settings_get_seq();
    if (seq != s_settings_seq)
    {
      s_settings_seq = seq;
      bool apply_cal = ((s_status.stage == SENSOR_JOY_STAGE_IDLE) ||
                        (s_status.stage == SENSOR_JOY_STAGE_DONE));
      sensor_apply_settings(joy, apply_cal);
    }

    if ((s_status.stage != SENSOR_JOY_STAGE_IDLE) &&
        (s_status.stage != SENSOR_JOY_STAGE_DONE))
    {
      timeout = kJoyCalTickMs;
    }
    if ((s_menu_nav_enabled != 0U) || (s_monitor_enabled != 0U))
    {
      if ((timeout == osWaitForever) || (timeout > kJoyMonitorTickMs))
      {
        timeout = kJoyMonitorTickMs;
      }
    }
    if (s_power_stats_enabled != 0U)
    {
      if ((timeout == osWaitForever) || (timeout > kPowerStatsTickMs))
      {
        timeout = kPowerStatsTickMs;
      }
    }
    if ((timeout == osWaitForever) || (timeout > kBattCutoffPollMs))
    {
      timeout = kBattCutoffPollMs;
    }

    osStatus_t status = osMessageQueueGet(qSensorReqHandle, &req, NULL, timeout);
    uint32_t now_ms = osKernelGetTickCount();
    if (status == osOK)
    {
      sensor_joy_handle_req(req, joy, now_ms);
    }

    if ((s_status.stage != SENSOR_JOY_STAGE_IDLE) &&
        (s_status.stage != SENSOR_JOY_STAGE_DONE))
    {
      sensor_joy_cal_step(joy, now_ms);
    }

    if ((s_monitor_enabled != 0U) && sensor_is_ui_mode())
    {
      sensor_joy_refresh_status(joy, true);
    }

    if ((s_menu_nav_enabled != 0U) && sensor_is_ui_mode())
    {
      sensor_joy_menu_poll(joy);
    }

    sensor_power_update(now_ms);
    sensor_power_lvco_update(now_ms);
  }
}

void sensor_joy_get_status(sensor_joy_status_t *out)
{
  if (out == NULL)
  {
    return;
  }

  *out = s_status;
}

void sensor_joy_get_menu_params(sensor_joy_menu_params_t *out)
{
  if (out == NULL)
  {
    return;
  }

  out->press_norm = s_menu_press_norm;
  out->release_norm = s_menu_release_norm;
  out->axis_ratio = s_menu_axis_ratio;
}

void sensor_power_get_status(sensor_power_status_t *out)
{
  if (out == NULL)
  {
    return;
  }

  *out = s_power_status;
}
