#include "TMAG_joy.h"
#include "main.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static inline float fclamp(float v, float lo, float hi)
{ return (v < lo) ? lo : (v > hi) ? hi : v; }

static inline uint8_t qnext(uint8_t x)
{ return (uint8_t)((x + 1u) & (TMAGJOY_QSIZE - 1u)); }

#ifndef TMAG_JOY_DEFAULT_DEADZONE
#define TMAG_JOY_DEFAULT_DEADZONE 0.30f
#endif

static struct {
    float cx, cy;
    float sx, sy;
    float cphi, sphi;
    float deadzone;
    uint8_t invert_x;
    uint8_t invert_y;
} s_cal = { 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, TMAG_JOY_DEFAULT_DEADZONE, 0u, 0u };

static uint8_t s_use_hz = 1u;
static uint8_t s_in_neutral = 1u;
static float s_dz_in = 0.34f;
static float s_dz_out = 0.38f;
static uint8_t s_abs_dz_en = 0u;
static float s_abs_dz_mT = 12.0f;
static float s_dir_bias_rad = 0.0f;

static void joy_reset(void)
{
    s_cal.cx = 0.0f;
    s_cal.cy = 0.0f;
    s_cal.sx = 1.0f;
    s_cal.sy = 1.0f;
    s_cal.cphi = 1.0f;
    s_cal.sphi = 0.0f;
    s_cal.deadzone = TMAG_JOY_DEFAULT_DEADZONE;
    s_cal.invert_x = 0u;
    s_cal.invert_y = 0u;
    s_use_hz = 1u;
    s_in_neutral = 1u;
    s_dz_in = 0.34f;
    s_dz_out = 0.38f;
    s_abs_dz_en = 0u;
    s_abs_dz_mT = 12.0f;
    s_dir_bias_rad = 0.0f;
}

static TMAGJoy_Dir dir8_from_angle(float ang)
{
    const float step = 3.14159265f / 4.0f;
    float a = ang + s_dir_bias_rad;
    int sector = (int)floorf((a + step * 0.5f) / step) & 7;
    static const TMAGJoy_Dir lut[8] = {
        TMAGJOY_RIGHT, TMAGJOY_UPRIGHT, TMAGJOY_UP, TMAGJOY_UPLEFT,
        TMAGJOY_LEFT, TMAGJOY_DOWNLEFT, TMAGJOY_DOWN, TMAGJOY_DOWNRIGHT
    };
    return lut[sector];
}

static TMAGJoy_Dir joy_read(float *norm_x, float *norm_y, float *radius_abs_mT)
{
    float x, y;
    if (TMAG5273_read_mT(&x, &y, NULL) != 0) return TMAGJOY_NEUTRAL;
    float dx = x - s_cal.cx;
    float dy = y - s_cal.cy;
    float r_abs = sqrtf(dx * dx + dy * dy);
    if (radius_abs_mT) *radius_abs_mT = r_abs;

    if (s_abs_dz_en && r_abs < s_abs_dz_mT) {
        if (norm_x) *norm_x = 0.0f;
        if (norm_y) *norm_y = 0.0f;
        return TMAGJOY_NEUTRAL;
    }

    float rx = s_cal.cphi * dx - s_cal.sphi * dy;
    float ry = s_cal.sphi * dx + s_cal.cphi * dy;
    if (s_cal.invert_x) rx = -rx;
    if (s_cal.invert_y) ry = -ry;
    float ux = rx / s_cal.sx;
    float uy = ry / s_cal.sy;
    float rN = sqrtf(ux * ux + uy * uy);

    if (s_use_hz) {
        if (s_in_neutral) {
            if (rN > s_dz_out) {
                s_in_neutral = 0u;
            } else {
                if (norm_x) *norm_x = 0.0f;
                if (norm_y) *norm_y = 0.0f;
                return TMAGJOY_NEUTRAL;
            }
        } else {
            if (rN < s_dz_in) {
                s_in_neutral = 1u;
                if (norm_x) *norm_x = 0.0f;
                if (norm_y) *norm_y = 0.0f;
                return TMAGJOY_NEUTRAL;
            }
        }
    } else {
        if (rN < s_cal.deadzone) {
            if (norm_x) *norm_x = 0.0f;
            if (norm_y) *norm_y = 0.0f;
            return TMAGJOY_NEUTRAL;
        }
    }

    if (norm_x) *norm_x = ux;
    if (norm_y) *norm_y = uy;
    return dir8_from_angle(atan2f(uy, ux));
}

