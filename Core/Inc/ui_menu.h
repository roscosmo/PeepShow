#ifndef UI_MENU_H
#define UI_MENU_H

#include "ui_pages.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  UI_MENU_ITEM_PAGE = 0,
  UI_MENU_ITEM_SUBMENU = 1,
  UI_MENU_ITEM_COMMAND = 2
} ui_menu_item_type_t;

typedef enum
{
  UI_MENU_CMD_NONE = 0,
  UI_MENU_CMD_START_RENDER_DEMO = 1
} ui_menu_cmd_t;

struct ui_menu;

typedef struct
{
  const char *label;
  ui_menu_item_type_t type;
  union
  {
    const struct ui_menu *submenu;
    const ui_page_t *page;
    ui_menu_cmd_t cmd;
  } target;
} ui_menu_item_t;

typedef struct ui_menu
{
  const char *title;
  const ui_menu_item_t *items;
  uint8_t count;
} ui_menu_t;

const ui_menu_t *ui_menu_root(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_MENU_H */
