/*
 * render_demo.c
 *
 * Demo/test scene renderer:
 *  - Optional 1bpp tiled scrolling background
 *  - Optional blinking/thick wireframe cube
 *  - Simple top/bottom UI bars with FPS + uptime
 *
 * Notes:
 *  - Background bitmap is 1bpp, stored MSB-first per byte (bit 7 = leftmost).
 *  - This module assumes the display_renderer API provides:
 *      renderGetWidth(), renderGetHeight(), renderFill(),
 *      renderFillRect(), renderBlit1bpp(), renderDrawText(),
 *      renderDrawLineThick()
 *  - Timebase comes from CMSIS-RTOS2 ticks (osKernelGetTickCount()).
 */

#include "render_demo.h"

#include "display_renderer.h"
#include "font8x8_basic.h"
#include "power_task.h"

#include "cmsis_os2.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* ----------------------------- Tunables ---------------------------------- */

#define UI_BAR_H_PIXELS     (14U)  /* Height of top/bottom UI bars (if enabled) */
#define EDGE_THICK_PIXELS   (5U)   /* Thickness for cube edges */

#define BG_PATTERN_W_PIXELS (20U)  /* Pattern bitmap width (pixels) */
#define BG_PATTERN_H_PIXELS (34U)  /* Pattern bitmap height (pixels) */

/* -------------------------- Static assets -------------------------------- */

/* 1bpp background pattern, tiled across the screen.
 * Bitmap layout:
 *  - Row-major
 *  - Each row stride = ceil(width/8)
 *  - Bit 7 is the leftmost pixel in each byte
 *
 * Pixel meaning in this demo:
 *  - bit=1 -> BLACK
 *  - bit=0 -> WHITE
 */
static const uint8_t kBgPattern1bpp[] =
{
  0xD5U, 0x62U, 0x20U, 0xAAU, 0xE8U, 0x80U, 0xD5U, 0x62U, 0x20U, 0xAAU, 0xE8U, 0x80U, 0xD5U, 0x62U, 0x20U, 0xAAU,
  0xE8U, 0x80U, 0xD5U, 0xF2U, 0x20U, 0xABU, 0xFCU, 0x80U, 0xDFU, 0x0FU, 0x20U, 0xBCU, 0x03U, 0xC0U, 0xF0U, 0x00U,
  0xF0U, 0xC0U, 0x00U, 0x30U, 0xB0U, 0x00U, 0xF0U, 0x8CU, 0x03U, 0x50U, 0xA3U, 0x0EU, 0xB0U, 0x88U, 0xF5U, 0x50U,
  0xA2U, 0x2AU, 0xB0U, 0x88U, 0xB5U, 0x50U, 0xA2U, 0x2AU, 0xB0U, 0x88U, 0xB5U, 0x50U, 0xA2U, 0x2AU, 0xB0U, 0x88U,
  0xB5U, 0x50U, 0xA2U, 0x2AU, 0xB0U, 0xC8U, 0xB5U, 0x70U, 0xF2U, 0x2AU, 0xF0U, 0x3CU, 0xB7U, 0xC0U, 0x0FU, 0x2FU,
  0x00U, 0x03U, 0xFCU, 0x00U, 0x00U, 0xF0U, 0x00U, 0x03U, 0xECU, 0x00U, 0x0DU, 0x63U, 0x00U, 0x3AU, 0xE8U, 0xC0U,
  0xD5U, 0x62U, 0x30U, 0xAAU, 0xE8U, 0x80U
};

/* ------------------------- Geometry / math ------------------------------- */

typedef struct { float   x, y, z; } vec3_t;
typedef struct { int16_t x, y;    } pt2_t;

/* Cube model vertices (centered). */
static const vec3_t kCubeVerts[8] =
{
  {-0.6f, -0.6f, -0.6f}, {+0.6f, -0.6f, -0.6f},
  {+0.6f, +0.6f, -0.6f}, {-0.6f, +0.6f, -0.6f},
  {-0.6f, -0.6f, +0.6f}, {+0.6f, -0.6f, +0.6f},
  {+0.6f, +0.6f, +0.6f}, {-0.6f, +0.6f, +0.6f}
};

