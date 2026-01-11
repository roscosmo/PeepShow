#include "render_demo.h"

#include "display_renderer.h"
#include "font8x8_basic.h"

#include "cmsis_os2.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define UI_BAR_H   14U
#define EDGE_THICK 5U

#define PATTERN_W 20U
#define PATTERN_H 34U

static const uint8_t pattern202thread0n[] =
{
  0xD5U, 0x62U, 0x20U, 0xAAU, 0xE8U, 0x80U, 0xD5U, 0x62U, 0x20U, 0xAAU, 0xE8U, 0x80U, 0xD5U, 0x62U, 0x20U, 0xAAU,
  0xE8U, 0x80U, 0xD5U, 0xF2U, 0x20U, 0xABU, 0xFCU, 0x80U, 0xDFU, 0x0FU, 0x20U, 0xBCU, 0x03U, 0xC0U, 0xF0U, 0x00U,
  0xF0U, 0xC0U, 0x00U, 0x30U, 0xB0U, 0x00U, 0xF0U, 0x8CU, 0x03U, 0x50U, 0xA3U, 0x0EU, 0xB0U, 0x88U, 0xF5U, 0x50U,
  0xA2U, 0x2AU, 0xB0U, 0x88U, 0xB5U, 0x50U, 0xA2U, 0x2AU, 0xB0U, 0x88U, 0xB5U, 0x50U, 0xA2U, 0x2AU, 0xB0U, 0x88U,
  0xB5U, 0x50U, 0xA2U, 0x2AU, 0xB0U, 0xC8U, 0xB5U, 0x70U, 0xF2U, 0x2AU, 0xF0U, 0x3CU, 0xB7U, 0xC0U, 0x0FU, 0x2FU,
  0x00U, 0x03U, 0xFCU, 0x00U, 0x00U, 0xF0U, 0x00U, 0x03U, 0xECU, 0x00U, 0x0DU, 0x63U, 0x00U, 0x3AU, 0xE8U, 0xC0U,
  0xD5U, 0x62U, 0x30U, 0xAAU, 0xE8U, 0x80U
};

static const uint8_t kUiSprite8x8[] =
{
  0x3CU, 0x42U, 0xA5U, 0x81U, 0xA5U, 0x99U, 0x42U, 0x3CU
};

typedef struct { float x, y, z; } vec3_t;
typedef struct { int16_t x, y; } pt2_t;

static const vec3_t kCubeV[8] =
{
  {-0.6f, -0.6f, -0.6f}, {+0.6f, -0.6f, -0.6f},
  {+0.6f, +0.6f, -0.6f}, {-0.6f, +0.6f, -0.6f},
  {-0.6f, -0.6f, +0.6f}, {+0.6f, -0.6f, +0.6f},
  {+0.6f, +0.6f, +0.6f}, {-0.6f, +0.6f, +0.6f}
};

static const uint8_t kCubeE[12][2] =
{
  {0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 0U},
  {4U, 5U}, {5U, 6U}, {6U, 7U}, {7U, 4U},
  {0U, 4U}, {1U, 5U}, {2U, 6U}, {3U, 7U}
};

typedef struct
{
  bool initialized;
  uint16_t width;
  uint16_t height;
  uint16_t ui_bar_h;
  uint16_t game_y0;
  uint16_t game_y1;
  float ay;
  float ax;
  uint32_t frame_id;
  uint32_t scroll_x;
  uint32_t scroll_y;
  uint32_t fps;
  uint32_t fps_ms_acc;
  uint32_t fps_frames;
  uint32_t boot_ms;
  uint32_t last_frame_ms;
  bool bg_enabled;
  bool cube_enabled;
} render_demo_state_t;

static render_demo_state_t s_demo =
{
  .initialized = false,
  .bg_enabled = true,
  .cube_enabled = true
};

static render_demo_mode_t s_mode = RENDER_DEMO_MODE_IDLE;

static int32_t iroundf(float x)
{
  return (int32_t)(x >= 0.0f ? x + 0.5f : x - 0.5f);
}

static char *u32_to_dec(char *dst, uint32_t v)
{
  char tmp[11];
  int32_t n = 0;
  do
  {
    tmp[n++] = (char)('0' + (v % 10U));
    v /= 10U;
  } while (v != 0U);

  while (n-- > 0)
  {
    *dst++ = tmp[n];
  }
  *dst = '\0';
  return dst;
}

