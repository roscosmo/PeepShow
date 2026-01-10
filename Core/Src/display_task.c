#include "display_task.h"

#include "LS013B7DH05.h"
#include "display_renderer.h"
#include "app_freertos.h"
#include "main.h"

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

  uint16_t start_row = 0U;
  uint16_t end_row = 0U;
  if (!renderTakeDirtySpan(&start_row, &end_row))
  {
    return;
  }

  const uint8_t *buf = renderGetBuffer();
  if (buf == NULL)
  {
    renderMarkDirtyRows(start_row, end_row);
    return;
  }

  (void)osThreadFlagsClear(kDisplayFlagDmaDone | kDisplayFlagDmaError);
  s_display_busy = true;
  HAL_StatusTypeDef st = HAL_ERROR;
  if ((start_row == 1U) && (end_row == DISPLAY_HEIGHT))
  {
    st = LCD_FlushAll_DMA(&s_display, buf);
  }
  else
  {
    uint16_t count = (uint16_t)(end_row - start_row + 1U);
    for (uint16_t i = 0U; i < count; ++i)
    {
      s_rows[i] = (uint16_t)(start_row + i);
    }
    st = LCD_FlushRows_DMA(&s_display, buf, s_rows, count);
  }

  if (st != HAL_OK)
  {
    s_display_busy = false;
    renderMarkDirtyRows(start_row, end_row);
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
    renderMarkDirtyRows(start_row, end_row);
    return;
  }

  if (LCD_FlushDMA_IsDone())
  {
    s_display_busy = false;
  }
  if ((flags & (int32_t)kDisplayFlagDmaError) != 0)
  {
    renderMarkDirtyRows(start_row, end_row);
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
    app_display_cmd_t cmd = 0U;
    if (osMessageQueueGet(qDisplayCmdHandle, &cmd, NULL, osWaitForever) != osOK)
    {
      continue;
    }

    display_handle_cmd(cmd);
    while (osMessageQueueGet(qDisplayCmdHandle, &cmd, NULL, 0U) == osOK)
    {
      display_handle_cmd(cmd);
    }

    display_flush_dirty();
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
