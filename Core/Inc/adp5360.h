#ifndef ADP5360_H
#define ADP5360_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "stm32u5xx_hal.h"   // adjust if your HAL include differs
#include <stdbool.h>  // for bool/true/false

// Provided by user/project:
extern I2C_HandleTypeDef hi2c3;

// If not already defined elsewhere, define the 7-bit address.
// (Datasheet default is 0x46.)
#ifndef ADP5360_I2C_ADDR
#define ADP5360_I2C_ADDR   (0x46u)
#endif


































// HAL uses 8-bit address field; auto-shift if a 7-bit value was supplied.
#if (ADP5360_I2C_ADDR <= 0x7Fu)
  #define ADP5360_I2C_ADDR_W  (ADP5360_I2C_ADDR << 1)
#else
  #define ADP5360_I2C_ADDR_W  (ADP5360_I2C_ADDR)
#endif

// ---- Timeouts / options ----
#ifndef ADP5360_I2C_TIMEOUT_MS
#define ADP5360_I2C_TIMEOUT_MS  100u
#endif

// ---- Register map (Table 17, 0x00..0x36) ----
#define ADP5360_REG_MANUF_MODEL_ID               0x00  // MANUF[3:0], MODEL[3:0]
#define ADP5360_REG_SILICON_REV                  0x01  // REV[3:0]
#define ADP5360_REG_CHARGER_VBUS_ILIM            0x02
#define ADP5360_REG_CHARGER_TERMINATION_SETTING  0x03
#define ADP5360_REG_CHARGER_CURRENT_SETTING      0x04
#define ADP5360_REG_CHARGER_VOLTAGE_THRESHOLD    0x05
#define ADP5360_REG_CHARGER_TIMER_SETTING        0x06
#define ADP5360_REG_CHARGER_FUNCTION_SETTING     0x07
#define ADP5360_REG_CHARGER_STATUS1              0x08
#define ADP5360_REG_CHARGER_STATUS2              0x09
#define ADP5360_REG_BATTERY_THERMISTOR_CONTROL   0x0A
#define ADP5360_REG_THERMISTOR_60C_THRESHOLD     0x0B
#define ADP5360_REG_THERMISTOR_45C_THRESHOLD     0x0C
#define ADP5360_REG_THERMISTOR_10C_THRESHOLD     0x0D
#define ADP5360_REG_THERMISTOR_0C_THRESHOLD      0x0E
#define ADP5360_REG_THR_VOLTAGE_LOW              0x0F
#define ADP5360_REG_THR_VOLTAGE_HIGH             0x10

#define ADP5360_REG_BATPRO_CONTROL               0x11
#define ADP5360_REG_BATPRO_UV_SETTING            0x12
#define ADP5360_REG_BATPRO_DISCH_OC_SETTING      0x13
#define ADP5360_REG_BATPRO_OV_SETTING            0x14
#define ADP5360_REG_BATPRO_CHG_OC_SETTING        0x15

// Fuel gauge table points (%)
#define ADP5360_REG_V_SOC_0                      0x16
#define ADP5360_REG_V_SOC_5                      0x17
#define ADP5360_REG_V_SOC_11                     0x18
#define ADP5360_REG_V_SOC_19                     0x19
#define ADP5360_REG_V_SOC_28                     0x1A
#define ADP5360_REG_V_SOC_41                     0x1B
#define ADP5360_REG_V_SOC_55                     0x1C
#define ADP5360_REG_V_SOC_69                     0x1D
#define ADP5360_REG_V_SOC_84                     0x1E
#define ADP5360_REG_V_SOC_100                    0x1F

#define ADP5360_REG_BAT_CAP                      0x20
#define ADP5360_REG_BAT_SOC                      0x21
#define ADP5360_REG_BAT_SOCACM_CTL               0x22
#define ADP5360_REG_BAT_SOCACM_H                 0x23
#define ADP5360_REG_BAT_SOCACM_L                 0x24
#define ADP5360_REG_VBAT_READ_H                  0x25
#define ADP5360_REG_VBAT_READ_L                  0x26
#define ADP5360_REG_FUEL_GAUGE_MODE              0x27
#define ADP5360_REG_SOC_RESET                    0x28

// Switching regulators
#define ADP5360_REG_BUCK_CONFIG                  0x29
#define ADP5360_REG_BUCK_VOUT_SETTING            0x2A
#define ADP5360_REG_BUCKBST_CONFIG               0x2B
#define ADP5360_REG_BUCKBST_VOUT_SETTING         0x2C

// Supervisory / status / interrupts
#define ADP5360_REG_SUPERVISORY_SETTING          0x2D
#define ADP5360_REG_FAULT                        0x2E
#define ADP5360_REG_PGOOD_STATUS                 0x2F
#define ADP5360_REG_PGOOD1_MASK                  0x30
#define ADP5360_REG_PGOOD2_MASK                  0x31
#define ADP5360_REG_INTERRUPT_ENABLE1            0x32
#define ADP5360_REG_INTERRUPT_ENABLE2            0x33
#define ADP5360_REG_INTERRUPT_FLAG1              0x34
#define ADP5360_REG_INTERRUPT_FLAG2              0x35
#define ADP5360_REG_SHIPMODE                     0x36

// ---- Basic API ----
HAL_StatusTypeDef ADP5360_init(void);                                 // probe only
HAL_StatusTypeDef ADP5360_read(uint8_t reg, uint8_t *buf, uint16_t n);
HAL_StatusTypeDef ADP5360_write(uint8_t reg, const uint8_t *buf, uint16_t n);

// byte helpers
static inline HAL_StatusTypeDef ADP5360_read_u8(uint8_t reg, uint8_t *val) {
    return ADP5360_read(reg, val, 1);
}
static inline HAL_StatusTypeDef ADP5360_write_u8(uint8_t reg, uint8_t val) {
    return ADP5360_write(reg, &val, 1);
}




// --- IDs / Revision (0x00, 0x01) bit fields ---
#define ADP5360_ID_MANUF_MASK   0xF0u  // bits [7:4]
#define ADP5360_ID_MANUF_SHIFT  4
#define ADP5360_ID_MODEL_MASK   0x0Fu  // bits [3:0]
#define ADP5360_REV_MASK        0x0Fu  // bits [3:0]

typedef struct {
    uint8_t manuf;   // 4-bit manufacturer ID (datasheet shows default 0b0001)
    uint8_t model;   // 4-bit model ID (default 0b0000)
} ADP5360_id_t;

// R: 0x00 Manufacturer/Model
HAL_StatusTypeDef ADP5360_get_id(ADP5360_id_t *out);

// R: 0x01 Silicon revision (REV[3:0])
HAL_StatusTypeDef ADP5360_get_revision(uint8_t *rev4);



// ---------- 0x02: CHARGER_VBUS_ILIM ----------
#define ADP5360_REG_CHARGER_VBUS_ILIM   0x02