static char *u2d(char *dst, uint32_t v)
{
  *dst++ = (char)('0' + ((v / 10U) % 10U));
  *dst++ = (char)('0' + (v % 10U));
  *dst = '\0';
  return dst;
}

static char *mv_to_vstr(char *dst, int32_t mv)
{
  if (mv < 0)
  {
    *dst++ = '-';
    mv = -mv;
  }
  uint32_t v100 = (uint32_t)((mv + 5) / 10);
  uint32_t ip = v100 / 100U;
  uint32_t fp = v100 % 100U;
  dst = u32_to_dec(dst, ip);
  *dst++ = '.';
  dst = u2d(dst, fp);
  *dst++ = 'V';
  *dst = '\0';
  return dst;
}

static uint16_t text_width(const char *text)
{
  if (text == NULL)
  {
    return 0U;
  }

  size_t len = strlen(text);
  if (len == 0U)
  {
    return 0U;
  }

  return (uint16_t)((len * (FONT8X8_WIDTH + 1U)) - 1U);
}

static render_state_t cube_blink_color(uint32_t frame_id)
{
  return ((frame_id & 1U) != 0U) ? RENDER_STATE_WHITE : RENDER_STATE_BLACK;
}

static void draw_ui_top(uint16_t width, uint32_t fps, int32_t batt_mv)
{
  if (s_demo.ui_bar_h == 0U)
  {
    return;
  }

  renderFillRect(0U, 0U, width, s_demo.ui_bar_h, RENDER_LAYER_UI, RENDER_STATE_BLACK);
  renderBlit1bpp(2U, 3U, 8U, 8U, kUiSprite8x8, 0U, RENDER_LAYER_UI, RENDER_STATE_WHITE);
  renderDrawText(14U, 3U, "UI", RENDER_LAYER_UI, RENDER_STATE_WHITE);

  char buf[40];
  char *p = buf;
  *p++ = 'F';
  *p++ = 'P';
  *p++ = 'S';
  *p++ = ':';
  *p++ = ' ';
  p = u32_to_dec(p, fps);
  *p++ = ' ';
  *p++ = ' ';
  *p++ = 'V';
  *p++ = ':';
  *p++ = ' ';
  p = mv_to_vstr(p, batt_mv);
  *p = '\0';

  uint16_t w = text_width(buf);
  uint16_t x = (width > (uint16_t)(w + 4U)) ? (uint16_t)(width - w - 4U) : 2U;
  renderDrawText(x, 3U, buf, RENDER_LAYER_UI, RENDER_STATE_WHITE);
}

static void draw_ui_bottom(uint16_t width, uint16_t height, uint32_t uptime_s)
{
  if (s_demo.ui_bar_h == 0U)
  {
    return;
  }

  uint16_t y0 = (uint16_t)(height - s_demo.ui_bar_h);
  renderFillRect(0U, y0, width, s_demo.ui_bar_h, RENDER_LAYER_UI, RENDER_STATE_BLACK);
  renderDrawText(4U, (uint16_t)(y0 + 3U), "SYS", RENDER_LAYER_UI, RENDER_STATE_WHITE);

  uint32_t ss = uptime_s % 60U;
  uint32_t mm = (uptime_s / 60U) % 60U;
  uint32_t hh = (uptime_s / 3600U) % 100U;
  char buf[24];
  char *p = buf;
  *p++ = 'U';
  *p++ = 'P';
  *p++ = ':';
  *p++ = ' ';
  p = u2d(p, hh);
  *p++ = ':';
  p = u2d(p, mm);
  *p++ = ':';
  p = u2d(p, ss);
  *p = '\0';

  uint16_t w = text_width(buf);
  uint16_t x = (width > (uint16_t)(w + 4U)) ? (uint16_t)(width - w - 4U) : 2U;
  renderDrawText(x, (uint16_t)(y0 + 3U), buf, RENDER_LAYER_UI, RENDER_STATE_WHITE);
}

static void draw_bg_span(uint16_t x, uint16_t y, uint16_t w, render_state_t color)
{
  if (w == 0U)
  {
    return;
  }
  renderFillRect(x, y, w, 1U, RENDER_LAYER_BG, color);
}

