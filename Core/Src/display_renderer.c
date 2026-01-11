#include "display_renderer.h"
#include "font8x8_basic.h"

#include <string.h>

#define DIRTY_WORD_COUNT ((DISPLAY_HEIGHT + 31U) / 32U)

#if (DISPLAY_HEIGHT % 32U) == 0U
  #define DIRTY_LAST_WORD_MASK 0xFFFFFFFFU
#else
  #define DIRTY_LAST_WORD_MASK ((1UL << (DISPLAY_HEIGHT % 32U)) - 1UL)
#endif

#define RENDER_UI_SHIFT 0U
#define RENDER_UI_MASK (0x3U << RENDER_UI_SHIFT)
#define RENDER_GAME_SHIFT 2U
#define RENDER_GAME_MASK (0x3U << RENDER_GAME_SHIFT)
#define RENDER_BG_SHIFT 4U
#define RENDER_BG_MASK (0x1U << RENDER_BG_SHIFT)
#define RENDER_DIRTY_MASK (0x1U << 7U)

static void renderDrawHLineClamped(int32_t x0, int32_t x1, int32_t y, render_layer_t layer, render_state_t state);
static void renderDrawVLineClamped(int32_t x, int32_t y0, int32_t y1, render_layer_t layer, render_state_t state);

/* Place framebuffer in SRAM4 for LPDMA access. */
#if defined(__GNUC__)
  #define SRAM4_BUF_ATTR __attribute__((section(".sram4"))) __attribute__((aligned(4)))
#elif defined(__ICCARM__)
  #define SRAM4_BUF_ATTR __attribute__((section(".sram4"))) __attribute__((aligned(4)))
#else
  #define SRAM4_BUF_ATTR
#endif

static uint8_t s_packed_buffer[BUFFER_LENGTH] SRAM4_BUF_ATTR;
static uint8_t s_l8_buffer[DISPLAY_WIDTH * DISPLAY_HEIGHT] __attribute__((aligned(4)));
static uint32_t s_dirty_mask[DIRTY_WORD_COUNT];
static render_rotation_t s_rotation = RENDER_ROTATION_270_CW;

static bool normalize_span(uint16_t *start_row, uint16_t *end_row)
{
  if ((start_row == NULL) || (end_row == NULL))
  {
    return false;
  }
  if ((*start_row == 0U) || (*end_row == 0U))
  {
    return false;
  }

  if (*start_row > DISPLAY_HEIGHT)
  {
    *start_row = DISPLAY_HEIGHT;
  }
  if (*end_row > DISPLAY_HEIGHT)
  {
    *end_row = DISPLAY_HEIGHT;
  }
  if (*start_row > *end_row)
  {
    uint16_t tmp = *start_row;
    *start_row = *end_row;
    *end_row = tmp;
  }

  return true;
}

static void render_get_logical_dims(uint16_t *width, uint16_t *height)
{
  if ((width == NULL) || (height == NULL))
  {
    return;
  }

  if ((s_rotation == RENDER_ROTATION_90_CW) || (s_rotation == RENDER_ROTATION_270_CW))
  {
    *width = DISPLAY_HEIGHT;
    *height = DISPLAY_WIDTH;
  }
  else
  {
    *width = DISPLAY_WIDTH;
    *height = DISPLAY_HEIGHT;
  }
}

static bool normalize_logical_span(uint16_t *start_row, uint16_t *end_row)
{
  if ((start_row == NULL) || (end_row == NULL))
  {
    return false;
  }
  if ((*start_row == 0U) || (*end_row == 0U))
  {
    return false;
  }

  uint16_t width = 0U;
  uint16_t height = 0U;
  render_get_logical_dims(&width, &height);

  if (*start_row > height)
  {
    *start_row = height;
  }
  if (*end_row > height)
  {
    *end_row = height;
  }
  if (*start_row > *end_row)
  {
    uint16_t tmp = *start_row;
    *start_row = *end_row;
    *end_row = tmp;
  }

  return true;
}

