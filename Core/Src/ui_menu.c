#include "ui_menu.h"

static const ui_menu_item_t k_menu_joystick_items[] =
{
  { "Calibrate", UI_MENU_ITEM_PAGE, { .page = &PAGE_JOY_CAL } },
  { "Target", UI_MENU_ITEM_PAGE, { .page = &PAGE_JOY_TARGET } },
  { "Cursor", UI_MENU_ITEM_PAGE, { .page = &PAGE_JOY_CURSOR } },
  { "Menu Input", UI_MENU_ITEM_PAGE, { .page = &PAGE_MENU_INPUT } }
};

static const ui_menu_t k_menu_joystick =
{
  .title = "JOYSTICK",
  .items = k_menu_joystick_items,
  .count = (uint8_t)(sizeof(k_menu_joystick_items) / sizeof(k_menu_joystick_items[0]))
};

static const ui_menu_item_t k_menu_root_items[] =
{
  { "Joystick", UI_MENU_ITEM_SUBMENU, { .submenu = &k_menu_joystick } },
  { "Render Demo", UI_MENU_ITEM_COMMAND, { .cmd = UI_MENU_CMD_START_RENDER_DEMO } },
  { "Sound Settings", UI_MENU_ITEM_PAGE, { .page = &PAGE_SOUND } }
};

static const ui_menu_t k_menu_root =
{
  .title = "MENU",
  .items = k_menu_root_items,
  .count = (uint8_t)(sizeof(k_menu_root_items) / sizeof(k_menu_root_items[0]))
};

const ui_menu_t *ui_menu_root(void)
{
  return &k_menu_root;
}
