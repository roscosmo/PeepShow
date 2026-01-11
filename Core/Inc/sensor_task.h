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
  SENSOR_JOY_STAGE_EXTENTS = 2,
  SENSOR_JOY_STAGE_DONE = 3
} sensor_joy_stage_t;

typedef struct
{
  sensor_joy_stage_t stage;
  uint8_t neutral_done;
  uint8_t extents_done;
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

void sensor_task_run(void);
void sensor_joy_get_status(sensor_joy_status_t *out);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_TASK_H */
