#include "ui_router.h"

#include "app_freertos.h"
#include "audio_task.h"
#include "display_renderer.h"
#include "font8x8_basic.h"
#include "sensor_task.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
  const char *label;
  ui_router_cmd_t cmd;
} ui_menu_item_t;

static const ui_menu_item_t k_menu_items[] =
{
  { "Joystick Cal", UI_ROUTER_CMD_OPEN_JOY_CAL },
  { "Joy Target", UI_ROUTER_CMD_OPEN_JOY_TARGET },
  { "Joy Cursor", UI_ROUTER_CMD_OPEN_JOY_CURSOR },
  { "Render Demo", UI_ROUTER_CMD_START_RENDER_DEMO },
  { "Sound Settings", UI_ROUTER_CMD_OPEN_SOUND }
};

static const uint8_t k_menu_item_count = (uint8_t)(sizeof(k_menu_items) / sizeof(k_menu_items[0]));

typedef struct
{
  ui_page_t page;
  uint8_t menu_index;
} ui_router_state_t;

static ui_router_state_t s_ui =
{
  .page = UI_PAGE_MENU,
  .menu_index = 0U
};

static uint16_t s_joy_cursor_x = 0U;
static uint16_t s_joy_cursor_y = 0U;
static bool s_keyclick_enabled = true;

static const uint16_t k_cursor_sprite_w = 32U;
static const uint16_t k_cursor_sprite_h = 32U;
static const uint8_t k_cursor_sprite[] =
{
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x8F, 0xC3, 0x80, 0x01, 0xE2, 0xF3, 0x80,
  0x01, 0xED, 0x3C, 0xC0, 0x01, 0xDF, 0x38, 0xC0, 0x00, 0x84, 0xF8, 0x80, 0x00, 0xDF, 0xFE, 0x80,
  0x00, 0xFF, 0xFF, 0x00, 0x01, 0xBF, 0xFF, 0x00, 0x02, 0x00, 0x3F, 0x80, 0x06, 0x7F, 0xDF, 0xC0,
  0x0F, 0xFF, 0xFF, 0xE0, 0x03, 0xC2, 0xE7, 0xC0, 0x03, 0xB3, 0xB3, 0x80, 0x03, 0xF3, 0xF8, 0x80,
  0x07, 0xE1, 0xFC, 0xFC, 0x36, 0xC8, 0xFC, 0xFC, 0x3E, 0x08, 0x31, 0xCC, 0x1B, 0x00, 0x07, 0xE4,
  0x09, 0xC0, 0x1F, 0x76, 0x07, 0xFF, 0xFB, 0x9C, 0x03, 0xFF, 0xFD, 0xDC, 0x01, 0x9F, 0x9D, 0xEC,
  0x01, 0xFF, 0xBF, 0xF8, 0x01, 0xFB, 0xFB, 0xB0, 0x01, 0xFB, 0xFF, 0xE0, 0x00, 0xFF, 0xFF, 0xC0,
  0x00, 0xFF, 0xFE, 0x00, 0x00, 0x6F, 0xCC, 0x00, 0x00, 0x7C, 0xFC, 0x00, 0x00, 0x10, 0x30, 0x00
};

static uint16_t ui_text_width(const char *text)
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

static void ui_render_home(uint16_t width, uint16_t height)
{
  (void)width;
  (void)height;

  renderFill(false);
  renderDrawText(4U, 4U, "HOME", RENDER_LAYER_UI, RENDER_STATE_BLACK);
  renderDrawText(4U, 16U, "BOOT: MENU", RENDER_LAYER_UI, RENDER_STATE_BLACK);
}

