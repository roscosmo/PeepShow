#include "ui_pages.h"

#include "display_renderer.h"
#include "font8x8_basic.h"
#include "sensor_task.h"
#include "ui_actions.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static sensor_lis2_status_t s_last;
static uint8_t s_has_last = 0U;
static uint8_t s_steps_index = 0U;

enum
{
  STEPS_ITEM_ENABLE = 0U,
  STEPS_ITEM_RESET = 1U,
  STEPS_ITEM_COUNT = 2U
};

static const char *lis2_err_label(uint8_t err_src, char *buf, size_t len)
{
  if ((buf == NULL) || (len < 3U))
  {
    return "";
  }

  if (err_src == 0U)
  {
    (void)snprintf(buf, len, "--");
    return buf;
  }

  uint32_t pos = 0U;
  if ((err_src & SENSOR_LIS2_ERR_SRC_STATUS) != 0U)
  {
    buf[pos++] = 'S';
  }
  if ((err_src & SENSOR_LIS2_ERR_SRC_EMB) != 0U)
  {
    buf[pos++] = 'E';
  }
  if ((err_src & SENSOR_LIS2_ERR_SRC_XL) != 0U)
  {
    buf[pos++] = 'X';
  }
  if ((err_src & SENSOR_LIS2_ERR_SRC_TEMP) != 0U)
  {
    buf[pos++] = 'T';
  }
  if ((err_src & SENSOR_LIS2_ERR_SRC_STEP) != 0U)
  {
    buf[pos++] = 'P';
  }

  if (pos >= len)
  {
    pos = len - 1U;
  }
  buf[pos] = '\0';
  return buf;
}

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

static uint16_t ui_text_width(const char *text)
{
  if (text == NULL)
  {
    return 0U;
  }

  size_t len = strlen(text);
  if (len == 0U)
  {
    return 0U;
  }

  return (uint16_t)((len * (FONT8X8_WIDTH + 1U)) - 1U);
}

static void ui_draw_menu_line(uint16_t x, uint16_t y, const char *text, bool selected)
{
  if (text == NULL)
  {
    return;
  }

  if (selected)
  {
    uint16_t text_w = ui_text_width(text);
    uint16_t box_w = (uint16_t)(text_w + 4U);
    if (box_w < 10U)
    {
      box_w = 10U;
    }
    renderFillRect(2U, (uint16_t)(y - 1U), box_w,
                   (uint16_t)(FONT8X8_HEIGHT + 2U),
                   RENDER_LAYER_UI, RENDER_STATE_BLACK);
    renderDrawText(x, y, text, RENDER_LAYER_UI, RENDER_STATE_WHITE);
  }
  else
  {
    renderDrawText(x, y, text, RENDER_LAYER_UI, RENDER_STATE_BLACK);
  }
}

static void page_lis2_steps_enter(void)
{
  s_has_last = 0U;
  s_steps_index = 0U;
}

static uint32_t page_lis2_steps_event(ui_evt_t evt)
{
  if (evt == UI_EVT_BACK)
  {
    return UI_PAGE_EVENT_BACK;
  }

  if (evt == UI_EVT_NAV_UP)
  {
    if (s_steps_index == 0U)
    {
      s_steps_index = (uint8_t)(STEPS_ITEM_COUNT - 1U);
    }
    else
    {
      s_steps_index--;
    }
    return UI_PAGE_EVENT_RENDER;
  }

  if (evt == UI_EVT_NAV_DOWN)
  {
    s_steps_index = (uint8_t)((s_steps_index + 1U) % STEPS_ITEM_COUNT);
    return UI_PAGE_EVENT_RENDER;
  }

  if (evt == UI_EVT_SELECT)
  {
    if (s_steps_index == STEPS_ITEM_ENABLE)
    {
      sensor_lis2_status_t status;
      sensor_lis2_get_status(&status);
      app_sensor_req_t req = (status.step_enabled != 0U) ?
                             APP_SENSOR_REQ_LIS2_STEP_OFF :
                             APP_SENSOR_REQ_LIS2_STEP_ON;
      ui_actions_send_sensor_req(req);
      return UI_PAGE_EVENT_RENDER;
    }
    if (s_steps_index == STEPS_ITEM_RESET)
    {
      ui_actions_send_sensor_req(APP_SENSOR_REQ_LIS2_STEP_RESET);
      return UI_PAGE_EVENT_RENDER;
    }
  }

  if (evt == UI_EVT_TICK)
  {
    sensor_lis2_status_t status;
    sensor_lis2_get_status(&status);
    if ((s_has_last == 0U) ||
        (status.error_count != s_last.error_count) ||
        (status.err_src != s_last.err_src) ||
        (status.init_ok != s_last.init_ok) ||
        (status.id_valid != s_last.id_valid) ||
        (status.step_valid != s_last.step_valid) ||
        (status.step_count != s_last.step_count) ||
        (status.step_enabled != s_last.step_enabled) ||
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
  char err_buf[8];

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
    (void)snprintf(line, sizeof(line), "STEP:-----");
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

  const char *step_state = (status.step_enabled != 0U) ? "ON" : "OFF";
  (void)snprintf(line, sizeof(line), "ENABLE: %s", step_state);
  ui_draw_menu_line(4U, y, line, (s_steps_index == STEPS_ITEM_ENABLE));
  y = (uint16_t)(y + step);

  ui_draw_menu_line(4U, y, "RESET", (s_steps_index == STEPS_ITEM_RESET));
  y = (uint16_t)(y + step);

  const char *err_label = lis2_err_label(status.err_src, err_buf, sizeof(err_buf));
  (void)snprintf(line, sizeof(line), "ERR %lu %s",
                 (unsigned long)status.error_count, err_label);
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
