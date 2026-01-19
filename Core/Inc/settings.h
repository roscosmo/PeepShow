#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SETTINGS_MAGIC 0x564C5453UL
#define SETTINGS_VERSION 1U
#define SETTINGS_PATH "/settings.tlv"
#define SETTINGS_PATH_TMP "/settings.tmp"

typedef enum
{
  SETTINGS_KEY_JOY_CAL = 1,
  SETTINGS_KEY_MENU_PRESS_NORM = 2,
  SETTINGS_KEY_MENU_RELEASE_NORM = 3,
  SETTINGS_KEY_MENU_AXIS_RATIO = 4,
  SETTINGS_KEY_KEYCLICK_ENABLED = 5,
  SETTINGS_KEY_VOLUME = 6,
  SETTINGS_KEY_SLEEP_ENABLED = 7,
  SETTINGS_KEY_SLEEP_ALLOW_GAME = 8,
  SETTINGS_KEY_SLEEP_TIMEOUT_MS = 9,
  SETTINGS_KEY_SLEEP_FACE_INTERVAL_S = 10,
  SETTINGS_KEY_RTC_DATETIME = 11
} settings_key_t;

typedef struct
{
  float cx;
  float cy;
  float sx;
  float sy;
  float rot_deg;
  float thr_x_mT;
  float thr_y_mT;
  float abs_deadzone_mT;
  uint8_t invert_x;
  uint8_t invert_y;
  uint8_t abs_deadzone_en;
  uint8_t valid;
  uint8_t reserved_u8[4];
} settings_joy_cal_t;

typedef struct
{
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
  uint8_t day;
  uint8_t month;
  uint16_t year;
} settings_rtc_datetime_t;

void settings_init(void);
void settings_reset_defaults(void);
uint32_t settings_get_seq(void);

bool settings_get(settings_key_t key, void *out);
bool settings_set(settings_key_t key, const void *value);
bool settings_is_dirty(void);
bool settings_is_loaded(void);
bool settings_commit(void);
void settings_mark_loaded(void);
void settings_mark_saved(void);

bool settings_encode(uint8_t *out, uint32_t max, uint32_t *out_len);
bool settings_decode(const uint8_t *data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* SETTINGS_H */
