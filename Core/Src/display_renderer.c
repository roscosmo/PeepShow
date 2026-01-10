#include "display_renderer.h"

#include <string.h>

/* Place framebuffer in SRAM4 for LPDMA access. */
#if defined(__GNUC__)
  #define SRAM4_BUF_ATTR __attribute__((section(".sram4"))) __attribute__((aligned(4)))
#elif defined(__ICCARM__)
  #define SRAM4_BUF_ATTR __attribute__((section(".sram4"))) __attribute__((aligned(4)))
#else
  #define SRAM4_BUF_ATTR
#endif

static uint8_t s_framebuffer[BUFFER_LENGTH] SRAM4_BUF_ATTR;
static uint16_t s_dirty_min = 0U;
static uint16_t s_dirty_max = 0U;

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

static void mark_dirty_span(uint16_t start_row, uint16_t end_row)
{
  if (!normalize_span(&start_row, &end_row))
  {
    return;
  }

  if (s_dirty_min == 0U)
  {
    s_dirty_min = start_row;
    s_dirty_max = end_row;
    return;
  }

  if (start_row < s_dirty_min)
  {
    s_dirty_min = start_row;
  }
  if (end_row > s_dirty_max)
  {
    s_dirty_max = end_row;
  }
}

void renderInit(void)
{
  memset(s_framebuffer, 0xFF, BUFFER_LENGTH);
  s_dirty_min = 0U;
  s_dirty_max = 0U;
}

const uint8_t *renderGetBuffer(void)
{
  return s_framebuffer;
}

bool renderTakeDirtySpan(uint16_t *start_row, uint16_t *end_row)
{
  if ((start_row == NULL) || (end_row == NULL))
  {
    return false;
  }
  if ((s_dirty_min == 0U) || (s_dirty_max == 0U))
  {
    return false;
  }

  *start_row = s_dirty_min;
  *end_row = s_dirty_max;
  s_dirty_min = 0U;
  s_dirty_max = 0U;
  return true;
}

void renderMarkDirtyRows(uint16_t start_row, uint16_t end_row)
{
  mark_dirty_span(start_row, end_row);
}

void renderFill(bool fill)
{
  memset(s_framebuffer, fill ? 0x00 : 0xFF, BUFFER_LENGTH);
  mark_dirty_span(1U, DISPLAY_HEIGHT);
}

void renderInvert(void)
{
  for (uint32_t i = 0U; i < BUFFER_LENGTH; i++)
  {
    s_framebuffer[i] = (uint8_t)~s_framebuffer[i];
  }
  mark_dirty_span(1U, DISPLAY_HEIGHT);
}

void renderFillRows(uint16_t start_row, uint16_t end_row, bool fill)
{
  if (!normalize_span(&start_row, &end_row))
  {
    return;
  }

  for (uint16_t row = start_row; row <= end_row; ++row)
  {
    uint32_t offset = (uint32_t)(row - 1U) * LINE_WIDTH;
    memset(&s_framebuffer[offset], fill ? 0x00 : 0xFF, LINE_WIDTH);
  }
  mark_dirty_span(start_row, end_row);
}

void renderInvertRows(uint16_t start_row, uint16_t end_row)
{
  if (!normalize_span(&start_row, &end_row))
  {
    return;
  }

  for (uint16_t row = start_row; row <= end_row; ++row)
  {
    uint32_t offset = (uint32_t)(row - 1U) * LINE_WIDTH;
    for (uint32_t col = 0U; col < LINE_WIDTH; ++col)
    {
      s_framebuffer[offset + col] = (uint8_t)~s_framebuffer[offset + col];
    }
  }
  mark_dirty_span(start_row, end_row);
}
