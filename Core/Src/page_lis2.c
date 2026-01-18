#include "ui_pages.h"

#include "display_renderer.h"
#include "font8x8_basic.h"
#include "sensor_task.h"
#include "LIS2DUX12.h"

#include <stdio.h>
#include <string.h>

static sensor_lis2_status_t s_last;
static uint8_t s_has_last = 0U;

static const char *lis2_odr_label(uint8_t odr)
{
  switch (odr)
  {
    case LIS2DUX12_OFF:      return "OFF";
    case LIS2DUX12_1Hz6_ULP: return "1.6U";
    case LIS2DUX12_3Hz_ULP:  return "3U";
    case LIS2DUX12_25Hz_ULP: return "25U";
    case LIS2DUX12_6Hz_LP:   return "6LP";
    case LIS2DUX12_12Hz5_LP: return "12.5L";
    case LIS2DUX12_25Hz_LP:  return "25LP";
    case LIS2DUX12_50Hz_LP:  return "50LP";
    case LIS2DUX12_100Hz_LP: return "100LP";
    case LIS2DUX12_200Hz_LP: return "200LP";
    case LIS2DUX12_400Hz_LP: return "400LP";
    case LIS2DUX12_800Hz_LP: return "800LP";
    case LIS2DUX12_6Hz_HP:   return "6HP";
    case LIS2DUX12_12Hz5_HP: return "12.5H";
    case LIS2DUX12_25Hz_HP:  return "25HP";
    case LIS2DUX12_50Hz_HP:  return "50HP";
    case LIS2DUX12_100Hz_HP: return "100HP";
    case LIS2DUX12_200Hz_HP: return "200HP";
    case LIS2DUX12_400Hz_HP: return "400HP";
    case LIS2DUX12_800Hz_HP: return "800HP";
    case LIS2DUX12_TRIG_PIN: return "TRIGP";
    case LIS2DUX12_TRIG_SW:  return "TRIGS";
    default:                 return "UNK";
  }
}

static const char *lis2_fs_label(uint8_t fs)
{
  switch (fs)
  {
    case LIS2DUX12_2g:  return "2G";
    case LIS2DUX12_4g:  return "4G";
    case LIS2DUX12_8g:  return "8G";
    case LIS2DUX12_16g: return "16G";
    default:            return "UNK";
  }
}

static const char *lis2_bw_label(uint8_t bw)
{
  switch (bw)
  {
    case LIS2DUX12_ODR_div_2:  return "/2";
    case LIS2DUX12_ODR_div_4:  return "/4";
    case LIS2DUX12_ODR_div_8:  return "/8";
    case LIS2DUX12_ODR_div_16: return "/16";
    default:                   return "/?";
  }
}

static const char *lis2_err_label(uint8_t err_src, char *buf, size_t len)
{
  if ((buf == NULL) || (len < 3U))
  {
    return "";
  }

  if (err_src == 0U)
  {
    (void)snprintf(buf, len, "--");
    return buf;
  }

  uint32_t pos = 0U;
  if ((err_src & SENSOR_LIS2_ERR_SRC_STATUS) != 0U)
  {
    buf[pos++] = 'S';
  }
  if ((err_src & SENSOR_LIS2_ERR_SRC_EMB) != 0U)
  {
    buf[pos++] = 'E';
  }
  if ((err_src & SENSOR_LIS2_ERR_SRC_XL) != 0U)
  {
    buf[pos++] = 'X';
  }
  if ((err_src & SENSOR_LIS2_ERR_SRC_TEMP) != 0U)
  {
    buf[pos++] = 'T';
  }
  if ((err_src & SENSOR_LIS2_ERR_SRC_STEP) != 0U)
  {
    buf[pos++] = 'P';
  }

  if (pos >= len)
  {
    pos = len - 1U;
  }
  buf[pos] = '\0';
  return buf;
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

  char tmp[48];
  (void)strncpy(tmp, text, sizeof(tmp) - 1U);
  tmp[sizeof(tmp) - 1U] = '\0';

  if (strlen(tmp) > max_chars)
  {
    tmp[max_chars] = '\0';
  }

  renderDrawText(x, y, tmp, RENDER_LAYER_UI, RENDER_STATE_BLACK);
}

