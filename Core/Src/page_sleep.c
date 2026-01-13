#include "display_renderer.h"
#include "font8x8_basic.h"
#include "power_task.h"
#include "settings.h"
#include "ui_pages.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static uint8_t s_sleep_index = 0U;
enum { SLEEP_ITEM_COUNT = 3 };

static const uint32_t k_sleep_timeout_ms[] =
{
  15000U,
  30000U,
  60000U,
  120000U,
  300000U
};

static void page_sleep_enter(void)
{
  s_sleep_index = 0U;
}

static uint8_t sleep_timeout_index(uint32_t timeout_ms)
{
  for (uint8_t i = 0U; i < (uint8_t)(sizeof(k_sleep_timeout_ms) / sizeof(k_sleep_timeout_ms[0])); ++i)
  {
    if (k_sleep_timeout_ms[i] == timeout_ms)
    {
      return i;
    }
  }
  return 0U;
}

static const char *sleep_timeout_label(uint32_t timeout_ms, char *buf, size_t len)
{
  if ((buf == NULL) || (len < 4U))
  {
    return "?";
  }

  uint32_t seconds = timeout_ms / 1000U;
  if (seconds >= 60U)
  {
    uint32_t mins = seconds / 60U;
    (void)snprintf(buf, len, "%lum", (unsigned long)mins);
  }
  else
  {
    (void)snprintf(buf, len, "%lus", (unsigned long)seconds);
  }

  return buf;
}

static uint32_t page_sleep_event(ui_evt_t evt)
{
  uint32_t result = UI_PAGE_EVENT_NONE;

  if (evt == UI_EVT_BACK)
  {
    return UI_PAGE_EVENT_BACK;
  }

  if (evt == UI_EVT_NAV_UP)
  {
    if (s_sleep_index == 0U)
    {
      s_sleep_index = (uint8_t)(SLEEP_ITEM_COUNT - 1U);
    }
    else
    {
      s_sleep_index--;
    }
    return UI_PAGE_EVENT_RENDER;
  }

  if (evt == UI_EVT_NAV_DOWN)
  {
    s_sleep_index = (uint8_t)((s_sleep_index + 1U) % SLEEP_ITEM_COUNT);
    return UI_PAGE_EVENT_RENDER;
  }

  if ((evt == UI_EVT_DEC) || (evt == UI_EVT_INC) || (evt == UI_EVT_SELECT))
  {
    settings_data_t data;
    settings_get(&data);
    bool inc = (evt == UI_EVT_INC);

    if ((evt == UI_EVT_SELECT) && (s_sleep_index == 2U))
    {
      return result;
    }

    if (s_sleep_index == 0U)
    {
      uint8_t enabled = (data.sleep_enabled != 0U) ? 1U : 0U;
      enabled = (evt == UI_EVT_SELECT) ? (uint8_t)(!enabled) : (uint8_t)((inc != 0U) ? 1U : 0U);
      settings_set_sleep_enabled(enabled);
      power_task_set_sleep_enabled(enabled);
      result |= UI_PAGE_EVENT_RENDER;
    }
    else if (s_sleep_index == 1U)
    {
      uint8_t allow = (data.sleep_allow_game != 0U) ? 1U : 0U;
      allow = (evt == UI_EVT_SELECT) ? (uint8_t)(!allow) : (uint8_t)((inc != 0U) ? 1U : 0U);
      settings_set_sleep_allow_game(allow);
      power_task_set_game_sleep_allowed(allow);
      result |= UI_PAGE_EVENT_RENDER;
    }
    else
    {
      uint8_t idx = sleep_timeout_index(data.sleep_timeout_ms);
      uint8_t count = (uint8_t)(sizeof(k_sleep_timeout_ms) / sizeof(k_sleep_timeout_ms[0]));
      if (inc)
      {
        idx = (uint8_t)((idx + 1U) % count);
      }
      else
      {
        idx = (idx == 0U) ? (uint8_t)(count - 1U) : (uint8_t)(idx - 1U);
      }
      uint32_t timeout_ms = k_sleep_timeout_ms[idx];
      settings_set_sleep_timeout_ms(timeout_ms);
      power_task_set_inactivity_timeout_ms(timeout_ms);
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

static void page_sleep_render(void)
{
  settings_data_t data;
  settings_get(&data);

  renderFill(false);
  renderDrawText(4U, 4U, "SLEEP OPTIONS", RENDER_LAYER_UI, RENDER_STATE_BLACK);

  const char *sleep_on = (data.sleep_enabled != 0U) ? "ON" : "OFF";
  const char *game_on = (data.sleep_allow_game != 0U) ? "ON" : "OFF";
  char timeout_buf[8];
  const char *timeout = sleep_timeout_label(data.sleep_timeout_ms, timeout_buf, sizeof(timeout_buf));

  const char *lines[SLEEP_ITEM_COUNT] =
  {
    "SLEEP:",
    "IN GAME:",
    "TIMEOUT:"
  };

  char line_bufs[SLEEP_ITEM_COUNT][24];
  (void)snprintf(line_bufs[0], sizeof(line_bufs[0]), "%s %s", lines[0], sleep_on);
  (void)snprintf(line_bufs[1], sizeof(line_bufs[1]), "%s %s", lines[1], game_on);
  (void)snprintf(line_bufs[2], sizeof(line_bufs[2]), "%s %s", lines[2], timeout);

  uint16_t height = renderGetHeight();
  uint16_t y = 20U;
  uint16_t step = (uint16_t)(FONT8X8_HEIGHT + 4U);

  for (uint8_t i = 0U; i < SLEEP_ITEM_COUNT; ++i)
  {
    const char *line = line_bufs[i];
    bool selected = (i == s_sleep_index);
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

    y = (uint16_t)(y + step);
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
                   "L/R: ADJUST  A: TOGGLE  B: BACK",
                   RENDER_LAYER_UI, RENDER_STATE_BLACK);
  }
}

const ui_page_t PAGE_SLEEP =
{
  .name = "Sleep Options",
  .enter = page_sleep_enter,
  .event = page_sleep_event,
  .render = page_sleep_render,
  .exit = NULL,
  .tick_ms = 0U,
  .flags = UI_PAGE_FLAG_JOY_MENU
};
