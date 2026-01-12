#include "ui_pages.h"

#include "cmsis_os2.h"
#include "display_renderer.h"
#include "sensor_task.h"
#include "font8x8_basic.h"

#include <stdbool.h>
#include <math.h>

typedef struct
{
  float x;
  float y;
  uint16_t draw_x;
  uint16_t draw_y;
  uint32_t last_ms;
} ui_cursor_state_t;

static ui_cursor_state_t s_cursor;
static const float kCursorSpeedPxPerS = 120.0f;
static const uint16_t kCursorSpriteW = 16U;
static const uint16_t kCursorSpriteH = 16U;
static const uint8_t kCursorSprite[] =
{
	0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x00, 0x7c, 0x00, 0x7e, 0x00, 
	0x7f, 0x00, 0x7f, 0x80, 0x7c, 0x00, 0x6c, 0x00, 0x44, 0x00, 0x06, 0x00, 0x02, 0x00, 0x00, 0x00
};
// {
//   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x8F, 0xC3, 0x80, 0x01, 0xE2, 0xF3, 0x80,
//   0x01, 0xED, 0x3C, 0xC0, 0x01, 0xDF, 0x38, 0xC0, 0x00, 0x84, 0xF8, 0x80, 0x00, 0xDF, 0xFE, 0x80,
//   0x00, 0xFF, 0xFF, 0x00, 0x01, 0xBF, 0xFF, 0x00, 0x02, 0x00, 0x3F, 0x80, 0x06, 0x7F, 0xDF, 0xC0,
//   0x0F, 0xFF, 0xFF, 0xE0, 0x03, 0xC2, 0xE7, 0xC0, 0x03, 0xB3, 0xB3, 0x80, 0x03, 0xF3, 0xF8, 0x80,
//   0x07, 0xE1, 0xFC, 0xFC, 0x36, 0xC8, 0xFC, 0xFC, 0x3E, 0x08, 0x31, 0xCC, 0x1B, 0x00, 0x07, 0xE4,
//   0x09, 0xC0, 0x1F, 0x76, 0x07, 0xFF, 0xFB, 0x9C, 0x03, 0xFF, 0xFD, 0xDC, 0x01, 0x9F, 0x9D, 0xEC,
//   0x01, 0xFF, 0xBF, 0xF8, 0x01, 0xFB, 0xFB, 0xB0, 0x01, 0xFB, 0xFF, 0xE0, 0x00, 0xFF, 0xFF, 0xC0,
//   0x00, 0xFF, 0xFE, 0x00, 0x00, 0x6F, 0xCC, 0x00, 0x00, 0x7C, 0xFC, 0x00, 0x00, 0x10, 0x30, 0x00
// };

static void page_joy_cursor_enter(void)
{
  uint16_t width = renderGetWidth();
  uint16_t height = renderGetHeight();
  float start_x = 0.0f;
  float start_y = 0.0f;

  if (width > kCursorSpriteW)
  {
    start_x = (float)((width - kCursorSpriteW) / 2U);
  }
  if (height > kCursorSpriteH)
  {
    start_y = (float)((height - kCursorSpriteH) / 2U);
  }

  s_cursor.x = start_x;
  s_cursor.y = start_y;
  s_cursor.draw_x = (uint16_t)(start_x + 0.5f);
  s_cursor.draw_y = (uint16_t)(start_y + 0.5f);
  s_cursor.last_ms = 0U;
}