static bool render_map_xy(uint16_t x, uint16_t y, uint16_t *out_x, uint16_t *out_y)
{
  if ((out_x == NULL) || (out_y == NULL))
  {
    return false;
  }

  uint16_t width = 0U;
  uint16_t height = 0U;
  render_get_logical_dims(&width, &height);
  if ((x >= width) || (y >= height))
  {
    return false;
  }

  switch (s_rotation)
  {
    case RENDER_ROTATION_0:
      *out_x = x;
      *out_y = y;
      break;
    case RENDER_ROTATION_90_CW:
      *out_x = y;
      *out_y = (uint16_t)(DISPLAY_HEIGHT - 1U - x);
      break;
    case RENDER_ROTATION_180:
      *out_x = (uint16_t)(DISPLAY_WIDTH - 1U - x);
      *out_y = (uint16_t)(DISPLAY_HEIGHT - 1U - y);
      break;
    case RENDER_ROTATION_270_CW:
      *out_x = (uint16_t)(DISPLAY_WIDTH - 1U - y);
      *out_y = x;
      break;
    default:
      return false;
  }

  return true;
}

static uint8_t get_ui_state(uint8_t pixel)
{
  return (uint8_t)((pixel & RENDER_UI_MASK) >> RENDER_UI_SHIFT);
}

static uint8_t get_game_state(uint8_t pixel)
{
  return (uint8_t)((pixel & RENDER_GAME_MASK) >> RENDER_GAME_SHIFT);
}

static uint8_t set_ui_state(uint8_t pixel, uint8_t state)
{
  pixel &= (uint8_t)~RENDER_UI_MASK;
  pixel |= (uint8_t)((state & 0x3U) << RENDER_UI_SHIFT);
  return pixel;
}

static uint8_t set_game_state(uint8_t pixel, uint8_t state)
{
  pixel &= (uint8_t)~RENDER_GAME_MASK;
  pixel |= (uint8_t)((state & 0x3U) << RENDER_GAME_SHIFT);
  return pixel;
}

static uint8_t set_bg_state(uint8_t pixel, render_state_t state)
{
  if (state == RENDER_STATE_BLACK)
  {
    pixel |= RENDER_BG_MASK;
  }
  else if (state == RENDER_STATE_WHITE)
  {
    pixel &= (uint8_t)~RENDER_BG_MASK;
  }
  return pixel;
}

static uint8_t apply_layer_state(uint8_t pixel, render_layer_t layer, render_state_t state)
{
  if (layer == RENDER_LAYER_UI)
  {
    return set_ui_state(pixel, (uint8_t)state);
  }
  if (layer == RENDER_LAYER_GAME)
  {
    return set_game_state(pixel, (uint8_t)state);
  }
  return set_bg_state(pixel, state);
}

static uint8_t swap_bw_state(uint8_t state)
{
  if (state == RENDER_STATE_BLACK)
  {
    return RENDER_STATE_WHITE;
  }
  if (state == RENDER_STATE_WHITE)
  {
    return RENDER_STATE_BLACK;
  }
  return state;
}

static uint8_t resolve_pixel(uint8_t pixel)
{
  uint8_t ui = get_ui_state(pixel);
  if (ui == RENDER_STATE_BLACK)
  {
    return 0U;
  }
  if (ui == RENDER_STATE_WHITE)
  {
    return 1U;
  }

  uint8_t game = get_game_state(pixel);
  if (game == RENDER_STATE_BLACK)
  {
    return 0U;
  }
  if (game == RENDER_STATE_WHITE)
  {
    return 1U;
  }

  return ((pixel & RENDER_BG_MASK) != 0U) ? 0U : 1U;
}

static uint8_t invert_pixel(uint8_t pixel)
{
  uint8_t ui = get_ui_state(pixel);
  if ((ui == RENDER_STATE_BLACK) || (ui == RENDER_STATE_WHITE))
  {
    return set_ui_state(pixel, swap_bw_state(ui));
  }

  uint8_t game = get_game_state(pixel);
  if ((game == RENDER_STATE_BLACK) || (game == RENDER_STATE_WHITE))
  {
    return set_game_state(pixel, swap_bw_state(game));
  }

  return (uint8_t)(pixel ^ (uint8_t)RENDER_BG_MASK);
}