void TMAGJoy_ReadCalibratedRaw(TMAGJoy *joy, float *nx, float *ny, float *r_abs_mT)
{
    (void)joy;
    float x, y;
    if (TMAG5273_read_mT(&x, &y, NULL) != 0) {
        if (nx) *nx = 0.0f;
        if (ny) *ny = 0.0f;
        if (r_abs_mT) *r_abs_mT = 0.0f;
        return;
    }

    float dx = x - s_cal.cx;
    float dy = y - s_cal.cy;
    float r_abs = sqrtf(dx * dx + dy * dy);
    if (r_abs_mT) *r_abs_mT = r_abs;

    float rx = s_cal.cphi * dx - s_cal.sphi * dy;
    float ry = s_cal.sphi * dx + s_cal.cphi * dy;
    if (s_cal.invert_x) rx = -rx;
    if (s_cal.invert_y) ry = -ry;

    float sx = s_cal.sx;
    float sy = s_cal.sy;
    if (sx < 1e-3f) sx = 1.0f;
    if (sy < 1e-3f) sy = 1.0f;
    if (nx) *nx = rx / sx;
    if (ny) *ny = ry / sy;
}

void TMAGJoy_GetCal(TMAGJoy *joy, TMAGJoy_Cal *out)
{
    (void)joy;
    if (!out) return;
    out->cx = s_cal.cx;
    out->cy = s_cal.cy;
    out->sx = s_cal.sx;
    out->sy = s_cal.sy;
    out->rot_deg = atan2f(s_cal.sphi, s_cal.cphi) * (180.0f / 3.14159265f);
    out->invert_x = s_cal.invert_x;
    out->invert_y = s_cal.invert_y;
}

void TMAGJoy_GetThresholds(TMAGJoy *joy, float *thr_x_mT, float *thr_y_mT)
{
    if (!joy) return;
    if (thr_x_mT) *thr_x_mT = joy->cfg.thr_x_mT;
    if (thr_y_mT) *thr_y_mT = joy->cfg.thr_y_mT;
}

void TMAGJoy_GetAbsDeadzone(TMAGJoy *joy, uint8_t *en, float *mT)
{
    if (!joy) {
        if (en) *en = 0u;
        if (mT) *mT = 0.0f;
        return;
    }
    if (en) *en = joy->cfg.abs_deadzone_en;
    if (mT) *mT = joy->cfg.abs_deadzone_mT;
}


// --- Proportional analog config (software layer) ---
static struct {
    float dz;      // radial deadzone in normalized units (0..0.9)
    float gamma;   // response curve exponent (1.0 = linear; 1.5 softer; 0.7 snappier)
} sJoyCfg = { 0.20f, 1.0f };

static TMAGJoy gJoy;

static TMAGJoy_Dir s_joy_menu_last = TMAGJOY_NEUTRAL;
static uint8_t s_joy_inited = 0u;
static uint8_t s_joy_wait_neutral = 0u;
static uint8_t s_joy_neutral_cnt = 0u;
static uint8_t s_joy_use_irq = 0u;
static uint8_t s_joy_menu_ready = 0u;

#define JOY_NEUTRAL_STABLE_COUNT 3u
#define JOY_NEUTRAL_NORM_THRESH 0.30f
#define JOY_MENU_PRESS_NORM     0.45f
#define JOY_MENU_RELEASE_NORM   0.25f

static void Joy_DisableExtiLine(void);
static void Joy_ConfigExtiPin(uint8_t enable);



// ---------- Defaults & init ----------
static int joy_begin_default(void)
{
    joy_reset();
    int rc = TMAG5273_init_default();
    rc |= TMAG5273_set_magnetic_channels(TMAG5273_CH_XY);
    if (rc == 0) {
        printf("JOY: begin (XY only, dz=%.2f, absDZ=%u @ %.1fmT)\r\n",
               s_cal.deadzone, (unsigned)s_abs_dz_en, s_abs_dz_mT);
    }
    return (rc == 0) ? 0 : -1;
}