/* Cube edges as index pairs into kCubeVerts. */
static const uint8_t kCubeEdges[12][2] =
{
  {0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 0U},
  {4U, 5U}, {5U, 6U}, {6U, 7U}, {7U, 4U},
  {0U, 4U}, {1U, 5U}, {2U, 6U}, {3U, 7U}
};

/* -------------------------- Demo state ----------------------------------- */

typedef struct
{
  bool     initialized;

  uint16_t width;
  uint16_t height;

  uint16_t ui_bar_h;
  uint16_t game_y0;      /* Inclusive */
  uint16_t game_y1;      /* Inclusive */

  float    ay;           /* Y rotation angle (radians) */
  float    ax;           /* X rotation angle (radians) */

  uint32_t frame_id;

  uint32_t scroll_x;     /* Background scroll offset (pixels) */
  uint32_t scroll_y;

  uint32_t fps;
  uint32_t fps_ms_acc;
  uint32_t fps_frames;

  uint32_t boot_ms;
  uint32_t last_frame_ms;

  bool     bg_enabled;
  bool     cube_enabled;
} render_demo_state_t;

static render_demo_state_t s_demo =
{
  .initialized  = false,
  .bg_enabled   = true,
  .cube_enabled = true
};

static render_demo_mode_t s_mode = RENDER_DEMO_MODE_IDLE;

/* -------------------------- Small helpers -------------------------------- */

static int32_t iroundf(float x)
{
  /* Symmetric rounding to nearest integer. */
  return (int32_t)(x >= 0.0f ? (x + 0.5f) : (x - 0.5f));
}

/* Decimal formatting helpers (no printf dependency). */
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
  *dst   = '\0';
  return dst;
}

/* Format millivolts as "X.XXV" (rounded to nearest 10mV). */
static uint16_t text_width_px(const char *text)
{
  if (text == NULL)
  {
    return 0U;
  }

  const size_t len = strlen(text);
  if (len == 0U)
  {
    return 0U;
  }

  /* FONT8X8_WIDTH pixels per glyph + 1px spacing between glyphs. */
  return (uint16_t)((len * (FONT8X8_WIDTH + 1U)) - 1U);
}

/* Alternate cube edge color each frame for a blinking effect. */
static render_state_t cube_blink_color(uint32_t frame_id)
{
  return ((frame_id & 1U) != 0U) ? RENDER_STATE_WHITE : RENDER_STATE_BLACK;
}

static const char *perf_mode_label(power_perf_mode_t mode)
{
  switch (mode)
  {
    case POWER_PERF_MODE_MID:
      return "MID";
    case POWER_PERF_MODE_TURBO:
      return "TUR";
    default:
      return "CRU";
  }
}

/* ---------------------------- UI drawing --------------------------------- */

static void draw_ui_top(uint16_t width, uint32_t fps, power_perf_mode_t mode)
{
  if (s_demo.ui_bar_h == 0U)
  {
    return;
  }

  renderFillRect(0U, 0U, width, s_demo.ui_bar_h, RENDER_LAYER_UI, RENDER_STATE_BLACK);

  const char *label = perf_mode_label(mode);

  char fps_buf[24];
  char *p = fps_buf;

  *p++ = 'F'; *p++ = 'P'; *p++ = 'S'; *p++ = ' ';
  p = u32_to_dec(p, fps);
  *p = '\0';

  renderDrawText(4U, 3U, fps_buf, RENDER_LAYER_UI, RENDER_STATE_WHITE);

  const char *mode_text = (label != NULL) ? label : "-";
  const uint16_t w = text_width_px(mode_text);
  uint16_t x = 2U;
  if (width > (uint16_t)(w + 4U))
  {
    x = (uint16_t)(width - w - 4U);
  }
  renderDrawText(x, 3U, mode_text, RENDER_LAYER_UI, RENDER_STATE_WHITE);
}

