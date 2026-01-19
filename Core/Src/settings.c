#include "settings.h"

#include "settings.h"

#include "app_freertos.h"
#include "cmsis_os2.h"
#include "lfs_util.h"
#include "storage_task.h"

#include <float.h>
#include <stddef.h>
#include <string.h>

typedef enum
{
  SETTINGS_TYPE_BOOL = 1,
  SETTINGS_TYPE_U8 = 2,
  SETTINGS_TYPE_U16 = 3,
  SETTINGS_TYPE_U32 = 4,
  SETTINGS_TYPE_I32 = 5,
  SETTINGS_TYPE_F32 = 6,
  SETTINGS_TYPE_BYTES = 7
} settings_type_t;

typedef union
{
  uint32_t u32;
  int32_t i32;
  uint16_t u16;
  uint8_t u8;
  float f32;
} settings_value_t;

typedef struct
{
  settings_key_t key;
  settings_type_t type;
  uint16_t offset;
  uint16_t size;
  settings_value_t def;
  settings_value_t min;
  settings_value_t max;
  const void *def_bytes;
} settings_registry_entry_t;

typedef struct
{
  settings_joy_cal_t joy;
  float menu_press_norm;
  float menu_release_norm;
  float menu_axis_ratio;
  uint8_t keyclick_enabled;
  uint8_t volume;
  uint8_t sleep_enabled;
  uint8_t sleep_allow_game;
  uint32_t sleep_timeout_ms;
  uint32_t sleep_face_interval_s;
  settings_rtc_datetime_t rtc;
  uint32_t reserved_u32[6];
} settings_data_t;

#define SETTINGS_HEADER_SIZE 16U
#define SETTINGS_RECORD_HEADER_SIZE 5U
#define SETTINGS_RECORD_CRC_SIZE 4U
#define SETTINGS_RECORD_MIN_SIZE (SETTINGS_RECORD_HEADER_SIZE + SETTINGS_RECORD_CRC_SIZE)

static settings_data_t s_settings;
static uint32_t s_seq = 0U;
static uint8_t s_dirty = 0U;
static uint8_t s_loaded = 0U;

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

static void settings_write_u16_le(uint8_t *dst, uint16_t value)
{
  dst[0] = (uint8_t)(value & 0xFFU);
  dst[1] = (uint8_t)((value >> 8) & 0xFFU);
}

static void settings_write_u32_le(uint8_t *dst, uint32_t value)
{
  dst[0] = (uint8_t)(value & 0xFFU);
  dst[1] = (uint8_t)((value >> 8) & 0xFFU);
  dst[2] = (uint8_t)((value >> 16) & 0xFFU);
  dst[3] = (uint8_t)((value >> 24) & 0xFFU);
}

static uint16_t settings_read_u16_le(const uint8_t *src)
{
  return (uint16_t)src[0] | (uint16_t)((uint16_t)src[1] << 8);
}

static uint32_t settings_read_u32_le(const uint8_t *src)
{
  return (uint32_t)src[0]
         | ((uint32_t)src[1] << 8)
         | ((uint32_t)src[2] << 16)
         | ((uint32_t)src[3] << 24);
}

static uint8_t settings_is_leap_year(uint16_t year)
{
  return ((year % 4U) == 0U) ? 1U : 0U;
}

static uint8_t settings_days_in_month(uint16_t year, uint8_t month)
{
  static const uint8_t kDays[12] = { 31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U };
  if ((month == 0U) || (month > 12U))
  {
    return 31U;
  }

  uint8_t days = kDays[month - 1U];
  if ((month == 2U) && (settings_is_leap_year(year) != 0U))
  {
    days = 29U;
  }
  return days;
}

static void settings_rtc_clamp(settings_rtc_datetime_t *dt)
{
  if (dt == NULL)
  {
    return;
  }

  if (dt->hours > 23U)
  {
    dt->hours = 23U;
  }
  if (dt->minutes > 59U)
  {
    dt->minutes = 59U;
  }
  if (dt->seconds > 59U)
  {
    dt->seconds = 59U;
  }
  if (dt->month < 1U)
  {
    dt->month = 1U;
  }
  if (dt->month > 12U)
  {
    dt->month = 12U;
  }
  if (dt->year < 2000U)
  {
    dt->year = 2000U;
  }
  if (dt->year > 2099U)
  {
    dt->year = 2099U;
  }

  uint8_t dim = settings_days_in_month(dt->year, dt->month);
  if (dt->day < 1U)
  {
    dt->day = 1U;
  }
  if (dt->day > dim)
  {
    dt->day = dim;
  }
}

