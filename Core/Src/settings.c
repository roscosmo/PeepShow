#include "settings.h"

#include "app_freertos.h"
#include "cmsis_os2.h"
#include "lfs_util.h"

#include <string.h>

typedef struct
{
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint32_t crc;
} settings_header_t;

static settings_data_t s_settings;
static uint32_t s_seq = 0U;

static void settings_lock(void)
{
  if (mtxSettingsHandle != NULL)
  {
    (void)osMutexAcquire(mtxSettingsHandle, osWaitForever);
  }
}

static void settings_unlock(void)
{
  if (mtxSettingsHandle != NULL)
  {
    (void)osMutexRelease(mtxSettingsHandle);
  }
}

static void settings_set_defaults(settings_data_t *data)
{
  if (data == NULL)
  {
    return;
  }

  (void)memset(data, 0, sizeof(*data));
  data->keyclick_enabled = 1U;
  data->volume = 7U;
  data->menu_press_norm = 0.45f;
  data->menu_release_norm = 0.25f;
  data->menu_axis_ratio = 1.4f;
  data->joy.sx = 1.0f;
  data->joy.sy = 1.0f;
  data->joy.valid = 0U;
}

void settings_init(void)
{
  settings_set_defaults(&s_settings);
  s_seq = 1U;
}

void settings_reset_defaults(void)
{
  settings_lock();
  settings_set_defaults(&s_settings);
  s_seq++;
  settings_unlock();
}

uint32_t settings_get_seq(void)
{
  return s_seq;
}

void settings_get(settings_data_t *out)
{
  if (out == NULL)
  {
    return;
  }

  settings_lock();
  *out = s_settings;
  settings_unlock();
}

void settings_set_keyclick(uint8_t enabled)
{
  settings_lock();
  s_settings.keyclick_enabled = (enabled != 0U) ? 1U : 0U;
  s_seq++;
  settings_unlock();
}

void settings_set_volume(uint8_t volume)
{
  if (volume > 10U)
  {
    volume = 10U;
  }

  settings_lock();
  s_settings.volume = volume;
  s_seq++;
  settings_unlock();
}

void settings_set_menu_params(float press_norm, float release_norm, float axis_ratio)
{
  settings_lock();
  s_settings.menu_press_norm = press_norm;
  s_settings.menu_release_norm = release_norm;
  s_settings.menu_axis_ratio = axis_ratio;
  s_seq++;
  settings_unlock();
}

void settings_set_joy_cal(const settings_joy_cal_t *cal)
{
  if (cal == NULL)
  {
    return;
  }

  settings_lock();
  s_settings.joy = *cal;
  s_seq++;
  settings_unlock();
}

bool settings_encode(uint8_t *out, uint32_t max, uint32_t *out_len)
{
  if ((out == NULL) || (out_len == NULL))
  {
    return false;
  }

  settings_data_t snapshot;
  settings_lock();
  snapshot = s_settings;
  settings_unlock();

  settings_header_t hdr;
  hdr.magic = SETTINGS_MAGIC;
  hdr.version = SETTINGS_VERSION;
  hdr.size = (uint16_t)sizeof(settings_data_t);
  hdr.crc = lfs_crc(0U, &snapshot, sizeof(snapshot));

  uint32_t total = (uint32_t)(sizeof(hdr) + sizeof(snapshot));
  if (total > max)
  {
    return false;
  }

  (void)memcpy(out, &hdr, sizeof(hdr));
  (void)memcpy(out + sizeof(hdr), &snapshot, sizeof(snapshot));
  *out_len = total;
  return true;
}

bool settings_decode(const uint8_t *data, uint32_t len)
{
  if ((data == NULL) || (len < sizeof(settings_header_t)))
  {
    return false;
  }

  settings_header_t hdr;
  (void)memcpy(&hdr, data, sizeof(hdr));

  if (hdr.magic != SETTINGS_MAGIC)
  {
    return false;
  }
  if (hdr.version > SETTINGS_VERSION)
  {
    return false;
  }

  uint32_t payload_len = hdr.size;
  if ((payload_len == 0U) || (payload_len > sizeof(settings_data_t)))
  {
    return false;
  }
  if ((sizeof(settings_header_t) + payload_len) > len)
  {
    return false;
  }

  const uint8_t *payload = data + sizeof(settings_header_t);
  uint32_t crc = lfs_crc(0U, payload, payload_len);
  if (crc != hdr.crc)
  {
    return false;
  }

  settings_data_t updated;
  settings_set_defaults(&updated);
  (void)memcpy(&updated, payload, payload_len);

  settings_lock();
  s_settings = updated;
  s_seq++;
  settings_unlock();
  return true;
}
