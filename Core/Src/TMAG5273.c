/* TMAG5273.c
 *
 * Minimal HAL-based driver for the Texas Instruments TMAG5273.
 * - Keeps small internal state for XY/Z range (used for conversions + threshold scaling)
 * - Provides basic register R/W, configuration helpers, thresholds, and data reads
 *
 * NOTE:
 * - This module assumes `hi2c3` is defined elsewhere (STM32CubeMX / user code).
 * - All functions return 0 on success, -1 on I2C failure (unless noted).
 */

#include "TMAG5273.h"
#include <string.h>
#include <math.h>

#ifndef TMAG_I2C_TIMEOUT_MS
#define TMAG_I2C_TIMEOUT_MS  50u
#endif

//==============================================================================
// Internal state
//==============================================================================
//
// The device has selectable ranges. We cache what we last configured so we can
// correctly scale:
//  - magnetic field conversion to mT
//  - threshold conversion from mT to device code
//
static uint8_t s_xy_range = TMAG5273_RANGE_80mT;
static uint8_t s_z_range  = TMAG5273_RANGE_80mT;

//==============================================================================
// Small helpers
//==============================================================================

/**
 * @brief Set a bitfield (width bits) at position pos in an 8-bit register value.
 */
static inline uint8_t set_bits(uint8_t reg, uint8_t pos, uint8_t width, uint8_t val)
{
    uint8_t mask = (uint8_t)(((1u << width) - 1u) << pos);
    reg &= (uint8_t)~mask;
    reg |= (uint8_t)((val << pos) & mask);
    return reg;
}

/**
 * @brief Fast rounding for float to int (ties away from zero).
 */
static inline int fast_roundf_to_int(float x)
{
    return (int)(x + (x >= 0.0f ? 0.5f : -0.5f));
}

/**
 * @brief Clamp integer to int8_t range.
 */
static inline int8_t clamp_i8_int(int v)
{
    if (v > 127)  return 127;
    if (v < -128) return -128;
    return (int8_t)v;
}

//==============================================================================
// I2C access (HAL)
//==============================================================================
//
// These helpers wrap STM32 HAL memory read/write for 8-bit register addresses.
// `TMAG5273_I2C_ADDR_8B` is expected to be the 8-bit address (7-bit << 1).
//

int TMAG5273_write_reg(uint8_t reg, uint8_t val)
{
    return (HAL_I2C_Mem_Write(&hi2c3,
                             TMAG5273_I2C_ADDR_8B,
                             reg,
                             I2C_MEMADD_SIZE_8BIT,
                             &val,
                             1,
                             TMAG_I2C_TIMEOUT_MS) == HAL_OK) ? 0 : -1;
}

int TMAG5273_read_reg(uint8_t reg, uint8_t* val)
{
    return (HAL_I2C_Mem_Read(&hi2c3,
                            TMAG5273_I2C_ADDR_8B,
                            reg,
                            I2C_MEMADD_SIZE_8BIT,
                            val,
                            1,
                            TMAG_I2C_TIMEOUT_MS) == HAL_OK) ? 0 : -1;
}

int TMAG5273_read_regs(uint8_t reg, uint8_t* buf, uint16_t len)
{
    return (HAL_I2C_Mem_Read(&hi2c3,
                            TMAG5273_I2C_ADDR_8B,
                            reg,
                            I2C_MEMADD_SIZE_8BIT,
                            buf,
                            len,
                            TMAG_I2C_TIMEOUT_MS) == HAL_OK) ? 0 : -1;
}

//==============================================================================
// Configuration
//==============================================================================

/**
 * @brief Set device operating mode (standby/continuous/etc depending on enum).
 */
int TMAG5273_set_operating_mode(TMAG5273_mode_t mode)
{
    uint8_t r = 0;

    if (TMAG5273_read_reg(TMAG5273_REG_DEVICE_CONFIG_2, &r)) {
        return -1;
    }

    r = set_bits(r,
                 TMAG5273_DEVICE_CONFIG_2_OPERATING_Pos,
                 2,
                 (uint8_t)mode);

    return TMAG5273_write_reg(TMAG5273_REG_DEVICE_CONFIG_2, r);
}

/**
 * @brief Enable magnetic channels (X/Y/Z). `ch` is expected to be a driver mask.
 */
int TMAG5273_set_magnetic_channels(uint8_t ch)
{
    uint8_t r = 0;

    if (TMAG5273_read_reg(TMAG5273_REG_SENSOR_CONFIG_1, &r)) {
        return -1;
    }

    // Note: original code masks with 0x7 even though width is 4.
    r = set_bits(r,
                 TMAG5273_SENSOR_CONFIG_1_MAG_CH_EN_Pos,
                 4,
                 (uint8_t)(ch & 0x7));

    return TMAG5273_write_reg(TMAG5273_REG_SENSOR_CONFIG_1, r);
}

/**
 * @brief Set XY range selection (40mT or 80mT).
 *
 * NOTE: This driver treats `range` as a boolean-ish selector:
 *   0 -> 40mT, nonzero -> 80mT
 * and caches the configured range in s_xy_range.
 */