static void ui_render_menu(uint16_t width, uint16_t height)
{
  renderFill(false);
  renderDrawText(4U, 4U, "MENU", RENDER_LAYER_UI, RENDER_STATE_BLACK);

  uint16_t y = 20U;
  for (uint8_t i = 0U; i < k_menu_item_count; ++i)
  {
    const char *label = k_menu_items[i].label;
    bool selected = (i == s_ui.menu_index);
    if (selected)
    {
      uint16_t text_w = ui_text_width(label);
      uint16_t box_w = (uint16_t)(text_w + 4U);
      if (box_w < 10U)
      {
        box_w = 10U;
      }
      renderFillRect(2U, (uint16_t)(y - 1U), box_w, (uint16_t)(FONT8X8_HEIGHT + 2U),
                     RENDER_LAYER_UI, RENDER_STATE_BLACK);
      renderDrawText(4U, y, label, RENDER_LAYER_UI, RENDER_STATE_WHITE);
    }
    else
    {
      renderDrawText(4U, y, label, RENDER_LAYER_UI, RENDER_STATE_BLACK);
    }
    y = (uint16_t)(y + (FONT8X8_HEIGHT + 4U));
    if ((height > 0U) && (y >= height))
    {
      break;
    }
  }

  if (height > (uint16_t)(FONT8X8_HEIGHT + 6U))
  {
    uint16_t hint_y = (uint16_t)(height - (FONT8X8_HEIGHT + 2U));
    renderDrawText(4U, hint_y, "A: ENTER  B: BACK", RENDER_LAYER_UI, RENDER_STATE_BLACK);
  }
}

static void ui_render_sound(uint16_t width, uint16_t height)
{
  renderFill(false);
  renderDrawText(4U, 4U, "SOUND SETTINGS", RENDER_LAYER_UI, RENDER_STATE_BLACK);

  uint8_t volume = audio_get_volume();
  char buf[24];
  (void)snprintf(buf, sizeof(buf), "VOLUME: %u/10", (unsigned)volume);
  renderDrawText(4U, 20U, buf, RENDER_LAYER_UI, RENDER_STATE_BLACK);

  const char *kc = s_keyclick_enabled ? "ON" : "OFF";
  (void)snprintf(buf, sizeof(buf), "KEYCLICK: %s", kc);
  renderDrawText(4U, (uint16_t)(20U + FONT8X8_HEIGHT + 4U),
                 buf, RENDER_LAYER_UI, RENDER_STATE_BLACK);

  if (height > (uint16_t)(FONT8X8_HEIGHT + 6U))
  {
    uint16_t hint_y = (uint16_t)(height - (FONT8X8_HEIGHT * 2U + 4U));
    renderDrawText(4U, hint_y, "L/R: LEVEL  A: TOGGLE", RENDER_LAYER_UI, RENDER_STATE_BLACK);
    renderDrawText(4U, (uint16_t)(hint_y + FONT8X8_HEIGHT + 2U),
                   "B: BACK", RENDER_LAYER_UI, RENDER_STATE_BLACK);
  }

  (void)width;
}

static void ui_render_joy_cal(uint16_t width, uint16_t height)
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

  if (height > (uint16_t)(FONT8X8_HEIGHT + 6U))
  {
    uint16_t hint_y = (uint16_t)(height - (FONT8X8_HEIGHT + 2U));
    renderDrawText(4U, hint_y, "BOOT: EXIT MENU", RENDER_LAYER_UI, RENDER_STATE_BLACK);
  }
}

