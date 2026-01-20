#include "ui_pages.h"

#include "audio_task.h"
#include "display_renderer.h"
#include "font8x8_basic.h"
#include "settings.h"
#include "ui_router.h"

#include <stdio.h>
#include <string.h>

static uint8_t s_sound_index = 0U;
enum
{
  SOUND_ITEM_MASTER = 0,
  SOUND_ITEM_UI = 1,
  SOUND_ITEM_SFX = 2,
  SOUND_ITEM_MUSIC = 3,
  SOUND_ITEM_KEYCLICK = 4,
  SOUND_ITEM_COUNT = 5
};

static void page_sound_enter(void)
{
  s_sound_index = 0U;
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

static uint32_t page_sound_event(ui_evt_t evt)
{
  uint32_t result = UI_PAGE_EVENT_NONE;

  if (evt == UI_EVT_BACK)
  {
    return UI_PAGE_EVENT_BACK;
  }

  if (evt == UI_EVT_NAV_UP)
  {
    s_sound_index = (s_sound_index == 0U)
                      ? (uint8_t)(SOUND_ITEM_COUNT - 1U)
                      : (uint8_t)(s_sound_index - 1U);
    return UI_PAGE_EVENT_RENDER;
  }

  if (evt == UI_EVT_NAV_DOWN)
  {
    s_sound_index = (uint8_t)((s_sound_index + 1U) % SOUND_ITEM_COUNT);
    return UI_PAGE_EVENT_RENDER;
  }

  if ((evt == UI_EVT_DEC) || (evt == UI_EVT_INC) || (evt == UI_EVT_SELECT))
  {
    bool inc = (evt == UI_EVT_INC);
    if (s_sound_index == SOUND_ITEM_MASTER)
    {
      if (evt == UI_EVT_SELECT)
      {
        return result;
      }
      uint8_t volume = audio_get_volume();
      if ((evt == UI_EVT_DEC) && (volume > 0U))
      {
        volume--;
      }
      else if ((evt == UI_EVT_INC) && (volume < 10U))
      {
        volume++;
      }
      else
      {
        return result;
      }
      audio_set_volume(volume);
      (void)settings_set(SETTINGS_KEY_VOLUME, &volume);
      result |= UI_PAGE_EVENT_RENDER;
    }
    else if (s_sound_index == SOUND_ITEM_UI)
    {
      if (evt == UI_EVT_SELECT)
      {
        return result;
      }
      uint8_t volume = audio_get_category_volume(SOUND_CAT_UI);
      if ((evt == UI_EVT_DEC) && (volume > 0U))
      {
        volume--;
      }
      else if ((evt == UI_EVT_INC) && (volume < 10U))
      {
        volume++;
      }
      else
      {
        return result;
      }
      audio_set_category_volume(SOUND_CAT_UI, volume);
      (void)settings_set(SETTINGS_KEY_VOLUME_UI, &volume);
      result |= UI_PAGE_EVENT_RENDER;
    }
    else if (s_sound_index == SOUND_ITEM_SFX)
    {
      if (evt == UI_EVT_SELECT)
      {
        return result;
      }
      uint8_t volume = audio_get_category_volume(SOUND_CAT_SFX);
      if ((evt == UI_EVT_DEC) && (volume > 0U))
      {
        volume--;
      }
      else if ((evt == UI_EVT_INC) && (volume < 10U))
      {
        volume++;
      }
      else
      {
        return result;
      }
      audio_set_category_volume(SOUND_CAT_SFX, volume);
      (void)settings_set(SETTINGS_KEY_VOLUME_SFX, &volume);
      result |= UI_PAGE_EVENT_RENDER;
    }
    else if (s_sound_index == SOUND_ITEM_MUSIC)
    {
      if (evt == UI_EVT_SELECT)
      {
        return result;
      }
      uint8_t volume = audio_get_category_volume(SOUND_CAT_MUSIC);
      if ((evt == UI_EVT_DEC) && (volume > 0U))
      {
        volume--;
      }
      else if ((evt == UI_EVT_INC) && (volume < 10U))
      {
        volume++;
      }
      else
      {
        return result;
      }
      audio_set_category_volume(SOUND_CAT_MUSIC, volume);
      (void)settings_set(SETTINGS_KEY_VOLUME_MUSIC, &volume);
      result |= UI_PAGE_EVENT_RENDER;
    }
    else if (s_sound_index == SOUND_ITEM_KEYCLICK)
    {
      uint8_t keyclick = ui_router_get_keyclick() ? 1U : 0U;
      keyclick = (evt == UI_EVT_SELECT) ? (uint8_t)(!keyclick)
                                        : (uint8_t)(inc ? 1U : 0U);
      ui_router_set_keyclick(keyclick != 0U);
      (void)settings_set(SETTINGS_KEY_KEYCLICK_ENABLED, &keyclick);
      result |= UI_PAGE_EVENT_RENDER;
    }
  }

  return result;
}

static void page_sound_render(void)
{
  uint8_t volume = audio_get_volume();
  uint8_t volume_ui = audio_get_category_volume(SOUND_CAT_UI);
  uint8_t volume_sfx = audio_get_category_volume(SOUND_CAT_SFX);
  uint8_t volume_music = audio_get_category_volume(SOUND_CAT_MUSIC);
  const char *kc = ui_router_get_keyclick() ? "ON" : "OFF";

  renderFill(false);
  renderDrawText(4U, 4U, "SOUND SETTINGS", RENDER_LAYER_UI, RENDER_STATE_BLACK);

  char line_bufs[SOUND_ITEM_COUNT][24];
  (void)snprintf(line_bufs[SOUND_ITEM_MASTER], sizeof(line_bufs[SOUND_ITEM_MASTER]),
                 "MASTER: %u/10", (unsigned)volume);
  (void)snprintf(line_bufs[SOUND_ITEM_UI], sizeof(line_bufs[SOUND_ITEM_UI]),
                 "UI: %u/10", (unsigned)volume_ui);
  (void)snprintf(line_bufs[SOUND_ITEM_SFX], sizeof(line_bufs[SOUND_ITEM_SFX]),
                 "SFX: %u/10", (unsigned)volume_sfx);
  (void)snprintf(line_bufs[SOUND_ITEM_MUSIC], sizeof(line_bufs[SOUND_ITEM_MUSIC]),
                 "MUSIC: %u/10", (unsigned)volume_music);
  (void)snprintf(line_bufs[SOUND_ITEM_KEYCLICK], sizeof(line_bufs[SOUND_ITEM_KEYCLICK]),
                 "KEYCLICK: %s", kc);

  uint16_t height = renderGetHeight();
  uint16_t y = 20U;
  uint16_t step = (uint16_t)(FONT8X8_HEIGHT + 4U);

  for (uint8_t i = 0U; i < SOUND_ITEM_COUNT; ++i)
  {
    const char *line = line_bufs[i];
    bool selected = (i == s_sound_index);
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

const ui_page_t PAGE_SOUND =
{
  .name = "Sound Settings",
  .enter = page_sound_enter,
  .event = page_sound_event,
  .render = page_sound_render,
  .exit = NULL,
  .tick_ms = 0U,
  .flags = UI_PAGE_FLAG_JOY_MENU
};