int TMAG5273_set_xy_range(uint8_t range)
{
    uint8_t r = 0;

    s_xy_range = (range ? TMAG5273_RANGE_80mT : TMAG5273_RANGE_40mT);

    if (TMAG5273_read_reg(TMAG5273_REG_SENSOR_CONFIG_2, &r)) {
        return -1;
    }

    r = set_bits(r,
                 TMAG5273_SENSOR_CONFIG_2_XY_RANGE_Pos,
                 1,
                 (uint8_t)(s_xy_range & 1));

    return TMAG5273_write_reg(TMAG5273_REG_SENSOR_CONFIG_2, r);
}

/**
 * @brief Set Z range selection (40mT or 80mT).
 *
 * NOTE: This driver treats `range` as a boolean-ish selector:
 *   0 -> 40mT, nonzero -> 80mT
 * and caches the configured range in s_z_range.
 */
int TMAG5273_set_z_range(uint8_t range)
{
    uint8_t r = 0;

    s_z_range = (range ? TMAG5273_RANGE_80mT : TMAG5273_RANGE_40mT);

    if (TMAG5273_read_reg(TMAG5273_REG_SENSOR_CONFIG_2, &r)) {
        return -1;
    }

    r = set_bits(r,
                 TMAG5273_SENSOR_CONFIG_2_Z_RANGE_Pos,
                 1,
                 (uint8_t)(s_z_range & 1));

    return TMAG5273_write_reg(TMAG5273_REG_SENSOR_CONFIG_2, r);
}

/**
 * @brief Configure sleep time field (4-bit) in SENSOR_CONFIG_1.
 */
int TMAG5273_set_sleep_time_n(uint8_t n)
{
    uint8_t r = 0;

    if (n > 0x0F) {
        n = 0x0F;
    }

    if (TMAG5273_read_reg(TMAG5273_REG_SENSOR_CONFIG_1, &r)) {
        return -1;
    }

    r = set_bits(r,
                 TMAG5273_SENSOR_CONFIG_1_SLEEPTIME_Pos,
                 4,
                 (uint8_t)(n & 0x0F));

    return TMAG5273_write_reg(TMAG5273_REG_SENSOR_CONFIG_1, r);
}

/**
 * @brief Enable/disable temperature channel in T_CONFIG.
 *
 * Static because it's only used internally by init_default() in this module.
 */
static int TMAG5273_enable_temperature(bool en)
{
    uint8_t r = 0;

    if (TMAG5273_read_reg(TMAG5273_REG_T_CONFIG, &r)) {
        return -1;
    }

    r = set_bits(r,
                 TMAG5273_T_CONFIG_T_CH_EN_Pos,
                 1,
                 en ? 1u : 0u);

    return TMAG5273_write_reg(TMAG5273_REG_T_CONFIG, r);
}

/**
 * @brief Configure interrupt behavior.
 *
 * @param result_int     Enable result interrupt
 * @param threshold_int  Enable threshold interrupt
 * @param pulse_10us     Pulse vs latched (per datasheet field meaning)
 * @param mode           Interrupt mode selection
 * @param mask_intb      Mask INTB output
 */
int TMAG5273_config_int(bool result_int,
                        bool threshold_int,
                        bool pulse_10us,
                        TMAG5273_int_mode_t mode,
                        bool mask_intb)
{
    uint8_t r = 0;

    if (TMAG5273_read_reg(TMAG5273_REG_INT_CONFIG_1, &r)) {
        return -1;
    }

    r = set_bits(r, TMAG5273_INT_CONFIG_1_RSLT_INT_Pos,   1, result_int ? 1u : 0u);
    r = set_bits(r, TMAG5273_INT_CONFIG_1_THRSLD_INT_Pos, 1, threshold_int ? 1u : 0u);
    r = set_bits(r, TMAG5273_INT_CONFIG_1_INT_STATE_Pos,  1, pulse_10us ? 1u : 0u);
    r = set_bits(r, TMAG5273_INT_CONFIG_1_INT_MODE_Pos,   3, (uint8_t)mode);
    r = set_bits(r, TMAG5273_INT_CONFIG_1_MASK_INTB_Pos,  1, mask_intb ? 1u : 0u);

    return TMAG5273_write_reg(TMAG5273_REG_INT_CONFIG_1, r);
}

//==============================================================================
// Thresholds (mT -> register code)
//==============================================================================
//
// The device threshold registers are signed 8-bit values scaling with range.
// Code = round(mT * 128 / range_mT), clamped to [-128, 127].
//

static int8_t xy_mT_to_code(float mT)
{
    const float range = (s_xy_range == TMAG5273_RANGE_80mT) ? 80.0f : 40.0f;
    int code = fast_roundf_to_int((mT * 128.0f) / range);
    return clamp_i8_int(code);
}

static int8_t z_mT_to_code(float mT)
{
    const float range = (s_z_range == TMAG5273_RANGE_80mT) ? 80.0f : 40.0f;
    int code = fast_roundf_to_int((mT * 128.0f) / range);
    return clamp_i8_int(code);
}

