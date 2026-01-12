#include "ui_pages.h"

#include "display_renderer.h"
#include "font8x8_basic.h"
#include "sensor_task.h"
#include "ui_actions.h"

#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

static sensor_joy_status_t s_last_status;
static bool s_has_status = false;

static bool joy_status_changed(void)
{
  sensor_joy_status_t current;
  sensor_joy_get_status(&current);

  if (!s_has_status || (memcmp(&current, &s_last_status, sizeof(current)) != 0))
  {
    s_last_status = current;
    s_has_status = true;
    return true;
  }

  return false;
}

static void page_joy_target_enter(void)
{
  s_has_status = false;
}

static uint32_t page_joy_target_event(ui_evt_t evt)
{
  if (evt == UI_EVT_BACK)
  {
    return UI_PAGE_EVENT_BACK;
  }

  if (evt == UI_EVT_DEC)
  {
    ui_actions_send_sensor_req(APP_SENSOR_REQ_JOY_DZ_DEC);
    return UI_PAGE_EVENT_RENDER;
  }

  if (evt == UI_EVT_INC)
  {
    ui_actions_send_sensor_req(APP_SENSOR_REQ_JOY_DZ_INC);
    return UI_PAGE_EVENT_RENDER;
  }

  if (evt == UI_EVT_TICK)
  {
    if (joy_status_changed())
    {
      return UI_PAGE_EVENT_RENDER;
    }
  }

  return UI_PAGE_EVENT_NONE;
}

static void page_joy_target_render(void)
{
  sensor_joy_status_t status;
  sensor_joy_get_status(&status);

  uint16_t width = renderGetWidth();
  uint16_t height = renderGetHeight();

  renderFill(false);
  renderDrawText(4U, 4U, "JOY TARGET", RENDER_LAYER_UI, RENDER_STATE_BLACK);

  uint16_t top = (uint16_t)(FONT8X8_HEIGHT + 6U);
  uint16_t bottom = (uint16_t)(FONT8X8_HEIGHT * 2U + 12U);
  if (height <= (uint16_t)(top + bottom + 10U))
  {
    bottom = 10U;
  }

  uint16_t usable_h = 0U;
  if (height > (top + bottom))
  {
    usable_h = (uint16_t)(height - top - bottom);
  }
  else if (height > top)
  {
    usable_h = (uint16_t)(height - top);
  }

  uint16_t cy = (uint16_t)(top + (usable_h / 2U));
  uint16_t cx = (uint16_t)(width / 2U);
  uint16_t max_r = (width / 2U);
  uint16_t half_h = (uint16_t)(usable_h / 2U);
  if (half_h < max_r)
  {
    max_r = half_h;
  }
  uint16_t R = (max_r > 6U) ? (uint16_t)(max_r - 6U) : max_r;

  renderDrawLine((uint16_t)(cx - R), cy, (uint16_t)(cx + R), cy,
                 RENDER_LAYER_UI, RENDER_STATE_BLACK);
  renderDrawLine(cx, (uint16_t)(cy - R), cx, (uint16_t)(cy + R),
                 RENDER_LAYER_UI, RENDER_STATE_BLACK);

  float max_span = (status.sx_mT > status.sy_mT) ? status.sx_mT : status.sy_mT;
  if (max_span < 1e-3f)
  {
    max_span = 1.0f;
  }

  uint16_t rx = (uint16_t)((status.sx_mT / max_span) * (float)R);
  uint16_t ry = (uint16_t)((status.sy_mT / max_span) * (float)R);
  if (rx < 1U)
  {
    rx = 1U;
  }
  if (ry < 1U)
  {
    ry = 1U;
  }

  const uint8_t segments = 32U;
  float prev_x = (float)cx + (float)rx;
  float prev_y = (float)cy;
  for (uint8_t i = 1U; i <= segments; ++i)
  {
    float a = (2.0f * 3.14159265f * (float)i) / (float)segments;
    float x = (float)cx + cosf(a) * (float)rx;
    float y = (float)cy + sinf(a) * (float)ry;
    renderDrawLine((uint16_t)prev_x, (uint16_t)prev_y,
                   (uint16_t)x, (uint16_t)y,
                   RENDER_LAYER_UI, RENDER_STATE_BLACK);
    prev_x = x;
    prev_y = y;
  }

  if (status.thr_mT > 0.0f)
  {
    float r_thr = (status.thr_mT / max_span) * (float)R;
    if (r_thr > (float)R)
    {
      r_thr = (float)R;
    }
    renderDrawCircle(cx, cy, (uint16_t)r_thr, RENDER_LAYER_UI, RENDER_STATE_BLACK);
  }

  if ((status.deadzone_en != 0U) && (status.deadzone_mT > 0.0f))
  {
    float r_dz = (status.deadzone_mT / max_span) * (float)R;
    if (r_dz > (float)R)
    {
      r_dz = (float)R;
    }
    renderDrawCircle(cx, cy, (uint16_t)r_dz, RENDER_LAYER_UI, RENDER_STATE_BLACK);
  }

  float scale_x = (status.sx_mT / max_span) * (float)R;
  float scale_y = (status.sy_mT / max_span) * (float)R;
  if (scale_x < 1.0f)
  {
    scale_x = 1.0f;
  }
  if (scale_y < 1.0f)
  {
    scale_y = 1.0f;
  }
  int16_t px = (int16_t)((int32_t)cx + (int32_t)(status.nx * scale_x));
  int16_t py = (int16_t)((int32_t)cy - (int32_t)(status.ny * scale_y));
  renderFillCircle((uint16_t)px, (uint16_t)py, 2U,
                   RENDER_LAYER_GAME, RENDER_STATE_BLACK);

  uint16_t text_y1 = (uint16_t)(height - (FONT8X8_HEIGHT * 2U + 2U));
  uint16_t text_y2 = (uint16_t)(height - (FONT8X8_HEIGHT + 2U));
  char buf[32];
  (void)snprintf(buf, sizeof(buf), "r=%.1fmT", status.r_abs_mT);
  renderDrawText(6U, text_y1, buf, RENDER_LAYER_UI, RENDER_STATE_BLACK);
  renderDrawText((uint16_t)(width / 2U), text_y1, "L/R: DEADZONE",
                 RENDER_LAYER_UI, RENDER_STATE_BLACK);
  (void)snprintf(buf, sizeof(buf), "thr=%.1fmT", status.thr_mT);
  renderDrawText(6U, text_y2, buf, RENDER_LAYER_UI, RENDER_STATE_BLACK);
  (void)snprintf(buf, sizeof(buf), "dz=%.1fmT", status.deadzone_mT);
  renderDrawText((uint16_t)(width / 2U), text_y2, buf,
                 RENDER_LAYER_UI, RENDER_STATE_BLACK);
}

const ui_page_t PAGE_JOY_TARGET =
{
  .name = "Joy Target",
  .enter = page_joy_target_enter,
  .event = page_joy_target_event,
  .render = page_joy_target_render,
  .exit = NULL,
  .tick_ms = 100U,
  .flags = UI_PAGE_FLAG_JOY_MONITOR
};