static void draw_bg_bitmap_tiled_scrolled(const uint8_t *bmp,
                                          uint16_t bw, uint16_t bh,
                                          uint32_t ox, uint32_t oy,
                                          uint16_t width,
                                          uint16_t game_y0, uint16_t game_y1)
{
  if ((bmp == NULL) || (bw == 0U) || (bh == 0U) || (width == 0U))
  {
    return;
  }

  if (game_y1 < game_y0)
  {
    return;
  }

  uint16_t stride = (uint16_t)((bw + 7U) >> 3U);
  uint16_t oxm = (uint16_t)(ox % bw);
  uint16_t oym = (uint16_t)(oy % bh);

  for (uint16_t y = game_y0; y <= game_y1; ++y)
  {
    uint16_t sy = (uint16_t)((y - game_y0 + oym) % bh);
    const uint8_t *row = bmp + ((uint32_t)sy * stride);

    uint16_t sxi = oxm;
    uint16_t span_x0 = 0U;
    uint8_t byte = row[sxi >> 3U];
    uint8_t bit = (uint8_t)(0x80U >> (sxi & 7U));
    render_state_t cur = ((byte & bit) != 0U) ? RENDER_STATE_BLACK : RENDER_STATE_WHITE;

    for (uint16_t x = 0U; x < width; ++x)
    {
      byte = row[sxi >> 3U];
      bit = (uint8_t)(0x80U >> (sxi & 7U));
      render_state_t px = ((byte & bit) != 0U) ? RENDER_STATE_BLACK : RENDER_STATE_WHITE;

      if (x == 0U)
      {
        cur = px;
        span_x0 = 0U;
      }
      else if (px != cur)
      {
        draw_bg_span(span_x0, y, (uint16_t)(x - span_x0), cur);
        cur = px;
        span_x0 = x;
      }

      sxi++;
      if (sxi == bw)
      {
        sxi = 0U;
      }
    }

    if (span_x0 < width)
    {
      draw_bg_span(span_x0, y, (uint16_t)(width - span_x0), cur);
    }
  }
}

static void rot_yx(const vec3_t *v, float cy, float sy, float cx, float sx, vec3_t *o)
{
  float xx = v->x * cy + v->z * sy;
  float zz = -v->x * sy + v->z * cy;
  o->x = xx;
  o->y = v->y * cx - zz * sx;
  o->z = v->y * sx + zz * cx;
}

static void project_points(const vec3_t *vin, pt2_t *out, uint8_t n,
                           uint16_t width, uint16_t game_y0, uint16_t game_y1)
{
  const float z_off = 2.3f;
  const float nearz = 0.25f;
  const float f = 84.0f;
  const float cx = (float)(width / 2U);
  const float cy = (float)((game_y0 + game_y1) / 2U);

  for (uint8_t i = 0U; i < n; ++i)
  {
    float z = vin[i].z + z_off;
    if (z < nearz)
    {
      z = nearz;
    }
    float px = cx + (f * vin[i].x) / z;
    float py = cy - (f * vin[i].y) / z;
    out[i].x = (int16_t)iroundf(px);
    out[i].y = (int16_t)iroundf(py);
  }
}

static void draw_wire_cube(float ay, float ax, render_state_t edge_color)
{
  vec3_t r[8];
  pt2_t p[8];
  float cy = cosf(ay);
  float sy = sinf(ay);
  float cx = cosf(ax);
  float sx = sinf(ax);

  for (uint8_t i = 0U; i < 8U; ++i)
  {
    rot_yx(&kCubeV[i], cy, sy, cx, sx, &r[i]);
  }
  project_points(r, p, 8U, s_demo.width, s_demo.game_y0, s_demo.game_y1);

  int32_t width = (int32_t)s_demo.width;
  int32_t y0 = (int32_t)s_demo.game_y0;
  int32_t y1 = (int32_t)s_demo.game_y1;
  int32_t x_min = 0;
  int32_t x_max = (width > 0) ? (width - 1) : 0;
  uint16_t thick = EDGE_THICK;
  if (thick > 8U)
  {
    thick = 8U;
  }

  for (uint8_t e = 0U; e < 12U; ++e)
  {
    int32_t x0 = (int32_t)p[kCubeE[e][0U]].x;
    int32_t y0p = (int32_t)p[kCubeE[e][0U]].y;
    int32_t x1 = (int32_t)p[kCubeE[e][1U]].x;
    int32_t y1p = (int32_t)p[kCubeE[e][1U]].y;

    if ((x0 < 0 && x1 < 0) || (x0 >= width && x1 >= width))
    {
      continue;
    }
    if ((y0p < y0 && y1p < y0) || (y0p > y1 && y1p > y1))
    {
      continue;
    }

    if (x0 < x_min)
    {
      x0 = x_min;
    }
    else if (x0 > x_max)
    {
      x0 = x_max;
    }
    if (x1 < x_min)
    {
      x1 = x_min;
    }
    else if (x1 > x_max)
    {
      x1 = x_max;
    }
    if (y0p < y0)
    {
      y0p = y0;
    }
    else if (y0p > y1)
    {
      y0p = y1;
    }
    if (y1p < y0)
    {
      y1p = y0;
    }
    else if (y1p > y1)
    {
      y1p = y1;
    }

    renderDrawLineThick((uint16_t)x0, (uint16_t)y0p,
                        (uint16_t)x1, (uint16_t)y1p,
                        thick, RENDER_LAYER_GAME, edge_color);
  }
}

