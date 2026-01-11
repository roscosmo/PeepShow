#include "ui_router.h"

#include "app_freertos.h"
#include "display_renderer.h"
#include "font8x8_basic.h"

#include <string.h>

typedef struct
{
  const char *label;
  ui_router_cmd_t cmd;
} ui_menu_item_t;

static const ui_menu_item_t k_menu_items[] =
{
  { "Render Demo", UI_ROUTER_CMD_START_RENDER_DEMO }
};

static const uint8_t k_menu_item_count = (uint8_t)(sizeof(k_menu_items) / sizeof(k_menu_items[0]));

typedef struct
{
  ui_page_t page;
  uint8_t menu_index;
} ui_router_state_t;

static ui_router_state_t s_ui =
{
  .page = UI_PAGE_MENU,
  .menu_index = 0U
};

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

static void ui_render_home(uint16_t width, uint16_t height)
{
  (void)width;
  (void)height;

  renderFill(false);
  renderDrawText(4U, 4U, "HOME", RENDER_LAYER_UI, RENDER_STATE_BLACK);
  renderDrawText(4U, 16U, "BOOT: MENU", RENDER_LAYER_UI, RENDER_STATE_BLACK);
}

static void ui_render_menu(uint16_t width, uint16_t height)
{
  renderFill(false);
  renderDrawText(4U, 4U, "MENU", RENDER_LAYER_UI, RENDER_STATE_BLACK);

  uint16_t y = 20U;
  for (uint8_t i = 0U; i < k_menu_item_count; ++i)
  {
    const char *label = k_menu_items[i].label;
    bool selected = (i == s_ui.menu_index);
    if (selected)
    {
      uint16_t text_w = ui_text_width(label);
      uint16_t box_w = (uint16_t)(text_w + 4U);
      if (box_w < 10U)
      {
        box_w = 10U;
      }
      renderFillRect(2U, (uint16_t)(y - 1U), box_w, (uint16_t)(FONT8X8_HEIGHT + 2U),
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

void ui_router_init(void)
{
  s_ui.page = UI_PAGE_MENU;
  s_ui.menu_index = 0U;
}

ui_page_t ui_router_get_page(void)
{
  return s_ui.page;
}

void ui_router_set_page(ui_page_t page)
{
  s_ui.page = page;
}

bool ui_router_handle_button(uint32_t button_id, ui_router_cmd_t *out_cmd)
{
  if (out_cmd != NULL)
  {
    *out_cmd = UI_ROUTER_CMD_NONE;
  }

  ui_page_t prev_page = s_ui.page;
  uint8_t prev_index = s_ui.menu_index;

  if (s_ui.page == UI_PAGE_MENU)
  {
    if (button_id == (uint32_t)APP_BUTTON_A)
    {
      if (out_cmd != NULL)
      {
        *out_cmd = k_menu_items[s_ui.menu_index].cmd;
      }
    }
    else if (button_id == (uint32_t)APP_BUTTON_L)
    {
      if (k_menu_item_count > 1U)
      {
        if (s_ui.menu_index == 0U)
        {
          s_ui.menu_index = (uint8_t)(k_menu_item_count - 1U);
        }
        else
        {
          s_ui.menu_index--;
        }
      }
    }
    else if (button_id == (uint32_t)APP_BUTTON_R)
    {
      if (k_menu_item_count > 1U)
      {
        s_ui.menu_index = (uint8_t)((s_ui.menu_index + 1U) % k_menu_item_count);
      }
    }
  }

  return ((s_ui.page != prev_page) || (s_ui.menu_index != prev_index));
}

void ui_router_render(void)
{
  uint16_t width = renderGetWidth();
  uint16_t height = renderGetHeight();
  if ((width == 0U) || (height == 0U))
  {
    return;
  }

  switch (s_ui.page)
  {
    case UI_PAGE_MENU:
      ui_render_menu(width, height);
      break;
    case UI_PAGE_HOME:
      ui_render_home(width, height);
      break;
    default:
      ui_render_menu(width, height);
      break;
  }
}
