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

static void display_test_layers(void)
{
  uint16_t width = renderGetWidth();
  uint16_t height = renderGetHeight();

  if ((width == 0U) || (height == 0U))
  {
    return;
  }

  renderFill(false);

  uint16_t band_y = (uint16_t)((height * 2U) / 3U);
  uint16_t band_h = (uint16_t)(height - band_y);
  renderFillRect(0U, band_y, width, band_h, RENDER_LAYER_BG, RENDER_STATE_BLACK);

  uint16_t game_x = (uint16_t)(width / 10U);
  uint16_t game_y = (uint16_t)(height / 10U);
  uint16_t game_w = (uint16_t)(width / 2U);
  uint16_t game_h = (uint16_t)(height / 4U);
  renderFillRect(game_x, game_y, game_w, game_h, RENDER_LAYER_GAME, RENDER_STATE_BLACK);

  uint16_t game2_y = (uint16_t)(band_y + (band_h / 4U));
  uint16_t game2_h = (uint16_t)(band_h / 2U);
  renderFillRect(game_x, game2_y, game_w, game2_h, RENDER_LAYER_GAME, RENDER_STATE_WHITE);

  uint16_t ui_x = (uint16_t)(game_x + (game_w / 4U));
  uint16_t ui_y = (uint16_t)(game_y + (game_h / 4U));
  uint16_t ui_w = (uint16_t)(game_w / 2U);
  uint16_t ui_h = (uint16_t)(game_h / 2U);
  renderFillRect(ui_x, ui_y, ui_w, ui_h, RENDER_LAYER_UI, RENDER_STATE_WHITE);

  uint16_t ui2_y = (uint16_t)(game2_y + (game2_h / 4U));
  uint16_t ui2_h = (uint16_t)(game2_h / 2U);
  renderFillRect(ui_x, ui2_y, ui_w, ui2_h, RENDER_LAYER_UI, RENDER_STATE_BLACK);

  renderDrawRect(game_x, game_y, game_w, game_h, RENDER_LAYER_GAME, RENDER_STATE_WHITE);
  renderDrawLine(0U, 0U, (uint16_t)(width - 1U), (uint16_t)(height - 1U),
                 RENDER_LAYER_UI, RENDER_STATE_BLACK);

  uint16_t min_dim = (width < height) ? width : height;
  uint16_t radius = (uint16_t)(min_dim / 6U);
  if (radius > 0U)
  {
    renderDrawCircle((uint16_t)(width / 2U), (uint16_t)(height / 2U), radius,
                     RENDER_LAYER_UI, RENDER_STATE_BLACK);
    renderFillCircle((uint16_t)(width / 2U), (uint16_t)(height / 2U),
                     (uint16_t)(radius / 2U), RENDER_LAYER_GAME, RENDER_STATE_WHITE);
  }
}

static void display_handle_cmd(app_display_cmd_t cmd)
{
  switch (cmd)
  {
    case APP_DISPLAY_CMD_TOGGLE:
      renderInvert();
      break;
    case APP_DISPLAY_CMD_TEST_LAYERS:
      display_test_layers();
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
