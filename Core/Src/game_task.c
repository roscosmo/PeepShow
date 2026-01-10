#include "game_task.h"

#include "app_freertos.h"
#include "cmsis_os2.h"

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
    if (button_id != (uint32_t)APP_BUTTON_L)
    {
      continue;
    }

    app_display_cmd_t cmd = APP_DISPLAY_CMD_TOGGLE;
    (void)osMessageQueuePut(qDisplayCmdHandle, &cmd, 0U, 0U);
  }
}