// Bitfields
#define ADP5360_VADPICHG_MASK  0xE0u  // [7:5]
#define ADP5360_VADPICHG_SHIFT 5
#define ADP5360_VSYSTEM_MASK   0x08u  // [3]
#define ADP5360_ILIM_MASK      0x07u  // [2:0]

// Helpers for allowed values
typedef enum {
    ADP5360_VSYS_VTRM_P200mV = 0,  // VSYS = VTRM + 200mV
    ADP5360_VSYS_5V          = 1,  // VSYS = 5.0V
} ADP5360_vsys_t;

// Read parsed values (units: mV and mA)
HAL_StatusTypeDef ADP5360_get_vbus_ilim(uint16_t *vADPichg_mV,
                                        ADP5360_vsys_t *vsys_mode,
                                        uint16_t *ilim_mA);

// Set all fields atomically; rounds to nearest supported step.
// Allowed: vADPichg_mV ∈ {4400,4500,4600,4700,4800,4900}
//          ilim_mA ∈ {50,100,150,200,250,300,400,500}
HAL_StatusTypeDef ADP5360_set_vbus_ilim(uint16_t vadpichg_mV,
                                        ADP5360_vsys_t vsys_mode,
                                        uint16_t ilim_mA);



// ---------- 0x03: CHARGER_TERMINATION_SETTING ----------
#define ADP5360_REG_CHARGER_TERMINATION_SETTING  0x03

// Bitfields
#define ADP5360_VTRM_MASK    0xFCu  // [7:2] 6-bit code
#define ADP5360_VTRM_SHIFT   2
#define ADP5360_ITRK_MASK    0x03u  // [1:0]

// Get VTRM (mV) and ITRK/WEAK (deci-mA; e.g., 25 => 2.5 mA)
HAL_StatusTypeDef ADP5360_get_chg_term(uint16_t *vtrm_mV,
                                       uint16_t *itrk_deci_mA);

// Set VTRM (mV: 3560..4660, step 20) and ITRK/WEAK (deci-mA: 10,25,50,100).
// Values are clamped to the nearest supported code.
HAL_StatusTypeDef ADP5360_set_chg_term(uint16_t vtrm_mV,
                                       uint16_t itrk_deci_mA);


// ---------- 0x04: CHARGER_CURRENT_SETTING ----------
#define ADP5360_REG_CHARGER_CURRENT_SETTING   0x04

#define ADP5360_IEND_MASK   0xE0u  // [7:5]
#define ADP5360_IEND_SHIFT  5
#define ADP5360_ICHG_MASK   0x1Fu  // [4:0]

// Get termination current (mA) and fast charge current (mA)
HAL_StatusTypeDef ADP5360_get_chg_current(uint16_t *iend_mA,
                                          uint16_t *ichg_mA);

// Set termination and fast charge current (mA).
// Valid IEND: {5,7.5,12.5,17.5,22.5,27.5,32.5}
// Valid ICHG: 10..320 in 10 mA steps.
HAL_StatusTypeDef ADP5360_set_chg_current(uint16_t iend_mA,
                                          uint16_t ichg_mA);


// Precise deci-mA versions (do not replace existing ones)
HAL_StatusTypeDef ADP5360_get_chg_current_dmA(uint16_t *iend_dmA,
                                              uint16_t *ichg_dmA);
HAL_StatusTypeDef ADP5360_set_chg_current_dmA(uint16_t iend_dmA,
                                              uint16_t ichg_dmA);




// ---------- 0x05: CHARGER_VOLTAGE_THRESHOLD ----------
#define ADP5360_REG_CHARGER_VOLTAGE_THRESHOLD  0x05

// Bitfields
#define ADP5360_DIS_RCH_MASK   0x80u     // [7]
#define ADP5360_VRCH_MASK      0x60u     // [6:5]
#define ADP5360_VRCH_SHIFT     5
#define ADP5360_VTRK_MASK      0x18u     // [4:3]
#define ADP5360_VTRK_SHIFT     3
#define ADP5360_VWEAK_MASK     0x07u     // [2:0]

// API (units: mV; booleans are 0/1)
HAL_StatusTypeDef ADP5360_get_voltage_thresholds(
    uint8_t  *recharge_disabled,  // DIS_RCH (1=disabled)
    uint16_t *vrch_mV,            // 120/180/240
    uint16_t *vtrk_dead_mV,       // 2000/2500/2600/2900
    uint16_t *vweak_mV            // 2700..3400 step 100
);

HAL_StatusTypeDef ADP5360_set_voltage_thresholds(
    uint8_t  recharge_disabled,   // 0 or 1
    uint16_t vrch_mV,             // must be 120/180/240
    uint16_t vtrk_dead_mV,        // 2000/2500/2600/2900
    uint16_t vweak_mV             // 2700..3400 step 100
);


// ---------- 0x06: CHARGER_TIMER_SETTING ----------
#define ADP5360_REG_CHARGER_TIMER_SETTING  0x06

#define ADP5360_EN_TEND_MASK     0x08u   // [3]
#define ADP5360_EN_CHG_TIMER_MASK 0x04u  // [2]
#define ADP5360_CHG_TMR_MASK     0x03u   // [1:0]

typedef enum {
    ADP5360_TMR_15_150 = 0,  // tmx=15 min, tcc=150 min
    ADP5360_TMR_30_300 = 1,
    ADP5360_TMR_45_450 = 2,
    ADP5360_TMR_60_600 = 3   // default
} ADP5360_tmr_period_t;

// Read: booleans + enum, plus minutes (optional outputs can be NULL)
HAL_StatusTypeDef ADP5360_get_chg_timers(
    uint8_t *en_tend,
    uint8_t *en_chg_timer,
    ADP5360_tmr_period_t *period,
    uint16_t *tmx_min,    // trickle timer (15/30/45/60)
    uint16_t *tcc_min     // fast-charge timer (150/300/450/600)
);

// Set: booleans + enum
HAL_StatusTypeDef ADP5360_set_chg_timers(
    uint8_t en_tend,
    uint8_t en_chg_timer,
    ADP5360_tmr_period_t period
);


// ---------- 0x07: CHARGER_FUNCTION_SETTING ----------
#define ADP5360_REG_CHARGER_FUNCTION_SETTING  0x07

#define ADP5360_EN_JEITA_MASK        0x80u
#define ADP5360_ILIM_JEITA_COOL_MASK 0x40u
// bit5 reserved
#define ADP5360_OFF_ISOFET_MASK      0x10u
#define ADP5360_EN_LDO_MASK          0x08u
#define ADP5360_EN_EOC_MASK          0x04u
#define ADP5360_EN_ADPICHG_MASK      0x02u
#define ADP5360_EN_CHG_MASK          0x01u