static void ui_render_joy_target(uint16_t width, uint16_t height)
{
  sensor_joy_status_t status;
  sensor_joy_get_status(&status);

  renderFill(false);
  renderDrawText(4U, 4U, "JOY TARGET", RENDER_LAYER_UI, RENDER_STATE_BLACK);

  uint16_t top = (uint16_t)(FONT8X8_HEIGHT + 6U);
  uint16_t bottom = (uint16_t)(FONT8X8_HEIGHT * 2U + 12U);
  if (height <= (top + bottom + 10U))
  {
    bottom = 10U;
  }

  uint16_t usable_h = 0U;
  if (height > (top + bottom))
  {
    usable_h = (uint16_t)(height - top - bottom);
  }
  else if (height > top)
  {
    usable_h = (uint16_t)(height - top);
  }

  uint16_t cy = (uint16_t)(top + (usable_h / 2U));
  uint16_t cx = (uint16_t)(width / 2U);
  uint16_t max_r = (width / 2U);
  uint16_t half_h = (uint16_t)(usable_h / 2U);
  if (half_h < max_r)
  {
    max_r = half_h;
  }
  uint16_t R = (max_r > 6U) ? (uint16_t)(max_r - 6U) : max_r;

  renderDrawLine((uint16_t)(cx - R), cy, (uint16_t)(cx + R), cy, RENDER_LAYER_UI, RENDER_STATE_BLACK);
  renderDrawLine(cx, (uint16_t)(cy - R), cx, (uint16_t)(cy + R), RENDER_LAYER_UI, RENDER_STATE_BLACK);

  float max_span = (status.sx_mT > status.sy_mT) ? status.sx_mT : status.sy_mT;
  if (max_span < 1e-3f)
  {
    max_span = 1.0f;
  }

  uint16_t rx = (uint16_t)((status.sx_mT / max_span) * (float)R);
  uint16_t ry = (uint16_t)((status.sy_mT / max_span) * (float)R);
  if (rx < 1U)
  {
    rx = 1U;
  }
  if (ry < 1U)
  {
    ry = 1U;
  }

  const uint8_t segments = 32U;
  float prev_x = (float)cx + (float)rx;
  float prev_y = (float)cy;
  for (uint8_t i = 1U; i <= segments; ++i)
  {
    float a = (2.0f * 3.14159265f * (float)i) / (float)segments;
    float x = (float)cx + cosf(a) * (float)rx;
    float y = (float)cy + sinf(a) * (float)ry;
    renderDrawLine((uint16_t)prev_x, (uint16_t)prev_y,
                   (uint16_t)x, (uint16_t)y,
                   RENDER_LAYER_UI, RENDER_STATE_BLACK);
    prev_x = x;
    prev_y = y;
  }

  if (status.thr_mT > 0.0f)
  {
    float r_thr = (status.thr_mT / max_span) * (float)R;
    if (r_thr > (float)R)
    {
      r_thr = (float)R;
    }
    renderDrawCircle(cx, cy, (uint16_t)r_thr, RENDER_LAYER_UI, RENDER_STATE_BLACK);
  }

  if ((status.deadzone_en != 0U) && (status.deadzone_mT > 0.0f))
  {
    float r_dz = (status.deadzone_mT / max_span) * (float)R;
    if (r_dz > (float)R)
    {
      r_dz = (float)R;
    }
    renderDrawCircle(cx, cy, (uint16_t)r_dz, RENDER_LAYER_UI, RENDER_STATE_BLACK);
  }

  float scale_x = (status.sx_mT / max_span) * (float)R;
  float scale_y = (status.sy_mT / max_span) * (float)R;
  if (scale_x < 1.0f)
  {
    scale_x = 1.0f;
  }
  if (scale_y < 1.0f)
  {
    scale_y = 1.0f;
  }
  int16_t px = (int16_t)((int32_t)cx + (int32_t)(status.nx * scale_x));
  int16_t py = (int16_t)((int32_t)cy - (int32_t)(status.ny * scale_y));
  renderFillCircle((uint16_t)px, (uint16_t)py, 2U, RENDER_LAYER_GAME, RENDER_STATE_BLACK);

  uint16_t text_y1 = (uint16_t)(height - (FONT8X8_HEIGHT * 2U + 2U));
  uint16_t text_y2 = (uint16_t)(height - (FONT8X8_HEIGHT + 2U));
  char buf[32];
  (void)snprintf(buf, sizeof(buf), "r=%.1fmT", status.r_abs_mT);
  renderDrawText(6U, text_y1, buf, RENDER_LAYER_UI, RENDER_STATE_BLACK);
  renderDrawText((uint16_t)(width / 2U), text_y1, "L/R: DEADZONE",
                 RENDER_LAYER_UI, RENDER_STATE_BLACK);
  (void)snprintf(buf, sizeof(buf), "thr=%.1fmT", status.thr_mT);
  renderDrawText(6U, text_y2, buf, RENDER_LAYER_UI, RENDER_STATE_BLACK);
  (void)snprintf(buf, sizeof(buf), "dz=%.1fmT", status.deadzone_mT);
  renderDrawText((uint16_t)(width / 2U), text_y2, buf,
                 RENDER_LAYER_UI, RENDER_STATE_BLACK);
}

