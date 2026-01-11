#include "sensor_task.h"

#include "app_freertos.h"
#include "cmsis_os2.h"

#include <string.h>

extern TMAGJoy *UI_GetJoy(void);

static sensor_joy_status_t s_status;
static uint8_t s_menu_nav_enabled = 0U;
static uint8_t s_monitor_enabled = 0U;

static const uint32_t kJoyCalNeutralMs = 2000U;
static const uint32_t kJoyCalExtentsMs = 5000U;
static const uint32_t kJoyCalSampleMs = 100U;
static const uint32_t kJoyCalTickMs = 100U;

static void sensor_joy_status_reset(void)
{
  (void)memset(&s_status, 0, sizeof(s_status));
  s_status.stage = SENSOR_JOY_STAGE_IDLE;
  s_status.dir = TMAGJOY_NEUTRAL;
  s_status.sx_mT = 1.0f;
  s_status.sy_mT = 1.0f;
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

static void sensor_joy_start_neutral(TMAGJoy *joy)
{
  if (joy == NULL)
  {
    return;
  }

  TMAGJoy_CalNeutral_Begin(joy, kJoyCalNeutralMs, kJoyCalSampleMs);
  s_status.stage = SENSOR_JOY_STAGE_NEUTRAL;
  s_status.progress = 0.0f;
  s_status.neutral_done = 0U;
  s_status.extents_done = 0U;
  sensor_joy_refresh_status(joy, false);
}

static void sensor_joy_start_extents(TMAGJoy *joy)
{
  if (joy == NULL)
  {
    return;
  }

  TMAGJoy_CalExtents_Begin(joy, kJoyCalExtentsMs, kJoyCalSampleMs);
  s_status.stage = SENSOR_JOY_STAGE_EXTENTS;
  s_status.progress = 0.0f;
  s_status.extents_done = 0U;
  sensor_joy_refresh_status(joy, false);
}

static void sensor_joy_save(void)
{
  if (s_status.extents_done != 0U)
  {
    s_status.stage = SENSOR_JOY_STAGE_DONE;
    s_status.progress = 1.0f;
  }
  else
  {
    s_status.stage = SENSOR_JOY_STAGE_IDLE;
    s_status.progress = 0.0f;
  }
}

static void sensor_joy_set_menu_nav(TMAGJoy *joy, uint8_t enable)
{
  s_menu_nav_enabled = enable ? 1U : 0U;
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
}

static void sensor_joy_emit_menu_event(TMAGJoy_Dir dir)
{
  if ((qInputHandle == NULL) || !sensor_is_ui_mode())
  {
    return;
  }

  uint8_t button_id = 0U;
  switch (dir)
  {
    case TMAGJOY_LEFT:
    case TMAGJOY_UPLEFT:
    case TMAGJOY_UP:
    case TMAGJOY_DOWNLEFT:
      button_id = (uint8_t)APP_BUTTON_L;
      break;
    case TMAGJOY_RIGHT:
    case TMAGJOY_UPRIGHT:
    case TMAGJOY_DOWNRIGHT:
    case TMAGJOY_DOWN:
      button_id = (uint8_t)APP_BUTTON_R;
      break;
    default:
      return;
  }

  app_input_event_t evt = {0};
  evt.button_id = button_id;
  evt.pressed = 1U;
  (void)osMessageQueuePut(qInputHandle, &evt, 0U, 0U);
}

static void sensor_joy_handle_req(app_sensor_req_t req, TMAGJoy *joy)
{
  if ((req & APP_SENSOR_REQ_JOY_CAL_NEUTRAL) != 0U)
  {
    sensor_joy_start_neutral(joy);
  }
  if ((req & APP_SENSOR_REQ_JOY_CAL_EXTENTS) != 0U)
  {
    sensor_joy_start_extents(joy);
  }
  if ((req & APP_SENSOR_REQ_JOY_CAL_SAVE) != 0U)
  {
    sensor_joy_save();
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
}

static void sensor_joy_step(TMAGJoy *joy, uint32_t now_ms)
{
  float progress = 0.0f;

  if (s_status.stage == SENSOR_JOY_STAGE_NEUTRAL)
  {
    bool done = TMAGJoy_CalNeutral_Step(joy, now_ms, &progress);
    s_status.progress = progress;
    sensor_joy_refresh_status(joy, true);
    if (done)
    {
      s_status.neutral_done = 1U;
      s_status.progress = 1.0f;
      s_status.stage = SENSOR_JOY_STAGE_IDLE;
    }
  }
  else if (s_status.stage == SENSOR_JOY_STAGE_EXTENTS)
  {
    bool done = TMAGJoy_CalExtents_Step(joy, now_ms, &progress);
    s_status.progress = progress;
    sensor_joy_refresh_status(joy, true);
    if (done)
    {
      s_status.extents_done = 1U;
      s_status.progress = 1.0f;
      s_status.stage = SENSOR_JOY_STAGE_DONE;
    }
  }
}

void sensor_task_run(void)
{
  TMAGJoy_InitOnce();
  TMAGJoy *joy = UI_GetJoy();
  sensor_joy_status_reset();
  sensor_joy_set_menu_nav(joy, 0U);
  sensor_joy_set_monitor(0U);
  sensor_joy_refresh_status(joy, false);

  for (;;)
  {
    app_sensor_req_t req = 0U;
    uint32_t timeout = osWaitForever;

    if ((s_status.stage == SENSOR_JOY_STAGE_NEUTRAL) ||
        (s_status.stage == SENSOR_JOY_STAGE_EXTENTS) ||
        (s_menu_nav_enabled != 0U) ||
        (s_monitor_enabled != 0U))
    {
      timeout = kJoyCalTickMs;
    }

    osStatus_t status = osMessageQueueGet(qSensorReqHandle, &req, NULL, timeout);
    if (status == osOK)
    {
      sensor_joy_handle_req(req, joy);
    }

    if ((s_status.stage == SENSOR_JOY_STAGE_NEUTRAL) ||
        (s_status.stage == SENSOR_JOY_STAGE_EXTENTS))
    {
      sensor_joy_step(joy, osKernelGetTickCount());
    }

    if ((s_monitor_enabled != 0U) && sensor_is_ui_mode())
    {
      sensor_joy_refresh_status(joy, true);
    }

    if ((s_menu_nav_enabled != 0U) && sensor_is_ui_mode())
    {
      TMAGJoy_Dir dir = TMAGJoy_MenuPoll(1U, 0U);
      if (dir != TMAGJOY_NEUTRAL)
      {
        sensor_joy_emit_menu_event(dir);
      }
    }
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