static void dirty_clear_all(void)
{
  memset(s_dirty_mask, 0, sizeof(s_dirty_mask));
}

static void dirty_set_all(void)
{
  for (uint32_t i = 0U; i < DIRTY_WORD_COUNT; ++i)
  {
    s_dirty_mask[i] = 0xFFFFFFFFU;
  }
  s_dirty_mask[DIRTY_WORD_COUNT - 1U] = DIRTY_LAST_WORD_MASK;
}

static bool dirty_any(void)
{
  for (uint32_t i = 0U; i < DIRTY_WORD_COUNT; ++i)
  {
    if (s_dirty_mask[i] != 0U)
    {
      return true;
    }
  }
  return false;
}

static void dirty_set_row(uint16_t row)
{
  if ((row < 1U) || (row > DISPLAY_HEIGHT))
  {
    return;
  }
  uint32_t idx = (uint32_t)(row - 1U);
  s_dirty_mask[idx / 32U] |= (1UL << (idx % 32U));
}

static void dirty_clear_row(uint16_t row)
{
  if ((row < 1U) || (row > DISPLAY_HEIGHT))
  {
    return;
  }
  uint32_t idx = (uint32_t)(row - 1U);
  s_dirty_mask[idx / 32U] &= ~(1UL << (idx % 32U));
}

static bool dirty_is_row(uint16_t row)
{
  if ((row < 1U) || (row > DISPLAY_HEIGHT))
  {
    return false;
  }
  uint32_t idx = (uint32_t)(row - 1U);
  return ((s_dirty_mask[idx / 32U] >> (idx % 32U)) & 1UL) != 0U;
}

static void render_write_pixel_physical(uint16_t x, uint16_t y, uint8_t pixel)
{
  uint32_t idx = ((uint32_t)y * DISPLAY_WIDTH) + x;
  s_l8_buffer[idx] = (uint8_t)(pixel | RENDER_DIRTY_MASK);
  dirty_set_row((uint16_t)(y + 1U));
}

static void render_set_pixel_physical(uint16_t x, uint16_t y, render_layer_t layer, render_state_t state)
{
  uint32_t idx = ((uint32_t)y * DISPLAY_WIDTH) + x;
  uint8_t pixel = s_l8_buffer[idx];
  pixel = apply_layer_state(pixel, layer, state);
  s_l8_buffer[idx] = (uint8_t)(pixel | RENDER_DIRTY_MASK);
  dirty_set_row((uint16_t)(y + 1U));
}

static void render_invert_pixel_physical(uint16_t x, uint16_t y)
{
  uint32_t idx = ((uint32_t)y * DISPLAY_WIDTH) + x;
  uint8_t pixel = s_l8_buffer[idx];
  pixel = invert_pixel(pixel);
  s_l8_buffer[idx] = (uint8_t)(pixel | RENDER_DIRTY_MASK);
  dirty_set_row((uint16_t)(y + 1U));
}

static void mark_row_dirty_bits(uint16_t row)
{
  if ((row < 1U) || (row > DISPLAY_HEIGHT))
  {
    return;
  }
  uint32_t offset = (uint32_t)(row - 1U) * DISPLAY_WIDTH;
  for (uint32_t i = 0U; i < DISPLAY_WIDTH; ++i)
  {
    s_l8_buffer[offset + i] |= RENDER_DIRTY_MASK;
  }
}

static void mark_dirty_span(uint16_t start_row, uint16_t end_row)
{
  if (!normalize_span(&start_row, &end_row))
  {
    return;
  }

  for (uint16_t row = start_row; row <= end_row; ++row)
  {
    dirty_set_row(row);
  }
}

