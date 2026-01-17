#include "sleep_face.h"

#include "display_renderer.h"
#include "font8x8_basic.h"

#include <stdio.h>
#include <string.h>

static uint16_t sleep_face_text_width(const char *text)
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

void sleep_face_render(const power_rtc_datetime_t *dt)
{
  if (dt == NULL)
  {
    return;
  }

  char time_buf[12];
  char date_buf[16];
  (void)snprintf(time_buf, sizeof(time_buf), "%02u:%02u:%02u",
                 (unsigned)dt->hours, (unsigned)dt->minutes, (unsigned)dt->seconds);
  (void)snprintf(date_buf, sizeof(date_buf), "%02u-%02u-%04u",
                 (unsigned)dt->day, (unsigned)dt->month, (unsigned)dt->year);

  renderFill(false);

  uint16_t width = renderGetWidth();
  uint16_t height = renderGetHeight();
  uint16_t time_w = sleep_face_text_width(time_buf);
  uint16_t date_w = sleep_face_text_width(date_buf);

  uint16_t x_time = (width > time_w) ? (uint16_t)((width - time_w) / 2U) : 0U;
  uint16_t x_date = (width > date_w) ? (uint16_t)((width - date_w) / 2U) : 0U;

  uint16_t y_time = 0U;
  uint16_t y_date = (uint16_t)(FONT8X8_HEIGHT + 6U);
  if (height > (uint16_t)(FONT8X8_HEIGHT * 3U))
  {
    uint16_t block_h = (uint16_t)((FONT8X8_HEIGHT * 2U) + 6U);
    y_time = (uint16_t)((height - block_h) / 2U);
    y_date = (uint16_t)(y_time + FONT8X8_HEIGHT + 6U);
  }

  renderDrawText(x_time, y_time, time_buf, RENDER_LAYER_UI, RENDER_STATE_BLACK);
  renderDrawText(x_date, y_date, date_buf, RENDER_LAYER_UI, RENDER_STATE_BLACK);
}
