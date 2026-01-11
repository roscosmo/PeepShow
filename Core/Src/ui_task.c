#include "ui_task.h"

#include "app_freertos.h"
#include "cmsis_os2.h"
#include "display_renderer.h"
#include "render_demo.h"
#include "sensor_task.h"
#include "ui_router.h"

#include <math.h>
#include <stdbool.h>
#include <string.h>

static bool ui_wait_for_mode(uint32_t flags, uint32_t timeout_ms)
{
  if (egModeHandle == NULL)
  {
    return false;
  }

  int32_t result = (int32_t)osEventFlagsWait(egModeHandle, flags,
                                             (osFlagsWaitAll | osFlagsNoClear),
                                             timeout_ms);
  if (result < 0)
  {
    return false;
  }

  return (((uint32_t)result & flags) == flags);
}

static void ui_send_display_invalidate(void)
{
  app_display_cmd_t cmd = APP_DISPLAY_CMD_INVALIDATE;
  (void)osMessageQueuePut(qDisplayCmdHandle, &cmd, 0U, 0U);
}

static void ui_send_sensor_req(app_sensor_req_t req)
{
  if (qSensorReqHandle == NULL)
  {
    return;
  }

  (void)osMessageQueuePut(qSensorReqHandle, &req, 0U, 0U);
}

static void ui_update_sensor_mode(ui_page_t page)
{
  app_sensor_req_t req = 0U;

  if (page == UI_PAGE_MENU)
  {
    req |= APP_SENSOR_REQ_JOY_MENU_ON;
    req |= APP_SENSOR_REQ_JOY_MONITOR_OFF;
  }
  else if ((page == UI_PAGE_JOY_TARGET) || (page == UI_PAGE_JOY_CURSOR))
  {
    req |= APP_SENSOR_REQ_JOY_MENU_OFF;
    req |= APP_SENSOR_REQ_JOY_MONITOR_ON;
  }
  else
  {
    req |= APP_SENSOR_REQ_JOY_MENU_OFF;
    req |= APP_SENSOR_REQ_JOY_MONITOR_OFF;
  }

  if (req != 0U)
  {
    ui_send_sensor_req(req);
  }
}

static bool ui_joy_status_changed(sensor_joy_status_t *last_status, bool *has_last)
{
  sensor_joy_status_t current;
  sensor_joy_get_status(&current);

  if (!*has_last || memcmp(&current, last_status, sizeof(current)) != 0)
  {
    *last_status = current;
    *has_last = true;
    return true;
  }

  return false;
}

typedef struct
{
  float x;
  float y;
  uint16_t draw_x;
  uint16_t draw_y;
  uint32_t last_ms;
} ui_cursor_state_t;

static ui_cursor_state_t s_cursor;
static const float kCursorSpeedPxPerS = 120.0f;
static const uint16_t kCursorSpriteW = 32U;
static const uint16_t kCursorSpriteH = 32U;

static void ui_cursor_enter(void)
{
  uint16_t width = renderGetWidth();
  uint16_t height = renderGetHeight();
  float start_x = 0.0f;
  float start_y = 0.0f;

  if (width > kCursorSpriteW)
  {
    start_x = (float)((width - kCursorSpriteW) / 2U);
  }
  if (height > kCursorSpriteH)
  {
    start_y = (float)((height - kCursorSpriteH) / 2U);
  }

  s_cursor.x = start_x;
  s_cursor.y = start_y;
  s_cursor.draw_x = (uint16_t)(start_x + 0.5f);
  s_cursor.draw_y = (uint16_t)(start_y + 0.5f);
  s_cursor.last_ms = 0U;
  ui_router_set_joy_cursor(s_cursor.draw_x, s_cursor.draw_y);
}

