#include "ui_task.h"

#include "app_freertos.h"
#include "cmsis_os2.h"
#include "render_demo.h"
#include "sensor_task.h"
#include "ui_router.h"

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
  else if (page == UI_PAGE_JOY_TARGET)
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
    if ((page_before == UI_PAGE_JOY_CAL) || (page_before == UI_PAGE_JOY_TARGET))
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
          if (status_now.neutral_done == 0U)
          {
            ui_send_sensor_req(APP_SENSOR_REQ_JOY_CAL_NEUTRAL);
          }
          else if (status_now.extents_done == 0U)
          {
            ui_send_sensor_req(APP_SENSOR_REQ_JOY_CAL_EXTENTS);
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
  }
}