static const settings_joy_cal_t k_default_joy_cal =
{
  .cx = 0.0f,
  .cy = 0.0f,
  .sx = 1.0f,
  .sy = 1.0f,
  .rot_deg = 0.0f,
  .thr_x_mT = 0.0f,
  .thr_y_mT = 0.0f,
  .abs_deadzone_mT = 0.0f,
  .invert_x = 0U,
  .invert_y = 0U,
  .abs_deadzone_en = 0U,
  .valid = 0U,
  .reserved_u8 = {0U, 0U, 0U, 0U}
};

static const settings_rtc_datetime_t k_default_rtc =
{
  .hours = 0U,
  .minutes = 0U,
  .seconds = 0U,
  .day = 1U,
  .month = 1U,
  .year = 2026U
};

static const settings_registry_entry_t k_registry[] =
{
  {
    .key = SETTINGS_KEY_JOY_CAL,
    .type = SETTINGS_TYPE_BYTES,
    .offset = (uint16_t)offsetof(settings_data_t, joy),
    .size = (uint16_t)sizeof(settings_joy_cal_t),
    .def = {0U},
    .min = {0U},
    .max = {0U},
    .def_bytes = &k_default_joy_cal
  },
  {
    .key = SETTINGS_KEY_MENU_PRESS_NORM,
    .type = SETTINGS_TYPE_F32,
    .offset = (uint16_t)offsetof(settings_data_t, menu_press_norm),
    .size = (uint16_t)sizeof(float),
    .def.f32 = 0.45f,
    .min.f32 = -FLT_MAX,
    .max.f32 = FLT_MAX,
    .def_bytes = NULL
  },
  {
    .key = SETTINGS_KEY_MENU_RELEASE_NORM,
    .type = SETTINGS_TYPE_F32,
    .offset = (uint16_t)offsetof(settings_data_t, menu_release_norm),
    .size = (uint16_t)sizeof(float),
    .def.f32 = 0.25f,
    .min.f32 = -FLT_MAX,
    .max.f32 = FLT_MAX,
    .def_bytes = NULL
  },
  {
    .key = SETTINGS_KEY_MENU_AXIS_RATIO,
    .type = SETTINGS_TYPE_F32,
    .offset = (uint16_t)offsetof(settings_data_t, menu_axis_ratio),
    .size = (uint16_t)sizeof(float),
    .def.f32 = 1.4f,
    .min.f32 = -FLT_MAX,
    .max.f32 = FLT_MAX,
    .def_bytes = NULL
  },
  {
    .key = SETTINGS_KEY_KEYCLICK_ENABLED,
    .type = SETTINGS_TYPE_BOOL,
    .offset = (uint16_t)offsetof(settings_data_t, keyclick_enabled),
    .size = (uint16_t)sizeof(uint8_t),
    .def.u8 = 1U,
    .min.u8 = 0U,
    .max.u8 = 1U,
    .def_bytes = NULL
  },
  {
    .key = SETTINGS_KEY_VOLUME,
    .type = SETTINGS_TYPE_U8,
    .offset = (uint16_t)offsetof(settings_data_t, volume),
    .size = (uint16_t)sizeof(uint8_t),
    .def.u8 = 7U,
    .min.u8 = 0U,
    .max.u8 = 10U,
    .def_bytes = NULL
  },
  {
    .key = SETTINGS_KEY_SLEEP_ENABLED,
    .type = SETTINGS_TYPE_BOOL,
    .offset = (uint16_t)offsetof(settings_data_t, sleep_enabled),
    .size = (uint16_t)sizeof(uint8_t),
    .def.u8 = 1U,
    .min.u8 = 0U,
    .max.u8 = 1U,
    .def_bytes = NULL
  },
  {
    .key = SETTINGS_KEY_SLEEP_ALLOW_GAME,
    .type = SETTINGS_TYPE_BOOL,
    .offset = (uint16_t)offsetof(settings_data_t, sleep_allow_game),
    .size = (uint16_t)sizeof(uint8_t),
    .def.u8 = 1U,
    .min.u8 = 0U,
    .max.u8 = 1U,
    .def_bytes = NULL
  },
  {
    .key = SETTINGS_KEY_SLEEP_TIMEOUT_MS,
    .type = SETTINGS_TYPE_U32,
    .offset = (uint16_t)offsetof(settings_data_t, sleep_timeout_ms),
    .size = (uint16_t)sizeof(uint32_t),
    .def.u32 = 15000U,
    .min.u32 = 0U,
    .max.u32 = UINT32_MAX,
    .def_bytes = NULL
  },
  {
    .key = SETTINGS_KEY_SLEEP_FACE_INTERVAL_S,
    .type = SETTINGS_TYPE_U32,
    .offset = (uint16_t)offsetof(settings_data_t, sleep_face_interval_s),
    .size = (uint16_t)sizeof(uint32_t),
    .def.u32 = 1U,
    .min.u32 = 0U,
    .max.u32 = UINT32_MAX,
    .def_bytes = NULL
  },
  {
    .key = SETTINGS_KEY_RTC_DATETIME,
    .type = SETTINGS_TYPE_BYTES,
    .offset = (uint16_t)offsetof(settings_data_t, rtc),
    .size = (uint16_t)sizeof(settings_rtc_datetime_t),
    .def = {0U},
    .min = {0U},
    .max = {0U},
    .def_bytes = &k_default_rtc
  }
};

