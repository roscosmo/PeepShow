#include "ui_pages.h"

#include "ADP5360.h"
#include "display_renderer.h"
#include "font8x8_basic.h"
#include "sensor_task.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// Battery stats page
//
// Sharp Memory LCD is 168px wide. With FONT8X8 (8px), that is 21 characters.
// renderDrawText() does not clip, so we hard-clip every string we draw.

static sensor_power_status_t s_last;
static uint8_t s_has_last = 0U;

static const char *ui_yes_no(uint8_t v)
{
  return (v != 0U) ? "Y" : "N";
}

static const char *ui_chg_short(ADP5360_chg_state_t st)
{
  switch (st)
  {
    case ADP5360_CHG_OFF:            return "OFF";
    case ADP5360_CHG_TRICKLE:        return "TRK";
    case ADP5360_CHG_FAST_CC:        return "CC";
    case ADP5360_CHG_FAST_CV:        return "CV";
    case ADP5360_CHG_COMPLETE:       return "DONE";
    case ADP5360_CHG_LDO_MODE:       return "LDO";
    case ADP5360_CHG_TIMER_EXPIRED:  return "TMO";
    case ADP5360_CHG_BATT_DETECT:    return "DET";
    default:                         return "UNK";
  }
}

static const char *ui_bat_short(ADP5360_bat_chg_status_t b)
{
  switch (b)
  {
    case ADP5360_BATSTAT_NORMAL:   return "NORM";
    case ADP5360_BATSTAT_NO_BATT:  return "NOBAT";
    case ADP5360_BATSTAT_LE_VTRK:  return "DEAD";
    case ADP5360_BATSTAT_BETWEEN:  return "WEAK";
    case ADP5360_BATSTAT_GE_VWEAK: return "OK";
    default:                       return "UNK";
  }
}

static const char *ui_thr_short(ADP5360_thr_status_t t)
{
  switch (t)
  {
    case ADP5360_THR_OFF:   return "OFF";
    case ADP5360_THR_COLD:  return "COLD";
    case ADP5360_THR_COOL:  return "COOL";
    case ADP5360_THR_WARM:  return "WARM";
    case ADP5360_THR_HOT:   return "HOT";
    case ADP5360_THR_OK:    return "OK";
    default:                return "UNK";
  }
}

static void ui_draw_text_clipped(uint16_t x, uint16_t y, const char *text)
{
  if (text == NULL)
  {
    return;
  }

  const uint16_t w = renderGetWidth();
  const uint16_t max_chars = (w > x) ? (uint16_t)((w - x) / FONT8X8_WIDTH) : 0U;
  if (max_chars == 0U)
  {
    return;
  }

  char tmp[64];
  (void)strncpy(tmp, text, sizeof(tmp) - 1U);
  tmp[sizeof(tmp) - 1U] = '\0';

  if (strlen(tmp) > max_chars)
  {
    tmp[max_chars] = '\0';
  }

  renderDrawText(x, y, tmp, RENDER_LAYER_UI, RENDER_STATE_BLACK);
}

static void ui_draw_linef(uint16_t y, const char *fmt, ...)
{
  char buf[96];
  va_list ap;
  va_start(ap, fmt);
  (void)vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  // Use x=0 so we get the full 21 characters at 168px width.
  ui_draw_text_clipped(0U, y, buf);
}

static void page_batt_stats_enter(void)
{
  s_has_last = 0U;
}

static uint32_t page_batt_stats_event(ui_evt_t evt)
{
  if (evt == UI_EVT_BACK)
  {
    return UI_PAGE_EVENT_BACK;
  }

  if (evt == UI_EVT_TICK)
  {
    sensor_power_status_t current;
    sensor_power_get_status(&current);

    if ((s_has_last == 0U) || (memcmp(&current, &s_last, sizeof(current)) != 0))
    {
      s_last = current;
      s_has_last = 1U;
      return UI_PAGE_EVENT_RENDER;
    }
  }

  return UI_PAGE_EVENT_NONE;
}