static void pack_row(uint16_t row)
{
  uint32_t row_index = (uint32_t)(row - 1U);
  uint8_t *dst = &s_packed_buffer[row_index * LINE_WIDTH];
  uint8_t *src = &s_l8_buffer[row_index * DISPLAY_WIDTH];
  uint8_t out_byte = 0U;

  for (uint16_t x = 0U; x < DISPLAY_WIDTH; ++x)
  {
    uint8_t pixel = src[x];
    uint8_t bit = resolve_pixel(pixel);
    if (bit != 0U)
    {
      out_byte |= (uint8_t)(1U << (x & 7U));
    }
    src[x] = (uint8_t)(pixel & (uint8_t)~RENDER_DIRTY_MASK);
    if ((x & 7U) == 7U)
    {
      dst[x >> 3U] = out_byte;
      out_byte = 0U;
    }
  }
}

void renderInit(void)
{
  memset(s_packed_buffer, 0xFF, BUFFER_LENGTH);
  memset(s_l8_buffer, 0x00, sizeof(s_l8_buffer));
  dirty_clear_all();
  s_rotation = RENDER_ROTATION_270_CW;
}

void renderSetRotation(render_rotation_t rotation)
{
  if ((rotation == RENDER_ROTATION_0) ||
      (rotation == RENDER_ROTATION_90_CW) ||
      (rotation == RENDER_ROTATION_180) ||
      (rotation == RENDER_ROTATION_270_CW))
  {
    s_rotation = rotation;
  }
}

render_rotation_t renderGetRotation(void)
{
  return s_rotation;
}

uint16_t renderGetWidth(void)
{
  uint16_t width = 0U;
  uint16_t height = 0U;
  render_get_logical_dims(&width, &height);
  return width;
}

uint16_t renderGetHeight(void)
{
  uint16_t width = 0U;
  uint16_t height = 0U;
  render_get_logical_dims(&width, &height);
  return height;
}

const uint8_t *renderGetBuffer(void)
{
  return s_packed_buffer;
}

bool renderTakeDirtyRows(uint16_t *rows, uint16_t max_rows, uint16_t *out_count, bool *out_full)
{
  if ((rows == NULL) || (out_count == NULL) || (max_rows == 0U))
  {
    return false;
  }

  if (!dirty_any())
  {
    return false;
  }

  uint16_t count = 0U;
  for (uint16_t row = 1U; row <= DISPLAY_HEIGHT; ++row)
  {
    if (!dirty_is_row(row))
    {
      continue;
    }
    if (count < max_rows)
    {
      rows[count++] = row;
      dirty_clear_row(row);
      pack_row(row);
    }
    else
    {
      break;
    }
  }

  *out_count = count;
  if (out_full != NULL)
  {
    *out_full = ((count == DISPLAY_HEIGHT) && !dirty_any());
  }

  return (count != 0U);
}

void renderMarkDirtyRows(uint16_t start_row, uint16_t end_row)
{
  mark_dirty_span(start_row, end_row);
  if (!normalize_span(&start_row, &end_row))
  {
    return;
  }
  for (uint16_t row = start_row; row <= end_row; ++row)
  {
    mark_row_dirty_bits(row);
  }
}

void renderMarkDirtyList(const uint16_t *rows, uint16_t row_count)
{
  if ((rows == NULL) || (row_count == 0U))
  {
    return;
  }

  for (uint16_t i = 0U; i < row_count; ++i)
  {
    dirty_set_row(rows[i]);
    mark_row_dirty_bits(rows[i]);
  }
}

void renderFill(bool fill)
{
  uint8_t pixel = 0U;
  if (fill)
  {
    pixel |= RENDER_BG_MASK;
  }
  pixel |= RENDER_DIRTY_MASK;
  memset(s_l8_buffer, pixel, sizeof(s_l8_buffer));
  dirty_set_all();
}

void renderInvert(void)
{
  for (uint32_t i = 0U; i < (uint32_t)DISPLAY_WIDTH * DISPLAY_HEIGHT; ++i)
  {
    uint8_t pixel = s_l8_buffer[i];
    pixel = invert_pixel(pixel);
    s_l8_buffer[i] = (uint8_t)(pixel | RENDER_DIRTY_MASK);
  }
  dirty_set_all();
}