typedef struct {
    uint8_t en_jeita;         // 0/1
    uint8_t ilim_jeita_cool;  // 0≈50% ICHG, 1≈10% ICHG
    uint8_t off_isofet;       // 1 forces ISOFET off (VSYS off if only battery)
    uint8_t en_ldo;           // 1 enable charge LDO
    uint8_t en_eoc;           // 1 allow end-of-charge
    uint8_t en_adpichg;       // 1 enable adaptive VBUS current limit
    uint8_t en_chg;           // 1 enable charging
} ADP5360_func_t;

HAL_StatusTypeDef ADP5360_get_chg_function(ADP5360_func_t *f);
HAL_StatusTypeDef ADP5360_set_chg_function(const ADP5360_func_t *f);


// ---------- 0x08: CHARGER_STATUS1 (READ ONLY) ----------
#define ADP5360_REG_CHARGER_STATUS1  0x08

#define ADP5360_ST1_VBUS_OV_MASK   0x80u  // [7]
#define ADP5360_ST1_ADPICHG_MASK   0x40u  // [6]
#define ADP5360_ST1_VBUS_ILIM_MASK 0x20u  // [5]
#define ADP5360_ST1_STATE_MASK     0x07u  // [2:0]

// 3-bit charger state (CHARGER_STATUS[2:0])
typedef enum {
    ADP5360_CHG_OFF          = 0x0,
    ADP5360_CHG_TRICKLE      = 0x1,
    ADP5360_CHG_FAST_CC      = 0x2,  // constant-current mode
    ADP5360_CHG_FAST_CV      = 0x3,  // constant-voltage mode
    ADP5360_CHG_COMPLETE     = 0x4,
    ADP5360_CHG_LDO_MODE     = 0x5,
    ADP5360_CHG_TIMER_EXPIRED= 0x6,  // trickle or fast charge timer expired
    ADP5360_CHG_BATT_DETECT  = 0x7
} ADP5360_chg_state_t;

typedef struct {
    uint8_t vbus_ov;        // 1 if VBUS over threshold (VBUS_OK)
    uint8_t adpichg_active; // 1 if adaptive charge current active
    uint8_t vbus_ilim;      // 1 if limited by VBUS input ILIM
    ADP5360_chg_state_t state;
} ADP5360_status1_t;

// Read & decode STATUS1
HAL_StatusTypeDef ADP5360_get_status1(ADP5360_status1_t *s);

// Optional: tiny helper to stringify the state for logs
const char* ADP5360_chg_state_str(ADP5360_chg_state_t st);

bool ADP5360_is_charging(void);

// ---------- 0x09: CHARGER_STATUS2 (READ ONLY) ----------
#define ADP5360_REG_CHARGER_STATUS2  0x09

#define ADP5360_ST2_THR_MASK     0xE0u  // [7:5]
#define ADP5360_ST2_BAT_OV_MASK  0x10u  // [4]
#define ADP5360_ST2_BAT_UV_MASK  0x08u  // [3]
#define ADP5360_ST2_BAT_CHG_MASK 0x07u  // [2:0]

// THR pin / NTC state
typedef enum {
    ADP5360_THR_OFF   = 0x0,
    ADP5360_THR_COLD  = 0x1,
    ADP5360_THR_COOL  = 0x2,
    ADP5360_THR_WARM  = 0x3,
    ADP5360_THR_HOT   = 0x4,
    ADP5360_THR_OK    = 0x7,  // in-range
} ADP5360_thr_status_t;

// Battery status when charging (per table)
typedef enum {
    ADP5360_BATSTAT_NORMAL   = 0x0, // normal (not charging / outside special cases)
    ADP5360_BATSTAT_NO_BATT  = 0x1, // no battery detected
    ADP5360_BATSTAT_LE_VTRK  = 0x2, // Vbat ≤ VTRK_DEAD (when in charge)
    ADP5360_BATSTAT_BETWEEN  = 0x3, // VTRK_DEAD < Vbat < VWEAK (when in charge)
    ADP5360_BATSTAT_GE_VWEAK = 0x4, // Vbat ≥ VWEAK (when in charge)
    // other codes not listed → treat as unknown
} ADP5360_bat_chg_status_t;

typedef struct {
    ADP5360_thr_status_t     thr;
    uint8_t                  bat_ov;     // 1=OV protection active
    uint8_t                  bat_uv;     // 1=UV protection active
    ADP5360_bat_chg_status_t bat_status; // BAT_CHG_STATUS[2:0]
} ADP5360_status2_t;

HAL_StatusTypeDef ADP5360_get_status2(ADP5360_status2_t *s);
const char* ADP5360_thr_status_str(ADP5360_thr_status_t t);
const char* ADP5360_bat_status_str(ADP5360_bat_chg_status_t b);


// ---------- 0x0A: BATTERY_THERMISTOR_CONTROL ----------
#define ADP5360_REG_BATTERY_THERMISTOR_CONTROL  0x0A

#define ADP5360_ITHR_MASK   0xC0u   // [7:6]
#define ADP5360_ITHR_SHIFT  6
#define ADP5360_EN_THR_MASK 0x01u   // [0]

typedef enum {
    ADP5360_ITHR_60UA = 0,   // 60 µA
    ADP5360_ITHR_12UA = 1,   // 12 µA
    ADP5360_ITHR_6UA  = 2    // 6 µA (codes 10 or 11)
} ADP5360_ithr_t;

HAL_StatusTypeDef ADP5360_get_ntc_ctrl(ADP5360_ithr_t *ithr, uint8_t *en_thr);
HAL_StatusTypeDef ADP5360_set_ntc_ctrl(ADP5360_ithr_t ithr, uint8_t en_thr);


// ---------- 0x0B -> 0x0E: BATTERY_THERMISTOR_THRESHOLDS ----------
// --- NTC thresholds: 0x0B..0x0E (mV-based helpers) ---
HAL_StatusTypeDef ADP5360_get_ntc_thresholds(
    uint16_t *v60c_mV,  // code * 2 mV
    uint16_t *v45c_mV,  // code * 2 mV
    uint16_t *v10c_mV,  // code * 10 mV
    uint16_t *v0c_mV    // code * 10 mV
);

// Clamp to LSB size & 8-bit range. Pass mV at THR pin.
HAL_StatusTypeDef ADP5360_set_ntc_thresholds(
    uint16_t v60c_mV,   // multiples of 2 mV
    uint16_t v45c_mV,   // multiples of 2 mV
    uint16_t v10c_mV,   // multiples of 10 mV
    uint16_t v0c_mV     // multiples of 10 mV
);


// ---------- 0x0F -> 0x10: THR_VOLTAGE (readback) ----------
/*
 * THR_VOLTAGE Low/High give the instantaneous thermistor-node voltage in mV.
 * Raw is a 12-bit value: RAW = {HIGH[3:0], LOW[7:0]} and THR (mV) = RAW.
 * (From DS note: NTC[kΩ] ≈ THR[mV] / ITHR[µA].)
 */
HAL_StatusTypeDef ADP5360_get_thr_voltage(uint16_t *thr_mV, uint16_t *raw12);



// ---------- 0x11: BATTERY_PROTECTION_CONTROL ----------
#define ADP5360_REG_BATPRO_CONTROL       0x11

