#include "ui_pages.h"

#include "display_renderer.h"
#include "font8x8_basic.h"
#include "main.h"
#include "power_task.h"

#include <stdio.h>

extern RTC_HandleTypeDef hrtc;

typedef enum
{
  RTC_FIELD_HOUR = 0,
  RTC_FIELD_MINUTE = 1,
  RTC_FIELD_SECOND = 2,
  RTC_FIELD_DAY = 3,
  RTC_FIELD_MONTH = 4,
  RTC_FIELD_YEAR = 5,
  RTC_FIELD_COUNT = 6
} rtc_field_t;

static power_rtc_datetime_t s_edit = {0};
static rtc_field_t s_field = RTC_FIELD_HOUR;

static uint8_t rtc_is_leap_year(uint16_t year)
{
  return ((year % 4U) == 0U) ? 1U : 0U;
}

static uint8_t rtc_days_in_month(uint16_t year, uint8_t month)
{
  static const uint8_t kDays[12] = { 31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U };
  if ((month == 0U) || (month > 12U))
  {
    return 31U;
  }

  uint8_t days = kDays[month - 1U];
  if ((month == 2U) && (rtc_is_leap_year(year) != 0U))
  {
    days = 29U;
  }
  return days;
}

static void rtc_clamp_day(power_rtc_datetime_t *dt)
{
  if (dt == NULL)
  {
    return;
  }

  uint8_t dim = rtc_days_in_month(dt->year, dt->month);
  if (dt->day < 1U)
  {
    dt->day = 1U;
  }
  if (dt->day > dim)
  {
    dt->day = dim;
  }
}

static void page_rtc_set_enter(void)
{
  RTC_TimeTypeDef t = {0};
  RTC_DateTypeDef d = {0};
  if (HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN) == HAL_OK)
  {
    s_edit.hours = (uint8_t)t.Hours;
    s_edit.minutes = (uint8_t)t.Minutes;
    s_edit.seconds = (uint8_t)t.Seconds;
  }
  if (HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN) == HAL_OK)
  {
    s_edit.day = (uint8_t)d.Date;
    s_edit.month = (uint8_t)d.Month;
    s_edit.year = (uint16_t)(2000U + (uint16_t)d.Year);
  }
  else
  {
    s_edit.day = 1U;
    s_edit.month = 1U;
    s_edit.year = 2000U;
  }
  rtc_clamp_day(&s_edit);
  s_field = RTC_FIELD_HOUR;
}

static void rtc_field_inc(power_rtc_datetime_t *dt, rtc_field_t field, uint8_t inc)
{
  if (dt == NULL)
  {
    return;
  }

  switch (field)
  {
    case RTC_FIELD_HOUR:
      if (inc != 0U)
      {
        dt->hours = (uint8_t)((dt->hours + 1U) % 24U);
      }
      else
      {
        dt->hours = (dt->hours == 0U) ? 23U : (uint8_t)(dt->hours - 1U);
      }
      break;
    case RTC_FIELD_MINUTE:
      if (inc != 0U)
      {
        dt->minutes = (uint8_t)((dt->minutes + 1U) % 60U);
      }
      else
      {
        dt->minutes = (dt->minutes == 0U) ? 59U : (uint8_t)(dt->minutes - 1U);
      }
      break;
    case RTC_FIELD_SECOND:
      if (inc != 0U)
      {
        dt->seconds = (uint8_t)((dt->seconds + 1U) % 60U);
      }
      else
      {
        dt->seconds = (dt->seconds == 0U) ? 59U : (uint8_t)(dt->seconds - 1U);
      }
      break;
    case RTC_FIELD_DAY:
    {
      uint8_t dim = rtc_days_in_month(dt->year, dt->month);
      if (inc != 0U)
      {
        dt->day = (dt->day >= dim) ? 1U : (uint8_t)(dt->day + 1U);
      }
      else
      {
        dt->day = (dt->day <= 1U) ? dim : (uint8_t)(dt->day - 1U);
      }
      break;
    }
    case RTC_FIELD_MONTH:
      if (inc != 0U)
      {
        dt->month = (dt->month >= 12U) ? 1U : (uint8_t)(dt->month + 1U);
      }
      else
      {
        dt->month = (dt->month <= 1U) ? 12U : (uint8_t)(dt->month - 1U);
      }
      rtc_clamp_day(dt);
      break;
    case RTC_FIELD_YEAR:
      if (inc != 0U)
      {
        dt->year = (dt->year >= 2099U) ? 2000U : (uint16_t)(dt->year + 1U);
      }
      else
      {
        dt->year = (dt->year <= 2000U) ? 2099U : (uint16_t)(dt->year - 1U);
      }
      rtc_clamp_day(dt);
      break;
    default:
      break;
  }
}

