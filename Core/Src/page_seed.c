#include "ui_pages.h"

#include "display_renderer.h"
#include "font8x8_basic.h"
#include "storage_task.h"

static uint32_t page_seed_event(ui_evt_t evt)
{
  if (evt == UI_EVT_TICK)
  {
    return UI_PAGE_EVENT_RENDER;
  }

  return UI_PAGE_EVENT_NONE;
}

static void page_seed_render(void)
{
  renderFill(false);
  renderDrawText(4U, 4U, "AUDIO SEED", RENDER_LAYER_UI, RENDER_STATE_BLACK);

  storage_seed_state_t state = storage_get_seed_state();
  const char *status = "IDLE";
  if (state == STORAGE_SEED_ACTIVE)
  {
    status = "SEEDING...";
  }
  else if (state == STORAGE_SEED_DONE)
  {
    status = "TRANSFER COMPLETE";
  }
  else if (state == STORAGE_SEED_ERROR)
  {
    status = "TRANSFER ERROR";
  }

  renderDrawText(4U, (uint16_t)(FONT8X8_HEIGHT + 8U),
                 status, RENDER_LAYER_UI, RENDER_STATE_BLACK);
}

const ui_page_t PAGE_SEED_AUDIO =
{
  .name = "Seed Audio",
  .enter = NULL,
  .event = page_seed_event,
  .render = page_seed_render,
  .exit = NULL,
  .tick_ms = 250U,
  .flags = UI_PAGE_FLAG_JOY_MENU
};
