#ifndef UI_ROUTER_H
#define UI_ROUTER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  UI_PAGE_MENU = 0,
  UI_PAGE_HOME = 1,
  UI_PAGE_JOY_CAL = 2,
  UI_PAGE_JOY_TARGET = 3,
  UI_PAGE_JOY_CURSOR = 4,
  UI_PAGE_SOUND = 5
} ui_page_t;

typedef enum
{
  UI_ROUTER_CMD_NONE = 0,
  UI_ROUTER_CMD_START_RENDER_DEMO = 1,
  UI_ROUTER_CMD_OPEN_JOY_CAL = 2,
  UI_ROUTER_CMD_OPEN_JOY_TARGET = 3,
  UI_ROUTER_CMD_OPEN_JOY_CURSOR = 4,
  UI_ROUTER_CMD_TOGGLE_KEYCLICK = 5,
  UI_ROUTER_CMD_OPEN_SOUND = 6
} ui_router_cmd_t;

void ui_router_init(void);
ui_page_t ui_router_get_page(void);
void ui_router_set_page(ui_page_t page);
bool ui_router_handle_button(uint32_t button_id, ui_router_cmd_t *out_cmd);
void ui_router_render(void);
void ui_router_set_joy_cursor(uint16_t x, uint16_t y);
bool ui_router_get_keyclick(void);
void ui_router_set_keyclick(bool enable);

#ifdef __cplusplus
}
#endif

#endif /* UI_ROUTER_H */