static void draw_ui_bottom(uint16_t width, uint16_t height, uint32_t uptime_s)
{
  if (s_demo.ui_bar_h == 0U)
  {
    return;
  }

  const uint16_t y0 = (uint16_t)(height - s_demo.ui_bar_h);

  renderFillRect(0U, y0, width, s_demo.ui_bar_h, RENDER_LAYER_UI, RENDER_STATE_BLACK);
  renderDrawText(4U, (uint16_t)(y0 + 3U), "SYS", RENDER_LAYER_UI, RENDER_STATE_WHITE);

  const uint32_t ss = uptime_s % 60U;
  const uint32_t mm = (uptime_s / 60U) % 60U;
  const uint32_t hh = (uptime_s / 3600U) % 100U; /* clamp to 2 digits */

  char buf[24];
  char *p = buf;

  *p++ = 'U'; *p++ = 'P'; *p++ = ':'; *p++ = ' ';
  p = u2d(p, hh);
  *p++ = ':';
  p = u2d(p, mm);
  *p++ = ':';
  p = u2d(p, ss);
  *p = '\0';

  const uint16_t w = text_width_px(buf);
  const uint16_t x = (width > (uint16_t)(w + 4U)) ? (uint16_t)(width - w - 4U) : 2U;

  renderDrawText(x, (uint16_t)(y0 + 3U), buf, RENDER_LAYER_UI, RENDER_STATE_WHITE);
}

/* ------------------------ Background drawing ----------------------------- */

static void draw_bg_span(uint16_t x, uint16_t y, uint16_t w, render_state_t color)
{
  if (w == 0U)
  {
    return;
  }

  renderFillRect(x, y, w, 1U, RENDER_LAYER_BG, color);
}

/* Draw a 1bpp bitmap as a tiled background, with scroll offsets (ox, oy).
 * This implementation uses run-length spans per scanline to reduce draw calls.
 */
static void draw_bg_bitmap_tiled_scrolled(const uint8_t *bmp,
                                          uint16_t bw, uint16_t bh,
                                          uint32_t ox, uint32_t oy,
                                          uint16_t screen_w,
                                          uint16_t game_y0, uint16_t game_y1)
{
  if ((bmp == NULL) || (bw == 0U) || (bh == 0U) || (screen_w == 0U))
  {
    return;
  }

  if (game_y1 < game_y0)
  {
    return;
  }

  const uint16_t stride = (uint16_t)((bw + 7U) >> 3U); /* bytes per row */
  const uint16_t oxm    = (uint16_t)(ox % bw);
  const uint16_t oym    = (uint16_t)(oy % bh);

  for (uint16_t y = game_y0; y <= game_y1; ++y)
  {
    const uint16_t sy = (uint16_t)((y - game_y0 + oym) % bh);
    const uint8_t *row = bmp + ((uint32_t)sy * stride);

    uint16_t sxi = oxm;     /* source x in bitmap space [0..bw-1] */
    uint16_t span_x0 = 0U;

    /* Determine initial color. */
    uint8_t byte = row[sxi >> 3U];
    uint8_t bit  = (uint8_t)(0x80U >> (sxi & 7U));
    render_state_t cur = ((byte & bit) != 0U) ? RENDER_STATE_BLACK : RENDER_STATE_WHITE;

    for (uint16_t x = 0U; x < screen_w; ++x)
    {
      byte = row[sxi >> 3U];
      bit  = (uint8_t)(0x80U >> (sxi & 7U));
      const render_state_t px = ((byte & bit) != 0U) ? RENDER_STATE_BLACK : RENDER_STATE_WHITE;

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

      /* advance bitmap x with wrap */
      sxi++;
      if (sxi == bw)
      {
        sxi = 0U;
      }
    }

    /* Flush last span. */
    if (span_x0 < screen_w)
    {
      draw_bg_span(span_x0, y, (uint16_t)(screen_w - span_x0), cur);
    }
  }
}

