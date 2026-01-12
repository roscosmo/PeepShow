#include "ui_pages.h"

#include "display_renderer.h"

static void page_home_render(void)
{
  renderFill(false);
  renderDrawText(4U, 4U, "HOME", RENDER_LAYER_UI, RENDER_STATE_BLACK);
  renderDrawText(4U, 16U, "BOOT: MENU", RENDER_LAYER_UI, RENDER_STATE_BLACK);
}

const ui_page_t PAGE_HOME =
{
  .name = "Home",
  .enter = NULL,
  .event = NULL,
  .render = page_home_render,
  .exit = NULL,
  .tick_ms = 0U,
  .flags = 0U
};