static const settings_registry_entry_t *settings_find_entry(settings_key_t key)
{
  uint32_t count = (uint32_t)(sizeof(k_registry) / sizeof(k_registry[0]));
  for (uint32_t i = 0U; i < count; ++i)
  {
    if (k_registry[i].key == key)
    {
      return &k_registry[i];
    }
  }

  return NULL;
}

static void settings_set_defaults(settings_data_t *data)
{
  if (data == NULL)
  {
    return;
  }

  (void)memset(data, 0, sizeof(*data));

  uint32_t count = (uint32_t)(sizeof(k_registry) / sizeof(k_registry[0]));
  for (uint32_t i = 0U; i < count; ++i)
  {
    const settings_registry_entry_t *entry = &k_registry[i];
    uint8_t *dst = (uint8_t *)data + entry->offset;

    switch (entry->type)
    {
      case SETTINGS_TYPE_BOOL:
        dst[0] = (entry->def.u8 != 0U) ? 1U : 0U;
        break;
      case SETTINGS_TYPE_U8:
        dst[0] = entry->def.u8;
        break;
      case SETTINGS_TYPE_U16:
      {
        uint16_t value = entry->def.u16;
        (void)memcpy(dst, &value, sizeof(value));
        break;
      }
      case SETTINGS_TYPE_U32:
      {
        uint32_t value = entry->def.u32;
        (void)memcpy(dst, &value, sizeof(value));
        break;
      }
      case SETTINGS_TYPE_I32:
      {
        int32_t value = entry->def.i32;
        (void)memcpy(dst, &value, sizeof(value));
        break;
      }
      case SETTINGS_TYPE_F32:
      {
        float value = entry->def.f32;
        (void)memcpy(dst, &value, sizeof(value));
        break;
      }
      case SETTINGS_TYPE_BYTES:
        if (entry->def_bytes != NULL)
        {
          (void)memcpy(dst, entry->def_bytes, entry->size);
        }
        break;
      default:
        break;
    }
  }
}