void renderFillRows(uint16_t start_row, uint16_t end_row, bool fill)
{
  if (!normalize_logical_span(&start_row, &end_row))
  {
    return;
  }

  uint16_t width = 0U;
  uint16_t height = 0U;
  render_get_logical_dims(&width, &height);
  (void)height;

  uint8_t base_pixel = 0U;
  if (fill)
  {
    base_pixel |= RENDER_BG_MASK;
  }

  for (uint16_t row = start_row; row <= end_row; ++row)
  {
    uint16_t y = (uint16_t)(row - 1U);
    for (uint16_t x = 0U; x < width; ++x)
    {
      uint16_t px = 0U;
      uint16_t py = 0U;
      if (!render_map_xy(x, y, &px, &py))
      {
        continue;
      }
      render_write_pixel_physical(px, py, base_pixel);
    }
  }
}

void renderInvertRows(uint16_t start_row, uint16_t end_row)
{
  if (!normalize_logical_span(&start_row, &end_row))
  {
    return;
  }

  uint16_t width = 0U;
  uint16_t height = 0U;
  render_get_logical_dims(&width, &height);
  (void)height;

  for (uint16_t row = start_row; row <= end_row; ++row)
  {
    uint16_t y = (uint16_t)(row - 1U);
    for (uint16_t x = 0U; x < width; ++x)
    {
      uint16_t px = 0U;
      uint16_t py = 0U;
      if (!render_map_xy(x, y, &px, &py))
      {
        continue;
      }
      render_invert_pixel_physical(px, py);
    }
  }
}

void renderSetPixel(uint16_t x, uint16_t y, render_layer_t layer, render_state_t state)
{
  uint16_t px = 0U;
  uint16_t py = 0U;
  if (!render_map_xy(x, y, &px, &py))
  {
    return;
  }

  render_set_pixel_physical(px, py, layer, state);
}

void renderDrawHLine(uint16_t x, uint16_t y, uint16_t length, render_layer_t layer, render_state_t state)
{
  uint16_t width = 0U;
  uint16_t height = 0U;
  render_get_logical_dims(&width, &height);

  if ((y >= height) || (x >= width) || (length == 0U))
  {
    return;
  }

  uint16_t end = (uint16_t)(x + length);
  if (end > width)
  {
    end = width;
  }

  for (uint16_t ix = x; ix < end; ++ix)
  {
    renderSetPixel(ix, y, layer, state);
  }
}

void renderDrawVLine(uint16_t x, uint16_t y, uint16_t length, render_layer_t layer, render_state_t state)
{
  uint16_t width = 0U;
  uint16_t height = 0U;
  render_get_logical_dims(&width, &height);

  if ((x >= width) || (y >= height) || (length == 0U))
  {
    return;
  }

  uint16_t end = (uint16_t)(y + length);
  if (end > height)
  {
    end = height;
  }

  for (uint16_t iy = y; iy < end; ++iy)
  {
    renderSetPixel(x, iy, layer, state);
  }
}

void renderFillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, render_layer_t layer, render_state_t state)
{
  uint16_t logical_width = 0U;
  uint16_t logical_height = 0U;
  render_get_logical_dims(&logical_width, &logical_height);

  if ((x >= logical_width) || (y >= logical_height) || (width == 0U) || (height == 0U))
  {
    return;
  }

  uint16_t end_x = (uint16_t)(x + width);
  uint16_t end_y = (uint16_t)(y + height);
  if (end_x > logical_width)
  {
    end_x = logical_width;
  }
  if (end_y > logical_height)
  {
    end_y = logical_height;
  }

  for (uint16_t iy = y; iy < end_y; ++iy)
  {
    uint16_t span = (uint16_t)(end_x - x);
    renderDrawHLine(x, iy, span, layer, state);
  }
}

void renderDrawRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, render_layer_t layer, render_state_t state)
{
  if ((width == 0U) || (height == 0U))
  {
    return;
  }

  renderDrawHLine(x, y, width, layer, state);
  if (height > 1U)
  {
    renderDrawHLine(x, (uint16_t)(y + height - 1U), width, layer, state);
  }

  if (height > 2U)
  {
    renderDrawVLine(x, (uint16_t)(y + 1U), (uint16_t)(height - 2U), layer, state);
    if (width > 1U)
    {
      renderDrawVLine((uint16_t)(x + width - 1U), (uint16_t)(y + 1U), (uint16_t)(height - 2U), layer, state);
    }
  }
}

void renderDrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, render_layer_t layer, render_state_t state)
{
  int32_t ix0 = (int32_t)x0;
  int32_t iy0 = (int32_t)y0;
  int32_t ix1 = (int32_t)x1;
  int32_t iy1 = (int32_t)y1;

  int32_t dx = (ix0 < ix1) ? (ix1 - ix0) : (ix0 - ix1);
  int32_t sx = (ix0 < ix1) ? 1 : -1;
  int32_t dy = (iy0 < iy1) ? (iy1 - iy0) : (iy0 - iy1);
  int32_t sy = (iy0 < iy1) ? 1 : -1;
  int32_t err = dx - dy;

  for (;;)
  {
    renderSetPixel((uint16_t)ix0, (uint16_t)iy0, layer, state);
    if ((ix0 == ix1) && (iy0 == iy1))
    {
      break;
    }
    int32_t e2 = err * 2;
    if (e2 > -dy)
    {
      err -= dy;
      ix0 += sx;
    }
    if (e2 < dx)
    {
      err += dx;
      iy0 += sy;
    }
  }
}

void renderDrawLineThick(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t thickness,
                         render_layer_t layer, render_state_t state)
{
  if (thickness <= 1U)
  {
    renderDrawLine(x0, y0, x1, y1, layer, state);
    return;
  }

  int32_t r_lo = (int32_t)(thickness - 1U) / 2;
  int32_t r_hi = (int32_t)thickness / 2;

  int32_t ix0 = (int32_t)x0;
  int32_t iy0 = (int32_t)y0;
  int32_t ix1 = (int32_t)x1;
  int32_t iy1 = (int32_t)y1;

  int32_t dx = (ix0 < ix1) ? (ix1 - ix0) : (ix0 - ix1);
  int32_t sx = (ix0 < ix1) ? 1 : -1;
  int32_t dy = (iy0 < iy1) ? (iy1 - iy0) : (iy0 - iy1);
  int32_t sy = (iy0 < iy1) ? 1 : -1;
  int32_t err = dx - dy;

  if (dx >= dy)
  {
    for (;;)
    {
      renderDrawVLineClamped(ix0, (int32_t)iy0 - r_lo, (int32_t)iy0 + r_hi, layer, state);
      if ((ix0 == ix1) && (iy0 == iy1))
      {
        break;
      }
      int32_t e2 = err * 2;
      if (e2 > -dy)
      {
        err -= dy;
        ix0 += sx;
      }
      if (e2 < dx)
      {
        err += dx;
        iy0 += sy;
      }
    }
  }
  else
  {
    for (;;)
    {
      renderDrawHLineClamped((int32_t)ix0 - r_lo, (int32_t)ix0 + r_hi, iy0, layer, state);
      if ((ix0 == ix1) && (iy0 == iy1))
      {
        break;
      }
      int32_t e2 = err * 2;
      if (e2 > -dy)
      {
        err -= dy;
        ix0 += sx;
      }
      if (e2 < dx)
      {
        err += dx;
        iy0 += sy;
      }
    }
  }
}

static void renderDrawHLineClamped(int32_t x0, int32_t x1, int32_t y, render_layer_t layer, render_state_t state)
{
  uint16_t width = 0U;
  uint16_t height = 0U;
  render_get_logical_dims(&width, &height);

  if ((y < 0) || (y >= (int32_t)height))
  {
    return;
  }

  if (x0 > x1)
  {
    int32_t tmp = x0;
    x0 = x1;
    x1 = tmp;
  }

  if ((x1 < 0) || (x0 >= (int32_t)width))
  {
    return;
  }

  if (x0 < 0)
  {
    x0 = 0;
  }
  if (x1 >= (int32_t)width)
  {
    x1 = (int32_t)width - 1;
  }

  uint16_t span = (uint16_t)(x1 - x0 + 1);
  renderDrawHLine((uint16_t)x0, (uint16_t)y, span, layer, state);
}