#define ADP5360_ISOFET_OVCHG_MASK        0x10u  // [4]
#define ADP5360_OC_DIS_HICCUP_MASK       0x08u  // [3] 0=latch, 1=hiccup
#define ADP5360_OC_CHG_HICCUP_MASK       0x04u  // [2] 0=latch, 1=hiccup
#define ADP5360_EN_CHGLB_MASK            0x02u  // [1] 1=allow charge under UVP
#define ADP5360_EN_BATPRO_MASK           0x01u  // [0] 1=enable battery protection

typedef struct {
    uint8_t en_batpro;        // 0=disable, 1=enable battery protection (recommended: 1)
    uint8_t en_chglb;         // 0=block charge on UVP, 1=allow charge on UVP
    uint8_t oc_chg_hiccup;    // 0=latch on charge OC, 1=hiccup mode
    uint8_t oc_dis_hiccup;    // 0=latch on discharge OC, 1=hiccup mode
    uint8_t isofet_ovchg;     // 0=ISOFET turns ON on OVCHG, 1=ISOFET turns OFF on OVCHG
} ADP5360_batpro_ctrl_t;

HAL_StatusTypeDef ADP5360_get_batpro_ctrl(ADP5360_batpro_ctrl_t *cfg);
HAL_StatusTypeDef ADP5360_set_batpro_ctrl(const ADP5360_batpro_ctrl_t *cfg);



// ---------- 0x12: BATPRO_UNDERVOLTAGE_SETTING ----------
#define ADP5360_REG_BATPRO_UV_SETTING      0x12

#define ADP5360_UV_DISCH_MASK              0xF0u  // [7:4] 2.05V..2.80V (50 mV/step)
#define ADP5360_UV_DISCH_SHIFT             4
#define ADP5360_HYS_UV_DISCH_MASK          0x0Cu  // [3:2] {2,4,6,8} %
#define ADP5360_HYS_UV_DISCH_SHIFT         2
#define ADP5360_DGT_UV_DISCH_MASK          0x03u  // [1:0] {30,60,120,240} ms

// Get UV threshold (mV), hysteresis (%), and deglitch time (ms)
HAL_StatusTypeDef ADP5360_get_uv_setting(uint16_t *uv_mV,
                                         uint8_t  *hys_pct,
                                         uint16_t *dgt_ms);

// Set UV threshold (mV), hysteresis (%), and deglitch time (ms).
// uv_mV valid range: 2050..2800 mV (50 mV steps -> nearest).
// hys_pct: nearest of {2,4,6,8}. dgt_ms: nearest of {30,60,120,240}.
HAL_StatusTypeDef ADP5360_set_uv_setting(uint16_t uv_mV,
                                         uint8_t  hys_pct,
                                         uint16_t dgt_ms);



// ---------- 0x13: BATPRO_DISCH_OC_SETTING ----------
#define ADP5360_REG_BATPRO_DISCH_OC_SETTING   0x13

#define ADP5360_OC_DISCH_MASK     0xE0u  // [7:5]  {50,100,150,200,300,400,500,600} mA
#define ADP5360_OC_DISCH_SHIFT    5
// bit4 reserved
#define ADP5360_DGT_OC_DISCH_MASK 0x0Eu  // [3:1]  {0.5,1,5,10,20,50,100} ms (001..111)
#define ADP5360_DGT_OC_DISCH_SHIFT 1

// Get: discharge OC threshold (mA) and deglitch (ms). If code=000 for deglitch, returns 0 ms.
HAL_StatusTypeDef ADP5360_get_dis_oc(uint16_t *oc_mA, uint16_t *dgt_ms);

// Set: threshold (mA) ∈ {50,100,150,200,300,400,500,600}, deglitch (ms) nearest of {0.5,1,5,10,20,50,100}
HAL_StatusTypeDef ADP5360_set_dis_oc(uint16_t oc_mA, float dgt_ms);


// ---------- 0x14: BATPRO_OV_SETTING ----------
#define ADP5360_REG_BATPRO_OV_SETTING       0x14

#define ADP5360_OV_CHG_MASK                 0xF8u  // [7:3] 3.55..4.80 V (≈50 mV/step; top codes clamp to 4.80 V)
#define ADP5360_OV_CHG_SHIFT                3
#define ADP5360_HYS_OV_CHG_MASK             0x06u  // [2:1] hysteresis {2,4,6,8} % of OV_CHG
#define ADP5360_HYS_OV_CHG_SHIFT            1
#define ADP5360_DGT_OV_CHG_MASK             0x01u  // [0] deglitch: 0=0.5 s, 1=1 s

// Read OV threshold (mV), hysteresis (% of threshold), and deglitch time (ms).
HAL_StatusTypeDef ADP5360_get_ov_setting(uint16_t *ov_mV,
                                         uint8_t  *hys_pct,
                                         uint16_t *dgt_ms);

// Set OV threshold (mV), hysteresis (%), and deglitch (ms).
// ov_mV valid ~3550..4800 (rounded to 50 mV steps, clamped to 4800).
// hys_pct nearest of {2,4,6,8}. dgt_ms nearest of {500, 1000}.
HAL_StatusTypeDef ADP5360_set_ov_setting(uint16_t ov_mV,
                                         uint8_t  hys_pct,
                                         uint16_t dgt_ms);



// ---------- 0x15: BATPRO_CHG_OC_SETTING ----------
#define ADP5360_REG_BATPRO_CHG_OC_SETTING   0x15

#define ADP5360_OC_CHG_MASK        0xE0u  // [7:5] {25,50,100,150,200,250,300,400} mA
#define ADP5360_OC_CHG_SHIFT       5
#define ADP5360_DGT_OC_CHG_MASK    0x18u  // [4:3] {5,10,20,40} ms
#define ADP5360_DGT_OC_CHG_SHIFT   3
// [2:0] reserved

// Read charge overcurrent threshold (mA) and deglitch (ms)
HAL_StatusTypeDef ADP5360_get_chg_oc(uint16_t *oc_mA, uint16_t *dgt_ms);

// Set charge overcurrent threshold (mA) and deglitch (ms)
// oc_mA ∈ {25,50,100,150,200,250,300,400} (nearest used)
// dgt_ms nearest of {5,10,20,40}
HAL_StatusTypeDef ADP5360_set_chg_oc(uint16_t oc_mA, uint16_t dgt_ms);



// ---------- 0x16 -> 0x1F: FUEL_GAUGE_V_SOC_TABLE ----------
/*
 * Ten points mapping VBAT to SOC (%). Each register is 8 mV/LSB with 2.5 V offset:
 *   VBAT(mV) = 2500 + 8 * CODE
 * Addresses: 0x16 (0%), 0x17 (5%), 0x18 (11%), 0x19 (19%), 0x1A (28%),
 *            0x1B (41%), 0x1C (55%), 0x1D (69%), 0x1E (84%), 0x1F (100%)
 */