/* -------------------------- Cube drawing --------------------------------- */

/* Rotate around Y then X (simple Euler combo). */
static void rot_yx(const vec3_t *v, float cy, float sy, float cx, float sx, vec3_t *o)
{
  const float xx = (v->x * cy) + (v->z * sy);
  const float zz = (-v->x * sy) + (v->z * cy);

  o->x = xx;
  o->y = (v->y * cx) - (zz * sx);
  o->z = (v->y * sx) + (zz * cx);
}

/* Perspective project 3D points to screen coordinates. */
static void project_points(const vec3_t *vin, pt2_t *out, uint8_t n,
                           uint16_t width, uint16_t game_y0, uint16_t game_y1)
{
  /* Camera-ish constants tuned to “look good” on small displays. */
  const float z_off  = 2.3f;   /* push the model away from camera */
  const float near_z = 0.25f;  /* clamp to avoid insane projection */
  const float f      = 84.0f;  /* focal length in pixels */

  const float cx = (float)(width / 2U);
  const float cy = (float)((game_y0 + game_y1) / 2U);

  for (uint8_t i = 0U; i < n; ++i)
  {
    float z = vin[i].z + z_off;
    if (z < near_z)
    {
      z = near_z;
    }

    const float px = cx + (f * vin[i].x) / z;
    const float py = cy - (f * vin[i].y) / z;

    out[i].x = (int16_t)iroundf(px);
    out[i].y = (int16_t)iroundf(py);
  }
}

/* Draw cube edges as thick lines, clipped to the “game” region. */
static void draw_wire_cube(float ay, float ax, render_state_t edge_color)
{
  vec3_t rotated[8];
  pt2_t  proj[8];

  const float cy = cosf(ay);
  const float sy = sinf(ay);
  const float cx = cosf(ax);
  const float sx = sinf(ax);

  for (uint8_t i = 0U; i < 8U; ++i)
  {
    rot_yx(&kCubeVerts[i], cy, sy, cx, sx, &rotated[i]);
  }

  project_points(rotated, proj, 8U, s_demo.width, s_demo.game_y0, s_demo.game_y1);

  const int32_t w  = (int32_t)s_demo.width;
  const int32_t y0 = (int32_t)s_demo.game_y0;
  const int32_t y1 = (int32_t)s_demo.game_y1;

  const int32_t x_min = 0;
  const int32_t x_max = (w > 0) ? (w - 1) : 0;

  uint16_t thick = EDGE_THICK_PIXELS;
  if (thick > 8U)
  {
    thick = 8U; /* safety cap */
  }

  for (uint8_t e = 0U; e < 12U; ++e)
  {
    int32_t x0 = (int32_t)proj[kCubeEdges[e][0U]].x;
    int32_t y0p = (int32_t)proj[kCubeEdges[e][0U]].y;
    int32_t x1 = (int32_t)proj[kCubeEdges[e][1U]].x;
    int32_t y1p = (int32_t)proj[kCubeEdges[e][1U]].y;

    /* Trivial reject if the whole segment is off-screen horizontally. */
    if ((x0 < 0 && x1 < 0) || (x0 >= w && x1 >= w))
    {
      continue;
    }

    /* Trivial reject if the whole segment is outside vertical game window. */
    if ((y0p < y0 && y1p < y0) || (y0p > y1 && y1p > y1))
    {
      continue;
    }

    /* Clamp endpoints into drawable region (simple clamp, not true line clipping). */
    if (x0 < x_min) x0 = x_min;
    else if (x0 > x_max) x0 = x_max;

    if (x1 < x_min) x1 = x_min;
    else if (x1 > x_max) x1 = x_max;

    if (y0p < y0) y0p = y0;
    else if (y0p > y1) y0p = y1;

    if (y1p < y0) y1p = y0;
    else if (y1p > y1) y1p = y1;

    renderDrawLineThick((uint16_t)x0, (uint16_t)y0p,
                        (uint16_t)x1, (uint16_t)y1p,
                        thick, RENDER_LAYER_GAME, edge_color);
  }
}