TMAGJoy_Config TMAGJoy_DefaultConfig(void)
{
    TMAGJoy_Config c;

    c.int_port = NULL;
    c.int_pin  = 0;

    c.mode         = TMAG5273_MODE_CONTINUOUS;
    c.sleep_time_n = 0;

    c.irq_source     = TMAGJOY_IRQ_RESULT;
    c.int_pulse_10us = 1;
    c.thr_x_mT = 3.0f;
    c.thr_y_mT = 3.0f;

    c.use_hysteresis  = 1;
    c.dz_norm_in      = 0.25f;
    c.dz_norm_out     = 0.35f;
    c.abs_deadzone_en = 0;
    c.abs_deadzone_mT = 2.0f;
    c.invert_x = 0;
    c.invert_y = 0;
    c.extra_rotate_deg = 0.0f;
    c.dir_bias_deg     = 0.0f;

    c.digital_thresh_norm = 0.60f;

    return c;
}

int TMAGJoy_Init(TMAGJoy *joy, const TMAGJoy_Config *cfg)
{
    if (!joy || !cfg) return -1;
    memset(joy, 0, sizeof *joy);
    joy->cfg = *cfg;

    if (joy_begin_default() != 0) return -1;

    (void)TMAG5273_set_operating_mode(cfg->mode);
    (void)TMAG5273_set_sleep_time_n(cfg->sleep_time_n);

    if (cfg->irq_source == TMAGJOY_IRQ_RESULT) {
        (void)TMAG5273_config_int(true, false, cfg->int_pulse_10us,
                                  TMAG5273_INT_MODE_INT, false);
    } else {
        (void)TMAG5273_config_int(false, true, cfg->int_pulse_10us,
                                  TMAG5273_INT_MODE_INT, false);
        (void)TMAG5273_set_x_threshold_mT(cfg->thr_x_mT);
        (void)TMAG5273_set_y_threshold_mT(cfg->thr_y_mT);
    }

    if (cfg->use_hysteresis) {
        TMAGJoy_SetHysteresis(joy, 1u, cfg->dz_norm_in, cfg->dz_norm_out);
    } else {
        TMAGJoy_SetHysteresis(joy, 0u, 0.0f, 0.0f);
        TMAGJoy_SetDeadzoneNorm(joy, cfg->dz_norm_out);
    }
    TMAGJoy_SetAbsDeadzone(joy, cfg->abs_deadzone_en, cfg->abs_deadzone_mT);
    TMAGJoy_SetInvert(joy, cfg->invert_x, cfg->invert_y);
    if (cfg->extra_rotate_deg != 0.0f) {
        TMAGJoy_AddExtraRotation(joy, cfg->extra_rotate_deg);
    }
    if (cfg->dir_bias_deg != 0.0f) {
        TMAGJoy_SetDirBias(joy, cfg->dir_bias_deg);
    }

    (void)TMAG5273_get_device_status(); // clear any latched INT

    return 0;
}

// ---------- Non-blocking calibration: Neutral ----------
void TMAGJoy_CalNeutral_Begin(TMAGJoy *joy, uint32_t window_ms, uint32_t sample_every_ms)
{
    if (!joy) return;
    if (sample_every_ms == 0) sample_every_ms = 10;

    joy->cal_neutral.active          = 1;
    joy->cal_neutral.t_start_ms      = 0;
    joy->cal_neutral.duration_ms     = window_ms ? window_ms : 2000;
    joy->cal_neutral.sample_every_ms = sample_every_ms;
    joy->cal_neutral.last_sample_ms  = 0;
    joy->cal_neutral.n               = 0;
    joy->cal_neutral.sum_x           = 0.0;
    joy->cal_neutral.sum_y           = 0.0;
}