// Read all 10 points into an array of mV (index order: 0,5,11,19,28,41,55,69,84,100).
HAL_StatusTypeDef ADP5360_get_vsoc_table_mv(uint16_t vsoc_mV[10]);

// Read a single point (pass the register address: ADP5360_REG_V_SOC_xx).
HAL_StatusTypeDef ADP5360_get_vsoc_point_mv(uint8_t reg_addr, uint16_t *vbatt_mV);


// ---------- 0x20: BAT_CAP ----------
#define ADP5360_REG_BAT_CAP  0x20
// Capacity (mAh) = REG * 2 mAh  (8-bit register, 0..510 mAh)

HAL_StatusTypeDef ADP5360_get_bat_capacity(uint16_t *capacity_mAh);
HAL_StatusTypeDef ADP5360_set_bat_capacity(uint16_t capacity_mAh);  // rounded to nearest 2 mAh, clamped 0..510


// ---------- 0x21: BAT_SOC ----------
#define ADP5360_REG_BAT_SOC  0x21
// BAT_SOC[6:0] -> percentage 0..100 (%)

HAL_StatusTypeDef ADP5360_get_soc(uint8_t *soc_percent, uint8_t *raw7);


// ---------- 0x22: BAT_SOCACM_CTL ----------
#define ADP5360_REG_BAT_SOCACM_CTL  0x22

#define ADP5360_BATCAP_AGE_MASK   0xC0u  // [7:6] capacity reduction on overflow {0.8,1.5,3.1,6.3} %
#define ADP5360_BATCAP_AGE_SHIFT  6
#define ADP5360_BATCAP_TEMP_MASK  0x30u  // [5:4] temp coeff {0.2,0.4,0.6,0.8} %/°C
#define ADP5360_BATCAP_TEMP_SHIFT 4
// [3:2] reserved
#define ADP5360_EN_BATCAP_TEMP    0x02u  // [1] 1=enable temp compensation
#define ADP5360_EN_BATCAP_AGE     0x01u  // [0] 1=enable aging auto-adjust

typedef struct {
    float age_reduction_pct;     // {0.8, 1.5, 3.1, 6.3}
    float temp_coeff_pct_per_C;  // {0.2, 0.4, 0.6, 0.8}
    uint8_t en_temp_comp;        // 0/1
    uint8_t en_age_comp;         // 0/1
} ADP5360_socacm_ctl_t;

HAL_StatusTypeDef ADP5360_get_socaccum_ctl(ADP5360_socacm_ctl_t *ctl);
HAL_StatusTypeDef ADP5360_set_socaccum_ctl(const ADP5360_socacm_ctl_t *ctl);



// ---------- 0x23 -> 0x24: BAT_SOCACM_H/L ----------
#define ADP5360_REG_BAT_SOCACM_H  0x23  // [11:4]
#define ADP5360_REG_BAT_SOCACM_L  0x24  // [7:4] -> low nibble [3:0]

// Read the 12-bit accumulator and convert to “number of times charging”.
// Per DS: charge_times = RAW12 / 100.
HAL_StatusTypeDef ADP5360_get_soc_accumulator(uint16_t *raw12, float *charge_times);



// ---------- 0x25 -> 0x26: VBAT_READ ----------
/*
 * 12-bit battery voltage reading in mV:
 *   RAW12 = {VBAT_READ_H[7:0], VBAT_READ_L[7:3]}
 *   VBAT(mV) = RAW12
 */
HAL_StatusTypeDef ADP5360_get_vbat(uint16_t *vbat_mV, uint16_t *raw12);

// ---------- 0x27: FUEL_GAUGE_MODE ----------
#define ADP5360_REG_FUEL_GAUGE_MODE  0x27

#define ADP5360_SOC_LOW_TH_MASK   0xC0u  // [7:6] {6,11,21,31} %
#define ADP5360_SOC_LOW_TH_SHIFT  6
#define ADP5360_SLP_CURR_MASK     0x30u  // [5:4] {5,10,20,40} mA
#define ADP5360_SLP_CURR_SHIFT    4
#define ADP5360_SLP_TIME_MASK     0x0Cu  // [3:2] {1,4,8,16} min (update period in sleep)
#define ADP5360_SLP_TIME_SHIFT    2
#define ADP5360_FG_MODE_MASK      0x02u  // [1] 1=sleep, 0=active
#define ADP5360_EN_FG_MASK        0x01u  // [0] 1=enable fuel gauge

typedef struct {
    uint8_t  soc_low_th_pct;   // 6/11/21/31
    uint16_t slp_curr_mA;      // 5/10/20/40
    uint16_t slp_time_min;     // 1/4/8/16
    uint8_t  fg_mode_sleep;    // 1=sleep, 0=active
    uint8_t  en_fg;            // 1=enabled
} ADP5360_fg_mode_t;

HAL_StatusTypeDef ADP5360_get_fg_mode(ADP5360_fg_mode_t *m);
HAL_StatusTypeDef ADP5360_set_fg_mode(const ADP5360_fg_mode_t *m);

// Convenience: enable FG in ACTIVE mode with typical defaults (11%, 10mA, 1min)
HAL_StatusTypeDef ADP5360_fg_enable_active(void);

// ---------- 0x28: SOC_RESET ----------
#define ADP5360_REG_SOC_RESET  0x28
#define ADP5360_SOC_RESET_MASK 0x80u  // [7] write-only pulse: write 1, then write 0

// Pulse the SOC reset bit (refreshes BAT_SOC and VBAT_READ_H/L).
HAL_StatusTypeDef ADP5360_fg_refresh(void);


// ---------- 0x29: BUCK_CONFIGURE ----------
#define ADP5360_REG_BUCK_CONFIGURE  0x29

#define ADP5360_BUCK_SS_MASK    0xC0u   // [7:6] soft-start {1,8,64,512} ms
#define ADP5360_BUCK_SS_SHIFT   6
#define ADP5360_BUCK_ILIM_MASK  0x30u   // [5:4] peak current limit {100,200,300,400} mA
#define ADP5360_BUCK_ILIM_SHIFT 4
#define ADP5360_BUCK_MODE_MASK  0x08u   // [3] 0=hysteresis, 1=FPWM
#define ADP5360_STP_BUCK_MASK   0x04u   // [2] 1=enable pulse-stop feature
#define ADP5360_DISCHG_BUCK_MASK 0x02u  // [1] 1=enable output discharge
#define ADP5360_EN_BUCK_MASK    0x01u   // [0] 1=enable buck output

typedef struct {
    uint16_t softstart_ms;   // 1/8/64/512
    uint16_t ilim_mA;        // 100/200/300/400
    uint8_t  fpwm_mode;      // 0=hysteresis, 1=FPWM
    uint8_t  stop_enable;    // 0/1
    uint8_t  discharge_en;   // 0/1
    uint8_t  enable;         // 0/1
} ADP5360_buck_cfg_t;

