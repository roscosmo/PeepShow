#include "ui_task.h"

#include "app_freertos.h"
#include "cmsis_os2.h"
#include "debug_uart.h"

void ui_task_run(void)
{
  app_ui_event_t ui_event = 0U;
  uint8_t inverted = 0U;
  uint8_t menu_open = 0U;

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

    uint32_t button_id = (ui_event & 0xFFU);
    if (button_id == (uint32_t)APP_BUTTON_A)
    {
      inverted = (uint8_t)((inverted == 0U) ? 1U : 0U);

      app_display_cmd_t cmd = APP_DISPLAY_CMD_TOGGLE;
      (void)osMessageQueuePut(qDisplayCmdHandle, &cmd, 0U, 0U);
    }
    else if (button_id == (uint32_t)APP_BUTTON_R)
    {
      app_display_cmd_t cmd = APP_DISPLAY_CMD_TEST_LAYERS;
      (void)osMessageQueuePut(qDisplayCmdHandle, &cmd, 0U, 0U);
    }
    else if (button_id == (uint32_t)APP_BUTTON_BOOT)
    {
      uint32_t mode_flags = 0U;
      if (egModeHandle != NULL)
      {
        uint32_t flags = osEventFlagsGet(egModeHandle);
        if ((int32_t)flags >= 0)
        {
          mode_flags = flags;
        }
      }

      if ((mode_flags & APP_MODE_GAME) != 0U)
      {
        menu_open = (uint8_t)((menu_open == 0U) ? 1U : 0U);
      }
    }
  }
}