bool TMAGJoy_CalNeutral_Step(TMAGJoy *joy, uint32_t now_ms, float *progress_0to1)
{
    if (!joy || !joy->cal_neutral.active) {
        if (progress_0to1) *progress_0to1 = 0.0f;
        return true;
    }

    if (joy->cal_neutral.t_start_ms == 0) {
        joy->cal_neutral.t_start_ms   = now_ms;
        joy->cal_neutral.last_sample_ms = now_ms;
    }

    if ((now_ms - joy->cal_neutral.last_sample_ms) >= joy->cal_neutral.sample_every_ms) {
        float x, y;
        if (TMAG5273_read_mT(&x, &y, NULL) == 0) {
            joy->cal_neutral.sum_x += x;
            joy->cal_neutral.sum_y += y;
            joy->cal_neutral.n++;
        }
        joy->cal_neutral.last_sample_ms += joy->cal_neutral.sample_every_ms;
    }

    const uint32_t elapsed = now_ms - joy->cal_neutral.t_start_ms;
    float p = (joy->cal_neutral.duration_ms > 0)
            ? (float)elapsed / (float)joy->cal_neutral.duration_ms : 1.0f;
    if (progress_0to1) *progress_0to1 = fclamp(p, 0.0f, 1.0f);

    if (elapsed >= joy->cal_neutral.duration_ms) {
        if (joy->cal_neutral.n > 0u) {
            const float cx = (float)(joy->cal_neutral.sum_x / (double)joy->cal_neutral.n);
            const float cy = (float)(joy->cal_neutral.sum_y / (double)joy->cal_neutral.n);
            TMAGJoy_SetCenter(joy, cx, cy);
        } else {
            (void)TMAGJoy_ZeroHere(joy);
        }
        joy->cal_neutral.active = 0;
        return true;
    }
    return false;
}

// ---------- Non-blocking calibration: Extents / ellipse ----------
void TMAGJoy_CalExtents_Begin(TMAGJoy *joy, uint32_t duration_ms, uint32_t sample_every_ms)
{
    if (!joy) return;
    if (sample_every_ms == 0) sample_every_ms = 20;

    joy->cal_ext.active          = 1;
    joy->cal_ext.t_start_ms      = 0;
    joy->cal_ext.duration_ms     = duration_ms ? duration_ms : 5000;
    joy->cal_ext.sample_every_ms = sample_every_ms;
    joy->cal_ext.last_sample_ms  = 0;

    joy->cal_ext.xmin =  1e9f; joy->cal_ext.xmax = -1e9f;
    joy->cal_ext.ymin =  1e9f; joy->cal_ext.ymax = -1e9f;

    joy->cal_ext.sumx = joy->cal_ext.sumy = 0.0;
    joy->cal_ext.sumxx = joy->cal_ext.sumyy = joy->cal_ext.sumxy = 0.0;
    joy->cal_ext.n = 0;
}

bool TMAGJoy_CalExtents_Step(TMAGJoy *joy, uint32_t now_ms, float *progress_0to1)
{
    if (!joy || !joy->cal_ext.active) {
        if (progress_0to1) *progress_0to1 = 0.0f;
        return true;
    }

    if (joy->cal_ext.t_start_ms == 0) {
        joy->cal_ext.t_start_ms   = now_ms;
        joy->cal_ext.last_sample_ms = now_ms;
    }

    if ((now_ms - joy->cal_ext.last_sample_ms) >= joy->cal_ext.sample_every_ms) {
        float x, y;
        if (TMAG5273_read_mT(&x, &y, NULL) == 0) {
            if (x < joy->cal_ext.xmin) joy->cal_ext.xmin = x;
            if (x > joy->cal_ext.xmax) joy->cal_ext.xmax = x;
            if (y < joy->cal_ext.ymin) joy->cal_ext.ymin = y;
            if (y > joy->cal_ext.ymax) joy->cal_ext.ymax = y;

            joy->cal_ext.sumx  += x;
            joy->cal_ext.sumy  += y;
            joy->cal_ext.sumxx += (double)x * (double)x;
            joy->cal_ext.sumyy += (double)y * (double)y;
            joy->cal_ext.sumxy += (double)x * (double)y;
            joy->cal_ext.n++;
        }
        joy->cal_ext.last_sample_ms += joy->cal_ext.sample_every_ms;
    }

    const uint32_t elapsed = now_ms - joy->cal_ext.t_start_ms;
    float p = (joy->cal_ext.duration_ms > 0)
            ? (float)elapsed / (float)joy->cal_ext.duration_ms : 1.0f;
    if (progress_0to1) *progress_0to1 = fclamp(p, 0.0f, 1.0f);

    if (elapsed >= joy->cal_ext.duration_ms) {
        if (joy->cal_ext.n > 10u) {
            const float sx = (joy->cal_ext.xmax - joy->cal_ext.xmin) * 0.5f;
            const float sy = (joy->cal_ext.ymax - joy->cal_ext.ymin) * 0.5f;
            TMAGJoy_SetSpan(joy, sx, sy);

            const double n = (double)joy->cal_ext.n;
            const double mx = joy->cal_ext.sumx / n;
            const double my = joy->cal_ext.sumy / n;
            const double A = joy->cal_ext.sumxx / n - mx*mx;
            const double B = joy->cal_ext.sumyy / n - my*my;
            const double C = joy->cal_ext.sumxy / n - mx*my;
            const float phi = 0.5f * (float)atan2(2.0*C, (A - B));
            TMAGJoy_SetRotationDeg(joy, phi * (180.0f / (float)M_PI));
        }
        joy->cal_ext.active = 0;
        return true;
    }
    return false;
}

