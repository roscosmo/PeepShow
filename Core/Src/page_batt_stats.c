#include "ui_pages.h"

#include "ADP5360.h"
#include "display_renderer.h"
#include "font8x8_basic.h"
#include "sensor_task.h"

#include <stdio.h>
#include <string.h>

static sensor_power_status_t s_last;
static uint8_t s_has_last = 0U;

static const char *ui_safe_str(const char *text)
{
  return (text != NULL) ? text : "UNK";
}

static const char *ui_yes_no(uint8_t value)
{
  return (value != 0U) ? "Y" : "N";
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
    if ((s_has_last == 0U) ||
        (memcmp(&current, &s_last, sizeof(current)) != 0))
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
  renderDrawText(4U, 4U, "BATTERY STATS", RENDER_LAYER_UI, RENDER_STATE_BLACK);

  uint16_t y = (uint16_t)(FONT8X8_HEIGHT + 8U);
  uint16_t step = (uint16_t)(FONT8X8_HEIGHT + 2U);

  if (status.valid == 0U)
  {
    renderDrawText(4U, y, "Reading...", RENDER_LAYER_UI, RENDER_STATE_BLACK);
    return;
  }

  char line[32];
  (void)snprintf(line, sizeof(line), "VBAT: %u mV", (unsigned)status.vbat_mV);
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_STATE_BLACK);
  y = (uint16_t)(y + step);

  (void)snprintf(line, sizeof(line), "SOC: %u%%", (unsigned)status.soc_percent);
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_STATE_BLACK);
  y = (uint16_t)(y + step);

  (void)snprintf(line, sizeof(line), "CHG: %s",
                 ui_safe_str(ADP5360_chg_state_str(status.st1.state)));
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_STATE_BLACK);
  y = (uint16_t)(y + step);

  (void)snprintf(line, sizeof(line), "BATSTAT: %s",
                 ui_safe_str(ADP5360_bat_status_str(status.st2.bat_status)));
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_STATE_BLACK);
  y = (uint16_t)(y + step);

  (void)snprintf(line, sizeof(line), "VBUS:%s BATOK:%s",
                 ui_yes_no(status.pgood.vbus_ok),
                 ui_yes_no(status.pgood.bat_ok));
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_STATE_BLACK);
  y = (uint16_t)(y + step);

  (void)snprintf(line, sizeof(line), "UV:%u OV:%u THR:%s",
                 (unsigned)status.st2.bat_uv,
                 (unsigned)status.st2.bat_ov,
                 ui_safe_str(ADP5360_thr_status_str(status.st2.thr)));
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_STATE_BLACK);
  y = (uint16_t)(y + step);

  (void)snprintf(line, sizeof(line), "FLT: 0x%02X",
                 (unsigned)status.fault_mask);
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_STATE_BLACK);
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
