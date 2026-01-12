#include "game_task.h"

#include "app_freertos.h"
#include "cmsis_os2.h"
#include "render_demo.h"

void game_task_run(void)
{
  app_game_event_t event = 0U;

  for (;;)
  {
    if (osMessageQueueGet(qGameEventsHandle, &event, NULL, osWaitForever) != osOK)
    {
      continue;
    }

    if ((event & (1UL << 8U)) == 0U)
    {
      continue;
    }

    uint32_t button_id = (event & 0xFFU);
    if (button_id == (uint32_t)APP_BUTTON_B)
    {
      app_audio_cmd_t audio_cmd = APP_AUDIO_CMD_MUSIC_TOGGLE;
      (void)osMessageQueuePut(qAudioCmdHandle, &audio_cmd, 0U, 0U);
      continue;
    }
    if (button_id == (uint32_t)APP_BUTTON_L)
    {
      if (render_demo_get_mode() == RENDER_DEMO_MODE_RUN)
      {
        render_demo_toggle_background();
        app_display_cmd_t cmd = APP_DISPLAY_CMD_RENDER_DEMO;
        (void)osMessageQueuePut(qDisplayCmdHandle, &cmd, 0U, 0U);
      }
      else
      {
        app_display_cmd_t cmd = APP_DISPLAY_CMD_TOGGLE;
        (void)osMessageQueuePut(qDisplayCmdHandle, &cmd, 0U, 0U);
      }
      continue;
    }

    if (button_id == (uint32_t)APP_BUTTON_A)
    {
      if (render_demo_get_mode() == RENDER_DEMO_MODE_RUN)
      {
        render_demo_toggle_cube();
        app_display_cmd_t cmd = APP_DISPLAY_CMD_RENDER_DEMO;
        (void)osMessageQueuePut(qDisplayCmdHandle, &cmd, 0U, 0U);
      }
      continue;
    }

    if (button_id == (uint32_t)APP_GAME_DEMO_BUTTON)
    {
      render_demo_mode_t mode = render_demo_get_mode();
      if (mode == RENDER_DEMO_MODE_RUN)
      {
        render_demo_set_mode(RENDER_DEMO_MODE_IDLE);
      }
      else
      {
        render_demo_set_mode(RENDER_DEMO_MODE_RUN);
        app_display_cmd_t cmd = APP_DISPLAY_CMD_RENDER_DEMO;
        (void)osMessageQueuePut(qDisplayCmdHandle, &cmd, 0U, 0U);
      }
    }
  }
}