// ---------- Convenience ----------
int TMAGJoy_ZeroHere(TMAGJoy *joy)
{
    (void)joy;
    float x, y;
    if (TMAG5273_read_mT(&x, &y, NULL) != 0) return -1;
    s_cal.cx = x;
    s_cal.cy = y;
    s_in_neutral = 1u;
    printf("JOY: zero_here cx=%.2f cy=%.2f\r\n", s_cal.cx, s_cal.cy);
    return 0;
}

void TMAGJoy_SetCenter(TMAGJoy *joy, float cx, float cy)
{
    (void)joy;
    s_cal.cx = cx;
    s_cal.cy = cy;
    s_in_neutral = 1u;
}

void TMAGJoy_SetSpan(TMAGJoy *joy, float sx, float sy)
{
    (void)joy;
    if (sx < 1e-3f) sx = 1.0f;
    if (sy < 1e-3f) sy = 1.0f;
    s_cal.sx = sx;
    s_cal.sy = sy;
}

void TMAGJoy_SetRotationDeg(TMAGJoy *joy, float deg)
{
    (void)joy;
    const float a = deg * (3.14159265f / 180.0f);
    s_cal.cphi = cosf(a);
    s_cal.sphi = sinf(a);
}

// ---------- Reading ----------
TMAGJoy_Sample TMAGJoy_ReadAnalog(TMAGJoy *joy)
{
    (void)joy;
    TMAGJoy_Sample s = { TMAGJOY_NEUTRAL, 0, 0, 0 };
    float nx = 0.0f, ny = 0.0f, r_abs = 0.0f;

    TMAGJoy_Dir d = joy_read(&nx, &ny, &r_abs);

    // --- Begin: proportional mapping with radial deadzone (no snap) ---
    const float DZ = fclamp(sJoyCfg.dz, 0.0f, 0.90f);

    // Length of normalized vector
    float r = sqrtf(nx*nx + ny*ny);

    if (r <= DZ) {
        // Inside deadzone: hard neutral, no drift.
        nx = 0.0f;
        ny = 0.0f;
        s.dir = TMAGJOY_NEUTRAL;
    } else {
        // Proportional: subtract DZ and re-scale so (DZ..1) -> (0..1)
        float k = (r - DZ) / (1.0f - DZ);
        if (k < 0.0f) k = 0.0f;
        if (k > 1.0f) k = 1.0f;
        if (sJoyCfg.gamma != 1.0f) {
            k = powf(k, sJoyCfg.gamma);
        }

        // Preserve direction; avoid div by zero
        float scale = (r > 1e-6f) ? (k / r) : 0.0f;
        nx *= scale;
        ny *= scale;

        // Direction from your map (keeps UI hints etc.)
        s.dir = d;
    }
    // --- End: proportional mapping with radial deadzone ---

    s.nx = fclamp(nx, -1.0f, 1.0f);
    s.ny = fclamp(ny, -1.0f, 1.0f);
    s.r_abs_mT = r_abs;
    return s;
}


