#include "display_task.h"

#include "LS013B7DH05.h"
#include "display_renderer.h"
#include "render_demo.h"
#include "app_freertos.h"
#include "main.h"
#include "power_task.h"

extern SPI_HandleTypeDef hspi3;

static LS013B7DH05 s_display;
static volatile bool s_display_busy = false;
static uint16_t s_rows[DISPLAY_HEIGHT];
static const uint32_t kDisplayFlagDmaDone = (1UL << 0U);
static const uint32_t kDisplayFlagDmaError = (1UL << 1U);
static const uint32_t kDisplayFlushTimeoutMs = 200U;

static void display_handle_cmd(app_display_cmd_t cmd)
{
  switch (cmd)
  {
    case APP_DISPLAY_CMD_TOGGLE:
      renderInvert();
      break;
    case APP_DISPLAY_CMD_RENDER_DEMO:
      render_demo_draw();
      break;
    case APP_DISPLAY_CMD_INVALIDATE:
      break;
    default:
      break;
  }
}

static void display_flush_dirty(void)
{
  if (s_display_busy)
  {
    if (LCD_FlushDMA_IsDone())
    {
      s_display_busy = false;
    }
    else
    {
      return;
    }
  }

  uint16_t count = 0U;
  bool full = false;
  if (!renderTakeDirtyRows(s_rows, DISPLAY_HEIGHT, &count, &full))
  {
    return;
  }

  const uint8_t *buf = renderGetBuffer();
  if (buf == NULL)
  {
    renderMarkDirtyList(s_rows, count);
    return;
  }

  (void)osThreadFlagsClear(kDisplayFlagDmaDone | kDisplayFlagDmaError);
  s_display_busy = true;
  HAL_StatusTypeDef st = HAL_ERROR;
  if (full)
  {
    st = LCD_FlushAll_DMA(&s_display, buf);
  }
  else
  {
    st = LCD_FlushRows_DMA(&s_display, buf, s_rows, count);
  }

  if (st != HAL_OK)
  {
    s_display_busy = false;
    renderMarkDirtyList(s_rows, count);
    return;
  }

  int32_t flags = (int32_t)osThreadFlagsWait(kDisplayFlagDmaDone | kDisplayFlagDmaError,
                                             osFlagsWaitAny,
                                             kDisplayFlushTimeoutMs);
  if (flags < 0)
  {
    if (LCD_FlushDMA_IsDone())
    {
      s_display_busy = false;
    }
    renderMarkDirtyList(s_rows, count);
    return;
  }

  if (LCD_FlushDMA_IsDone())
  {
    s_display_busy = false;
  }
  if ((flags & (int32_t)kDisplayFlagDmaError) != 0)
  {
    renderMarkDirtyList(s_rows, count);
  }
}

static void display_init(void)
{
  renderInit();

  /* VLT_LCD is active-low and held low in main; do not change it here. */

  HAL_StatusTypeDef st = LCD_Init(&s_display, &hspi3, SPI3_CS_GPIO_Port, SPI3_CS_Pin);
  if (st != HAL_OK)
  {
    return;
  }

}

void display_task_run(void)
{
  display_init();

  for (;;)
  {
    if (power_task_is_quiescing() != 0U)
    {
      if (!s_display_busy)
      {
        power_task_quiesce_ack(POWER_QUIESCE_ACK_DISPLAY);
      }
      else
      {
        power_task_quiesce_clear(POWER_QUIESCE_ACK_DISPLAY);
      }
      osDelay(5U);
      continue;
    }
    power_task_quiesce_clear(POWER_QUIESCE_ACK_DISPLAY);

    app_display_cmd_t cmd = 0U;
    if (osMessageQueueGet(qDisplayCmdHandle, &cmd, NULL, 20U) != osOK)
    {
      continue;
    }

    display_handle_cmd(cmd);
    while (osMessageQueueGet(qDisplayCmdHandle, &cmd, NULL, 0U) == osOK)
    {
      display_handle_cmd(cmd);
    }

    display_flush_dirty();

    if (!s_display_busy && (render_demo_get_mode() == RENDER_DEMO_MODE_RUN))
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
        app_display_cmd_t cmd = APP_DISPLAY_CMD_RENDER_DEMO;
        (void)osMessageQueuePut(qDisplayCmdHandle, &cmd, 0U, 0U);
      }
      else
      {
        render_demo_set_mode(RENDER_DEMO_MODE_IDLE);
      }
    }
  }
}

bool display_is_busy(void)
{
  return s_display_busy;
}

void LCD_FlushDmaDoneCallback(void)
{
  if (tskDisplayHandle != NULL)
  {
    (void)osThreadFlagsSet(tskDisplayHandle, kDisplayFlagDmaDone);
  }
}

void LCD_FlushDmaErrorCallback(void)
{
  if (tskDisplayHandle != NULL)
  {
    (void)osThreadFlagsSet(tskDisplayHandle, kDisplayFlagDmaError);
  }
}