HAL_StatusTypeDef ADP5360_get_buck(ADP5360_buck_cfg_t *cfg);
HAL_StatusTypeDef ADP5360_set_buck(const ADP5360_buck_cfg_t *cfg);


// ---------- 0x2A: BUCK_OUTPUT_VOLTAGE_SETTING ----------
#define ADP5360_REG_BUCK_OUTPUT_VOLTAGE   0x2A

#define ADP5360_BUCK_DLY_MASK   0xC0u  // [7:6] {0,5,10,20} µs
#define ADP5360_BUCK_DLY_SHIFT  6
#define ADP5360_VOUT_BUCK_MASK  0x3Fu  // [5:0] 0.60 V + 50 mV * code (0..63)

// Read VOUT (mV) and the hysteresis delay (µs)
HAL_StatusTypeDef ADP5360_get_buck_vout(uint16_t *vout_mV, uint16_t *dly_us);

// Set VOUT (mV) and delay (µs). VOUT is clamped 600..3750 mV in 50 mV steps.
// Delay is snapped to nearest of {0,5,10,20} µs.
HAL_StatusTypeDef ADP5360_set_buck_vout(uint16_t vout_mV, uint16_t dly_us);


// ---------- 0x2B: BUCKBOOST_CONFIGURE ----------
#define ADP5360_REG_BUCKBST_CONFIGURE   0x2B

#define ADP5360_BUCKBST_SS_MASK   0xC0u  // [7:6] soft-start {1,8,64,512} ms
#define ADP5360_BUCKBST_SS_SHIFT  6
#define ADP5360_BUCKBST_ILIM_MASK 0x38u  // [5:3] peak current {100..800} mA (8 options)
#define ADP5360_BUCKBST_ILIM_SHIFT 3
#define ADP5360_STP_BUCKBST_MASK  0x04u  // [2] 1=enable pulse stop
#define ADP5360_DISCHG_BUCKBST_MASK 0x02u // [1] 1=enable output discharge
#define ADP5360_EN_BUCKBST_MASK   0x01u  // [0] 1=enable buck-boost output

typedef struct {
    uint16_t softstart_ms;   // 1/8/64/512
    uint16_t ilim_mA;        // 100/200/300/400/500/600/700/800
    uint8_t  stop_enable;    // 0/1
    uint8_t  discharge_en;   // 0/1
    uint8_t  enable;         // 0/1
} ADP5360_buckbst_cfg_t;

HAL_StatusTypeDef ADP5360_get_buckboost(ADP5360_buckbst_cfg_t *cfg);
HAL_StatusTypeDef ADP5360_set_buckboost(const ADP5360_buckbst_cfg_t *cfg);


// ---------- 0x2C: BUCKBOOST_OUTPUT_VOLTAGE_SETTING ----------
#define ADP5360_REG_BUCKBST_OUTPUT_VOLTAGE  0x2C

#define ADP5360_BUCKBST_DLY_MASK   0xC0u  // [7:6] {0,5,10,20} µs
#define ADP5360_BUCKBST_DLY_SHIFT  6
#define ADP5360_VOUT_BUCKBST_MASK  0x3Fu  // [5:0] piecewise Vout code:
                                          // 0..11  -> 1.8V + 0.1V*code   (1.8..2.9V)
                                          // 12..63 -> 2.95V + 0.05V*(code-12) (2.95..5.5V)

HAL_StatusTypeDef ADP5360_get_buckboost_vout(uint16_t *vout_mV, uint16_t *dly_us);
HAL_StatusTypeDef ADP5360_set_buckboost_vout(uint16_t vout_mV, uint16_t dly_us);


// ---------- 0x2D: SUPERVISORY_SETTING ----------
#define ADP5360_REG_SUPERVISORY_SETTING   0x2D

#define ADP5360_VOUT1_RST_MASK    0x80u  // [7] 1=route BUCK VMON to RESET
#define ADP5360_VOUT2_RST_MASK    0x40u  // [6] 1=route BUCK-BOOST VMON to RESET
#define ADP5360_RESET_TIME_MASK   0x20u  // [5] 0=200ms, 1=1.6s
#define ADP5360_WD_TIME_MASK      0x18u  // [4:3] 00=12.5s, 01=25.6s, 10=50s, 11=100s
#define ADP5360_WD_TIME_SHIFT     3
#define ADP5360_EN_WD_MASK        0x04u  // [2] 1=enable watchdog
#define ADP5360_EN_MR_SD_MASK     0x02u  // [1] 1=MR(12s) -> shipment mode
#define ADP5360_RESET_WD_MASK     0x01u  // [0] write 1 to kick watchdog (auto-clear)

typedef struct {
    uint8_t  buck_rst_en;       // route BUCK VMON to RESET (0/1)
    uint8_t  buckbst_rst_en;    // route BUCK-BOOST VMON to RESET (0/1)
    uint16_t reset_time_ms;     // 200 or 1600
    float    wd_time_s;         // 12.5 / 25.6 / 50 / 100
    uint8_t  wd_enable;         // 0/1
    uint8_t  mr_shipment_en;    // 0/1 (MR low 12s enters shipment)
} ADP5360_supervisory_t;

HAL_StatusTypeDef ADP5360_get_supervisory(ADP5360_supervisory_t *cfg);
HAL_StatusTypeDef ADP5360_set_supervisory(const ADP5360_supervisory_t *cfg);

// Kick the watchdog (write 1 to RESET_WD; bit auto-clears)
HAL_StatusTypeDef ADP5360_watchdog_kick(void);


// ---------- 0x2E: FAULT ----------
#define ADP5360_REG_FAULT      0x2E

#define ADP5360_FLT_BAT_UV     0x80u  // [7] undervoltage during overdischarge
#define ADP5360_FLT_BAT_OC     0x40u  // [6] overcurrent during overdischarge
#define ADP5360_FLT_BAT_CHGOC  0x20u  // [5] charge overcurrent during overcharge
#define ADP5360_FLT_BAT_CHGOV  0x10u  // [4] charge overvoltage during overcharge
// [3] reserved
#define ADP5360_FLT_WD_TIMEOUT 0x04u  // [2] watchdog timeout occurred
// [1] reserved
#define ADP5360_FLT_TSD110     0x01u  // [0] thermal shutdown

// Read fault flags (returns raw mask above)
HAL_StatusTypeDef ADP5360_get_fault(uint8_t *mask);

// Clear selected faults: pass a mask made of ADP5360_FLT_* bits (write-1-to-clear)
HAL_StatusTypeDef ADP5360_clear_fault(uint8_t mask);

// Convenience: clear all defined fault bits
HAL_StatusTypeDef ADP5360_clear_all_faults(void);


// ---------- 0x2F: PGOOD_STATUS ----------
#define ADP5360_REG_PGOOD_STATUS  0x2F