TMAGJoy_Dir TMAGJoy_ReadDigital(TMAGJoy *joy)
{
    float nx = 0.0f, ny = 0.0f, r_abs = 0.0f;
    TMAGJoy_Dir d = joy_read(&nx, &ny, &r_abs);
    (void)r_abs;

    const float rN = sqrtf(nx*nx + ny*ny);
    if (rN < joy->cfg.digital_thresh_norm) return TMAGJOY_NEUTRAL;
    return d;
}

// ---------- Optional INT queue ----------
void TMAGJoy_OnIRQ(TMAGJoy *joy)
{
    if (!joy) return;
    (void)TMAG5273_get_device_status(); // ack
    TMAGJoy_Sample s = TMAGJoy_ReadAnalog(joy);
    if (s.dir == TMAGJOY_NEUTRAL) return;
    uint8_t next = qnext(joy->q_head);
    if (next != joy->q_tail) { joy->q[joy->q_head] = s; joy->q_head = next; }
}

int TMAGJoy_Pop(TMAGJoy *joy, TMAGJoy_Sample *out)
{
    if (!joy || joy->q_head == joy->q_tail) return 0;
    if (out) *out = joy->q[joy->q_tail];
    joy->q_tail = qnext(joy->q_tail);
    return 1;
}

// ---------- Pass-through knobs ----------
void TMAGJoy_SetAbsDeadzone(TMAGJoy *joy, uint8_t en, float mT)
{
    if (joy) {
        joy->cfg.abs_deadzone_en = en ? 1u : 0u;
        joy->cfg.abs_deadzone_mT = mT;
    }
    s_abs_dz_en = en ? 1u : 0u;
    if (mT < 0.0f) mT = 0.0f;
    if (mT > 80.0f) mT = 80.0f;
    s_abs_dz_mT = mT;
}
void TMAGJoy_SetHysteresis(TMAGJoy *joy, uint8_t en, float in_n, float out_n)
{
    (void)joy;
    s_use_hz = en ? 1u : 0u;
    if (en) {
        if (in_n < 0.0f) in_n = 0.0f;
        if (out_n < 0.0f) out_n = 0.0f;
        if (in_n > 0.95f) in_n = 0.95f;
        if (out_n > 0.98f) out_n = 0.98f;
        if (out_n < in_n) out_n = in_n + 0.02f;
        s_dz_in = in_n;
        s_dz_out = out_n;
    }
}
void TMAGJoy_SetInvert(TMAGJoy *joy, uint8_t ix, uint8_t iy)
{
    (void)joy;
    s_cal.invert_x = ix ? 1u : 0u;
    s_cal.invert_y = iy ? 1u : 0u;
}
void TMAGJoy_AddExtraRotation(TMAGJoy *joy, float deg)
{
    (void)joy;
    float a = atan2f(s_cal.sphi, s_cal.cphi) + deg * (3.14159265f / 180.0f);
    s_cal.cphi = cosf(a);
    s_cal.sphi = sinf(a);
}
void TMAGJoy_SetDirBias(TMAGJoy *joy, float deg)
{
    (void)joy;
    s_dir_bias_rad = deg * (3.14159265f / 180.0f);
}

void TMAGJoy_SetDeadzoneNorm(TMAGJoy *joy, float dz_norm)
{
    (void)joy;
    // Ensure hardware hysteresis is OFF for proportional analog response
    s_use_hz = 0u;
    // Clamp and apply a simple normalized deadzone (0..~0.9)
    if (dz_norm < 0.0f) dz_norm = 0.0f;
    if (dz_norm > 0.9f) dz_norm = 0.9f;
    s_cal.deadzone = dz_norm;
}


void TMAGJoy_SetRadialDeadzoneNorm(TMAGJoy *joy, float dz_norm)
{
    (void)joy;
    if (dz_norm < 0.0f) dz_norm = 0.0f;
    if (dz_norm > 0.90f) dz_norm = 0.90f;
    sJoyCfg.dz = dz_norm;
}

