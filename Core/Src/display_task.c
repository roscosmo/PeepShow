#include "display_task.h"

#include "LS013B7DH05.h"
#include "app_freertos.h"
#include "main.h"

extern SPI_HandleTypeDef hspi3;

static LS013B7DH05 s_display;
static uint8_t s_disp_buf[BUFFER_LENGTH] __attribute__((aligned(4)));
static uint16_t s_dirty_min = 0U;
static uint16_t s_dirty_max = 0U;
static uint8_t s_fill_black = 0U;
static uint16_t s_rows[DISPLAY_HEIGHT];
static void display_mark_all_dirty(void)
{
  s_dirty_min = 1U;
  s_dirty_max = DISPLAY_HEIGHT;
}

static void display_handle_cmd(app_display_cmd_t cmd)
{
  switch (cmd)
  {
    case APP_DISPLAY_CMD_TOGGLE:
      s_fill_black = (uint8_t)((s_fill_black == 0U) ? 1U : 0U);
      LCD_Fill((bool)(s_fill_black != 0U));
      display_mark_all_dirty();
      break;
    default:
      break;
  }
}

static void display_flush_dirty(void)
{
  if ((s_dirty_min == 0U) || (s_dirty_max == 0U))
  {
    return;
  }

  HAL_StatusTypeDef st = HAL_ERROR;
  uint16_t count = 0U;
  if ((s_dirty_min == 1U) && (s_dirty_max == DISPLAY_HEIGHT))
  {
    count = DISPLAY_HEIGHT;
  }
  else
  {
    count = (uint16_t)(s_dirty_max - s_dirty_min + 1U);
    for (uint16_t i = 0U; i < count; ++i)
    {
      s_rows[i] = (uint16_t)(s_dirty_min + i);
    }
  }

  s_dirty_min = 0U;
  s_dirty_max = 0U;

  if (count == DISPLAY_HEIGHT)
  {
    st = LCD_FlushAll_DMA(&s_display);
  }
  else
  {
    st = LCD_FlushRows_DMA(&s_display, s_rows, count);
  }

  if (st != HAL_OK)
  {
    return;
  }

  (void)LCD_FlushDMA_WaitWFI(200U);
}

static void display_init(void)
{
  DispBuf = s_disp_buf;

  /* VLT_LCD is active-low and held low in main; do not change it here. */

  HAL_StatusTypeDef st = LCD_Init(&s_display, &hspi3, SPI3_CS_GPIO_Port, SPI3_CS_Pin);
  if (st != HAL_OK)
  {
    return;
  }

  LCD_Fill(false);
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