#define ADP5360_PG_MR_PRESS   0x20u  // [5] MR pin pulled low (after tDG)
#define ADP5360_PG_CHG_CMPLT  0x10u  // [4] charger complete
#define ADP5360_PG_VBUSOK     0x08u  // [3] VBUS within OK window
#define ADP5360_PG_BATOK      0x04u  // [2] VBAT > VWEAK (FG enabled)
#define ADP5360_PG_VOUT2OK    0x02u  // [1] Buck-Boost power-good
#define ADP5360_PG_VOUT1OK    0x01u  // [0] Buck power-good

typedef struct {
    uint8_t mr_press;   // 1=MR low detected
    uint8_t chg_cmplt;  // 1=charge complete
    uint8_t vbus_ok;    // 1=VBUS_OK window
    uint8_t bat_ok;     // 1=VBAT > VWEAK (FG on)
    uint8_t vout2_ok;   // 1=BUCK-BOOST PG
    uint8_t vout1_ok;   // 1=BUCK PG
} ADP5360_pgood_t;

HAL_StatusTypeDef ADP5360_get_pgood(ADP5360_pgood_t *pg, uint8_t *raw);


// ---------- 0x30: PGOOD1_MASK ----------
#define ADP5360_REG_PGOOD1_MASK     0x30

#define ADP5360_PG1_REV_MASK        0x80u  // [7] 1=PGOOD1 pin is active-low enabled
// [6:5] reserved
#define ADP5360_CHGCMPLT_MASK1      0x10u  // [4] 1=route CHG_CMPLT to PGOOD1
#define ADP5360_VBUSOK_MASK1        0x08u  // [3] 1=route VBUSOK to PGOOD1
#define ADP5360_BATOK_MASK1         0x04u  // [2] 1=route BATOK to PGOOD1
#define ADP5360_VOUT2OK_MASK1       0x02u  // [1] 1=route BUCKBST PG to PGOOD1
#define ADP5360_VOUT1OK_MASK1       0x01u  // [0] 1=route BUCK PG to PGOOD1

typedef struct {
    uint8_t active_low;   // PG1_REV: 1=enable active-low output on PGOOD1
    uint8_t chg_cmplt;    // route charger-complete
    uint8_t vbus_ok;      // route VBUSOK
    uint8_t bat_ok;       // route BATOK
    uint8_t vout2_ok;     // route BUCK-BOOST PG
    uint8_t vout1_ok;     // route BUCK PG
} ADP5360_pgood1_mask_t;

HAL_StatusTypeDef ADP5360_get_pgood1_mask(ADP5360_pgood1_mask_t *m, uint8_t *raw);
HAL_StatusTypeDef ADP5360_set_pgood1_mask(const ADP5360_pgood1_mask_t *m);


// ---------- 0x32: INTERRUPT_ENABLE1 ----------
#define ADP5360_REG_INT_ENABLE1     0x32
#define ADP5360_EN_SOCLOW_INT       0x80u  // [7]
#define ADP5360_EN_SOCACM_INT       0x40u  // [6]
#define ADP5360_EN_ADPICHG_INT      0x20u  // [5]
#define ADP5360_EN_BATPRO_INT       0x10u  // [4]
#define ADP5360_EN_THR_INT          0x08u  // [3]
#define ADP5360_EN_BAT_INT          0x04u  // [2]
#define ADP5360_EN_CHG_INT          0x02u  // [1]
#define ADP5360_EN_VBUS_INT         0x01u  // [0]

// ---------- 0x33: INTERRUPT_ENABLE2 ----------
#define ADP5360_REG_INT_ENABLE2     0x33
#define ADP5360_EN_MR_INT           0x80u  // [7]
#define ADP5360_EN_WD_INT           0x40u  // [6]
#define ADP5360_EN_BUCKPG_INT       0x20u  // [5] (VOUT1OK change)
#define ADP5360_EN_BUCKBSTPG_INT    0x10u  // [4] (VOUT2OK change)
// [3:0] reserved

typedef struct {
    // 0x32
    uint8_t soc_low;    // SOC low threshold
    uint8_t soc_acm;    // SOC accumulator event
    uint8_t adpichg;    // adaptive input current limit
    uint8_t batpro;     // battery protection events
    uint8_t thr;        // thermistor threshold
    uint8_t bat;        // battery voltage threshold
    uint8_t chg;        // charger mode change
    uint8_t vbus;       // VBUS threshold window
    // 0x33
    uint8_t mr;         // MR press
    uint8_t wd;         // watchdog alarm
    uint8_t buck_pg;    // VOUT1OK change
    uint8_t buckbst_pg; // VOUT2OK change
} ADP5360_irq_enable_t;

HAL_StatusTypeDef ADP5360_get_irq_enable(ADP5360_irq_enable_t *en, uint8_t *raw1, uint8_t *raw2);
HAL_StatusTypeDef ADP5360_set_irq_enable(const ADP5360_irq_enable_t *en);


// ---------- 0x34: INTERRUPT_FLAG1 (read-to-clear) ----------
#define ADP5360_REG_INT_FLAG1      0x34
#define ADP5360_IF_SOCLOW_INT      0x80u
#define ADP5360_IF_SOCACM_INT      0x40u
#define ADP5360_IF_ADPICHG_INT     0x20u
#define ADP5360_IF_BATPRO_INT      0x10u
#define ADP5360_IF_THR_INT         0x08u
#define ADP5360_IF_BAT_INT         0x04u
#define ADP5360_IF_CHG_INT         0x02u
#define ADP5360_IF_VBUS_INT        0x01u

// ---------- 0x35: INTERRUPT_FLAG2 (read-to-clear) ----------
#define ADP5360_REG_INT_FLAG2      0x35
#define ADP5360_IF_MR_INT          0x80u
#define ADP5360_IF_WD_INT          0x40u
#define ADP5360_IF_BUCKPG_INT      0x20u
#define ADP5360_IF_BUCKBSTPG_INT   0x10u
// [3:0] reserved

// Read both flag bytes (reading clears them). Any pointer can be NULL.
HAL_StatusTypeDef ADP5360_read_irq_flags(uint8_t *flag1, uint8_t *flag2);


// ---------- 0x36: SHIPMODE ----------
#define ADP5360_REG_SHIPMODE     0x36
#define ADP5360_EN_SHIPMODE_MASK 0x01u  // [0] 1=enter shipment mode

// Read the raw shipmode bit (note: if already in ship mode, I2C may be unavailable)
HAL_StatusTypeDef ADP5360_get_shipmode(uint8_t *enabled);

// Enter/exit ship mode.
// WARNING: Setting enable=1 will typically shut down rails and the device may not
// respond over I2C until a wake event (e.g., MR long-press or VBUS insert).
HAL_StatusTypeDef ADP5360_set_shipmode(uint8_t enable);