void TMAGJoy_SetResponseCurve(TMAGJoy *joy, float gamma)
{
    (void)joy;
    if (gamma < 0.2f) gamma = 0.2f;
    if (gamma > 3.0f) gamma = 3.0f;
    sJoyCfg.gamma = gamma;
}

static TMAGJoy_Sample Joy_ReadMenuSample(void)
{
    TMAGJoy_Sample s = { TMAGJOY_NEUTRAL, 0.0f, 0.0f, 0.0f };
    float nx = 0.0f, ny = 0.0f, r_abs = 0.0f;
    TMAGJoy_Dir d = joy_read(&nx, &ny, &r_abs);
    if (d == TMAGJOY_NEUTRAL) return s;
    s.dir = d;
    s.nx = nx;
    s.ny = ny;
    s.r_abs_mT = r_abs;
    return s;
}

void TMAGJoy_SetIntEnabled(uint8_t enable)
{
    if (!s_joy_inited) {
        if (!enable) {
            Joy_DisableExtiLine();
        }
        return;
    }

    uint8_t result_int = 0u;
    uint8_t thr_int = 0u;
    uint8_t mask_intb = 1u;
    if (enable) {
        result_int = (gJoy.cfg.irq_source == TMAGJOY_IRQ_RESULT) ? 1u : 0u;
        thr_int = (gJoy.cfg.irq_source == TMAGJOY_IRQ_THRESHOLD) ? 1u : 0u;
        mask_intb = 0u;
    }

    (void)TMAG5273_config_int(result_int, thr_int, gJoy.cfg.int_pulse_10us,
                              TMAG5273_INT_MODE_INT, mask_intb);
    (void)TMAG5273_get_device_status();
    __HAL_GPIO_EXTI_CLEAR_IT(TMAG5273_INT_Pin);

    if (enable) {
        Joy_ConfigExtiPin(1u);
        SET_BIT(EXTI->IMR1, TMAG5273_INT_Pin);
        SET_BIT(EXTI->EMR1, TMAG5273_INT_Pin);
        HAL_NVIC_EnableIRQ(TMAG5273_INT_EXTI_IRQn);
    } else {
        Joy_DisableExtiLine();
    }
}

static void Joy_DisableExtiLine(void)
{
    HAL_NVIC_DisableIRQ(TMAG5273_INT_EXTI_IRQn);
    CLEAR_BIT(EXTI->IMR1, TMAG5273_INT_Pin);
    CLEAR_BIT(EXTI->EMR1, TMAG5273_INT_Pin);
    __HAL_GPIO_EXTI_CLEAR_IT(TMAG5273_INT_Pin);
    NVIC_ClearPendingIRQ(TMAG5273_INT_EXTI_IRQn);
    Joy_ConfigExtiPin(0u);
}

static void Joy_ConfigExtiPin(uint8_t enable)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = TMAG5273_INT_Pin;
    if (enable) {
        GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
    } else {
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
    }
    HAL_GPIO_Init(TMAG5273_INT_GPIO_Port, &GPIO_InitStruct);
}

void TMAGJoy_DisableIntLine(void)
{
    Joy_DisableExtiLine();
}

void TMAGJoy_InitOnce(void)
{
    static uint8_t inited = 0u;
    if (inited) return;
    inited = 1u;

    TMAGJoy_Config cfg = TMAGJoy_DefaultConfig();
    cfg.mode = TMAG5273_MODE_CONTINUOUS;
    cfg.irq_source = TMAGJOY_IRQ_THRESHOLD;
    cfg.int_pulse_10us = 1u;
    cfg.thr_x_mT = 6.0f;
    cfg.thr_y_mT = 6.0f;
    (void)TMAGJoy_Init(&gJoy, &cfg);
    s_joy_inited = 1u;
    TMAGJoy_SetIntEnabled(s_joy_use_irq);
}

TMAGJoy *UI_GetJoy(void)
{
    return &gJoy;
}

void TMAGJoy_SetUseIRQ(uint8_t enable)
{
    s_joy_use_irq = enable ? 1u : 0u;
}

void TMAGJoy_OnSleep(void)
{
    s_joy_menu_last = TMAGJOY_NEUTRAL;
    s_joy_wait_neutral = 0u;
    s_joy_neutral_cnt = 0u;
    TMAGJoy_SetIntEnabled(0u);
}

