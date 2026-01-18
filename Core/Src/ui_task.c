#include "ui_task.h"

#include "app_freertos.h"
#include "audio_task.h"
#include "cmsis_os2.h"
#include "render_demo.h"
#include "settings.h"
#include "storage_task.h"
#include "ui_actions.h"
#include "ui_router.h"
#include "power_task.h"

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

static void ui_apply_settings(uint32_t *seq_cache)
{
  uint32_t seq = settings_get_seq();
  if ((seq_cache != NULL) && (*seq_cache == seq))
  {
    return;
  }

  settings_data_t data;
  settings_get(&data);
  audio_set_volume(data.volume);
  ui_router_set_keyclick(data.keyclick_enabled != 0U);
  power_task_set_sleep_enabled(data.sleep_enabled);
  power_task_set_game_sleep_allowed(data.sleep_allow_game);
  power_task_set_inactivity_timeout_ms(data.sleep_timeout_ms);
  power_task_set_sleepface_interval_s(data.sleep_face_interval_s);

  if (seq_cache != NULL)
  {
    *seq_cache = seq;
  }
}

static void ui_update_sensor_mode(const ui_page_t *page)
{
  app_sensor_req_t req = 0U;
  uint16_t flags = (page != NULL) ? page->flags : 0U;

  if ((flags & UI_PAGE_FLAG_JOY_MENU) != 0U)
  {
    req |= APP_SENSOR_REQ_JOY_MENU_ON;
  }
  else
  {
    req |= APP_SENSOR_REQ_JOY_MENU_OFF;
  }

  if ((flags & UI_PAGE_FLAG_JOY_MONITOR) != 0U)
  {
    req |= APP_SENSOR_REQ_JOY_MONITOR_ON;
  }
  else
  {
    req |= APP_SENSOR_REQ_JOY_MONITOR_OFF;
  }

  if ((flags & UI_PAGE_FLAG_POWER_STATS) != 0U)
  {
    req |= APP_SENSOR_REQ_POWER_STATS_ON;
  }
  else
  {
    req |= APP_SENSOR_REQ_POWER_STATS_OFF;
  }

  if ((flags & UI_PAGE_FLAG_LIS2) != 0U)
  {
    req |= APP_SENSOR_REQ_LIS2_ON;
  }
  else
  {
    req |= APP_SENSOR_REQ_LIS2_OFF;
  }

  if ((flags & UI_PAGE_FLAG_LIS2_STEP) != 0U)
  {
    req |= APP_SENSOR_REQ_LIS2_STEP_VIEW_ON;
  }
  else
  {
    req |= APP_SENSOR_REQ_LIS2_STEP_VIEW_OFF;
  }

  if (req != 0U)
  {
    ui_actions_send_sensor_req(req);
  }
}

static ui_evt_t ui_event_from_button(uint32_t button_id)
{
  switch (button_id)
  {
    case APP_BUTTON_A:
      return UI_EVT_SELECT;
    case APP_BUTTON_B:
      return UI_EVT_BACK;
    case APP_BUTTON_L:
      return UI_EVT_DEC;
    case APP_BUTTON_R:
      return UI_EVT_INC;
    case APP_BUTTON_JOY_UP:
    case APP_BUTTON_JOY_UPLEFT:
    case APP_BUTTON_JOY_UPRIGHT:
      return UI_EVT_NAV_UP;
    case APP_BUTTON_JOY_DOWN:
    case APP_BUTTON_JOY_DOWNLEFT:
    case APP_BUTTON_JOY_DOWNRIGHT:
      return UI_EVT_NAV_DOWN;
    case APP_BUTTON_JOY_LEFT:
      return UI_EVT_NAV_LEFT;
    case APP_BUTTON_JOY_RIGHT:
      return UI_EVT_NAV_RIGHT;
    default:
      return UI_EVT_NONE;
  }
}

