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

static const ui_menu_item_t k_menu_power_items[] =
{
  { "Battery Stats", UI_MENU_ITEM_PAGE, { .page = &PAGE_BATT_STATS } },
  { "Sleep Options", UI_MENU_ITEM_PAGE, { .page = &PAGE_SLEEP } },
  { "Set Time/Date", UI_MENU_ITEM_PAGE, { .page = &PAGE_RTC_SET } }
};

static const ui_menu_t k_menu_power =
{
  .title = "POWER",
  .items = k_menu_power_items,
  .count = (uint8_t)(sizeof(k_menu_power_items) / sizeof(k_menu_power_items[0]))
};

static const ui_menu_item_t k_menu_storage_items[] =
{
  { "Storage Test", UI_MENU_ITEM_PAGE, { .page = &PAGE_STORAGE } }
};

static const ui_menu_t k_menu_storage =
{
  .title = "STORAGE",
  .items = k_menu_storage_items,
  .count = (uint8_t)(sizeof(k_menu_storage_items) / sizeof(k_menu_storage_items[0]))
};

static const ui_menu_item_t k_menu_accel_items[] =
{
  { "IMU", UI_MENU_ITEM_PAGE, { .page = &PAGE_LIS2_IMU } },
  { "Step Counter", UI_MENU_ITEM_PAGE, { .page = &PAGE_LIS2_STEPS } }
};

static const ui_menu_t k_menu_accel =
{
  .title = "ACCEL",
  .items = k_menu_accel_items,
  .count = (uint8_t)(sizeof(k_menu_accel_items) / sizeof(k_menu_accel_items[0]))
};

static const ui_menu_item_t k_menu_root_items[] =
{
  { "Power", UI_MENU_ITEM_SUBMENU, { .submenu = &k_menu_power } },
  { "Storage", UI_MENU_ITEM_SUBMENU, { .submenu = &k_menu_storage } },
  { "Joystick", UI_MENU_ITEM_SUBMENU, { .submenu = &k_menu_joystick } },
  { "Accelerometer", UI_MENU_ITEM_SUBMENU, { .submenu = &k_menu_accel } },
  { "Sound Settings", UI_MENU_ITEM_PAGE, { .page = &PAGE_SOUND } },
  { "Save & Exit", UI_MENU_ITEM_COMMAND, { .cmd = UI_MENU_CMD_SAVE_EXIT } }
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
