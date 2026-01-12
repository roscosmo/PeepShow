#ifndef UI_ROUTER_H
#define UI_ROUTER_H

#include "ui_menu.h"
#include "ui_pages.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  UI_ROUTER_ACTION_NONE = 0,
  UI_ROUTER_ACTION_START_RENDER_DEMO = 1,
  UI_ROUTER_ACTION_EXIT_MENU = 2
} ui_router_action_t;

typedef struct
{
  const ui_menu_t *menu;
  uint8_t index;
  uint8_t depth;
} ui_router_menu_state_t;

void ui_router_init(void);
const ui_page_t *ui_router_get_page(void);
void ui_router_set_page(const ui_page_t *page);
bool ui_router_handle_event(ui_evt_t evt, ui_router_action_t *out_action);
void ui_router_render(void);
void ui_router_get_menu_state(ui_router_menu_state_t *out_state);
bool ui_router_get_keyclick(void);
void ui_router_set_keyclick(bool enable);

#ifdef __cplusplus
}
#endif

#endif /* UI_ROUTER_H */
