#include "display_renderer.h"

#include <string.h>

#define DIRTY_WORD_COUNT ((DISPLAY_HEIGHT + 31U) / 32U)

#if (DISPLAY_HEIGHT % 32U) == 0U
  #define DIRTY_LAST_WORD_MASK 0xFFFFFFFFU
#else
  #define DIRTY_LAST_WORD_MASK ((1UL << (DISPLAY_HEIGHT % 32U)) - 1UL)
#endif

/* Place framebuffer in SRAM4 for LPDMA access. */
#if defined(__GNUC__)
  #define SRAM4_BUF_ATTR __attribute__((section(".sram4"))) __attribute__((aligned(4)))
#elif defined(__ICCARM__)
  #define SRAM4_BUF_ATTR __attribute__((section(".sram4"))) __attribute__((aligned(4)))
#else
  #define SRAM4_BUF_ATTR
#endif

static uint8_t s_framebuffer[BUFFER_LENGTH] SRAM4_BUF_ATTR;
static uint32_t s_dirty_mask[DIRTY_WORD_COUNT];

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

void renderInit(void)
{
  memset(s_framebuffer, 0xFF, BUFFER_LENGTH);
  dirty_clear_all();
}

const uint8_t *renderGetBuffer(void)
{
  return s_framebuffer;
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
  }
}

void renderFill(bool fill)
{
  memset(s_framebuffer, fill ? 0x00 : 0xFF, BUFFER_LENGTH);
  dirty_set_all();
}

void renderInvert(void)
{
  for (uint32_t i = 0U; i < BUFFER_LENGTH; i++)
  {
    s_framebuffer[i] = (uint8_t)~s_framebuffer[i];
  }
  dirty_set_all();
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
