#include "ui_pages.h"

#include "audio_task.h"
#include "display_renderer.h"
#include "font8x8_basic.h"
#include "ui_router.h"

#include <stdio.h>

static uint32_t page_sound_event(ui_evt_t evt)
{
  uint32_t result = UI_PAGE_EVENT_NONE;

  if (evt == UI_EVT_BACK)
  {
    return UI_PAGE_EVENT_BACK;
  }

  if (evt == UI_EVT_DEC)
  {
    uint8_t volume = audio_get_volume();
    if (volume > 0U)
    {
      volume--;
      audio_set_volume(volume);
      result |= UI_PAGE_EVENT_RENDER;
    }
  }
  else if (evt == UI_EVT_INC)
  {
    uint8_t volume = audio_get_volume();
    if (volume < 10U)
    {
      volume++;
      audio_set_volume(volume);
      result |= UI_PAGE_EVENT_RENDER;
    }
  }
  else if (evt == UI_EVT_SELECT)
  {
    ui_router_set_keyclick(!ui_router_get_keyclick());
    result |= UI_PAGE_EVENT_RENDER;
  }

  return result;
}

static void page_sound_render(void)
{
  renderFill(false);
  renderDrawText(4U, 4U, "SOUND SETTINGS", RENDER_LAYER_UI, RENDER_STATE_BLACK);

  uint8_t volume = audio_get_volume();
  char buf[24];
  (void)snprintf(buf, sizeof(buf), "VOLUME: %u/10", (unsigned)volume);
  renderDrawText(4U, 20U, buf, RENDER_LAYER_UI, RENDER_STATE_BLACK);

  const char *kc = ui_router_get_keyclick() ? "ON" : "OFF";
  (void)snprintf(buf, sizeof(buf), "KEYCLICK: %s", kc);
  renderDrawText(4U, (uint16_t)(20U + FONT8X8_HEIGHT + 4U),
                 buf, RENDER_LAYER_UI, RENDER_STATE_BLACK);

  uint16_t height = renderGetHeight();
  if (height > (uint16_t)(FONT8X8_HEIGHT + 6U))
  {
    uint16_t hint_y = (uint16_t)(height - (FONT8X8_HEIGHT * 2U + 4U));
    renderDrawText(4U, hint_y, "L/R: LEVEL  A: TOGGLE",
                   RENDER_LAYER_UI, RENDER_STATE_BLACK);
    renderDrawText(4U, (uint16_t)(hint_y + FONT8X8_HEIGHT + 2U),
                   "B: BACK", RENDER_LAYER_UI, RENDER_STATE_BLACK);
  }
}

const ui_page_t PAGE_SOUND =
{
  .name = "Sound Settings",
  .enter = NULL,
  .event = page_sound_event,
  .render = page_sound_render,
  .exit = NULL,
  .tick_ms = 0U,
  .flags = UI_PAGE_FLAG_JOY_MENU
};
