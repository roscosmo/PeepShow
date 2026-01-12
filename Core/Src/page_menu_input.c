#include "ui_pages.h"

#include "display_renderer.h"
#include "font8x8_basic.h"
#include "sensor_task.h"
#include "ui_actions.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static uint8_t s_menu_input_index = 0U;
static const uint8_t k_menu_input_item_count = 3U;

static void page_menu_input_enter(void)
{
  s_menu_input_index = 0U;
}

static uint32_t page_menu_input_event(ui_evt_t evt)
{
  uint32_t result = UI_PAGE_EVENT_NONE;

  if (evt == UI_EVT_BACK)
  {
    return UI_PAGE_EVENT_BACK;
  }

  if (evt == UI_EVT_NAV_UP)
  {
    if (s_menu_input_index == 0U)
    {
      s_menu_input_index = (uint8_t)(k_menu_input_item_count - 1U);
    }
    else
    {
      s_menu_input_index--;
    }
    result |= UI_PAGE_EVENT_RENDER;
  }
  else if (evt == UI_EVT_NAV_DOWN)
  {
    s_menu_input_index = (uint8_t)((s_menu_input_index + 1U) % k_menu_input_item_count);
    result |= UI_PAGE_EVENT_RENDER;
  }
  else if ((evt == UI_EVT_DEC) || (evt == UI_EVT_INC))
  {
    app_sensor_req_t req = 0U;
    bool inc = (evt == UI_EVT_INC);
    if (s_menu_input_index == 0U)
    {
      req = inc ? APP_SENSOR_REQ_JOY_MENU_PRESS_INC : APP_SENSOR_REQ_JOY_MENU_PRESS_DEC;
    }
    else if (s_menu_input_index == 1U)
    {
      req = inc ? APP_SENSOR_REQ_JOY_MENU_RELEASE_INC : APP_SENSOR_REQ_JOY_MENU_RELEASE_DEC;
    }
    else
    {
      req = inc ? APP_SENSOR_REQ_JOY_MENU_RATIO_INC : APP_SENSOR_REQ_JOY_MENU_RATIO_DEC;
    }

    if (req != 0U)
    {
      ui_actions_send_sensor_req(req);
      result |= UI_PAGE_EVENT_RENDER;
    }
  }

  return result;
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

static void page_menu_input_render(void)
{
  sensor_joy_menu_params_t params;
  sensor_joy_get_menu_params(&params);

  renderFill(false);
  renderDrawText(4U, 4U, "MENU INPUT", RENDER_LAYER_UI, RENDER_STATE_BLACK);

  uint16_t height = renderGetHeight();
  uint16_t y = 20U;
  for (uint8_t i = 0U; i < k_menu_input_item_count; ++i)
  {
    char line[24];
    if (i == 0U)
    {
      int32_t v = (int32_t)(params.press_norm * 100.0f + 0.5f);
      (void)snprintf(line, sizeof(line), "Press: %ld.%02ld",
                     (long)(v / 100), (long)(v % 100));
    }
    else if (i == 1U)
    {
      int32_t v = (int32_t)(params.release_norm * 100.0f + 0.5f);
      (void)snprintf(line, sizeof(line), "Release: %ld.%02ld",
                     (long)(v / 100), (long)(v % 100));
    }
    else
    {
      int32_t v = (int32_t)(params.axis_ratio * 10.0f + 0.5f);
      (void)snprintf(line, sizeof(line), "Axis: %ld.%01ld",
                     (long)(v / 10), (long)(v % 10));
    }

    bool selected = (i == s_menu_input_index);
    if (selected)
    {
      uint16_t text_w = ui_text_width(line);
      uint16_t box_w = (uint16_t)(text_w + 4U);
      if (box_w < 10U)
      {
        box_w = 10U;
      }
      renderFillRect(2U, (uint16_t)(y - 1U), box_w,
                     (uint16_t)(FONT8X8_HEIGHT + 2U),
                     RENDER_LAYER_UI, RENDER_STATE_BLACK);
      renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_STATE_WHITE);
    }
    else
    {
      renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_STATE_BLACK);
    }

    y = (uint16_t)(y + (FONT8X8_HEIGHT + 4U));
    if ((height > 0U) && (y >= height))
    {
      break;
    }
  }

  if (height > (uint16_t)(FONT8X8_HEIGHT + 6U))
  {
    uint16_t hint_y = (uint16_t)(height - (FONT8X8_HEIGHT * 2U + 4U));
    renderDrawText(4U, hint_y, "JOY U/D: SELECT",
                   RENDER_LAYER_UI, RENDER_STATE_BLACK);
    renderDrawText(4U, (uint16_t)(hint_y + FONT8X8_HEIGHT + 2U),
                   "L/R: ADJUST  B: BACK",
                   RENDER_LAYER_UI, RENDER_STATE_BLACK);
  }
}

const ui_page_t PAGE_MENU_INPUT =
{
  .name = "Menu Input",
  .enter = page_menu_input_enter,
  .event = page_menu_input_event,
  .render = page_menu_input_render,
  .exit = NULL,
  .tick_ms = 0U,
  .flags = UI_PAGE_FLAG_JOY_MENU
};