/* ------------------------ Lifecycle / public API -------------------------- */

static void render_demo_init(uint16_t width, uint16_t height, uint32_t now_ms)
{
  s_demo.width  = width;
  s_demo.height = height;

  /* Enable UI bars only if there is enough vertical space. */
  s_demo.ui_bar_h = (height > (UI_BAR_H_PIXELS * 2U + 1U)) ? UI_BAR_H_PIXELS : 0U;

  /* Define the game region between the bars (inclusive bounds). */
  s_demo.game_y0 = s_demo.ui_bar_h;

  if (height > s_demo.ui_bar_h)
  {
    s_demo.game_y1 = (uint16_t)(height - s_demo.ui_bar_h - 1U);
  }
  else
  {
    s_demo.game_y1 = 0U;
  }

  /* If the bars collapse the game region, fall back to full screen. */
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

  s_demo.fps        = 0U;
  s_demo.fps_ms_acc = 0U;
  s_demo.fps_frames = 0U;

  s_demo.boot_ms       = now_ms;
  s_demo.last_frame_ms = now_ms;

  s_demo.initialized = true;
}

void render_demo_reset(void)
{
  s_demo.initialized  = false;
  s_demo.bg_enabled   = true;
  s_demo.cube_enabled = true;
}

void render_demo_set_mode(render_demo_mode_t mode)
{
  if (mode == s_mode)
  {
    return;
  }

  s_mode = mode;

  /* For any active demo mode, force a clean restart on next draw. */
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
  const uint16_t width  = renderGetWidth();
  const uint16_t height = renderGetHeight();

  if ((width == 0U) || (height == 0U))
  {
    return;
  }

  const uint32_t now_ms = osKernelGetTickCount();

  if ((!s_demo.initialized) || (s_demo.width != width) || (s_demo.height != height))
  {
    render_demo_init(width, height, now_ms);
  }

  /* FPS estimation using elapsed tick accumulation. */
  const uint32_t dt_ms = now_ms - s_demo.last_frame_ms;
  s_demo.last_frame_ms = now_ms;

  s_demo.fps_ms_acc += dt_ms;
  s_demo.fps_frames++;

  if (s_demo.fps_ms_acc >= 1000U)
  {
    s_demo.fps = (s_demo.fps_frames * 1000U) / s_demo.fps_ms_acc;
    s_demo.fps_ms_acc = 0U;
    s_demo.fps_frames = 0U;
  }

  /* Clear render buffers (module-specific: false likely means "full clear"). */
  renderFill(false);

  /* Background: tiled bitmap, scrolled over time. */
  if (s_demo.bg_enabled)
  {
    draw_bg_bitmap_tiled_scrolled(kBgPattern1bpp,
                                  BG_PATTERN_W_PIXELS,
                                  BG_PATTERN_H_PIXELS,
                                  s_demo.scroll_x,
                                  s_demo.scroll_y,
                                  width,
                                  s_demo.game_y0,
                                  s_demo.game_y1);

    s_demo.scroll_x += 1U;
    s_demo.scroll_y += 1U;
  }

  /* Foreground: animated wireframe cube. */
  if (s_demo.cube_enabled)
  {
    s_demo.ay += 0.045f;
    s_demo.ax += 0.027f;

    draw_wire_cube(s_demo.ay, s_demo.ax, RENDER_STATE_BLACK);
  }

  /* UI overlays (show FPS + perf mode in top bar). */
  draw_ui_top(width, s_demo.fps, power_task_get_perf_mode());
  draw_ui_bottom(width, height, (now_ms - s_demo.boot_ms) / 1000U);

  s_demo.frame_id++;

  /* Single-shot mode returns to idle after one rendered frame. */
  if (s_mode == RENDER_DEMO_MODE_SINGLE)
  {
    s_mode = RENDER_DEMO_MODE_IDLE;
  }
}