static int32_t round_i32(float v)
{
  return (int32_t)(v >= 0.0f ? (v + 0.5f) : (v - 0.5f));
}

static uint32_t abs_u32(int32_t v)
{
  return (v < 0) ? (uint32_t)(-v) : (uint32_t)v;
}

static void page_lis2_imu_enter(void)
{
  s_has_last = 0U;
}

static uint32_t page_lis2_imu_event(ui_evt_t evt)
{
  if (evt == UI_EVT_BACK)
  {
    return UI_PAGE_EVENT_BACK;
  }

  if (evt == UI_EVT_TICK)
  {
    sensor_lis2_status_t status;
    sensor_lis2_get_status(&status);
    if ((s_has_last == 0U) ||
        (status.sample_seq != s_last.sample_seq) ||
        (status.error_count != s_last.error_count) ||
        (status.err_src != s_last.err_src) ||
        (status.init_ok != s_last.init_ok) ||
        (status.id_valid != s_last.id_valid))
    {
      s_last = status;
      s_has_last = 1U;
      return UI_PAGE_EVENT_RENDER;
    }
  }

  return UI_PAGE_EVENT_NONE;
}

static void page_lis2_imu_render(void)
{
  sensor_lis2_status_t status;
  sensor_lis2_get_status(&status);

  renderFill(false);
  ui_draw_text_clipped(4U, 4U, "IMU");

  uint16_t y = (uint16_t)(FONT8X8_HEIGHT + 8U);
  uint16_t step = (uint16_t)(FONT8X8_HEIGHT + 2U);
  char line[48];
  char mgx[8];
  char mgy[8];
  char mgz[8];
  char temp_buf[12];
  char err_buf[8];

  if (status.id_valid != 0U)
  {
    const char *init_label = (status.init_ok != 0U) ? "OK" : "ERR";
    (void)snprintf(line, sizeof(line), "ID 0x%02X @0x%02X %s",
                   (unsigned)status.device_id, (unsigned)status.i2c_addr_7b, init_label);
  }
  else
  {
    if (status.i2c_addr_7b != 0U)
    {
      (void)snprintf(line, sizeof(line), "ID -- @0x%02X ERR",
                     (unsigned)status.i2c_addr_7b);
    }
    else
    {
      (void)snprintf(line, sizeof(line), "ID -- @-- ERR");
    }
  }
  ui_draw_text_clipped(0U, y, line);
  y = (uint16_t)(y + step);

  if (status.init_ok != 0U)
  {
    (void)snprintf(line, sizeof(line), "ODR %s FS%s BW%s",
                   lis2_odr_label(status.odr),
                   lis2_fs_label(status.fs),
                   lis2_bw_label(status.bw));
  }
  else
  {
    (void)snprintf(line, sizeof(line), "ODR -- FS -- BW--");
  }
  ui_draw_text_clipped(0U, y, line);
  y = (uint16_t)(y + step);

  if (status.accel_valid != 0U)
  {
    int32_t x_mg = round_i32(status.accel_mg[0]);
    int32_t y_mg = round_i32(status.accel_mg[1]);
    int32_t z_mg = round_i32(status.accel_mg[2]);
    if (x_mg > 9999)
    {
      x_mg = 9999;
    }
    else if (x_mg < -9999)
    {
      x_mg = -9999;
    }
    if (y_mg > 9999)
    {
      y_mg = 9999;
    }
    else if (y_mg < -9999)
    {
      y_mg = -9999;
    }
    if (z_mg > 9999)
    {
      z_mg = 9999;
    }
    else if (z_mg < -9999)
    {
      z_mg = -9999;
    }

    (void)snprintf(mgx, sizeof(mgx), "%+05ld", (long)x_mg);
    (void)snprintf(mgy, sizeof(mgy), "%+05ld", (long)y_mg);
    (void)snprintf(mgz, sizeof(mgz), "%+05ld", (long)z_mg);
    (void)snprintf(line, sizeof(line), "MG X:%s Y:%s", mgx, mgy);
  }
  else
  {
    (void)snprintf(line, sizeof(line), "MG X:----- Y:-----");
  }
  ui_draw_text_clipped(0U, y, line);
  y = (uint16_t)(y + step);

  if (status.accel_valid != 0U)
  {
    (void)snprintf(line, sizeof(line), "MG Z:%s", mgz);
  }
  else
  {
    (void)snprintf(line, sizeof(line), "MG Z:-----");
  }
  ui_draw_text_clipped(0U, y, line);
  y = (uint16_t)(y + step);

  if (status.accel_valid != 0U)
  {
    (void)snprintf(line, sizeof(line), "RAW X:%+06ld", (long)status.accel_raw[0]);
  }
  else
  {
    (void)snprintf(line, sizeof(line), "RAW X:------");
  }
  ui_draw_text_clipped(0U, y, line);
  y = (uint16_t)(y + step);

  if (status.accel_valid != 0U)
  {
    (void)snprintf(line, sizeof(line), "RAW Y:%+06ld", (long)status.accel_raw[1]);
  }
  else
  {
    (void)snprintf(line, sizeof(line), "RAW Y:------");
  }
  ui_draw_text_clipped(0U, y, line);
  y = (uint16_t)(y + step);

  if (status.accel_valid != 0U)
  {
    (void)snprintf(line, sizeof(line), "RAW Z:%+06ld", (long)status.accel_raw[2]);
  }
  else
  {
    (void)snprintf(line, sizeof(line), "RAW Z:------");
  }
  ui_draw_text_clipped(0U, y, line);
  y = (uint16_t)(y + step);

  if (status.temp_valid != 0U)
  {
    int32_t t_c10 = round_i32(status.temp_c * 10.0f);
    int32_t t_c = t_c10 / 10;
    uint32_t t_frac = abs_u32(t_c10 % 10);
    (void)snprintf(temp_buf, sizeof(temp_buf), "%ld.%01luC",
                   (long)t_c, (unsigned long)t_frac);
    (void)snprintf(line, sizeof(line), "T:%s", temp_buf);
  }
  else
  {
    (void)snprintf(line, sizeof(line), "T:--.-C");
  }
  ui_draw_text_clipped(0U, y, line);
  y = (uint16_t)(y + step);

  if (status.status_valid != 0U)
  {
    (void)snprintf(line, sizeof(line), "STAT D%u B%u R%u",
                   (unsigned)status.status_drdy,
                   (unsigned)status.status_boot,
                   (unsigned)status.status_sw_reset);
  }
  else
  {
    (void)snprintf(line, sizeof(line), "STAT --");
  }
  ui_draw_text_clipped(0U, y, line);
  y = (uint16_t)(y + step);

  if (status.emb_valid != 0U)
  {
    (void)snprintf(line, sizeof(line), "EMB S%u T%u M%u",
                   (unsigned)status.emb_step,
                   (unsigned)status.emb_tilt,
                   (unsigned)status.emb_sigmot);
  }
  else
  {
    (void)snprintf(line, sizeof(line), "EMB --");
  }
  ui_draw_text_clipped(0U, y, line);
  y = (uint16_t)(y + step);

  const char *err_label = lis2_err_label(status.err_src, err_buf, sizeof(err_buf));
  (void)snprintf(line, sizeof(line), "ERR %lu %s",
                 (unsigned long)status.error_count, err_label);
  ui_draw_text_clipped(0U, y, line);
}

const ui_page_t PAGE_LIS2_IMU =
{
  .name = "IMU",
  .enter = page_lis2_imu_enter,
  .event = page_lis2_imu_event,
  .render = page_lis2_imu_render,
  .exit = NULL,
  .tick_ms = 200U,
  .flags = UI_PAGE_FLAG_LIS2
};