// ---------- Top-level init config ----------
typedef struct {
    // --- CHARGER block (0x02..0x07) ---
    struct { // 0x02
        uint16_t vadpichg_mV;    // 4400..4900 step 100 (use your helper's rounding)
        uint8_t  vsys_5V;        // 0: VTRM+200mV, 1: 5.0V
        uint16_t ilim_mA;        // 50..500 (table)
    } vbus_ilim;
    struct { // 0x03
        uint16_t vtrm_mV;        // 3560..4660 (table)
        uint16_t itrk_dead_mA;   // {1,2.5,5,10}
    } term;
    struct { // 0x04
        uint16_t iend_mA;        // {5,7.5,12.5,...,32.5}
        uint16_t ichg_mA;        // {10,20,30,...,320}
    } curr;
    struct { // 0x05
        uint8_t  dis_rch;        // 0/1
        uint16_t vrch_mV;        // {120,180,240}
        uint16_t vtrk_dead_mV;   // {2.0,2.5,2.9} V
        uint16_t vweak_mV;       // {2.7..3.4} V (step 100mV)
    } vth;
    struct { // 0x06
        uint8_t en_tend;         // 0/1
        uint8_t en_chg_timer;    // 0/1
        uint8_t period_sel;      // 0..3  -> tmx={15,30,45,60}min, tcc={150,300,450,600}min
    } tmr;
    struct { // 0x07
        uint8_t en_jeita;        // 0/1
        uint8_t ilim_jeita_cool_10pct; // 0=50%, 1=10%
        uint8_t off_isofet;      // 0/1
        uint8_t en_ldo;          // 0/1
        uint8_t en_eoc;          // 0/1
        uint8_t en_adpichg;      // 0/1
        uint8_t en_chg;          // 0/1
    } func;

    // --- THERMISTOR + thresholds (0x0A, 0x0B..0x0E) ---
    struct { // 0x0A
        uint8_t ithr_uA;         // {60,12,6} -> use nearest
        uint8_t en_thr;          // 0/1
    } thr_ctrl;
    struct { // 0x0B..0x0E (mV)
        uint16_t t60_mV, t45_mV, t10_mV, t0_mV;
    } thr_limits;

    // --- BATTERY PROTECTION (0x11..0x15) ---
    struct { // 0x11
        uint8_t en_batpro;       // 0/1
        uint8_t en_chglb;        // 0/1
        uint8_t chg_hiccup;      // 0/1
        uint8_t dis_hiccup;      // 0/1
        uint8_t isofet_ovchg;    // 0/1
    } batpro_ctrl;
    struct { uint16_t vth_mV; uint8_t hys_pct; uint16_t dgt_ms; } uv_prot;     // 0x12
    struct { uint16_t oc_mA;  uint16_t dgt_ms; } dis_oc;                        // 0x13
    struct { uint16_t vth_mV; uint8_t hys_pct; uint16_t dgt_ms; } ov_prot;      // 0x14
    struct { uint16_t oc_mA;  uint16_t dgt_ms; } chg_oc;                        // 0x15

    // --- FUEL GAUGE (0x20, 0x22, 0x27) ---
    uint16_t bat_cap_mAh;         // 0x20 (0..510)
    ADP5360_socacm_ctl_t socacm;  // 0x22
    ADP5360_fg_mode_t    fg_mode; // 0x27

    // --- BUCK (0x29, 0x2A) ---
    ADP5360_buck_cfg_t   buck_cfg;
    struct { uint16_t vout_mV; uint16_t dly_us; } buck_vout;

    // --- BUCK-BOOST (0x2B, 0x2C) ---
    ADP5360_buckbst_cfg_t buckbst_cfg;
    struct { uint16_t vout_mV; uint16_t dly_us; } buckbst_vout;

    // --- SUPERVISORY/PGOOD/IRQ (0x2D, 0x30, 0x32..0x33) ---
    ADP5360_supervisory_t   supv;
    ADP5360_pgood1_mask_t   pg1;
    ADP5360_irq_enable_t    irq_en;
} ADP5360_init_t;

// Apply full configuration (programs all blocks in a safe order)
HAL_StatusTypeDef ADP5360_power_init(const ADP5360_init_t *cfg);


// One place to edit everything:
static const ADP5360_init_t ADP_cfg = {
    // CHARGER
    .vbus_ilim = { .vadpichg_mV=4600, .vsys_5V=0, .ilim_mA=100 },
    .term      = { .vtrm_mV=4160, .itrk_dead_mA=5 },
    .curr      = { .iend_mA=10.5, .ichg_mA=25 },
    .vth       = { .dis_rch=0, .vrch_mV=120, .vtrk_dead_mV=2500, .vweak_mV=3000 },
    .tmr       = { .en_tend=0, .en_chg_timer=1, .period_sel=3 },  // 60/600 min
    .func      = { .en_jeita=0, .ilim_jeita_cool_10pct=0, .off_isofet=0,
                   .en_ldo=1, .en_eoc=1, .en_adpichg=0, .en_chg=0 },

    // THERMISTOR
    .thr_ctrl  = { .ithr_uA=6, .en_thr=1 },
    .thr_limits= { .t60_mV=172, .t45_mV=286, .t10_mV=1130, .t0_mV=1800 },

    // BATTERY PROTECTION
    .batpro_ctrl = { .en_batpro=1, .en_chglb=0, .chg_hiccup=1, .dis_hiccup=1, .isofet_ovchg=1 },
    .uv_prot     = { .vth_mV=2800, .hys_pct=2, .dgt_ms=30 },
    .dis_oc      = { .oc_mA=600, .dgt_ms=5 },
    .ov_prot     = { .vth_mV=4300, .hys_pct=2, .dgt_ms=500 },
    .chg_oc      = { .oc_mA=400, .dgt_ms=10 },

    // FUEL GAUGE
    .bat_cap_mAh = 120,
    .socacm      = { .age_reduction_pct=1.5f, .temp_coeff_pct_per_C=0.2f, .en_temp_comp=0, .en_age_comp=0 },
    .fg_mode     = { .soc_low_th_pct=11, .slp_curr_mA=10, .slp_time_min=1, .fg_mode_sleep=0, .en_fg=1 },

    // BUCK
    .buck_cfg    = { .softstart_ms=1, .ilim_mA=400, .fpwm_mode=0, .stop_enable=0, .discharge_en=0, .enable=1 },
    .buck_vout   = { .vout_mV=1800, .dly_us=0 },

    // BUCK-BOOST
    .buckbst_cfg = { .softstart_ms=8, .ilim_mA=600, .stop_enable=0, .discharge_en=0, .enable=1 },
    .buckbst_vout= { .vout_mV=3300, .dly_us=0 },

    // SUPERVISORY / PGOOD / IRQ
    .supv = { .buck_rst_en=1, .buckbst_rst_en=0, .reset_time_ms=200, .wd_time_s=12.5f, .wd_enable=0, .mr_shipment_en=1 },
    .pg1  = { .active_low=0, .chg_cmplt=0, .vbus_ok=0, .bat_ok=0, .vout2_ok=0, .vout1_ok=0 },
    .irq_en = {0},  // leave all disabled
};







#ifdef __cplusplus
}
#endif
#endif // ADP5360_H
