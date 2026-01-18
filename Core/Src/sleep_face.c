#include "sleep_face.h"

#include "display_renderer.h"

#include <math.h>
#include <stdint.h>

#ifndef SLEEP_FACE_PI
#define SLEEP_FACE_PI 3.14159265358979323846f
#endif

static inline int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi)
{
  if (v < lo)
  {
    return lo;
  }
  if (v > hi)
  {
    return hi;
  }
  return v;
}

static void sleep_face_draw_analog_clock(const power_rtc_datetime_t *dt)
{
  uint16_t width = renderGetWidth();
  uint16_t height = renderGetHeight();
  if ((width == 0U) || (height == 0U))
  {
    return;
  }

  int32_t cx = (int32_t)width / 2;
  int32_t cy = (int32_t)height / 2;

  uint16_t min_dim = (width < height) ? width : height;
  uint16_t radius = (min_dim > 10U) ? (uint16_t)((min_dim / 2U) - 4U) : (uint16_t)(min_dim / 2U);

  renderDrawCircleThick((uint16_t)cx, (uint16_t)cy, radius, 2U, RENDER_LAYER_UI, RENDER_STATE_BLACK);

  /* 12 hour ticks */
  for (uint32_t h = 0U; h < 12U; h++)
  {
    float a = (((float)h) / 12.0f) * (2.0f * SLEEP_FACE_PI) - (SLEEP_FACE_PI / 2.0f);
    float ca = cosf(a);
    float sa = sinf(a);

    uint16_t outer = radius;
    uint16_t inner = (radius > 6U) ? (uint16_t)(radius - 6U) : 0U;

    int32_t x0 = cx + (int32_t)lroundf(ca * (float)inner);
    int32_t y0 = cy + (int32_t)lroundf(sa * (float)inner);
    int32_t x1 = cx + (int32_t)lroundf(ca * (float)outer);
    int32_t y1 = cy + (int32_t)lroundf(sa * (float)outer);

    x0 = clamp_i32(x0, 0, (int32_t)width - 1);
    y0 = clamp_i32(y0, 0, (int32_t)height - 1);
    x1 = clamp_i32(x1, 0, (int32_t)width - 1);
    y1 = clamp_i32(y1, 0, (int32_t)height - 1);

    renderDrawLineThick((uint16_t)x0, (uint16_t)y0, (uint16_t)x1, (uint16_t)y1, 2U,
                        RENDER_LAYER_UI, RENDER_STATE_BLACK);
  }

  /* Hands */
  uint32_t sec_u = (uint32_t)dt->seconds;
  uint32_t min_u = (uint32_t)dt->minutes;
  uint32_t hour_u = (uint32_t)dt->hours;

  float sec = (float)(sec_u % 60U);
  float min = (float)(min_u % 60U) + (sec / 60.0f);
  float hour = (float)(hour_u % 12U) + (min / 60.0f);

  float a_sec = (sec / 60.0f) * (2.0f * SLEEP_FACE_PI) - (SLEEP_FACE_PI / 2.0f);
  float a_min = (min / 60.0f) * (2.0f * SLEEP_FACE_PI) - (SLEEP_FACE_PI / 2.0f);
  float a_hr  = (hour / 12.0f) * (2.0f * SLEEP_FACE_PI) - (SLEEP_FACE_PI / 2.0f);

  uint16_t len_sec = (radius > 6U) ? (uint16_t)(radius - 6U) : radius;
  uint16_t len_min = (radius > 12U) ? (uint16_t)(radius - 12U) : radius;
  uint16_t len_hr  = (radius > 20U) ? (uint16_t)(radius - 20U) : radius;

  /* Hour hand (thickest) */
  {
    int32_t x1 = cx + (int32_t)lroundf(cosf(a_hr) * (float)len_hr);
    int32_t y1 = cy + (int32_t)lroundf(sinf(a_hr) * (float)len_hr);
    x1 = clamp_i32(x1, 0, (int32_t)width - 1);
    y1 = clamp_i32(y1, 0, (int32_t)height - 1);
    renderDrawLineThick((uint16_t)cx, (uint16_t)cy, (uint16_t)x1, (uint16_t)y1, 3U,
                        RENDER_LAYER_UI, RENDER_STATE_BLACK);
  }

  /* Minute hand */
  {
    int32_t x1 = cx + (int32_t)lroundf(cosf(a_min) * (float)len_min);
    int32_t y1 = cy + (int32_t)lroundf(sinf(a_min) * (float)len_min);
    x1 = clamp_i32(x1, 0, (int32_t)width - 1);
    y1 = clamp_i32(y1, 0, (int32_t)height - 1);
    renderDrawLineThick((uint16_t)cx, (uint16_t)cy, (uint16_t)x1, (uint16_t)y1, 2U,
                        RENDER_LAYER_UI, RENDER_STATE_BLACK);
  }

  /* Second hand (thin) */
  {
    int32_t x1 = cx + (int32_t)lroundf(cosf(a_sec) * (float)len_sec);
    int32_t y1 = cy + (int32_t)lroundf(sinf(a_sec) * (float)len_sec);
    x1 = clamp_i32(x1, 0, (int32_t)width - 1);
    y1 = clamp_i32(y1, 0, (int32_t)height - 1);
    renderDrawLine((uint16_t)cx, (uint16_t)cy, (uint16_t)x1, (uint16_t)y1,
                   RENDER_LAYER_UI, RENDER_STATE_BLACK);
  }

  /* Center cap */
  renderFillCircle((uint16_t)cx, (uint16_t)cy, 2U, RENDER_LAYER_UI, RENDER_STATE_BLACK);
}

void sleep_face_render(const power_rtc_datetime_t *dt)
{
  if (dt == NULL)
  {
    return;
  }

  renderFill(false);
  sleep_face_draw_analog_clock(dt);
}
