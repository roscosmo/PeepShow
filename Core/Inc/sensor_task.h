#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H

#include "ADP5360.h"
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

typedef struct
{
  uint16_t vbat_mV;
  uint8_t soc_percent;
  ADP5360_status1_t st1;
  ADP5360_status2_t st2;
  ADP5360_pgood_t pgood;
  uint8_t fault_mask;
  uint8_t valid;
} sensor_power_status_t;

typedef enum
{
  SENSOR_LIS2_ERR_SRC_NONE = 0U,
  SENSOR_LIS2_ERR_SRC_STATUS = (1U << 0U),
  SENSOR_LIS2_ERR_SRC_EMB = (1U << 1U),
  SENSOR_LIS2_ERR_SRC_XL = (1U << 2U),
  SENSOR_LIS2_ERR_SRC_TEMP = (1U << 3U),
  SENSOR_LIS2_ERR_SRC_STEP = (1U << 4U)
} sensor_lis2_err_src_t;

typedef struct
{
  float accel_mg[3];
  int16_t accel_raw[3];
  float temp_c;
  uint16_t step_count;
  uint32_t sample_seq;
  uint32_t error_count;
  uint8_t err_src;
  uint8_t device_id;
  uint8_t i2c_addr_7b;
  uint8_t odr;
  uint8_t fs;
  uint8_t bw;
  uint8_t accel_valid;
  uint8_t temp_valid;
  uint8_t status_valid;
  uint8_t emb_valid;
  uint8_t status_drdy;
  uint8_t status_boot;
  uint8_t status_sw_reset;
  uint8_t emb_step;
  uint8_t emb_tilt;
  uint8_t emb_sigmot;
  uint8_t id_valid;
  uint8_t init_ok;
  uint8_t step_valid;
  uint8_t step_enabled;
} sensor_lis2_status_t;

void sensor_task_run(void);
void sensor_joy_get_status(sensor_joy_status_t *out);
void sensor_joy_get_menu_params(sensor_joy_menu_params_t *out);
void sensor_power_get_status(sensor_power_status_t *out);
void sensor_lis2_get_status(sensor_lis2_status_t *out);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_TASK_H */
