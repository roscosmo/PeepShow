#include "ui_router.h"

#include "ui_menu.h"
#include "ui_pages.h"

#include <stddef.h>

#define UI_MENU_STACK_DEPTH 4U

typedef struct
{
  const ui_menu_t *menu;
  uint8_t index;
} ui_menu_stack_entry_t;

static const ui_page_t *s_page = &PAGE_MENU;
static ui_menu_stack_entry_t s_menu_stack[UI_MENU_STACK_DEPTH];
static uint8_t s_menu_depth = 0U;
static bool s_keyclick_enabled = true;

static uint8_t ui_menu_clamp_index(const ui_menu_t *menu, uint8_t index)
{
  if ((menu == NULL) || (menu->count == 0U))
  {
    return 0U;
  }

  if (index >= menu->count)
  {
    return (uint8_t)(menu->count - 1U);
  }

  return index;
}

static void ui_menu_reset(void)
{
  s_menu_depth = 1U;
  s_menu_stack[0].menu = ui_menu_root();
  s_menu_stack[0].index = 0U;
}

static ui_menu_stack_entry_t *ui_menu_active(void)
{
  if (s_menu_depth == 0U)
  {
    return NULL;
  }

  return &s_menu_stack[s_menu_depth - 1U];
}

static void ui_menu_push(const ui_menu_t *menu)
{
  if ((menu == NULL) || (s_menu_depth >= UI_MENU_STACK_DEPTH))
  {
    return;
  }

  s_menu_stack[s_menu_depth].menu = menu;
  s_menu_stack[s_menu_depth].index = 0U;
  s_menu_depth++;
}

void ui_router_init(void)
{
  ui_menu_reset();
  s_page = &PAGE_MENU;
  if (s_page->enter != NULL)
  {
    s_page->enter();
  }
}

const ui_page_t *ui_router_get_page(void)
{
  return s_page;
}

void ui_router_set_page(const ui_page_t *page)
{
  if (page == NULL)
  {
    return;
  }

  if ((s_page != NULL) && (s_page->exit != NULL))
  {
    s_page->exit();
  }

  s_page = page;

  if (s_page->enter != NULL)
  {
    s_page->enter();
  }
}

bool ui_router_handle_event(ui_evt_t evt, ui_router_action_t *out_action, uint8_t *out_handled)
{
  bool render = false;
  bool handled = false;

  if (out_action != NULL)
  {
    *out_action = UI_ROUTER_ACTION_NONE;
  }
  if (out_handled != NULL)
  {
    *out_handled = 0U;
  }

  if (s_page == &PAGE_MENU)
  {
    ui_menu_stack_entry_t *active = ui_menu_active();
    if ((active == NULL) || (active->menu == NULL) ||
        (active->menu->items == NULL) || (active->menu->count == 0U))
    {
      return false;
    }

    uint8_t count = active->menu->count;
    active->index = ui_menu_clamp_index(active->menu, active->index);

    if ((evt == UI_EVT_NAV_UP) || (evt == UI_EVT_NAV_LEFT) || (evt == UI_EVT_DEC))
    {
      if (count > 1U)
      {
        if (active->index == 0U)
        {
          active->index = (uint8_t)(count - 1U);
        }
        else
        {
          active->index--;
        }
        render = true;
        handled = true;
      }
    }
    else if ((evt == UI_EVT_NAV_DOWN) || (evt == UI_EVT_NAV_RIGHT) || (evt == UI_EVT_INC))
    {
      if (count > 1U)
      {
        active->index = (uint8_t)((active->index + 1U) % count);
        render = true;
        handled = true;
      }
    }
    else if (evt == UI_EVT_SELECT)
    {
      const ui_menu_item_t *item = &active->menu->items[active->index];
      if (item->type == UI_MENU_ITEM_PAGE)
      {
        if (item->target.page != NULL)
        {
          ui_router_set_page(item->target.page);
          render = true;
          handled = true;
        }
      }
      else if (item->type == UI_MENU_ITEM_SUBMENU)
      {
        ui_menu_push(item->target.submenu);
        render = true;
        handled = true;
      }
      else if (item->type == UI_MENU_ITEM_COMMAND)
      {
        if (out_action != NULL)
        {
          if (item->target.cmd == UI_MENU_CMD_START_RENDER_DEMO)
          {
            *out_action = UI_ROUTER_ACTION_START_RENDER_DEMO;
            handled = true;
          }
          else if (item->target.cmd == UI_MENU_CMD_SAVE_EXIT)
          {
            *out_action = UI_ROUTER_ACTION_SAVE_EXIT;
            handled = true;
          }
        }
      }
    }
    else if (evt == UI_EVT_BACK)
    {
      if (s_menu_depth > 1U)
      {
        s_menu_depth--;
        render = true;
        handled = true;
      }
    }
  }
  else if ((s_page != NULL) && (s_page->event != NULL))
  {
    uint32_t result = s_page->event(evt);
    if ((result & UI_PAGE_EVENT_BACK) != 0U)
    {
      ui_router_set_page(&PAGE_MENU);
      render = true;
      handled = true;
    }
    if ((result & UI_PAGE_EVENT_RENDER) != 0U)
    {
      render = true;
      handled = true;
    }
    if ((result & UI_PAGE_EVENT_HANDLED) != 0U)
    {
      handled = true;
    }
  }

  if (out_handled != NULL)
  {
    *out_handled = handled ? 1U : 0U;
  }

  return render;
}

void ui_router_render(void)
{
  if ((s_page != NULL) && (s_page->render != NULL))
  {
    s_page->render();
  }
}

void ui_router_get_menu_state(ui_router_menu_state_t *out_state)
{
  if (out_state == NULL)
  {
    return;
  }

  ui_menu_stack_entry_t *active = ui_menu_active();
  if (active == NULL)
  {
    out_state->menu = NULL;
    out_state->index = 0U;
    out_state->depth = 0U;
    return;
  }

  active->index = ui_menu_clamp_index(active->menu, active->index);
  out_state->menu = active->menu;
  out_state->index = active->index;
  out_state->depth = s_menu_depth;
}

bool ui_router_get_keyclick(void)
{
  return s_keyclick_enabled;
}

void ui_router_set_keyclick(bool enable)
{
  s_keyclick_enabled = enable;
}
