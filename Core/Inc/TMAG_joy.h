#ifndef TMAG_JOY_H
#define TMAG_JOY_H

#include "TMAG5273.h"   // base driver API
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 8-way direction (RIGHT first, CCW)
typedef enum {
    TMAGJOY_NEUTRAL = 0,
    TMAGJOY_RIGHT,
    TMAGJOY_UPRIGHT,
    TMAGJOY_UP,
    TMAGJOY_UPLEFT,
    TMAGJOY_LEFT,
    TMAGJOY_DOWNLEFT,
    TMAGJOY_DOWN,
    TMAGJOY_DOWNRIGHT,
} TMAGJoy_Dir;

typedef enum {
    TMAGJOY_IRQ_RESULT = 0,     // INT on new result
    TMAGJOY_IRQ_THRESHOLD = 1,  // INT on threshold crossing
} TMAGJoy_IrqSource;

typedef struct {
    // (doc only) your EXTI wiring
    void    *int_port;
    uint16_t int_pin;

    // device cadence
    TMAG5273_mode_t mode;     // CONTINUOUS or WAKE_SLEEP
    uint8_t         sleep_time_n;

    // INT behavior
    TMAGJoy_IrqSource irq_source;
    uint8_t           int_pulse_10us;
    float             thr_x_mT;
    float             thr_y_mT;

    // shaping
    uint8_t use_hysteresis;
    float   dz_norm_in;
    float   dz_norm_out;
    uint8_t abs_deadzone_en;
    float   abs_deadzone_mT;
    uint8_t invert_x, invert_y;
    float   extra_rotate_deg;
    float   dir_bias_deg;

    // digital mode threshold (normalized 0..1)
    float   digital_thresh_norm;
} TMAGJoy_Config;

typedef struct {
    TMAGJoy_Dir dir;
    float nx, ny;     // normalized −1..+1
    float r_abs_mT;   // absolute raw radius (mT)
} TMAGJoy_Sample;

typedef struct {
    float cx, cy;
    float sx, sy;
    float rot_deg;
    uint8_t invert_x;
    uint8_t invert_y;
} TMAGJoy_Cal;

#ifndef TMAGJOY_QSIZE
#define TMAGJOY_QSIZE 8u
#endif

typedef struct {
    TMAGJoy_Config cfg;

    // optional IRQ queue
    volatile uint8_t q_head, q_tail;
    TMAGJoy_Sample   q[TMAGJOY_QSIZE];

    // Non-blocking calibration: neutral
    struct {
        uint8_t  active;
        uint32_t t_start_ms;
        uint32_t duration_ms;
        uint32_t sample_every_ms;
        uint32_t last_sample_ms;
        uint32_t n;
        double   sum_x;
        double   sum_y;
    } cal_neutral;

    // Non-blocking calibration: extents
    struct {
        uint8_t  active;
        uint32_t t_start_ms;
        uint32_t duration_ms;
        uint32_t sample_every_ms;
        uint32_t last_sample_ms;

        float xmin, xmax, ymin, ymax;
        double sumx, sumy, sumxx, sumyy, sumxy;
        uint32_t n;
    } cal_ext;
} TMAGJoy;

// Defaults & init
TMAGJoy_Config TMAGJoy_DefaultConfig(void);
int  TMAGJoy_Init(TMAGJoy *joy, const TMAGJoy_Config *cfg);

// Non-blocking calibration (separate menu items)
void TMAGJoy_CalNeutral_Begin(TMAGJoy *joy, uint32_t window_ms, uint32_t sample_every_ms);
bool TMAGJoy_CalNeutral_Step (TMAGJoy *joy, uint32_t now_ms, float *progress_0to1);

void TMAGJoy_CalExtents_Begin(TMAGJoy *joy, uint32_t duration_ms, uint32_t sample_every_ms);
bool TMAGJoy_CalExtents_Step (TMAGJoy *joy, uint32_t now_ms, float *progress_0to1);

// Convenience
int  TMAGJoy_ZeroHere(TMAGJoy *joy);
void TMAGJoy_SetCenter(TMAGJoy *joy, float cx, float cy);
void TMAGJoy_SetSpan(TMAGJoy *joy, float sx, float sy);
void TMAGJoy_SetRotationDeg(TMAGJoy *joy, float deg);

// Reading
TMAGJoy_Sample TMAGJoy_ReadAnalog(TMAGJoy *joy); // −1..+1, clamped
TMAGJoy_Dir    TMAGJoy_ReadDigital(TMAGJoy *joy); // 8-way gated by threshold

void TMAGJoy_ReadCalibratedRaw(TMAGJoy *joy, float *nx, float *ny, float *r_abs_mT);
void TMAGJoy_GetCal(TMAGJoy *joy, TMAGJoy_Cal *out);
void TMAGJoy_GetThresholds(TMAGJoy *joy, float *thr_x_mT, float *thr_y_mT);
void TMAGJoy_GetAbsDeadzone(TMAGJoy *joy, uint8_t *en, float *mT);

// Optional INT queue
void TMAGJoy_OnIRQ(TMAGJoy *joy);
int  TMAGJoy_Pop (TMAGJoy *joy, TMAGJoy_Sample *out);

// Pass-through knobs
void TMAGJoy_SetAbsDeadzone(TMAGJoy *joy, uint8_t enable, float mT);
void TMAGJoy_SetHysteresis (TMAGJoy *joy, uint8_t enable, float in_norm, float out_norm);
void TMAGJoy_SetInvert     (TMAGJoy *joy, uint8_t invert_x, uint8_t invert_y);
void TMAGJoy_AddExtraRotation(TMAGJoy *joy, float deg);
void TMAGJoy_SetDirBias    (TMAGJoy *joy, float deg);
void TMAGJoy_SetDeadzoneNorm(TMAGJoy *joy, float dz_norm);
void TMAGJoy_SetRadialDeadzoneNorm(TMAGJoy *joy, float dz_norm);   // 0..0.9 (e.g., 0.20)
void TMAGJoy_SetResponseCurve(TMAGJoy *joy, float gamma);          // 1.0 = linear

// Menu/IRQ helpers (optional)
void TMAGJoy_InitOnce(void);
void TMAGJoy_SetUseIRQ(uint8_t enable);
void TMAGJoy_SetIntEnabled(uint8_t enable);
void TMAGJoy_DisableIntLine(void);
void TMAGJoy_OnSleep(void);
void TMAGJoy_OnWake(void);
void TMAGJoy_MenuEnable(uint8_t on);
uint8_t TMAGJoy_MenuReady(void);
TMAGJoy_Dir TMAGJoy_MenuIRQ(uint8_t menu_mode, uint8_t in_sleep);
TMAGJoy_Dir TMAGJoy_MenuPoll(uint8_t menu_mode, uint8_t in_sleep);
void TMAGJoy_MenuNeutralStep(uint8_t menu_mode, uint8_t in_sleep);




#ifdef __cplusplus
}
#endif
#endif /* TMAG_JOY_H */