static void ui_enter_game(bool *resume_demo)
{
  app_sys_event_t sys_event = APP_SYS_EVENT_ENTER_GAME;
  (void)osMessageQueuePut(qSysEventsHandle, &sys_event, 0U, 0U);

  if ((resume_demo != NULL) && (*resume_demo))
  {
    if (ui_wait_for_mode(APP_MODE_GAME, 200U))
    {
      render_demo_set_mode(RENDER_DEMO_MODE_RUN);
      app_display_cmd_t cmd = APP_DISPLAY_CMD_RENDER_DEMO;
      (void)osMessageQueuePut(qDisplayCmdHandle, &cmd, 0U, 0U);
      *resume_demo = false;
    }
  }
}

static void ui_exit_game_to_menu(bool *resume_demo)
{
  if (resume_demo != NULL)
  {
    *resume_demo = (render_demo_get_mode() == RENDER_DEMO_MODE_RUN);
  }
  render_demo_set_mode(RENDER_DEMO_MODE_IDLE);

  app_sys_event_t sys_event = APP_SYS_EVENT_EXIT_GAME;
  (void)osMessageQueuePut(qSysEventsHandle, &sys_event, 0U, 0U);

  ui_router_set_page(&PAGE_MENU);
  ui_router_render();
  ui_send_display_invalidate();
  ui_update_sensor_mode(ui_router_get_page());
}

void ui_task_run(void)
{
  app_ui_event_t ui_event = 0U;
  bool resume_demo = false;
  uint32_t settings_seq = 0U;

  ui_router_init();
  ui_apply_settings(&settings_seq);
  ui_router_render();
  ui_send_display_invalidate();
  ui_update_sensor_mode(ui_router_get_page());

  for (;;)
  {
    const ui_page_t *page_before = ui_router_get_page();
    uint32_t timeout = osWaitForever;
    if ((page_before != NULL) && (page_before->tick_ms > 0U))
    {
      timeout = page_before->tick_ms;
    }

    osStatus_t status = osMessageQueueGet(qUIEventsHandle, &ui_event, NULL, timeout);
    if ((status != osOK) && (status != osErrorTimeout))
    {
      continue;
    }

    ui_apply_settings(&settings_seq);

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
        ui_exit_game_to_menu(&resume_demo);
      }
      else
      {
        resume_demo = true;
        ui_enter_game(&resume_demo);
      }
      continue;
    }

    if ((mode_flags & APP_MODE_GAME) != 0U)
    {
      continue;
    }

    ui_evt_t evt = UI_EVT_NONE;
    if (status == osErrorTimeout)
    {
      evt = UI_EVT_TICK;
    }
    else if (pressed)
    {
      evt = ui_event_from_button(button_id);
    }

    if (evt == UI_EVT_NONE)
    {
      continue;
    }

    if (pressed && ui_router_get_keyclick())
    {
      app_audio_cmd_t audio_cmd = APP_AUDIO_CMD_KEYCLICK;
      (void)osMessageQueuePut(qAudioCmdHandle, &audio_cmd, 0U, 0U);
    }

    ui_router_action_t action = UI_ROUTER_ACTION_NONE;
    bool render = ui_router_handle_event(evt, &action);

    const ui_page_t *page_after = ui_router_get_page();
    if (page_after != page_before)
    {
      ui_update_sensor_mode(page_after);
      render = true;
    }

    if (action == UI_ROUTER_ACTION_START_RENDER_DEMO)
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
      render = false;
    }
    else if (action == UI_ROUTER_ACTION_EXIT_MENU)
    {
      ui_enter_game(&resume_demo);
      render = false;
    }
    else if (action == UI_ROUTER_ACTION_SAVE_EXIT)
    {
      (void)storage_request_save_settings();
      resume_demo = true;
      ui_enter_game(&resume_demo);
      render = false;
    }

    if (render)
    {
      ui_router_render();
      ui_send_display_invalidate();
    }
  }
}