static bool settings_apply_entry_value(settings_data_t *data,
                                       const settings_registry_entry_t *entry,
                                       const uint8_t *value,
                                       bool *changed)
{
  if ((data == NULL) || (entry == NULL) || (value == NULL))
  {
    return false;
  }

  bool local_changed = false;
  uint8_t *dst = (uint8_t *)data + entry->offset;

  switch (entry->type)
  {
    case SETTINGS_TYPE_BOOL:
    {
      uint8_t val = (value[0] != 0U) ? 1U : 0U;
      if (val != dst[0])
      {
        dst[0] = val;
        local_changed = true;
      }
      break;
    }
    case SETTINGS_TYPE_U8:
    {
      uint8_t val = value[0];
      if (val < entry->min.u8)
      {
        val = entry->min.u8;
      }
      if (val > entry->max.u8)
      {
        val = entry->max.u8;
      }
      if (val != dst[0])
      {
        dst[0] = val;
        local_changed = true;
      }
      break;
    }
    case SETTINGS_TYPE_U16:
    {
      uint16_t val = settings_read_u16_le(value);
      if (val < entry->min.u16)
      {
        val = entry->min.u16;
      }
      if (val > entry->max.u16)
      {
        val = entry->max.u16;
      }
      uint16_t cur = 0U;
      (void)memcpy(&cur, dst, sizeof(cur));
      if (cur != val)
      {
        (void)memcpy(dst, &val, sizeof(val));
        local_changed = true;
      }
      break;
    }
    case SETTINGS_TYPE_U32:
    {
      uint32_t val = settings_read_u32_le(value);
      if (val < entry->min.u32)
      {
        val = entry->min.u32;
      }
      if (val > entry->max.u32)
      {
        val = entry->max.u32;
      }
      uint32_t cur = 0U;
      (void)memcpy(&cur, dst, sizeof(cur));
      if (cur != val)
      {
        (void)memcpy(dst, &val, sizeof(val));
        local_changed = true;
      }
      break;
    }
    case SETTINGS_TYPE_I32:
    {
      int32_t val = (int32_t)settings_read_u32_le(value);
      if (val < entry->min.i32)
      {
        val = entry->min.i32;
      }
      if (val > entry->max.i32)
      {
        val = entry->max.i32;
      }
      int32_t cur = 0;
      (void)memcpy(&cur, dst, sizeof(cur));
      if (cur != val)
      {
        (void)memcpy(dst, &val, sizeof(val));
        local_changed = true;
      }
      break;
    }
    case SETTINGS_TYPE_F32:
    {
      uint32_t raw = settings_read_u32_le(value);
      float val = 0.0f;
      (void)memcpy(&val, &raw, sizeof(val));
      if (val != val)
      {
        return false;
      }
      if (val < entry->min.f32)
      {
        val = entry->min.f32;
      }
      if (val > entry->max.f32)
      {
        val = entry->max.f32;
      }
      float cur = 0.0f;
      (void)memcpy(&cur, dst, sizeof(cur));
      if (cur != val)
      {
        (void)memcpy(dst, &val, sizeof(val));
        local_changed = true;
      }
      break;
    }
    case SETTINGS_TYPE_BYTES:
      if (entry->size == 0U)
      {
        return false;
      }
      if ((entry->key == SETTINGS_KEY_RTC_DATETIME) && (entry->size == sizeof(settings_rtc_datetime_t)))
      {
        settings_rtc_datetime_t rtc_value;
        (void)memcpy(&rtc_value, value, sizeof(rtc_value));
        settings_rtc_clamp(&rtc_value);
        if (memcmp(dst, &rtc_value, sizeof(rtc_value)) != 0)
        {
          (void)memcpy(dst, &rtc_value, sizeof(rtc_value));
          local_changed = true;
        }
        break;
      }
      if (memcmp(dst, value, entry->size) != 0)
      {
        (void)memcpy(dst, value, entry->size);
        local_changed = true;
      }
      break;
    default:
      return false;
  }

  if (changed != NULL)
  {
    *changed = local_changed;
  }

  return true;
}

void settings_init(void)
{
  settings_set_defaults(&s_settings);
  s_seq = 1U;
  s_dirty = 0U;
  s_loaded = 0U;
}

void settings_reset_defaults(void)
{
  settings_lock();
  settings_set_defaults(&s_settings);
  s_seq++;
  s_dirty = 1U;
  settings_unlock();
}

uint32_t settings_get_seq(void)
{
  return s_seq;
}

bool settings_get(settings_key_t key, void *out)
{
  if (out == NULL)
  {
    return false;
  }

  const settings_registry_entry_t *entry = settings_find_entry(key);
  if (entry == NULL)
  {
    return false;
  }

  settings_lock();
  const uint8_t *src = (const uint8_t *)&s_settings + entry->offset;
  switch (entry->type)
  {
    case SETTINGS_TYPE_BOOL:
    case SETTINGS_TYPE_U8:
      ((uint8_t *)out)[0] = src[0];
      break;
    case SETTINGS_TYPE_U16:
    case SETTINGS_TYPE_U32:
    case SETTINGS_TYPE_I32:
    case SETTINGS_TYPE_F32:
    case SETTINGS_TYPE_BYTES:
      (void)memcpy(out, src, entry->size);
      break;
    default:
      settings_unlock();
      return false;
  }
  settings_unlock();

  return true;
}

bool settings_set(settings_key_t key, const void *value)
{
  if (value == NULL)
  {
    return false;
  }

  const settings_registry_entry_t *entry = settings_find_entry(key);
  if (entry == NULL)
  {
    return false;
  }

  bool changed = false;
  settings_lock();
  if (settings_apply_entry_value(&s_settings, entry, (const uint8_t *)value, &changed))
  {
    if (changed)
    {
      s_seq++;
      s_dirty = 1U;
    }
    settings_unlock();
    return true;
  }
  settings_unlock();

  return false;
}

bool settings_is_dirty(void)
{
  settings_lock();
  uint8_t dirty = s_dirty;
  settings_unlock();
  return (dirty != 0U);
}

bool settings_is_loaded(void)
{
  settings_lock();
  uint8_t loaded = s_loaded;
  settings_unlock();
  return (loaded != 0U);
}

bool settings_commit(void)
{
  bool dirty = settings_is_dirty();
  if (!dirty)
  {
    return true;
  }

  return storage_request_save_settings();
}

