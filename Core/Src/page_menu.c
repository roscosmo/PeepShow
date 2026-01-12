#include "ui_pages.h"

#include "display_renderer.h"
#include "font8x8_basic.h"
#include "ui_router.h"

#include <string.h>

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

static void page_menu_render(void)
{
  ui_router_menu_state_t state;
  ui_router_get_menu_state(&state);

  renderFill(false);

  const char *title = (state.menu != NULL) ? state.menu->title : "MENU";
  renderDrawText(4U, 4U, title, RENDER_LAYER_UI, RENDER_STATE_BLACK);

  if ((state.menu == NULL) || (state.menu->items == NULL) || (state.menu->count == 0U))
  {
    renderDrawText(4U, 20U, "NO ITEMS", RENDER_LAYER_UI, RENDER_STATE_BLACK);
    return;
  }

  uint16_t height = renderGetHeight();
  uint16_t y = 20U;
  for (uint8_t i = 0U; i < state.menu->count; ++i)
  {
    const char *label = state.menu->items[i].label;
    bool selected = (i == state.index);
    if (selected)
    {
      uint16_t text_w = ui_text_width(label);
      uint16_t box_w = (uint16_t)(text_w + 4U);
      if (box_w < 10U)
      {
        box_w = 10U;
      }
      renderFillRect(2U, (uint16_t)(y - 1U), box_w,
                     (uint16_t)(FONT8X8_HEIGHT + 2U),
                     RENDER_LAYER_UI, RENDER_STATE_BLACK);
      renderDrawText(4U, y, label, RENDER_LAYER_UI, RENDER_STATE_WHITE);
    }
    else
    {
      renderDrawText(4U, y, label, RENDER_LAYER_UI, RENDER_STATE_BLACK);
    }
    y = (uint16_t)(y + (FONT8X8_HEIGHT + 4U));
    if ((height > 0U) && (y >= height))
    {
      break;
    }
  }

  if (height > (uint16_t)(FONT8X8_HEIGHT + 6U))
  {
    uint16_t hint_y = (uint16_t)(height - (FONT8X8_HEIGHT + 2U));
    renderDrawText(4U, hint_y, "A: ENTER  B: BACK", RENDER_LAYER_UI, RENDER_STATE_BLACK);
  }
}

const ui_page_t PAGE_MENU =
{
  .name = "Menu",
  .enter = NULL,
  .event = NULL,
  .render = page_menu_render,
  .exit = NULL,
  .tick_ms = 0U,
  .flags = UI_PAGE_FLAG_JOY_MENU
};