static uint32_t page_rtc_set_event(ui_evt_t evt)
{
  if (evt == UI_EVT_BACK)
  {
    return UI_PAGE_EVENT_BACK;
  }

  if (evt == UI_EVT_NAV_UP)
  {
    s_field = (s_field == RTC_FIELD_HOUR) ? (rtc_field_t)(RTC_FIELD_COUNT - 1U) : (rtc_field_t)(s_field - 1U);
    return UI_PAGE_EVENT_RENDER;
  }

  if (evt == UI_EVT_NAV_DOWN)
  {
    s_field = (rtc_field_t)((s_field + 1U) % (uint8_t)RTC_FIELD_COUNT);
    return UI_PAGE_EVENT_RENDER;
  }

  if (evt == UI_EVT_DEC)
  {
    rtc_field_inc(&s_edit, s_field, 0U);
    return UI_PAGE_EVENT_RENDER;
  }

  if (evt == UI_EVT_INC)
  {
    rtc_field_inc(&s_edit, s_field, 1U);
    return UI_PAGE_EVENT_RENDER;
  }

  if (evt == UI_EVT_SELECT)
  {
    power_task_request_rtc_set(&s_edit);
    return UI_PAGE_EVENT_RENDER;
  }

  return UI_PAGE_EVENT_NONE;
}

static void page_rtc_set_render(void)
{
  renderFill(false);
  renderDrawText(4U, 4U, "SET TIME/DATE", RENDER_LAYER_UI, RENDER_STATE_BLACK);

  char time_buf[12];
  char date_buf[16];
  (void)snprintf(time_buf, sizeof(time_buf), "%02u:%02u:%02u",
                 (unsigned)s_edit.hours, (unsigned)s_edit.minutes, (unsigned)s_edit.seconds);
  (void)snprintf(date_buf, sizeof(date_buf), "%02u-%02u-%04u",
                 (unsigned)s_edit.day, (unsigned)s_edit.month, (unsigned)s_edit.year);

  const uint16_t x = 8U;
  uint16_t y = 22U;
  renderDrawText(x, y, time_buf, RENDER_LAYER_UI, RENDER_STATE_BLACK);
  y = (uint16_t)(y + FONT8X8_HEIGHT + 6U);
  renderDrawText(x, y, date_buf, RENDER_LAYER_UI, RENDER_STATE_BLACK);

  uint16_t field_w2 = (uint16_t)((2U * (FONT8X8_WIDTH + 1U)) - 1U);
  uint16_t field_w4 = (uint16_t)((4U * (FONT8X8_WIDTH + 1U)) - 1U);
  uint16_t x_sel = x;
  uint16_t y_sel = 22U;
  uint16_t w_sel = field_w2;

  switch (s_field)
  {
    case RTC_FIELD_HOUR:
      x_sel = x;
      y_sel = 22U;
      w_sel = field_w2;
      break;
    case RTC_FIELD_MINUTE:
      x_sel = (uint16_t)(x + (uint16_t)(3U * (FONT8X8_WIDTH + 1U)));
      y_sel = 22U;
      w_sel = field_w2;
      break;
    case RTC_FIELD_SECOND:
      x_sel = (uint16_t)(x + (uint16_t)(6U * (FONT8X8_WIDTH + 1U)));
      y_sel = 22U;
      w_sel = field_w2;
      break;
    case RTC_FIELD_DAY:
      x_sel = x;
      y_sel = (uint16_t)(22U + FONT8X8_HEIGHT + 6U);
      w_sel = field_w2;
      break;
    case RTC_FIELD_MONTH:
      x_sel = (uint16_t)(x + (uint16_t)(3U * (FONT8X8_WIDTH + 1U)));
      y_sel = (uint16_t)(22U + FONT8X8_HEIGHT + 6U);
      w_sel = field_w2;
      break;
    case RTC_FIELD_YEAR:
      x_sel = (uint16_t)(x + (uint16_t)(6U * (FONT8X8_WIDTH + 1U)));
      y_sel = (uint16_t)(22U + FONT8X8_HEIGHT + 6U);
      w_sel = field_w4;
      break;
    default:
      break;
  }

  renderDrawRect(x_sel, (uint16_t)(y_sel - 1U),
                 (uint16_t)(w_sel + 2U), (uint16_t)(FONT8X8_HEIGHT + 2U),
                 RENDER_LAYER_UI, RENDER_STATE_BLACK);

  uint16_t height = renderGetHeight();
  if (height > (uint16_t)(FONT8X8_HEIGHT + 6U))
  {
    uint16_t hint_y = (uint16_t)(height - (FONT8X8_HEIGHT * 2U + 4U));
    renderDrawText(4U, hint_y, "U/D: FIELD  L/R: ADJUST",
                   RENDER_LAYER_UI, RENDER_STATE_BLACK);
    renderDrawText(4U, (uint16_t)(hint_y + FONT8X8_HEIGHT + 2U),
                   "A: SET  B: BACK",
                   RENDER_LAYER_UI, RENDER_STATE_BLACK);
  }
}

const ui_page_t PAGE_RTC_SET =
{
  .name = "Set Time/Date",
  .enter = page_rtc_set_enter,
  .event = page_rtc_set_event,
  .render = page_rtc_set_render,
  .exit = NULL,
  .tick_ms = 0U,
  .flags = UI_PAGE_FLAG_JOY_MENU
};