static void ui_render_joy_cursor(uint16_t width, uint16_t height)
{
  renderFill(false);
  renderDrawRect(0U, 0U, width, height, RENDER_LAYER_UI, RENDER_STATE_BLACK);
  renderDrawText(4U, 4U, "JOY CURSOR", RENDER_LAYER_UI, RENDER_STATE_BLACK);
  if (height > (uint16_t)(FONT8X8_HEIGHT + 6U))
  {
    uint16_t hint_y = (uint16_t)(height - (FONT8X8_HEIGHT + 2U));
    renderDrawText(4U, hint_y, "B: BACK", RENDER_LAYER_UI, RENDER_STATE_BLACK);
  }

  renderBlit1bppMsb(s_joy_cursor_x, s_joy_cursor_y,
                    k_cursor_sprite_w, k_cursor_sprite_h,
                    k_cursor_sprite, 0U,
                    RENDER_LAYER_GAME, RENDER_STATE_BLACK);
}

void ui_router_init(void)
{
  s_ui.page = UI_PAGE_MENU;
  s_ui.menu_index = 0U;
  s_joy_cursor_x = 0U;
  s_joy_cursor_y = 0U;
}

ui_page_t ui_router_get_page(void)
{
  return s_ui.page;
}

void ui_router_set_page(ui_page_t page)
{
  s_ui.page = page;
}

bool ui_router_handle_button(uint32_t button_id, ui_router_cmd_t *out_cmd)
{
  if (out_cmd != NULL)
  {
    *out_cmd = UI_ROUTER_CMD_NONE;
  }

  ui_page_t prev_page = s_ui.page;
  uint8_t prev_index = s_ui.menu_index;

  if (s_ui.page == UI_PAGE_MENU)
  {
    if (button_id == (uint32_t)APP_BUTTON_A)
    {
      if (out_cmd != NULL)
      {
        *out_cmd = k_menu_items[s_ui.menu_index].cmd;
      }
    }
    else if (button_id == (uint32_t)APP_BUTTON_L)
    {
      if (k_menu_item_count > 1U)
      {
        if (s_ui.menu_index == 0U)
        {
          s_ui.menu_index = (uint8_t)(k_menu_item_count - 1U);
        }
        else
        {
          s_ui.menu_index--;
        }
      }
    }
    else if (button_id == (uint32_t)APP_BUTTON_R)
    {
      if (k_menu_item_count > 1U)
      {
        s_ui.menu_index = (uint8_t)((s_ui.menu_index + 1U) % k_menu_item_count);
      }
    }
  }

  return ((s_ui.page != prev_page) || (s_ui.menu_index != prev_index));
}

void ui_router_render(void)
{
  uint16_t width = renderGetWidth();
  uint16_t height = renderGetHeight();
  if ((width == 0U) || (height == 0U))
  {
    return;
  }

  switch (s_ui.page)
  {
    case UI_PAGE_MENU:
      ui_render_menu(width, height);
      break;
    case UI_PAGE_HOME:
      ui_render_home(width, height);
      break;
    case UI_PAGE_JOY_CAL:
      ui_render_joy_cal(width, height);
      break;
    case UI_PAGE_JOY_TARGET:
      ui_render_joy_target(width, height);
      break;
    case UI_PAGE_JOY_CURSOR:
      ui_render_joy_cursor(width, height);
      break;
    case UI_PAGE_SOUND:
      ui_render_sound(width, height);
      break;
    default:
      ui_render_menu(width, height);
      break;
  }
}

void ui_router_set_joy_cursor(uint16_t x, uint16_t y)
{
  s_joy_cursor_x = x;
  s_joy_cursor_y = y;
}

bool ui_router_get_keyclick(void)
{
  return s_keyclick_enabled;
}

void ui_router_set_keyclick(bool enable)
{
  s_keyclick_enabled = enable;
}
