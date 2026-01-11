#include "ui_task.h"

#include "app_freertos.h"
#include "cmsis_os2.h"
#include "render_demo.h"
#include "ui_router.h"

#include <stdbool.h>

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

void ui_task_run(void)
{
  app_ui_event_t ui_event = 0U;
  bool resume_demo = false;

  ui_router_init();
  ui_router_set_page(UI_PAGE_MENU);
  ui_router_render();
  app_display_cmd_t init_cmd = APP_DISPLAY_CMD_INVALIDATE;
  (void)osMessageQueuePut(qDisplayCmdHandle, &init_cmd, 0U, 0U);

  for (;;)
  {
    if (osMessageQueueGet(qUIEventsHandle, &ui_event, NULL, osWaitForever) != osOK)
    {
      continue;
    }

    g_ui_event_count++;

    if ((ui_event & (1UL << 8U)) == 0U)
    {
      continue;
    }

    uint32_t mode_flags = 0U;
    if (egModeHandle != NULL)
    {
      uint32_t flags = osEventFlagsGet(egModeHandle);
      if ((int32_t)flags >= 0)
      {
        mode_flags = flags;
      }
    }

    uint32_t button_id = (ui_event & 0xFFU);
    if (button_id == (uint32_t)APP_BUTTON_BOOT)
    {
      if ((mode_flags & APP_MODE_GAME) != 0U)
      {
        resume_demo = (render_demo_get_mode() == RENDER_DEMO_MODE_RUN);
        render_demo_set_mode(RENDER_DEMO_MODE_IDLE);

        app_sys_event_t sys_event = APP_SYS_EVENT_EXIT_GAME;
        (void)osMessageQueuePut(qSysEventsHandle, &sys_event, 0U, 0U);

        ui_router_set_page(UI_PAGE_MENU);
        ui_router_render();
        app_display_cmd_t cmd = APP_DISPLAY_CMD_INVALIDATE;
        (void)osMessageQueuePut(qDisplayCmdHandle, &cmd, 0U, 0U);
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
      continue;
    }

    if ((mode_flags & APP_MODE_GAME) != 0U)
    {
      continue;
    }

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

      continue;
    }

    if (changed)
    {
      ui_router_render();
      app_display_cmd_t display_cmd = APP_DISPLAY_CMD_INVALIDATE;
      (void)osMessageQueuePut(qDisplayCmdHandle, &display_cmd, 0U, 0U);
    }
  }
}