void TMAGJoy_OnWake(void)
{
    TMAGJoy_SetIntEnabled(s_joy_use_irq);
}

void TMAGJoy_MenuEnable(uint8_t on)
{
    s_joy_menu_ready = on ? 1u : 0u;
    s_joy_wait_neutral = on ? 1u : 0u;
    s_joy_menu_last = TMAGJOY_NEUTRAL;
    s_joy_neutral_cnt = 0u;
}

uint8_t TMAGJoy_MenuReady(void)
{
    return s_joy_menu_ready;
}

TMAGJoy_Dir TMAGJoy_MenuIRQ(uint8_t menu_mode, uint8_t in_sleep)
{
    if (!s_joy_use_irq) return TMAGJOY_NEUTRAL;
    (void)TMAG5273_get_device_status();

    if (!s_joy_menu_ready || in_sleep || !menu_mode) {
        s_joy_menu_last = TMAGJOY_NEUTRAL;
        s_joy_wait_neutral = 0u;
        s_joy_neutral_cnt = 0u;
        return TMAGJOY_NEUTRAL;
    }
    if (s_joy_wait_neutral) return TMAGJOY_NEUTRAL;

    TMAGJoy_Sample s = Joy_ReadMenuSample();
    if (s.dir == TMAGJOY_NEUTRAL) {
        s_joy_menu_last = TMAGJOY_NEUTRAL;
        return TMAGJOY_NEUTRAL;
    }
    if (s_joy_menu_last != TMAGJOY_NEUTRAL) return TMAGJOY_NEUTRAL;

    s_joy_menu_last = s.dir;
    s_joy_wait_neutral = 1u;
    s_joy_neutral_cnt = 0u;
    return s.dir;
}

TMAGJoy_Dir TMAGJoy_MenuPoll(uint8_t menu_mode, uint8_t in_sleep)
{
    if (!s_joy_menu_ready || in_sleep || !menu_mode) {
        s_joy_wait_neutral = 0u;
        s_joy_menu_last = TMAGJOY_NEUTRAL;
        s_joy_neutral_cnt = 0u;
        return TMAGJOY_NEUTRAL;
    }

    TMAGJoy_Sample s = Joy_ReadMenuSample();
    const float rN = sqrtf(s.nx * s.nx + s.ny * s.ny);

    if (s_joy_wait_neutral) {
        if (rN <= JOY_MENU_RELEASE_NORM) {
            s_joy_wait_neutral = 0u;
            s_joy_menu_last = TMAGJOY_NEUTRAL;
            s_joy_neutral_cnt = 0u;
        }
        return TMAGJOY_NEUTRAL;
    }

    if (s.dir == TMAGJOY_NEUTRAL || rN < JOY_MENU_PRESS_NORM) return TMAGJOY_NEUTRAL;

    s_joy_menu_last = s.dir;
    s_joy_wait_neutral = 1u;
    s_joy_neutral_cnt = 0u;
    return s.dir;
}

void TMAGJoy_MenuNeutralStep(uint8_t menu_mode, uint8_t in_sleep)
{
    if (!s_joy_menu_ready || in_sleep || !menu_mode) return;
    if (!s_joy_wait_neutral) return;

    TMAGJoy *joy = UI_GetJoy();
    if (!joy) return;
    TMAGJoy_Sample s = TMAGJoy_ReadAnalog(joy);
    float th = (joy->cfg.abs_deadzone_en != 0u) ? joy->cfg.abs_deadzone_mT : 0.0f;
    if (th < 1.0f) th = 2.0f;
    float th_release = th * 1.15f;
    if (th_release < (th + 0.5f)) th_release = th + 0.5f;
    if (s.r_abs_mT <= th_release) {
        if (++s_joy_neutral_cnt >= JOY_NEUTRAL_STABLE_COUNT) {
            s_joy_wait_neutral = 0u;
            s_joy_menu_last = TMAGJOY_NEUTRAL;
            s_joy_neutral_cnt = 0u;
        }
    } else {
        s_joy_neutral_cnt = 0u;
    }
}
