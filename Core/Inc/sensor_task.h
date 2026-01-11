#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H

#include "tmag_joy.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  SENSOR_JOY_STAGE_IDLE = 0,
  SENSOR_JOY_STAGE_NEUTRAL = 1,
  SENSOR_JOY_STAGE_UP = 2,
  SENSOR_JOY_STAGE_RIGHT = 3,
  SENSOR_JOY_STAGE_DOWN = 4,
  SENSOR_JOY_STAGE_LEFT = 5,
  SENSOR_JOY_STAGE_SWEEP = 6,
  SENSOR_JOY_STAGE_DONE = 7
} sensor_joy_stage_t;

typedef struct
{
  sensor_joy_stage_t stage;
  float progress;
  TMAGJoy_Dir dir;
  float nx;
  float ny;
  float r_abs_mT;
  float sx_mT;
  float sy_mT;
  float thr_mT;
  float deadzone_mT;
  uint8_t deadzone_en;
} sensor_joy_status_t;

typedef struct
{
  float press_norm;
  float release_norm;
  float axis_ratio;
} sensor_joy_menu_params_t;

void sensor_task_run(void);
void sensor_joy_get_status(sensor_joy_status_t *out);
void sensor_joy_get_menu_params(sensor_joy_menu_params_t *out);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_TASK_H */