void settings_mark_loaded(void)
{
  settings_lock();
  s_loaded = 1U;
  settings_unlock();
}

void settings_mark_saved(void)
{
  settings_lock();
  s_dirty = 0U;
  settings_unlock();
}

bool settings_encode(uint8_t *out, uint32_t max, uint32_t *out_len)
{
  if ((out == NULL) || (out_len == NULL))
  {
    return false;
  }

  settings_data_t snapshot;
  uint32_t seq = 0U;
  settings_lock();
  snapshot = s_settings;
  seq = s_seq;
  settings_unlock();

  if (max < SETTINGS_HEADER_SIZE)
  {
    return false;
  }

  uint32_t offset = SETTINGS_HEADER_SIZE;
  uint32_t count = (uint32_t)(sizeof(k_registry) / sizeof(k_registry[0]));
  for (uint32_t i = 0U; i < count; ++i)
  {
    const settings_registry_entry_t *entry = &k_registry[i];
    uint32_t record_len = (uint32_t)SETTINGS_RECORD_HEADER_SIZE + entry->size + SETTINGS_RECORD_CRC_SIZE;
    if ((offset + record_len) > max)
    {
      return false;
    }

    uint8_t *rec = out + offset;
    settings_write_u16_le(rec, (uint16_t)entry->key);
    rec[2] = (uint8_t)entry->type;
    settings_write_u16_le(rec + 3U, entry->size);

    const uint8_t *src = (const uint8_t *)&snapshot + entry->offset;
    (void)memcpy(rec + SETTINGS_RECORD_HEADER_SIZE, src, entry->size);

    uint32_t crc = lfs_crc(0U, rec, SETTINGS_RECORD_HEADER_SIZE + entry->size);
    settings_write_u32_le(rec + SETTINGS_RECORD_HEADER_SIZE + entry->size, crc);

    offset += record_len;
  }

  settings_write_u32_le(out, SETTINGS_MAGIC);
  settings_write_u16_le(out + 4U, SETTINGS_VERSION);
  settings_write_u16_le(out + 6U, 0U);
  settings_write_u32_le(out + 8U, seq);

  uint32_t header_crc = lfs_crc(0U, out, SETTINGS_HEADER_SIZE - 4U);
  settings_write_u32_le(out + 12U, header_crc);

  *out_len = offset;
  return true;
}

bool settings_decode(const uint8_t *data, uint32_t len)
{
  if ((data == NULL) || (len < SETTINGS_HEADER_SIZE))
  {
    return false;
  }

  uint32_t magic = settings_read_u32_le(data);
  if (magic != SETTINGS_MAGIC)
  {
    return false;
  }

  uint16_t version = settings_read_u16_le(data + 4U);
  if (version > SETTINGS_VERSION)
  {
    return false;
  }

  uint32_t header_crc = settings_read_u32_le(data + 12U);
  uint32_t calc_crc = lfs_crc(0U, data, SETTINGS_HEADER_SIZE - 4U);
  if (calc_crc != header_crc)
  {
    return false;
  }

  settings_data_t updated;
  settings_set_defaults(&updated);

  uint32_t offset = SETTINGS_HEADER_SIZE;
  while ((offset + SETTINGS_RECORD_MIN_SIZE) <= len)
  {
    const uint8_t *rec = data + offset;
    uint16_t key = settings_read_u16_le(rec);
    uint8_t type = rec[2];
    uint16_t value_len = settings_read_u16_le(rec + 3U);
    uint32_t record_len = (uint32_t)SETTINGS_RECORD_HEADER_SIZE + value_len + SETTINGS_RECORD_CRC_SIZE;
    if ((offset + record_len) > len)
    {
      break;
    }

    uint32_t record_crc = settings_read_u32_le(rec + SETTINGS_RECORD_HEADER_SIZE + value_len);
    uint32_t record_calc = lfs_crc(0U, rec, SETTINGS_RECORD_HEADER_SIZE + value_len);
    if (record_crc == record_calc)
    {
      if ((key == 0xFFFFU) && (value_len == 0U))
      {
        break;
      }

      const settings_registry_entry_t *entry = settings_find_entry((settings_key_t)key);
      if ((entry != NULL) && (entry->type == (settings_type_t)type) && (entry->size == value_len))
      {
        (void)settings_apply_entry_value(&updated, entry, rec + SETTINGS_RECORD_HEADER_SIZE, NULL);
      }
    }

    offset += record_len;
  }

  settings_lock();
  s_settings = updated;
  s_seq++;
  s_dirty = 0U;
  s_loaded = 1U;
  settings_unlock();
  return true;
}
