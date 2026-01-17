#include "ui_pages.h"

#include "display_renderer.h"
#include "font8x8_basic.h"
#include "sensor_task.h"

#include <stdio.h>
#include <string.h>

static sensor_lis2_status_t s_last;
static uint8_t s_has_last = 0U;

static void ui_draw_text_clipped(uint16_t x, uint16_t y, const char *text)
{
  if (text == NULL)
  {
    return;
  }

  const uint16_t w = renderGetWidth();
  const uint16_t max_chars = (w > x) ? (uint16_t)((w - x) / FONT8X8_WIDTH) : 0U;
  if (max_chars == 0U)
  {
    return;
  }

  char tmp[48];
  (void)strncpy(tmp, text, sizeof(tmp) - 1U);
  tmp[sizeof(tmp) - 1U] = '\0';

  if (strlen(tmp) > max_chars)
  {
    tmp[max_chars] = '\0';
  }

  renderDrawText(x, y, tmp, RENDER_LAYER_UI, RENDER_STATE_BLACK);
}

static void page_lis2_steps_enter(void)
{
  s_has_last = 0U;
}

static uint32_t page_lis2_steps_event(ui_evt_t evt)
{
  if (evt == UI_EVT_BACK)
  {
    return UI_PAGE_EVENT_BACK;
  }

  if (evt == UI_EVT_TICK)
  {
    sensor_lis2_status_t status;
    sensor_lis2_get_status(&status);
    if ((s_has_last == 0U) ||
        (status.error_count != s_last.error_count) ||
        (status.init_ok != s_last.init_ok) ||
        (status.id_valid != s_last.id_valid) ||
        (status.step_valid != s_last.step_valid) ||
        (status.step_count != s_last.step_count) ||
        (status.emb_valid != s_last.emb_valid) ||
        (status.emb_step != s_last.emb_step) ||
        (status.emb_tilt != s_last.emb_tilt) ||
        (status.emb_sigmot != s_last.emb_sigmot))
    {
      s_last = status;
      s_has_last = 1U;
      return UI_PAGE_EVENT_RENDER;
    }
  }

  return UI_PAGE_EVENT_NONE;
}

static void page_lis2_steps_render(void)
{
  sensor_lis2_status_t status;
  sensor_lis2_get_status(&status);

  renderFill(false);
  ui_draw_text_clipped(4U, 4U, "STEP CNT");

  uint16_t y = (uint16_t)(FONT8X8_HEIGHT + 8U);
  uint16_t step = (uint16_t)(FONT8X8_HEIGHT + 2U);
  char line[48];

  if (status.id_valid != 0U)
  {
    const char *init_label = (status.init_ok != 0U) ? "OK" : "ERR";
    (void)snprintf(line, sizeof(line), "ID 0x%02X @0x%02X %s",
                   (unsigned)status.device_id, (unsigned)status.i2c_addr_7b, init_label);
  }
  else
  {
    if (status.i2c_addr_7b != 0U)
    {
      (void)snprintf(line, sizeof(line), "ID -- @0x%02X ERR",
                     (unsigned)status.i2c_addr_7b);
    }
    else
    {
      (void)snprintf(line, sizeof(line), "ID -- @-- ERR");
    }
  }
  ui_draw_text_clipped(0U, y, line);
  y = (uint16_t)(y + step);

  if (status.step_valid != 0U)
  {
    (void)snprintf(line, sizeof(line), "STEP:%5u", (unsigned)status.step_count);
  }
  else
  {
    (void)snprintf(line, sizeof(line), "STEP: N/A");
  }
  ui_draw_text_clipped(0U, y, line);
  y = (uint16_t)(y + step);

  if (status.emb_valid != 0U)
  {
    (void)snprintf(line, sizeof(line), "DET:%u TILT:%u",
                   (unsigned)status.emb_step,
                   (unsigned)status.emb_tilt);
  }
  else
  {
    (void)snprintf(line, sizeof(line), "DET:-- TILT:--");
  }
  ui_draw_text_clipped(0U, y, line);
  y = (uint16_t)(y + step);

  if (status.emb_valid != 0U)
  {
    (void)snprintf(line, sizeof(line), "SIG:%u", (unsigned)status.emb_sigmot);
  }
  else
  {
    (void)snprintf(line, sizeof(line), "SIG:--");
  }
  ui_draw_text_clipped(0U, y, line);
  y = (uint16_t)(y + step);

  ui_draw_text_clipped(0U, y, "MODE: OFF");
  y = (uint16_t)(y + step);

  ui_draw_text_clipped(0U, y, "INIT: PEND");
  y = (uint16_t)(y + step);

  (void)snprintf(line, sizeof(line), "ERR %lu", (unsigned long)status.error_count);
  ui_draw_text_clipped(0U, y, line);
}

const ui_page_t PAGE_LIS2_STEPS =
{
  .name = "Step Counter",
  .enter = page_lis2_steps_enter,
  .event = page_lis2_steps_event,
  .render = page_lis2_steps_render,
  .exit = NULL,
  .tick_ms = 500U,
  .flags = UI_PAGE_FLAG_LIS2
};
