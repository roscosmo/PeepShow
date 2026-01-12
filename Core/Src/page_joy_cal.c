#include "ui_pages.h"

#include "display_renderer.h"
#include "font8x8_basic.h"
#include "sensor_task.h"
#include "ui_actions.h"

#include <stdbool.h>
#include <string.h>

static sensor_joy_status_t s_last_status;
static bool s_has_status = false;

static bool joy_status_changed(void)
{
  sensor_joy_status_t current;
  sensor_joy_get_status(&current);

  if (!s_has_status || (memcmp(&current, &s_last_status, sizeof(current)) != 0))
  {
    s_last_status = current;
    s_has_status = true;
    return true;
  }

  return false;
}

static void page_joy_cal_enter(void)
{
  s_has_status = false;
}

static uint32_t page_joy_cal_event(ui_evt_t evt)
{
  if (evt == UI_EVT_BACK)
  {
    ui_actions_send_sensor_req(APP_SENSOR_REQ_JOY_CAL_SAVE);
    return UI_PAGE_EVENT_BACK;
  }

  if (evt == UI_EVT_SELECT)
  {
    sensor_joy_status_t status_now;
    sensor_joy_get_status(&status_now);
    if ((status_now.stage == SENSOR_JOY_STAGE_IDLE) ||
        (status_now.stage == SENSOR_JOY_STAGE_DONE))
    {
      ui_actions_send_sensor_req(APP_SENSOR_REQ_JOY_CAL_NEUTRAL);
      return UI_PAGE_EVENT_RENDER;
    }
  }
  else if (evt == UI_EVT_TICK)
  {
    if (joy_status_changed())
    {
      return UI_PAGE_EVENT_RENDER;
    }
  }

  return UI_PAGE_EVENT_NONE;
}

static void page_joy_cal_render(void)
{
  sensor_joy_status_t status;
  sensor_joy_get_status(&status);

  renderFill(false);
  renderDrawText(4U, 4U, "JOYSTICK CAL", RENDER_LAYER_UI, RENDER_STATE_BLACK);

  const char *line1 = NULL;
  const char *line2 = NULL;

  switch (status.stage)
  {
    case SENSOR_JOY_STAGE_IDLE:
      line1 = "A: START CAL";
      line2 = "B: BACK";
      break;
    case SENSOR_JOY_STAGE_NEUTRAL:
      line1 = "HOLD NEUTRAL";
      line2 = "MEASURING...";
      break;
    case SENSOR_JOY_STAGE_UP:
      line1 = "HOLD UP";
      line2 = "KEEP STEADY";
      break;
    case SENSOR_JOY_STAGE_RIGHT:
      line1 = "HOLD RIGHT";
      line2 = "KEEP STEADY";
      break;
    case SENSOR_JOY_STAGE_DOWN:
      line1 = "HOLD DOWN";
      line2 = "KEEP STEADY";
      break;
    case SENSOR_JOY_STAGE_LEFT:
      line1 = "HOLD LEFT";
      line2 = "KEEP STEADY";
      break;
    case SENSOR_JOY_STAGE_SWEEP:
      line1 = "SWEEP FULL RANGE";
      line2 = "BIG CIRCLES";
      break;
    case SENSOR_JOY_STAGE_DONE:
    default:
      line1 = "CAL DONE";
      line2 = "B: BACK";
      break;
  }

  uint16_t y = 20U;
  if (line1 != NULL)
  {
    renderDrawText(4U, y, line1, RENDER_LAYER_UI, RENDER_STATE_BLACK);
    y = (uint16_t)(y + (FONT8X8_HEIGHT + 4U));
  }
  if (line2 != NULL)
  {
    renderDrawText(4U, y, line2, RENDER_LAYER_UI, RENDER_STATE_BLACK);
    y = (uint16_t)(y + (FONT8X8_HEIGHT + 6U));
  }

  if ((status.stage != SENSOR_JOY_STAGE_IDLE) && (status.stage != SENSOR_JOY_STAGE_DONE))
  {
    float p01 = status.progress;
    if (p01 < 0.0f)
    {
      p01 = 0.0f;
    }
    if (p01 > 1.0f)
    {
      p01 = 1.0f;
    }

    uint16_t width = renderGetWidth();
    uint16_t bar_x = (width > 40U) ? 20U : 2U;
    uint16_t bar_w = (width > 40U) ? (uint16_t)(width - 40U) : (uint16_t)(width - 4U);
    uint16_t bar_h = 10U;

    if (bar_w < 12U)
    {
      bar_w = 12U;
    }

    renderDrawRect(bar_x, y, bar_w, bar_h, RENDER_LAYER_UI, RENDER_STATE_BLACK);
    uint16_t fill = (uint16_t)((float)(bar_w - 2U) * p01);
    if (fill > 0U)
    {
      renderFillRect((uint16_t)(bar_x + 1U), (uint16_t)(y + 1U),
                     fill, (uint16_t)(bar_h - 2U),
                     RENDER_LAYER_UI, RENDER_STATE_BLACK);
    }
  }

  uint16_t height = renderGetHeight();
  if (height > (uint16_t)(FONT8X8_HEIGHT + 6U))
  {
    uint16_t hint_y = (uint16_t)(height - (FONT8X8_HEIGHT + 2U));
    renderDrawText(4U, hint_y, "BOOT: EXIT MENU", RENDER_LAYER_UI, RENDER_STATE_BLACK);
  }
}

const ui_page_t PAGE_JOY_CAL =
{
  .name = "Joy Cal",
  .enter = page_joy_cal_enter,
  .event = page_joy_cal_event,
  .render = page_joy_cal_render,
  .exit = NULL,
  .tick_ms = 100U,
  .flags = 0U
};