static void renderDrawVLineClamped(int32_t x, int32_t y0, int32_t y1, render_layer_t layer, render_state_t state)
{
  uint16_t width = 0U;
  uint16_t height = 0U;
  render_get_logical_dims(&width, &height);

  if ((x < 0) || (x >= (int32_t)width))
  {
    return;
  }

  if (y0 > y1)
  {
    int32_t tmp = y0;
    y0 = y1;
    y1 = tmp;
  }

  if ((y1 < 0) || (y0 >= (int32_t)height))
  {
    return;
  }

  if (y0 < 0)
  {
    y0 = 0;
  }
  if (y1 >= (int32_t)height)
  {
    y1 = (int32_t)height - 1;
  }

  uint16_t span = (uint16_t)(y1 - y0 + 1);
  renderDrawVLine((uint16_t)x, (uint16_t)y0, span, layer, state);
}

void renderDrawCircle(uint16_t x0, uint16_t y0, uint16_t radius, render_layer_t layer, render_state_t state)
{
  int32_t x = (int32_t)radius;
  int32_t y = 0;
  int32_t err = 0;

  while (x >= y)
  {
    renderSetPixel((uint16_t)(x0 + x), (uint16_t)(y0 + y), layer, state);
    renderSetPixel((uint16_t)(x0 + y), (uint16_t)(y0 + x), layer, state);
    renderSetPixel((uint16_t)(x0 - y), (uint16_t)(y0 + x), layer, state);
    renderSetPixel((uint16_t)(x0 - x), (uint16_t)(y0 + y), layer, state);
    renderSetPixel((uint16_t)(x0 - x), (uint16_t)(y0 - y), layer, state);
    renderSetPixel((uint16_t)(x0 - y), (uint16_t)(y0 - x), layer, state);
    renderSetPixel((uint16_t)(x0 + y), (uint16_t)(y0 - x), layer, state);
    renderSetPixel((uint16_t)(x0 + x), (uint16_t)(y0 - y), layer, state);

    y++;
    err += 1 + (2 * y);
    if ((2 * (err - x)) + 1 > 0)
    {
      x--;
      err += 1 - (2 * x);
    }
  }
}

void renderDrawCircleThick(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t thickness, render_layer_t layer,
                           render_state_t state)
{
  if (thickness <= 1U)
  {
    renderDrawCircle(x0, y0, radius, layer, state);
    return;
  }

  if (radius == 0U)
  {
    renderSetPixel(x0, y0, layer, state);
    return;
  }

  if (thickness >= (uint16_t)(radius + 1U))
  {
    renderFillCircle(x0, y0, radius, layer, state);
    return;
  }

  int32_t inner = (int32_t)radius - (int32_t)thickness + 1;
  if (inner < 0)
  {
    inner = 0;
  }

  for (int32_t r = (int32_t)radius; r >= inner; --r)
  {
    renderDrawCircle(x0, y0, (uint16_t)r, layer, state);
  }
}

void renderFillCircle(uint16_t x0, uint16_t y0, uint16_t radius, render_layer_t layer, render_state_t state)
{
  int32_t x = (int32_t)radius;
  int32_t y = 0;
  int32_t err = 0;

  while (x >= y)
  {
    renderDrawHLineClamped((int32_t)x0 - x, (int32_t)x0 + x, (int32_t)y0 + y, layer, state);
    renderDrawHLineClamped((int32_t)x0 - x, (int32_t)x0 + x, (int32_t)y0 - y, layer, state);
    renderDrawHLineClamped((int32_t)x0 - y, (int32_t)x0 + y, (int32_t)y0 + x, layer, state);
    renderDrawHLineClamped((int32_t)x0 - y, (int32_t)x0 + y, (int32_t)y0 - x, layer, state);

    y++;
    err += 1 + (2 * y);
    if ((2 * (err - x)) + 1 > 0)
    {
      x--;
      err += 1 - (2 * x);
    }
  }
}

