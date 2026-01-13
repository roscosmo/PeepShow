#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SETTINGS_MAGIC 0x50534554UL
#define SETTINGS_VERSION 2U
#define SETTINGS_PATH "/settings.bin"

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
  settings_joy_cal_t joy;
  float menu_press_norm;
  float menu_release_norm;
  float menu_axis_ratio;
  uint8_t keyclick_enabled;
  uint8_t volume;
  uint8_t sleep_enabled;
  uint8_t sleep_allow_game;
  uint32_t sleep_timeout_ms;
  uint32_t reserved_u32[7];
} settings_data_t;

void settings_init(void);
void settings_reset_defaults(void);
uint32_t settings_get_seq(void);
void settings_get(settings_data_t *out);

void settings_set_keyclick(uint8_t enabled);
void settings_set_volume(uint8_t volume);
void settings_set_menu_params(float press_norm, float release_norm, float axis_ratio);
void settings_set_joy_cal(const settings_joy_cal_t *cal);
void settings_set_sleep_enabled(uint8_t enabled);
void settings_set_sleep_allow_game(uint8_t allow);
void settings_set_sleep_timeout_ms(uint32_t timeout_ms);

bool settings_encode(uint8_t *out, uint32_t max, uint32_t *out_len);
bool settings_decode(const uint8_t *data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* SETTINGS_H */