static void page_batt_stats_render(void)
{
  sensor_power_status_t status;
  sensor_power_get_status(&status);

  renderFill(false);

  // Header
  ui_draw_text_clipped(0U, 0U, "BATTERY");
  renderDrawHLine(0U, (uint16_t)(FONT8X8_HEIGHT + 1U), renderGetWidth(), RENDER_LAYER_UI, RENDER_STATE_BLACK);

  const uint16_t step = (uint16_t)(FONT8X8_HEIGHT + 2U);
  uint16_t y = (uint16_t)(FONT8X8_HEIGHT + 4U);

  if (status.valid == 0U)
  {
    ui_draw_text_clipped(0U, y, "Reading...");
    return;
  }

  // Readbacks for config lines (fall back to -- if a read fails)
  uint16_t cap_mAh = 0U;
  uint8_t have_cap = (ADP5360_get_bat_capacity(&cap_mAh) == HAL_OK) ? 1U : 0U;

  uint16_t vtrm_mV = 0U;
  uint16_t itrk_deci_mA = 0U;
  uint8_t have_term = (ADP5360_get_chg_term(&vtrm_mV, &itrk_deci_mA) == HAL_OK) ? 1U : 0U;

  uint16_t iend_mA = 0U;
  uint16_t ichg_mA = 0U;
  uint8_t have_curr = (ADP5360_get_chg_current(&iend_mA, &ichg_mA) == HAL_OK) ? 1U : 0U;

  uint16_t vadpichg_mV = 0U;
  ADP5360_vsys_t vsys_mode = ADP5360_VSYS_VTRM_P200mV;
  uint16_t ilim_mA = 0U;
  uint8_t have_ilim = (ADP5360_get_vbus_ilim(&vadpichg_mV, &vsys_mode, &ilim_mA) == HAL_OK) ? 1U : 0U;

  // Line 1: VB + SOC (2dp volts)
  {
    const unsigned mv = (unsigned)status.vbat_mV;
    const unsigned v = mv / 1000U;
    const unsigned cV = (mv % 1000U) / 10U;
    ui_draw_linef(y, "VB %u.%02uV SOC %u%%", v, cV, (unsigned)status.soc_percent);
    y = (uint16_t)(y + step);
  }

  // Line 2: battery/charge state short tokens
  ui_draw_linef(y, "BST %s CHG %s", ui_bat_short(status.st2.bat_status), ui_chg_short(status.st1.state));
  y = (uint16_t)(y + step);

  // Line 3: power-good
  ui_draw_linef(y, "PG VBUS %s BAT %s", ui_yes_no(status.pgood.vbus_ok), ui_yes_no(status.pgood.bat_ok));
  y = (uint16_t)(y + step);

  // Line 4: protection flags (keep compact)
  ui_draw_linef(y, "UV%u OV%u TH %s",
                (unsigned)status.st2.bat_uv,
                (unsigned)status.st2.bat_ov,
                ui_thr_short(status.st2.thr));
  y = (uint16_t)(y + step);

  // Line 5-6-7: config summary using the three spare rows
  if ((have_term != 0U) && (have_ilim != 0U))
  {
    ui_draw_linef(y, "CFG V%u IL%u", (unsigned)vtrm_mV, (unsigned)ilim_mA);
  }
  else
  {
    ui_draw_linef(y, "CFG V-- IL--");
  }
  y = (uint16_t)(y + step);

  if (have_curr != 0U)
  {
    // iend is discrete; show as integer mA. (LIR2450 uses small values anyway.)
    ui_draw_linef(y, "CFG CH%u END%u", (unsigned)ichg_mA, (unsigned)iend_mA);
  }
  else
  {
    ui_draw_linef(y, "CFG CH-- END--");
  }
  y = (uint16_t)(y + step);

  // CAP + FLT (both are useful during bring-up)
  if (have_cap != 0U)
  {
    ui_draw_linef(y, "CAP%u FLT0x%02X", (unsigned)cap_mAh, (unsigned)status.fault_mask);
  }
  else
  {
    ui_draw_linef(y, "CAP-- FLT0x%02X", (unsigned)status.fault_mask);
  }
}

const ui_page_t PAGE_BATT_STATS =
{
  .name = "Battery Stats",
  .enter = page_batt_stats_enter,
  .event = page_batt_stats_event,
  .render = page_batt_stats_render,
  .exit = NULL,
  .tick_ms = 1000U,
  .flags = UI_PAGE_FLAG_POWER_STATS
};