void renderDrawChar(uint16_t x, uint16_t y, char ch, render_layer_t layer, render_state_t fg)
{
  uint8_t code = (uint8_t)ch;
  if ((code < (uint8_t)FONT8X8_START_CHAR) || (code > (uint8_t)FONT8X8_END_CHAR))
  {
    code = (uint8_t)'?';
  }

  const uint8_t *glyph = font8x8_basic[code];
  for (uint16_t row = 0U; row < (uint16_t)FONT8X8_HEIGHT; ++row)
  {
    uint8_t bits = glyph[row];
    for (uint16_t col = 0U; col < (uint16_t)FONT8X8_WIDTH; ++col)
    {
      if ((bits & (uint8_t)(1U << col)) != 0U)
      {
        renderSetPixel((uint16_t)(x + col), (uint16_t)(y + row), layer, fg);
      }
    }
  }
}

void renderDrawText(uint16_t x, uint16_t y, const char *text, render_layer_t layer, render_state_t fg)
{
  if (text == NULL)
  {
    return;
  }

  uint16_t width = renderGetWidth();
  uint16_t height = renderGetHeight();
  if ((width == 0U) || (height == 0U))
  {
    return;
  }

  uint16_t cursor_x = x;
  uint16_t cursor_y = y;
  uint16_t advance_x = (uint16_t)(FONT8X8_WIDTH + 1U);
  uint16_t advance_y = (uint16_t)(FONT8X8_HEIGHT + 1U);

  for (const char *ptr = text; *ptr != '\0'; ++ptr)
  {
    if (*ptr == '\n')
    {
      cursor_x = x;
      cursor_y = (uint16_t)(cursor_y + advance_y);
      if (cursor_y >= height)
      {
        break;
      }
      continue;
    }

    renderDrawChar(cursor_x, cursor_y, *ptr, layer, fg);
    cursor_x = (uint16_t)(cursor_x + advance_x);
    if ((uint16_t)(cursor_x + FONT8X8_WIDTH) > width)
    {
      cursor_x = x;
      cursor_y = (uint16_t)(cursor_y + advance_y);
      if (cursor_y >= height)
      {
        break;
      }
    }
  }
}

void renderBlit1bpp(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t *data,
                    uint16_t stride_bytes, render_layer_t layer, render_state_t fg)
{
  if ((data == NULL) || (width == 0U) || (height == 0U))
  {
    return;
  }

  if (stride_bytes == 0U)
  {
    stride_bytes = (uint16_t)((width + 7U) / 8U);
  }

  /* Sprite data is LSB-left; bit0 is the leftmost pixel in each byte. */
  for (uint16_t row = 0U; row < height; ++row)
  {
    const uint8_t *row_ptr = &data[row * stride_bytes];
    for (uint16_t col = 0U; col < width; ++col)
    {
      uint8_t byte = row_ptr[col >> 3U];
      if ((byte & (uint8_t)(1U << (col & 7U))) != 0U)
      {
        renderSetPixel((uint16_t)(x + col), (uint16_t)(y + row), layer, fg);
      }
    }
  }
}

void renderBlit1bppMsb(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t *data,
                       uint16_t stride_bytes, render_layer_t layer, render_state_t fg)
{
  if ((data == NULL) || (width == 0U) || (height == 0U))
  {
    return;
  }

  if (stride_bytes == 0U)
  {
    stride_bytes = (uint16_t)((width + 7U) / 8U);
  }

  /* Sprite data is MSB-left; bit7 is the leftmost pixel in each byte. */
  for (uint16_t row = 0U; row < height; ++row)
  {
    const uint8_t *row_ptr = &data[row * stride_bytes];
    for (uint16_t col = 0U; col < width; ++col)
    {
      uint8_t byte = row_ptr[col >> 3U];
      if ((byte & (uint8_t)(0x80U >> (col & 7U))) != 0U)
      {
        renderSetPixel((uint16_t)(x + col), (uint16_t)(y + row), layer, fg);
      }
    }
  }
}