static bool page_joy_cursor_update(const sensor_joy_status_t *status, uint32_t now_ms)
{
  if (status == NULL)
  {
    return false;
  }

  uint16_t width = renderGetWidth();
  uint16_t height = renderGetHeight();
  if ((width == 0U) || (height == 0U))
  {
    return false;
  }

  uint32_t prev_ms = s_cursor.last_ms;
  if (prev_ms == 0U)
  {
    prev_ms = now_ms;
  }
  s_cursor.last_ms = now_ms;

  float dt_s = (float)(now_ms - prev_ms) * (1.0f / 1000.0f);
  if (dt_s <= 0.0f)
  {
    return false;
  }

  float min_span = (status->sx_mT < status->sy_mT) ? status->sx_mT : status->sy_mT;
  if (min_span < 1e-3f)
  {
    min_span = 1.0f;
  }

  float speed_norm = status->r_abs_mT / min_span;
  if (speed_norm > 1.0f)
  {
    speed_norm = 1.0f;
  }
  if ((status->deadzone_en != 0U) && (status->r_abs_mT < status->deadzone_mT))
  {
    speed_norm = 0.0f;
  }

  float mag = sqrtf(status->nx * status->nx + status->ny * status->ny);
  float dirx = (mag > 1e-6f) ? (status->nx / mag) : 0.0f;
  float diry = (mag > 1e-6f) ? (status->ny / mag) : 0.0f;

  float fx = s_cursor.x + dirx * speed_norm * kCursorSpeedPxPerS * dt_s;
  float fy = s_cursor.y - diry * speed_norm * kCursorSpeedPxPerS * dt_s;

  float max_x = (width > kCursorSpriteW) ? (float)(width - kCursorSpriteW) : 0.0f;
  float max_y = (height > kCursorSpriteH) ? (float)(height - kCursorSpriteH) : 0.0f;
  if (fx < 0.0f)
  {
    fx = 0.0f;
  }
  else if (fx > max_x)
  {
    fx = max_x;
  }
  if (fy < 0.0f)
  {
    fy = 0.0f;
  }
  else if (fy > max_y)
  {
    fy = max_y;
  }

  s_cursor.x = fx;
  s_cursor.y = fy;

  uint16_t draw_x = (uint16_t)(fx + 0.5f);
  uint16_t draw_y = (uint16_t)(fy + 0.5f);
  if ((draw_x != s_cursor.draw_x) || (draw_y != s_cursor.draw_y))
  {
    s_cursor.draw_x = draw_x;
    s_cursor.draw_y = draw_y;
    return true;
  }

  return false;
}

static uint32_t page_joy_cursor_event(ui_evt_t evt)
{
  if (evt == UI_EVT_BACK)
  {
    return UI_PAGE_EVENT_BACK;
  }

  if (evt == UI_EVT_TICK)
  {
    sensor_joy_status_t status_now;
    sensor_joy_get_status(&status_now);
    if (page_joy_cursor_update(&status_now, osKernelGetTickCount()))
    {
      return UI_PAGE_EVENT_RENDER;
    }
  }

  return UI_PAGE_EVENT_NONE;
}

static void page_joy_cursor_render(void)
{
  uint16_t width = renderGetWidth();
  uint16_t height = renderGetHeight();

  renderFill(false);
  renderDrawRect(0U, 0U, width, height, RENDER_LAYER_UI, RENDER_STATE_BLACK);
  renderDrawText(4U, 4U, "JOY CURSOR", RENDER_LAYER_UI, RENDER_STATE_BLACK);
  if (height > (uint16_t)(FONT8X8_HEIGHT + 6U))
  {
    uint16_t hint_y = (uint16_t)(height - (FONT8X8_HEIGHT + 2U));
    renderDrawText(4U, hint_y, "B: BACK", RENDER_LAYER_UI, RENDER_STATE_BLACK);
  }

  renderBlit1bppMsb(s_cursor.draw_x, s_cursor.draw_y,
                    kCursorSpriteW, kCursorSpriteH,
                    kCursorSprite, 0U,
                    RENDER_LAYER_GAME, RENDER_STATE_BLACK);
}

const ui_page_t PAGE_JOY_CURSOR =
{
  .name = "Joy Cursor",
  .enter = page_joy_cursor_enter,
  .event = page_joy_cursor_event,
  .render = page_joy_cursor_render,
  .exit = NULL,
  .tick_ms = 100U,
  .flags = UI_PAGE_FLAG_JOY_MONITOR
};