static bool ui_cursor_update(const sensor_joy_status_t *status, uint32_t now_ms)
{
  if (status == NULL)
  {
    return false;
  }

  uint16_t width = renderGetWidth();
  uint16_t height = renderGetHeight();
  if ((width == 0U) || (height == 0U))
  {
    return false;
  }

  uint32_t prev_ms = s_cursor.last_ms;
  if (prev_ms == 0U)
  {
    prev_ms = now_ms;
  }
  s_cursor.last_ms = now_ms;

  float dt_s = (float)(now_ms - prev_ms) * (1.0f / 1000.0f);
  if (dt_s <= 0.0f)
  {
    return false;
  }

  float min_span = (status->sx_mT < status->sy_mT) ? status->sx_mT : status->sy_mT;
  if (min_span < 1e-3f)
  {
    min_span = 1.0f;
  }

  float speed_norm = status->r_abs_mT / min_span;
  if (speed_norm > 1.0f)
  {
    speed_norm = 1.0f;
  }
  if ((status->deadzone_en != 0U) && (status->r_abs_mT < status->deadzone_mT))
  {
    speed_norm = 0.0f;
  }

  float mag = sqrtf(status->nx * status->nx + status->ny * status->ny);
  float dirx = (mag > 1e-6f) ? (status->nx / mag) : 0.0f;
  float diry = (mag > 1e-6f) ? (status->ny / mag) : 0.0f;

  float fx = s_cursor.x + dirx * speed_norm * kCursorSpeedPxPerS * dt_s;
  float fy = s_cursor.y - diry * speed_norm * kCursorSpeedPxPerS * dt_s;

  float max_x = (width > kCursorSpriteW) ? (float)(width - kCursorSpriteW) : 0.0f;
  float max_y = (height > kCursorSpriteH) ? (float)(height - kCursorSpriteH) : 0.0f;
  if (fx < 0.0f)
  {
    fx = 0.0f;
  }
  else if (fx > max_x)
  {
    fx = max_x;
  }
  if (fy < 0.0f)
  {
    fy = 0.0f;
  }
  else if (fy > max_y)
  {
    fy = max_y;
  }

  s_cursor.x = fx;
  s_cursor.y = fy;

  uint16_t draw_x = (uint16_t)(fx + 0.5f);
  uint16_t draw_y = (uint16_t)(fy + 0.5f);
  if ((draw_x != s_cursor.draw_x) || (draw_y != s_cursor.draw_y))
  {
    s_cursor.draw_x = draw_x;
    s_cursor.draw_y = draw_y;
    ui_router_set_joy_cursor(draw_x, draw_y);
    return true;
  }

  return false;
}