static void render_demo_init(uint16_t width, uint16_t height, uint32_t now)
{
  s_demo.width = width;
  s_demo.height = height;
  s_demo.ui_bar_h = (height > (UI_BAR_H * 2U + 1U)) ? UI_BAR_H : 0U;
  s_demo.game_y0 = s_demo.ui_bar_h;
  if (height > s_demo.ui_bar_h)
  {
    s_demo.game_y1 = (uint16_t)(height - s_demo.ui_bar_h - 1U);
  }
  else
  {
    s_demo.game_y1 = 0U;
  }
  if (s_demo.game_y1 < s_demo.game_y0)
  {
    s_demo.game_y0 = 0U;
    s_demo.game_y1 = (height > 0U) ? (uint16_t)(height - 1U) : 0U;
  }

  s_demo.ay = 0.0f;
  s_demo.ax = 0.0f;
  s_demo.frame_id = 0U;
  s_demo.scroll_x = 0U;
  s_demo.scroll_y = 0U;
  s_demo.fps = 0U;
  s_demo.fps_ms_acc = 0U;
  s_demo.fps_frames = 0U;
  s_demo.boot_ms = now;
  s_demo.last_frame_ms = now;
  s_demo.initialized = true;
}

void render_demo_reset(void)
{
  s_demo.initialized = false;
  s_demo.bg_enabled = true;
  s_demo.cube_enabled = true;
}

void render_demo_set_mode(render_demo_mode_t mode)
{
  if (mode == s_mode)
  {
    return;
  }

  s_mode = mode;
  if (mode != RENDER_DEMO_MODE_IDLE)
  {
    render_demo_reset();
  }
}

render_demo_mode_t render_demo_get_mode(void)
{
  return s_mode;
}

void render_demo_toggle_background(void)
{
  s_demo.bg_enabled = !s_demo.bg_enabled;
}

void render_demo_toggle_cube(void)
{
  s_demo.cube_enabled = !s_demo.cube_enabled;
}

void render_demo_draw(void)
{
  uint16_t width = renderGetWidth();
  uint16_t height = renderGetHeight();
  if ((width == 0U) || (height == 0U))
  {
    return;
  }

  uint32_t now = osKernelGetTickCount();
  if ((!s_demo.initialized) || (s_demo.width != width) || (s_demo.height != height))
  {
    render_demo_init(width, height, now);
  }

  uint32_t dt = now - s_demo.last_frame_ms;
  s_demo.last_frame_ms = now;
  s_demo.fps_ms_acc += dt;
  s_demo.fps_frames++;
  if (s_demo.fps_ms_acc >= 1000U)
  {
    s_demo.fps = (s_demo.fps_frames * 1000U) / s_demo.fps_ms_acc;
    s_demo.fps_ms_acc = 0U;
    s_demo.fps_frames = 0U;
  }

  renderFill(false);

  if (s_demo.bg_enabled)
  {
    draw_bg_bitmap_tiled_scrolled(pattern202thread0n, PATTERN_W, PATTERN_H,
                                  s_demo.scroll_x, s_demo.scroll_y,
                                  width, s_demo.game_y0, s_demo.game_y1);
    s_demo.scroll_x += 1U;
    s_demo.scroll_y += 1U;
  }

  if (s_demo.cube_enabled)
  {
    s_demo.ay += 0.045f;
    s_demo.ax += 0.027f;
    draw_wire_cube(s_demo.ay, s_demo.ax, cube_blink_color(s_demo.frame_id));
  }

  draw_ui_top(width, s_demo.fps, 0);
  draw_ui_bottom(width, height, (now - s_demo.boot_ms) / 1000U);

  s_demo.frame_id++;

  if (s_mode == RENDER_DEMO_MODE_SINGLE)
  {
    s_mode = RENDER_DEMO_MODE_IDLE;
  }
}