int TMAG5273_set_x_threshold_mT(float mT)
{
    uint8_t u = (uint8_t)xy_mT_to_code(mT);
    return TMAG5273_write_reg(TMAG5273_REG_X_THR_CONFIG, u);
}

int TMAG5273_set_y_threshold_mT(float mT)
{
    uint8_t u = (uint8_t)xy_mT_to_code(mT);
    return TMAG5273_write_reg(TMAG5273_REG_Y_THR_CONFIG, u);
}

int TMAG5273_set_z_threshold_mT(float mT)
{
    uint8_t u = (uint8_t)z_mT_to_code(mT);
    return TMAG5273_write_reg(TMAG5273_REG_Z_THR_CONFIG, u);
}

//==============================================================================
// Bring-up / default init
//==============================================================================

/**
 * @brief Initialize device with a basic default configuration.
 *
 * Prints ID info, enables XYZ + temperature, sets continuous mode, sets 80mT ranges.
 * Returns 0 if all writes succeed; -1 if any write fails.
 */
int TMAG5273_init_default(void)
{
    int rc = 0;

    printf("TMAG5273 @0x%02X  MFG=0x%04X  DEV_ID=0x%02X\r\n",
           (unsigned)TMAG5273_I2C_ADDR_7B,
           (unsigned)TMAG5273_get_manufacturer_id(),
           (unsigned)TMAG5273_get_device_id());

    rc |= TMAG5273_set_magnetic_channels(TMAG5273_CH_XYZ);
    rc |= TMAG5273_enable_temperature(true);
    rc |= TMAG5273_set_operating_mode(TMAG5273_MODE_CONTINUOUS);
    rc |= TMAG5273_set_xy_range(TMAG5273_RANGE_80mT);
    rc |= TMAG5273_set_z_range (TMAG5273_RANGE_80mT);

    return (rc == 0) ? 0 : -1;
}

//==============================================================================
// Data reads
//==============================================================================

/**
 * @brief Read raw magnetic results (signed 16-bit per axis).
 *
 * Reads 6 bytes starting at X_MSB_RESULT, then packs X/Y/Z.
 */
int TMAG5273_read_raw(int16_t* x, int16_t* y, int16_t* z)
{
    uint8_t b[6];

    if (TMAG5273_read_regs(TMAG5273_REG_X_MSB_RESULT, b, 6)) {
        return -1;
    }

    int16_t xr = (int16_t)((b[0] << 8) | b[1]);
    int16_t yr = (int16_t)((b[2] << 8) | b[3]);
    int16_t zr = (int16_t)((b[4] << 8) | b[5]);

    if (x) *x = xr;
    if (y) *y = yr;
    if (z) *z = zr;

    return 0;
}

/**
 * @brief Read magnetic results in milliTesla (mT).
 *
 * Uses cached range configuration to scale raw readings:
 *   mT = -(range_mT * raw) / 32768
 *
 * NOTE: Sign inversion is preserved exactly as in original code.
 */
int TMAG5273_read_mT(float* x, float* y, float* z)
{
    int16_t xr, yr, zr;

    if (TMAG5273_read_raw(&xr, &yr, &zr)) {
        return -1;
    }

    const float div = 32768.0f;
    const float rXY = (s_xy_range == TMAG5273_RANGE_80mT) ? 80.0f : 40.0f;
    const float rZ  = (s_z_range  == TMAG5273_RANGE_80mT) ? 80.0f : 40.0f;

    if (x) *x = -(rXY * (float)xr) / div;
    if (y) *y = -(rXY * (float)yr) / div;
    if (z) *z = -(rZ  * (float)zr) / div;

    return 0;
}

/**
 * @brief Read raw temperature register (signed 16-bit).
 */
int TMAG5273_get_temp_raw(int16_t* t)
{
    uint8_t b[2];

    if (TMAG5273_read_regs(TMAG5273_REG_T_MSB_RESULT, b, 2)) {
        return -1;
    }

    if (t) {
        *t = (int16_t)((b[0] << 8) | b[1]);
    }

    return 0;
}

//==============================================================================
// IDs & status
//==============================================================================

/**
 * @brief Read device ID (returns 0 if read fails, preserved original behavior).
 */
uint8_t TMAG5273_get_device_id(void)
{
    uint8_t v = 0;
    (void)TMAG5273_read_reg(TMAG5273_REG_DEVICE_ID, &v);
    return v;
}

/**
 * @brief Read manufacturer ID (16-bit). Returns 0 if read fails.
 *
 * Note: register address name implies LSB first; code packs [MSB:LSB] as b[1]:b[0].
 */
uint16_t TMAG5273_get_manufacturer_id(void)
{
    uint8_t b[2] = {0};

    if (TMAG5273_read_regs(TMAG5273_REG_MANUFACTURER_ID_LSB, b, 2)) {
        return 0;
    }

    return (uint16_t)((b[1] << 8) | b[0]);
}

/**
 * @brief Read device status (returns 0 if read fails, preserved original behavior).
 */
uint8_t TMAG5273_get_device_status(void)
{
    uint8_t v = 0;
    (void)TMAG5273_read_reg(TMAG5273_REG_DEVICE_STATUS, &v);
    return v;
}