void ui_task_run(void)
{
  app_ui_event_t ui_event = 0U;
  bool resume_demo = false;
  sensor_joy_status_t last_joy_status;
  bool have_joy_status = false;

  ui_router_init();
  ui_router_set_page(UI_PAGE_MENU);
  ui_router_render();
  ui_send_display_invalidate();
  ui_update_sensor_mode(UI_PAGE_MENU);

  for (;;)
  {
    ui_page_t page_before = ui_router_get_page();
    uint32_t timeout = osWaitForever;
    if ((page_before == UI_PAGE_JOY_CAL) || (page_before == UI_PAGE_JOY_TARGET) ||
        (page_before == UI_PAGE_JOY_CURSOR))
    {
      timeout = 100U;
    }

    osStatus_t status = osMessageQueueGet(qUIEventsHandle, &ui_event, NULL, timeout);
    if ((status != osOK) && (status != osErrorTimeout))
    {
      continue;
    }

    if (status == osOK)
    {
      g_ui_event_count++;
    }

    bool pressed = (status == osOK) && ((ui_event & (1UL << 8U)) != 0U);
    uint32_t button_id = (uint32_t)(ui_event & 0xFFU);

    uint32_t mode_flags = 0U;
    if (egModeHandle != NULL)
    {
      uint32_t flags = osEventFlagsGet(egModeHandle);
      if ((int32_t)flags >= 0)
      {
        mode_flags = flags;
      }
    }

    if (pressed && (button_id == (uint32_t)APP_BUTTON_BOOT))
    {
      if ((mode_flags & APP_MODE_GAME) != 0U)
      {
        resume_demo = (render_demo_get_mode() == RENDER_DEMO_MODE_RUN);
        render_demo_set_mode(RENDER_DEMO_MODE_IDLE);

        app_sys_event_t sys_event = APP_SYS_EVENT_EXIT_GAME;
        (void)osMessageQueuePut(qSysEventsHandle, &sys_event, 0U, 0U);

        ui_router_set_page(UI_PAGE_MENU);
        ui_router_render();
        ui_send_display_invalidate();
      }
      else
      {
        app_sys_event_t sys_event = APP_SYS_EVENT_ENTER_GAME;
        (void)osMessageQueuePut(qSysEventsHandle, &sys_event, 0U, 0U);

        if (resume_demo && ui_wait_for_mode(APP_MODE_GAME, 200U))
        {
          render_demo_set_mode(RENDER_DEMO_MODE_RUN);
          app_display_cmd_t cmd = APP_DISPLAY_CMD_RENDER_DEMO;
          (void)osMessageQueuePut(qDisplayCmdHandle, &cmd, 0U, 0U);
          resume_demo = false;
        }
      }
    }
    else if (pressed && ((mode_flags & APP_MODE_GAME) == 0U))
    {
      ui_page_t page_now = ui_router_get_page();
      if (page_now == UI_PAGE_JOY_CAL)
      {
        if (button_id == (uint32_t)APP_BUTTON_B)
        {
          ui_send_sensor_req(APP_SENSOR_REQ_JOY_CAL_SAVE);
          ui_router_set_page(UI_PAGE_MENU);
          ui_router_render();
          ui_send_display_invalidate();
        }
        else if (button_id == (uint32_t)APP_BUTTON_A)
        {
          sensor_joy_status_t status_now;
          sensor_joy_get_status(&status_now);
          if ((status_now.stage == SENSOR_JOY_STAGE_IDLE) ||
              (status_now.stage == SENSOR_JOY_STAGE_DONE))
          {
            ui_send_sensor_req(APP_SENSOR_REQ_JOY_CAL_NEUTRAL);
          }

          ui_router_render();
          ui_send_display_invalidate();
        }
      }
      else if (page_now == UI_PAGE_JOY_TARGET)
      {
        if (button_id == (uint32_t)APP_BUTTON_B)
        {
          ui_router_set_page(UI_PAGE_MENU);
          ui_router_render();
          ui_send_display_invalidate();
        }
        else if (button_id == (uint32_t)APP_BUTTON_L)
        {
          ui_send_sensor_req(APP_SENSOR_REQ_JOY_DZ_DEC);
        }
        else if (button_id == (uint32_t)APP_BUTTON_R)
        {
          ui_send_sensor_req(APP_SENSOR_REQ_JOY_DZ_INC);
        }
      }
      else if (page_now == UI_PAGE_JOY_CURSOR)
      {
        if (button_id == (uint32_t)APP_BUTTON_B)
        {
          ui_router_set_page(UI_PAGE_MENU);
          ui_router_render();
          ui_send_display_invalidate();
        }
      }
      else
      {
        ui_router_cmd_t cmd = UI_ROUTER_CMD_NONE;
        bool changed = ui_router_handle_button(button_id, &cmd);
        if (cmd == UI_ROUTER_CMD_START_RENDER_DEMO)
        {
          render_demo_set_mode(RENDER_DEMO_MODE_RUN);

          app_sys_event_t sys_event = APP_SYS_EVENT_ENTER_GAME;
          (void)osMessageQueuePut(qSysEventsHandle, &sys_event, 0U, 0U);

          if (ui_wait_for_mode(APP_MODE_GAME, 200U))
          {
            app_display_cmd_t display_cmd = APP_DISPLAY_CMD_RENDER_DEMO;
            (void)osMessageQueuePut(qDisplayCmdHandle, &display_cmd, 0U, 0U);
          }
          else
          {
            render_demo_set_mode(RENDER_DEMO_MODE_IDLE);
          }
        }
        else if (cmd == UI_ROUTER_CMD_OPEN_JOY_CAL)
        {
          ui_router_set_page(UI_PAGE_JOY_CAL);
          ui_router_render();
          ui_send_display_invalidate();
          have_joy_status = false;
        }
        else if (cmd == UI_ROUTER_CMD_OPEN_JOY_TARGET)
        {
          ui_router_set_page(UI_PAGE_JOY_TARGET);
          ui_router_render();
          ui_send_display_invalidate();
          have_joy_status = false;
        }
        else if (cmd == UI_ROUTER_CMD_OPEN_JOY_CURSOR)
        {
          ui_router_set_page(UI_PAGE_JOY_CURSOR);
          ui_cursor_enter();
          ui_router_render();
          ui_send_display_invalidate();
          have_joy_status = false;
        }
        else if (changed)
        {
          ui_router_render();
          ui_send_display_invalidate();
        }
      }
    }

    ui_page_t page_after = ui_router_get_page();
    if (page_after != page_before)
    {
      have_joy_status = false;
      ui_update_sensor_mode(page_after);
    }

    if ((page_after == UI_PAGE_JOY_CAL) || (page_after == UI_PAGE_JOY_TARGET))
    {
      if ((mode_flags & APP_MODE_GAME) == 0U)
      {
        if (ui_joy_status_changed(&last_joy_status, &have_joy_status))
        {
          ui_router_render();
          ui_send_display_invalidate();
        }
      }
      else
      {
        have_joy_status = false;
      }
    }
    else if (page_after == UI_PAGE_JOY_CURSOR)
    {
      if ((mode_flags & APP_MODE_GAME) == 0U)
      {
        sensor_joy_status_t status_now;
        sensor_joy_get_status(&status_now);
        if (ui_cursor_update(&status_now, osKernelGetTickCount()))
        {
          ui_router_render();
          ui_send_display_invalidate();
        }
      }
    }
  }
}
