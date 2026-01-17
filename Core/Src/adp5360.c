#include "ADP5360.h"

static inline HAL_StatusTypeDef _mem_read(uint8_t reg, uint8_t *buf, uint16_t n) {
    return HAL_I2C_Mem_Read(&hi2c3, ADP5360_I2C_ADDR_W,
                            reg, I2C_MEMADD_SIZE_8BIT,
                            buf, n, ADP5360_I2C_TIMEOUT_MS);
}

static inline HAL_StatusTypeDef _mem_write(uint8_t reg, const uint8_t *buf, uint16_t n) {
    return HAL_I2C_Mem_Write(&hi2c3, ADP5360_I2C_ADDR_W,
                             reg, I2C_MEMADD_SIZE_8BIT,
                             (uint8_t*)buf, n, ADP5360_I2C_TIMEOUT_MS);
}

HAL_StatusTypeDef ADP5360_read(uint8_t reg, uint8_t *buf, uint16_t n) {
    if (!buf || !n) return HAL_ERROR;
    return _mem_read(reg, buf, n);
}

HAL_StatusTypeDef ADP5360_write(uint8_t reg, const uint8_t *buf, uint16_t n) {
    if (!buf || !n) return HAL_ERROR;
    return _mem_write(reg, buf, n);
}

HAL_StatusTypeDef ADP5360_init(void) {
    // 1) Ping device
    HAL_StatusTypeDef st = HAL_I2C_IsDeviceReady(&hi2c3, ADP5360_I2C_ADDR_W, 2, ADP5360_I2C_TIMEOUT_MS);
    if (st != HAL_OK) return st;

    // 2) Lightweight sanity: read IDs (0x00, 0x01). No writes/config here.
    uint8_t id = 0, rev = 0;
    st = ADP5360_read_u8(ADP5360_REG_MANUF_MODEL_ID, &id);
    if (st != HAL_OK) return st;

    st = ADP5360_read_u8(ADP5360_REG_SILICON_REV, &rev);

    ADP5360_power_init(&ADP_cfg); // Initialize ADP5360 with configuration from main.c

    return st; // OK if both reads succeeded
}

HAL_StatusTypeDef ADP5360_get_id(ADP5360_id_t *out)
{
    if (!out) return HAL_ERROR;
    uint8_t v = 0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_MANUF_MODEL_ID, &v);
    if (st != HAL_OK) return st;

    out->manuf = (uint8_t)((v & ADP5360_ID_MANUF_MASK) >> ADP5360_ID_MANUF_SHIFT);
    out->model = (uint8_t)(v & ADP5360_ID_MODEL_MASK);
    return HAL_OK;
}

HAL_StatusTypeDef ADP5360_get_revision(uint8_t *rev4)
{
    if (!rev4) return HAL_ERROR;
    uint8_t v = 0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_SILICON_REV, &v);
    if (st != HAL_OK) return st;

    *rev4 = (uint8_t)(v & ADP5360_REV_MASK);  // 4-bit value
    return HAL_OK;
}


static uint8_t _encode_vadpichg_mv(uint16_t mv, uint8_t *ok)
{
    // Valid codes per DS: 010=4.4V, 011=4.5V, 100=4.6V, 101=4.7V, 110=4.8V, 111=4.9V
    // We accept 4400..4900 in 100 mV steps.
    *ok = 1;
    switch (mv) {
        case 4400: return 0x2u;
        case 4500: return 0x3u;
        case 4600: return 0x4u;
        case 4700: return 0x5u;
        case 4800: return 0x6u;
        case 4900: return 0x7u;
        default: *ok = 0; return 0;
    }
}

static uint16_t _decode_vadpichg_mv(uint8_t code)
{
    switch (code & 0x7u) {
        case 0x2u: return 4400;
        case 0x3u: return 4500;
        case 0x4u: return 4600;
        case 0x5u: return 4700;
        case 0x6u: return 4800;
        case 0x7u: return 4900;
        default:    return 0;  // 000/001 are not listed in DS table
    }
}

static uint8_t _encode_ilim_ma(uint16_t ma, uint8_t *ok)
{
    *ok = 1;
    switch (ma) {
        case  50: return 0x0u;
        case 100: return 0x1u;
        case 150: return 0x2u;
        case 200: return 0x3u;
        case 250: return 0x4u;
        case 300: return 0x5u;
        case 400: return 0x6u;
        case 500: return 0x7u;
        default: *ok = 0; return 0;
    }
}

static uint16_t _decode_ilim_ma(uint8_t code)
{
    switch (code & 0x7u) {
        case 0x0u: return  50;
        case 0x1u: return 100;
        case 0x2u: return 150;
        case 0x3u: return 200;
        case 0x4u: return 250;
        case 0x5u: return 300;
        case 0x6u: return 400;
        case 0x7u: return 500;
        default:    return 0;
    }
}

HAL_StatusTypeDef ADP5360_get_vbus_ilim(uint16_t *vadpichg_mV,
                                        ADP5360_vsys_t *vsys_mode,
                                        uint16_t *ilim_mA)
{
    uint8_t v = 0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_CHARGER_VBUS_ILIM, &v);
    if (st != HAL_OK) return st;

    if (vadpichg_mV) {
        uint8_t code = (uint8_t)((v & ADP5360_VADPICHG_MASK) >> ADP5360_VADPICHG_SHIFT);
        *vadpichg_mV = _decode_vadpichg_mv(code);
    }
    if (vsys_mode)  *vsys_mode = (v & ADP5360_VSYSTEM_MASK) ? ADP5360_VSYS_5V : ADP5360_VSYS_VTRM_P200mV;
    if (ilim_mA)    *ilim_mA = _decode_ilim_ma(v & ADP5360_ILIM_MASK);

    return HAL_OK;
}

HAL_StatusTypeDef ADP5360_set_vbus_ilim(uint16_t vadpichg_mV,
                                        ADP5360_vsys_t vsys_mode,
                                        uint16_t ilim_mA)
{
    uint8_t ok_v = 0, ok_i = 0;
    uint8_t v_code = _encode_vadpichg_mv(vadpichg_mV, &ok_v);
    uint8_t i_code = _encode_ilim_ma(ilim_mA, &ok_i);
    if (!ok_v || !ok_i) return HAL_ERROR;

    uint8_t v;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_CHARGER_VBUS_ILIM, &v);
    if (st != HAL_OK) return st;

    v &= ~(ADP5360_VADPICHG_MASK | ADP5360_VSYSTEM_MASK | ADP5360_ILIM_MASK);
    v |= (uint8_t)(v_code << ADP5360_VADPICHG_SHIFT);
    if (vsys_mode == ADP5360_VSYS_5V) v |= ADP5360_VSYSTEM_MASK;
    v |= (uint8_t)(i_code & ADP5360_ILIM_MASK);

    return ADP5360_write_u8(ADP5360_REG_CHARGER_VBUS_ILIM, v);
}


// ----- helpers: VTRM encode/decode -----
static inline uint8_t _encode_vtrm_code_from_mv(uint16_t mv) {
    // clamp to [3560..4660], 20 mV steps
    if (mv < 3560) mv = 3560;
    if (mv > 4660) mv = 4660;
    // code 0 -> 3560 mV, code 55 -> 4660 mV
    uint16_t steps = (uint16_t)((mv - 3560u) / 20u);
    if (steps > 55u) steps = 55u;
    return (uint8_t)steps;
}
static inline uint16_t _decode_vtrm_mv_from_code(uint8_t code6) {
    if (code6 > 55u) code6 = 55u;          // top codes saturate to 4.66 V
    return (uint16_t)(3560u + (uint16_t)code6 * 20u);
}

// ----- helpers: ITRK encode/decode (deci-mA) -----
static inline uint8_t _encode_itrk_dma(uint16_t deci_mA) {
    // map 10,25,50,100 -> 00,01,10,11; clamp to nearest
    uint16_t cand[4] = {10, 25, 50, 100};
    uint8_t best = 0; uint16_t best_err = 0xFFFF;
    for (uint8_t i = 0; i < 4; ++i) {
        uint16_t err = (deci_mA > cand[i]) ? (deci_mA - cand[i]) : (cand[i] - deci_mA);
        if (err < best_err) { best_err = err; best = i; }
    }
    return best; // 0..3
}
static inline uint16_t _decode_itrk_dma(uint8_t code2) {
    switch (code2 & 0x03u) {
        default:
        case 0: return 10;   // 1.0 mA
        case 1: return 25;   // 2.5 mA
        case 2: return 50;   // 5.0 mA
        case 3: return 100;  // 10.0 mA
    }
}

HAL_StatusTypeDef ADP5360_get_chg_term(uint16_t *vtrm_mV,
                                       uint16_t *itrk_deci_mA)
{
    uint8_t v = 0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_CHARGER_TERMINATION_SETTING, &v);
    if (st != HAL_OK) return st;

    if (vtrm_mV) {
        uint8_t code = (uint8_t)((v & ADP5360_VTRM_MASK) >> ADP5360_VTRM_SHIFT);
        *vtrm_mV = _decode_vtrm_mv_from_code(code);
    }
    if (itrk_deci_mA) {
        *itrk_deci_mA = _decode_itrk_dma(v & ADP5360_ITRK_MASK);
    }
    return HAL_OK;
}

HAL_StatusTypeDef ADP5360_set_chg_term(uint16_t vtrm_mV,
                                       uint16_t itrk_deci_mA)
{
    uint8_t regv;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_CHARGER_TERMINATION_SETTING, &regv);
    if (st != HAL_OK) return st;

    uint8_t vtrm_code = _encode_vtrm_code_from_mv(vtrm_mV);
    uint8_t itrk_code = _encode_itrk_dma(itrk_deci_mA);

    regv &= ~(ADP5360_VTRM_MASK | ADP5360_ITRK_MASK);
    regv |= (uint8_t)(vtrm_code << ADP5360_VTRM_SHIFT);
    regv |= (uint8_t)(itrk_code & ADP5360_ITRK_MASK);

    return ADP5360_write_u8(ADP5360_REG_CHARGER_TERMINATION_SETTING, regv);
}



// --- IEND encode/decode ---
static uint16_t _decode_iend_ma(uint8_t code3) {
    switch (code3 & 0x07u) {
        case 0x1u: return 5;
        case 0x2u: return 7;
        case 0x3u: return 12;
        case 0x4u: return 17;
        case 0x5u: return 22;
        case 0x6u: return 27;
        case 0x7u: return 32;
        default:    return 0; // 000 reserved
    }
}
static uint8_t _encode_iend_code(uint16_t ma) {
    // map nearest
    const uint16_t vals[7] = {5,7,12,17,22,27,32};
    const uint8_t codes[7] = {0x1u,0x2u,0x3u,0x4u,0x5u,0x6u,0x7u};
    uint8_t best = codes[0]; uint16_t best_err = 0xFFFF;
    for (int i=0;i<7;i++){
        uint16_t err = (ma>vals[i])?(ma-vals[i]):(vals[i]-ma);
        if (err<best_err){ best_err=err; best=codes[i]; }
    }
    return best;
}

// --- ICHG encode/decode ---
static uint16_t _decode_ichg_ma(uint8_t code5) {
    // 00000=10mA, then +10mA each step up to 11111=320mA
    if (code5 > 31) code5 = 31;
    return (uint16_t)(10 + code5*10);
}
static uint8_t _encode_ichg_code(uint16_t ma) {
    if (ma < 10) ma = 10;
    if (ma > 320) ma = 320;
    return (uint8_t)((ma - 10)/10);
}

HAL_StatusTypeDef ADP5360_get_chg_current(uint16_t *iend_mA,
                                          uint16_t *ichg_mA)
{
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_CHARGER_CURRENT_SETTING, &v);
    if (st != HAL_OK) return st;

    if (iend_mA) *iend_mA = _decode_iend_ma((v & ADP5360_IEND_MASK)>>ADP5360_IEND_SHIFT);
    if (ichg_mA) *ichg_mA = _decode_ichg_ma(v & ADP5360_ICHG_MASK);

    return HAL_OK;
}

HAL_StatusTypeDef ADP5360_set_chg_current(uint16_t iend_mA,
                                          uint16_t ichg_mA)
{
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_CHARGER_CURRENT_SETTING, &v);
    if (st != HAL_OK) return st;

    uint8_t iend_code = _encode_iend_code(iend_mA);
    uint8_t ichg_code = _encode_ichg_code(ichg_mA);

    v &= ~(ADP5360_IEND_MASK | ADP5360_ICHG_MASK);
    v |= (uint8_t)(iend_code << ADP5360_IEND_SHIFT);
    v |= (ichg_code & ADP5360_ICHG_MASK);

    return ADP5360_write_u8(ADP5360_REG_CHARGER_CURRENT_SETTING, v);
}

// use deci-mA for IEND exactness
static uint16_t _decode_iend_dmA(uint8_t code3) {
    switch (code3 & 0x07u) {
        case 0x1u: return  50;  // 5.0 mA
        case 0x2u: return  75;  // 7.5 mA
        case 0x3u: return 125;  // 12.5 mA
        case 0x4u: return 175;  // 17.5 mA
        case 0x5u: return 225;  // 22.5 mA
        case 0x6u: return 275;  // 27.5 mA
        case 0x7u: return 325;  // 32.5 mA
        default:    return   0;  // 000 reserved
    }
}
static uint8_t _encode_iend_code_dmA(uint16_t dmA) {
    const uint16_t vals[7] = { 50, 75,125,175,225,275,325 };
    const uint8_t  codes[7]= {0x1u,0x2u,0x3u,0x4u,0x5u,0x6u,0x7u};
    uint8_t best = codes[0]; uint16_t best_err = 0xFFFF;
    for (int i=0;i<7;i++){
        uint16_t err = (dmA>vals[i])?(dmA-vals[i]):(vals[i]-dmA);
        if (err<best_err){ best_err=err; best=codes[i]; }
    }
    return best;
}
static uint16_t _decode_ichg_dmA(uint8_t code5) {
    if (code5 > 31) code5 = 31;
    return (uint16_t)((10 + code5*10) * 10); // convert mA->dmA
}
static uint8_t _encode_ichg_code_dmA(uint16_t dmA) {
    uint16_t ma = (dmA + 5) / 10;           // round to nearest mA
    if (ma < 10) {
        ma = 10;
    }
    if (ma > 320) {
        ma = 320;
    }
    return (uint8_t)((ma - 10)/10);
}
HAL_StatusTypeDef ADP5360_get_chg_current_dmA(uint16_t *iend_dmA,
                                              uint16_t *ichg_dmA)
{
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_CHARGER_CURRENT_SETTING, &v);
    if (st != HAL_OK) return st;
    if (iend_dmA) *iend_dmA = _decode_iend_dmA((v & ADP5360_IEND_MASK)>>ADP5360_IEND_SHIFT);
    if (ichg_dmA) *ichg_dmA = _decode_ichg_dmA(v & ADP5360_ICHG_MASK);
    return HAL_OK;
}
HAL_StatusTypeDef ADP5360_set_chg_current_dmA(uint16_t iend_dmA,
                                              uint16_t ichg_dmA)
{
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_CHARGER_CURRENT_SETTING, &v);
    if (st != HAL_OK) return st;

    uint8_t iend_code = _encode_iend_code_dmA(iend_dmA);
    uint8_t ichg_code = _encode_ichg_code_dmA(ichg_dmA);

    v &= ~(ADP5360_IEND_MASK | ADP5360_ICHG_MASK);
    v |= (uint8_t)(iend_code << ADP5360_IEND_SHIFT);
    v |= (ichg_code & ADP5360_ICHG_MASK);

    return ADP5360_write_u8(ADP5360_REG_CHARGER_CURRENT_SETTING, v);
}

// ---- encode/decode helpers (0x05) ----
static int _vrch_code_from_mv(uint16_t mv) {
    switch (mv) {
        case 120: return 0x1;
        case 180: return 0x2;
        case 240: return 0x3;
        default:  return -1; // invalid per table
    }
}
static uint16_t _vrch_mv_from_code(uint8_t code) {
    switch (code & 0x03u) {
        case 0x1u: return 120;
        case 0x2u: return 180;
        case 0x3u: return 240;
        default:   return 0;   // 00 not defined in table
    }
}
static int _vtrk_code_from_mv(uint16_t mv) {
    switch (mv) {
        case 2000: return 0x0;
        case 2500: return 0x1;
        case 2600: return 0x2;
        case 2900: return 0x3;
        default:   return -1;
    }
}
static uint16_t _vtrk_mv_from_code(uint8_t code) {
    switch (code & 0x03u) {
        case 0x0u: return 2000;
        case 0x1u: return 2500;
        case 0x2u: return 2600;
        case 0x3u: return 2900;
        default:   return 0;
    }
}
static int _vweak_code_from_mv(uint16_t mv) {
    if (mv < 2700 || mv > 3400) return -1;
    if ((mv % 100) != 0) return -1;
    return (mv - 2700) / 100; // 0..7
}
static uint16_t _vweak_mv_from_code(uint8_t code3) {
    if (code3 > 7) code3 = 7;
    return (uint16_t)(2700 + 100*code3);
}

HAL_StatusTypeDef ADP5360_get_voltage_thresholds(
    uint8_t  *recharge_disabled,
    uint16_t *vrch_mV,
    uint16_t *vtrk_dead_mV,
    uint16_t *vweak_mV)
{
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_CHARGER_VOLTAGE_THRESHOLD, &v);
    if (st != HAL_OK) return st;

    if (recharge_disabled) *recharge_disabled = (v & ADP5360_DIS_RCH_MASK) ? 1u : 0u;

    if (vrch_mV) {
        uint8_t code = (uint8_t)((v & ADP5360_VRCH_MASK) >> ADP5360_VRCH_SHIFT);
        *vrch_mV = _vrch_mv_from_code(code);
    }
    if (vtrk_dead_mV) {
        uint8_t code = (uint8_t)((v & ADP5360_VTRK_MASK) >> ADP5360_VTRK_SHIFT);
        *vtrk_dead_mV = _vtrk_mv_from_code(code);
    }
    if (vweak_mV) {
        *vweak_mV = _vweak_mv_from_code(v & ADP5360_VWEAK_MASK);
    }
    return HAL_OK;
}

HAL_StatusTypeDef ADP5360_set_voltage_thresholds(
    uint8_t  recharge_disabled,
    uint16_t vrch_mV,
    uint16_t vtrk_dead_mV,
    uint16_t vweak_mV)
{
    int vrch_code = _vrch_code_from_mv(vrch_mV);
    int vtrk_code = _vtrk_code_from_mv(vtrk_dead_mV);
    int vweak_code= _vweak_code_from_mv(vweak_mV);
    if (vrch_code < 0 || vtrk_code < 0 || vweak_code < 0) return HAL_ERROR;

    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_CHARGER_VOLTAGE_THRESHOLD, &v);
    if (st != HAL_OK) return st;

    v &= ~(ADP5360_DIS_RCH_MASK | ADP5360_VRCH_MASK | ADP5360_VTRK_MASK | ADP5360_VWEAK_MASK);
    if (recharge_disabled) v |= ADP5360_DIS_RCH_MASK;
    v |= (uint8_t)( (vrch_code & 0x03) << ADP5360_VRCH_SHIFT );
    v |= (uint8_t)( (vtrk_code & 0x03) << ADP5360_VTRK_SHIFT );
    v |= (uint8_t)( vweak_code & 0x07 );

    return ADP5360_write_u8(ADP5360_REG_CHARGER_VOLTAGE_THRESHOLD, v);
}


static void _tmr_minutes_from_code(uint8_t code, uint16_t *tmx, uint16_t *tcc)
{
    switch (code & 0x03u) {
        case 0:
            if (tmx) *tmx = 15;
            if (tcc) *tcc = 150;
            break;
        case 1:
            if (tmx) *tmx = 30;
            if (tcc) *tcc = 300;
            break;
        case 2:
            if (tmx) *tmx = 45;
            if (tcc) *tcc = 450;
            break;
        default: // 3
            if (tmx) *tmx = 60;
            if (tcc) *tcc = 600;
            break;
    }
}

HAL_StatusTypeDef ADP5360_get_chg_timers(
    uint8_t *en_tend,
    uint8_t *en_chg_timer,
    ADP5360_tmr_period_t *period,
    uint16_t *tmx_min,
    uint16_t *tcc_min)
{
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_CHARGER_TIMER_SETTING, &v);
    if (st != HAL_OK) return st;

    if (en_tend)       *en_tend       = (v & ADP5360_EN_TEND_MASK) ? 1u : 0u;
    if (en_chg_timer)  *en_chg_timer  = (v & ADP5360_EN_CHG_TIMER_MASK) ? 1u : 0u;
    uint8_t code = (uint8_t)(v & ADP5360_CHG_TMR_MASK);
    if (period)        *period        = (ADP5360_tmr_period_t)code;
    _tmr_minutes_from_code(code, tmx_min, tcc_min);
    return HAL_OK;
}

HAL_StatusTypeDef ADP5360_set_chg_timers(
    uint8_t en_tend,
    uint8_t en_chg_timer,
    ADP5360_tmr_period_t period)
{
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_CHARGER_TIMER_SETTING, &v);
    if (st != HAL_OK) return st;

    v &= ~(ADP5360_EN_TEND_MASK | ADP5360_EN_CHG_TIMER_MASK | ADP5360_CHG_TMR_MASK);
    if (en_tend)      v |= ADP5360_EN_TEND_MASK;
    if (en_chg_timer) v |= ADP5360_EN_CHG_TIMER_MASK;
    v |= (uint8_t)period & ADP5360_CHG_TMR_MASK;

    return ADP5360_write_u8(ADP5360_REG_CHARGER_TIMER_SETTING, v);
}


HAL_StatusTypeDef ADP5360_get_chg_function(ADP5360_func_t *f)
{
    if (!f) return HAL_ERROR;
    uint8_t v = 0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_CHARGER_FUNCTION_SETTING, &v);
    if (st != HAL_OK) return st;

    f->en_jeita        = (v & ADP5360_EN_JEITA_MASK)        ? 1u : 0u;
    f->ilim_jeita_cool = (v & ADP5360_ILIM_JEITA_COOL_MASK) ? 1u : 0u;
    f->off_isofet      = (v & ADP5360_OFF_ISOFET_MASK)      ? 1u : 0u;
    f->en_ldo          = (v & ADP5360_EN_LDO_MASK)          ? 1u : 0u;
    f->en_eoc          = (v & ADP5360_EN_EOC_MASK)          ? 1u : 0u;
    f->en_adpichg      = (v & ADP5360_EN_ADPICHG_MASK)      ? 1u : 0u;
    f->en_chg          = (v & ADP5360_EN_CHG_MASK)          ? 1u : 0u;
    return HAL_OK;
}

HAL_StatusTypeDef ADP5360_set_chg_function(const ADP5360_func_t *f)
{
    if (!f) return HAL_ERROR;
    uint8_t v = 0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_CHARGER_FUNCTION_SETTING, &v);
    if (st != HAL_OK) return st;

    // clear writable bits (skip reserved bit5)
    v &= ~(ADP5360_EN_JEITA_MASK | ADP5360_ILIM_JEITA_COOL_MASK |
           ADP5360_OFF_ISOFET_MASK | ADP5360_EN_LDO_MASK | ADP5360_EN_EOC_MASK |
           ADP5360_EN_ADPICHG_MASK | ADP5360_EN_CHG_MASK);

    if (f->en_jeita)        v |= ADP5360_EN_JEITA_MASK;
    if (f->ilim_jeita_cool) v |= ADP5360_ILIM_JEITA_COOL_MASK;
    if (f->off_isofet)      v |= ADP5360_OFF_ISOFET_MASK;
    if (f->en_ldo)          v |= ADP5360_EN_LDO_MASK;
    if (f->en_eoc)          v |= ADP5360_EN_EOC_MASK;
    if (f->en_adpichg)      v |= ADP5360_EN_ADPICHG_MASK;
    if (f->en_chg)          v |= ADP5360_EN_CHG_MASK;

    return ADP5360_write_u8(ADP5360_REG_CHARGER_FUNCTION_SETTING, v);
}


const char* ADP5360_chg_state_str(ADP5360_chg_state_t st)
{
    switch (st) {
        case ADP5360_CHG_OFF:            return "off";
        case ADP5360_CHG_TRICKLE:        return "trickle";
        case ADP5360_CHG_FAST_CC:        return "fast-CC";
        case ADP5360_CHG_FAST_CV:        return "fast-CV";
        case ADP5360_CHG_COMPLETE:       return "charge-complete";
        case ADP5360_CHG_LDO_MODE:       return "LDO-mode";
        case ADP5360_CHG_TIMER_EXPIRED:  return "timer-expired";
        case ADP5360_CHG_BATT_DETECT:    return "battery-detect";
        default:                         return "?";
    }
}

HAL_StatusTypeDef ADP5360_get_status1(ADP5360_status1_t *s)
{
    if (!s) return HAL_ERROR;
    uint8_t v = 0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_CHARGER_STATUS1, &v);
    if (st != HAL_OK) return st;

    s->vbus_ov        = (v & ADP5360_ST1_VBUS_OV_MASK)   ? 1u : 0u;
    s->adpichg_active = (v & ADP5360_ST1_ADPICHG_MASK)   ? 1u : 0u;
    s->vbus_ilim      = (v & ADP5360_ST1_VBUS_ILIM_MASK) ? 1u : 0u;
    s->state          = (ADP5360_chg_state_t)(v & ADP5360_ST1_STATE_MASK);
    return HAL_OK;
}

bool ADP5360_is_charging(void)
{
    ADP5360_status1_t st;
    if (ADP5360_get_status1(&st) != HAL_OK)
        return false; // treat errors as "not charging"

    switch (st.state) {
    case ADP5360_CHG_TRICKLE:
    case ADP5360_CHG_FAST_CC:
    case ADP5360_CHG_FAST_CV:
        return true;
    default:
        return false;
    }
}




const char* ADP5360_thr_status_str(ADP5360_thr_status_t t)
{
    switch (t) {
        case ADP5360_THR_OFF:  return "off";
        case ADP5360_THR_COLD: return "cold";
        case ADP5360_THR_COOL: return "cool";
        case ADP5360_THR_WARM: return "warm";
        case ADP5360_THR_HOT:  return "hot";
        case ADP5360_THR_OK:   return "ok";
        default:               return "unknown";
    }
}

const char* ADP5360_bat_status_str(ADP5360_bat_chg_status_t b)
{
    switch (b) {
        case ADP5360_BATSTAT_NORMAL:   return "normal";
        case ADP5360_BATSTAT_NO_BATT:  return "no-battery";
        case ADP5360_BATSTAT_LE_VTRK:  return "≤VTRK_DEAD (chg)";
        case ADP5360_BATSTAT_BETWEEN:  return "between VTRK_DEAD & VWEAK (chg)";
        case ADP5360_BATSTAT_GE_VWEAK: return "≥VWEAK (chg)";
        default:                       return "unknown";
    }
}

HAL_StatusTypeDef ADP5360_get_status2(ADP5360_status2_t *s)
{
    if (!s) return HAL_ERROR;
    uint8_t v = 0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_CHARGER_STATUS2, &v);
    if (st != HAL_OK) return st;

    s->thr        = (ADP5360_thr_status_t)((v & ADP5360_ST2_THR_MASK) >> 5);
    s->bat_ov     = (v & ADP5360_ST2_BAT_OV_MASK) ? 1u : 0u;
    s->bat_uv     = (v & ADP5360_ST2_BAT_UV_MASK) ? 1u : 0u;
    s->bat_status = (ADP5360_bat_chg_status_t)(v & ADP5360_ST2_BAT_CHG_MASK);
    return HAL_OK;
}


static ADP5360_ithr_t _ithr_from_code(uint8_t code2)
{
    switch (code2 & 0x03u) {
        case 0: return ADP5360_ITHR_60UA;
        case 1: return ADP5360_ITHR_12UA;
        default: return ADP5360_ITHR_6UA; // 10 or 11
    }
}
static uint8_t _ithr_to_code(ADP5360_ithr_t ithr)
{
    switch (ithr) {
        case ADP5360_ITHR_60UA: return 0x0u;
        case ADP5360_ITHR_12UA: return 0x1u;
        case ADP5360_ITHR_6UA:  return 0x2u;  // choose 10 for 6 µA
        default:                return 0x2u;
    }
}

HAL_StatusTypeDef ADP5360_get_ntc_ctrl(ADP5360_ithr_t *ithr, uint8_t *en_thr)
{
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_BATTERY_THERMISTOR_CONTROL, &v);
    if (st != HAL_OK) return st;
    if (ithr)   *ithr   = _ithr_from_code((v & ADP5360_ITHR_MASK) >> ADP5360_ITHR_SHIFT);
    if (en_thr) *en_thr = (v & ADP5360_EN_THR_MASK) ? 1u : 0u;
    return HAL_OK;
}

HAL_StatusTypeDef ADP5360_set_ntc_ctrl(ADP5360_ithr_t ithr, uint8_t en_thr)
{
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_BATTERY_THERMISTOR_CONTROL, &v);
    if (st != HAL_OK) return st;

    v &= ~(ADP5360_ITHR_MASK | ADP5360_EN_THR_MASK);
    v |= (uint8_t)(_ithr_to_code(ithr) << ADP5360_ITHR_SHIFT);
    if (en_thr) v |= ADP5360_EN_THR_MASK;

    return ADP5360_write_u8(ADP5360_REG_BATTERY_THERMISTOR_CONTROL, v);
}


static inline uint8_t clamp_u8(uint32_t x) { return (x > 255u) ? 255u : (uint8_t)x; }

HAL_StatusTypeDef ADP5360_get_ntc_thresholds(
    uint16_t *v60c_mV, uint16_t *v45c_mV, uint16_t *v10c_mV, uint16_t *v0c_mV)
{
    uint8_t b;

    if (v60c_mV) {
        if (ADP5360_read_u8(ADP5360_REG_THERMISTOR_60C_THRESHOLD, &b) != HAL_OK) return HAL_ERROR;
        *v60c_mV = (uint16_t)b * 2u;
    }
    if (v45c_mV) {
        if (ADP5360_read_u8(ADP5360_REG_THERMISTOR_45C_THRESHOLD, &b) != HAL_OK) return HAL_ERROR;
        *v45c_mV = (uint16_t)b * 2u;
    }
    if (v10c_mV) {
        if (ADP5360_read_u8(ADP5360_REG_THERMISTOR_10C_THRESHOLD, &b) != HAL_OK) return HAL_ERROR;
        *v10c_mV = (uint16_t)b * 10u;
    }
    if (v0c_mV) {
        if (ADP5360_read_u8(ADP5360_REG_THERMISTOR_0C_THRESHOLD, &b) != HAL_OK) return HAL_ERROR;
        *v0c_mV = (uint16_t)b * 10u;
    }
    return HAL_OK;
}

HAL_StatusTypeDef ADP5360_set_ntc_thresholds(
    uint16_t v60c_mV, uint16_t v45c_mV, uint16_t v10c_mV, uint16_t v0c_mV)
{
    // Round to nearest LSB and clamp to 0..255 codes.
    uint8_t c60 = clamp_u8((v60c_mV + 1u) / 2u);    // 2 mV LSB
    uint8_t c45 = clamp_u8((v45c_mV + 1u) / 2u);
    uint8_t c10 = clamp_u8((v10c_mV + 5u) / 10u);   // 10 mV LSB
    uint8_t c0  = clamp_u8((v0c_mV  + 5u) / 10u);

    if (ADP5360_write_u8(ADP5360_REG_THERMISTOR_60C_THRESHOLD, c60) != HAL_OK) return HAL_ERROR;
    if (ADP5360_write_u8(ADP5360_REG_THERMISTOR_45C_THRESHOLD, c45) != HAL_OK) return HAL_ERROR;
    if (ADP5360_write_u8(ADP5360_REG_THERMISTOR_10C_THRESHOLD, c10) != HAL_OK) return HAL_ERROR;
    if (ADP5360_write_u8(ADP5360_REG_THERMISTOR_0C_THRESHOLD,  c0)  != HAL_OK) return HAL_ERROR;

    return HAL_OK;
}


HAL_StatusTypeDef ADP5360_get_thr_voltage(uint16_t *thr_mV, uint16_t *raw12)
{
    uint8_t lo = 0, hi = 0;
    HAL_StatusTypeDef st;

    st = ADP5360_read_u8(ADP5360_REG_THR_VOLTAGE_LOW, &lo);
    if (st != HAL_OK) return st;

    st = ADP5360_read_u8(ADP5360_REG_THR_VOLTAGE_HIGH, &hi);
    if (st != HAL_OK) return st;

    uint16_t r = (uint16_t)(((uint16_t)(hi & 0x0Fu) << 8) | lo); // 12-bit
    if (raw12)  *raw12  = r;
    if (thr_mV) *thr_mV = r;  // units are mV per datasheet

    return HAL_OK;
}


HAL_StatusTypeDef ADP5360_get_batpro_ctrl(ADP5360_batpro_ctrl_t *cfg)
{
    if (!cfg) return HAL_ERROR;
    uint8_t v = 0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_BATPRO_CONTROL, &v);
    if (st != HAL_OK) return st;

    cfg->en_batpro     = (v & ADP5360_EN_BATPRO_MASK)     ? 1u : 0u;
    cfg->en_chglb      = (v & ADP5360_EN_CHGLB_MASK)      ? 1u : 0u;
    cfg->oc_chg_hiccup = (v & ADP5360_OC_CHG_HICCUP_MASK) ? 1u : 0u;
    cfg->oc_dis_hiccup = (v & ADP5360_OC_DIS_HICCUP_MASK) ? 1u : 0u;
    cfg->isofet_ovchg  = (v & ADP5360_ISOFET_OVCHG_MASK)  ? 1u : 0u;
    return HAL_OK;
}

HAL_StatusTypeDef ADP5360_set_batpro_ctrl(const ADP5360_batpro_ctrl_t *cfg)
{
    if (!cfg) return HAL_ERROR;

    uint8_t v = 0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_BATPRO_CONTROL, &v);
    if (st != HAL_OK) return st;

    v &= ~(ADP5360_EN_BATPRO_MASK | ADP5360_EN_CHGLB_MASK |
           ADP5360_OC_CHG_HICCUP_MASK | ADP5360_OC_DIS_HICCUP_MASK |
           ADP5360_ISOFET_OVCHG_MASK);

    if (cfg->en_batpro)     v |= ADP5360_EN_BATPRO_MASK;
    if (cfg->en_chglb)      v |= ADP5360_EN_CHGLB_MASK;
    if (cfg->oc_chg_hiccup) v |= ADP5360_OC_CHG_HICCUP_MASK;
    if (cfg->oc_dis_hiccup) v |= ADP5360_OC_DIS_HICCUP_MASK;
    if (cfg->isofet_ovchg)  v |= ADP5360_ISOFET_OVCHG_MASK;

    return ADP5360_write_u8(ADP5360_REG_BATPRO_CONTROL, v);
}


// --- helpers: encode/decode for 0x12 ---
static inline uint8_t _uv_code_from_mv(uint16_t mv) {
    if (mv < 2050) mv = 2050;
    if (mv > 2800) mv = 2800;
    // round to nearest 50 mV
    uint16_t off = (uint16_t)(mv - 2050);
    uint16_t code = (off + 25u) / 50u; // nearest
    if (code > 15u) code = 15u;
    return (uint8_t)code; // 0..15
}
static inline uint16_t _uv_mv_from_code(uint8_t code) {
    if (code > 15) code = 15;
    return (uint16_t)(2050u + 50u * code);
}

static inline uint8_t _hys_code_from_pct(uint8_t pct) {
    // nearest of {2,4,6,8}
    uint8_t opts[4] = {2,4,6,8};
    uint8_t best = 0, best_err = 255;
    for (uint8_t i=0;i<4;i++){
        uint8_t err = (pct>opts[i])?(pct-opts[i]):(opts[i]-pct);
        if (err < best_err){ best_err=err; best=i; }
    }
    return best; // 0..3
}
static inline uint8_t _hys_pct_from_code(uint8_t code2) {
    switch (code2 & 0x03u) { case 0: return 2; case 1: return 4; case 2: return 6; default: return 8; }
}

static inline uint8_t _dgt_code_from_ms(uint16_t ms) {
    // nearest of {30,60,120,240}
    uint16_t opts[4] = {30,60,120,240};
    uint8_t best = 0; uint16_t best_err = 0xFFFF;
    for (uint8_t i=0;i<4;i++){
        uint16_t err = (ms>opts[i])?(ms-opts[i]):(opts[i]-ms);
        if (err < best_err){ best_err=err; best=i; }
    }
    return best; // 0..3
}
static inline uint16_t _dgt_ms_from_code(uint8_t code2) {
    switch (code2 & 0x03u) { case 0: return 30; case 1: return 60; case 2: return 120; default: return 240; }
}

HAL_StatusTypeDef ADP5360_get_uv_setting(uint16_t *uv_mV,
                                         uint8_t  *hys_pct,
                                         uint16_t *dgt_ms)
{
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_BATPRO_UV_SETTING, &v);
    if (st != HAL_OK) return st;

    if (uv_mV)   *uv_mV   = _uv_mv_from_code( (v & ADP5360_UV_DISCH_MASK) >> ADP5360_UV_DISCH_SHIFT );
    if (hys_pct) *hys_pct = _hys_pct_from_code( (v & ADP5360_HYS_UV_DISCH_MASK) >> ADP5360_HYS_UV_DISCH_SHIFT );
    if (dgt_ms)  *dgt_ms  = _dgt_ms_from_code( v & ADP5360_DGT_UV_DISCH_MASK );
    return HAL_OK;
}

HAL_StatusTypeDef ADP5360_set_uv_setting(uint16_t uv_mV,
                                         uint8_t  hys_pct,
                                         uint16_t dgt_ms)
{
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_BATPRO_UV_SETTING, &v);
    if (st != HAL_OK) return st;

    uint8_t uv_code  = _uv_code_from_mv(uv_mV);
    uint8_t hys_code = _hys_code_from_pct(hys_pct);
    uint8_t dgt_code = _dgt_code_from_ms(dgt_ms);

    v &= ~(ADP5360_UV_DISCH_MASK | ADP5360_HYS_UV_DISCH_MASK | ADP5360_DGT_UV_DISCH_MASK);
    v |= (uint8_t)(uv_code  << ADP5360_UV_DISCH_SHIFT);
    v |= (uint8_t)(hys_code << ADP5360_HYS_UV_DISCH_SHIFT);
    v |= (uint8_t)(dgt_code & ADP5360_DGT_UV_DISCH_MASK);

    return ADP5360_write_u8(ADP5360_REG_BATPRO_UV_SETTING, v);
}


// --- helpers: 0x13 DISCH OC threshold ---
static uint8_t _ocdisch_code_from_ma(uint16_t ma) {
    // nearest among the allowed set
    const uint16_t vals[8] = {50,100,150,200,300,400,500,600};
    uint8_t best=0; uint16_t best_err=0xFFFF;
    for (uint8_t i=0;i<8;i++){
        uint16_t err = (ma>vals[i])?(ma-vals[i]):(vals[i]-ma);
        if (err<best_err){best_err=err;best=i;}
    }
    return best; // 0..7
}
static uint16_t _ocdisch_ma_from_code(uint8_t code3) {
    const uint16_t vals[8] = {50,100,150,200,300,400,500,600};
    return vals[(code3 & 0x07u)];
}

// --- helpers: 0x13 DISCH OC deglitch ---
static uint8_t _dgt_dis_code_from_ms(float ms) {
    // nearest of {0.5,1,5,10,20,50,100} -> codes 001..111
    const float opts[7] = {0.5f,1.f,5.f,10.f,20.f,50.f,100.f};
    uint8_t best_idx=0; float best_err=1e9f;
    for (uint8_t i=0;i<7;i++){
        float err = (ms>opts[i])?(ms-opts[i]):(opts[i]-ms);
        if (err < best_err){ best_err = err; best_idx = i; }
    }
    return (uint8_t)(best_idx + 1u); // 1..7
}
static uint16_t _dgt_dis_ms_from_code(uint8_t code3) {
    // 000 not listed -> return 0
    switch (code3 & 0x07u) {
        case 0x1u: return 1;   // 0.5 ms -> we’ll return integer 1 for display granularity
        case 0x2u: return 1;   // 1 ms
        case 0x3u: return 5;
        case 0x4u: return 10;
        case 0x5u: return 20;
        case 0x6u: return 50;
        case 0x7u: return 100;
        default:    return 0;   // 000 (undefined)
    }
}

HAL_StatusTypeDef ADP5360_get_dis_oc(uint16_t *oc_mA, uint16_t *dgt_ms)
{
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_BATPRO_DISCH_OC_SETTING, &v);
    if (st != HAL_OK) return st;

    if (oc_mA)  *oc_mA  = _ocdisch_ma_from_code( (v & ADP5360_OC_DISCH_MASK) >> ADP5360_OC_DISCH_SHIFT );
    if (dgt_ms) *dgt_ms = _dgt_dis_ms_from_code( (v & ADP5360_DGT_OC_DISCH_MASK) >> ADP5360_DGT_OC_DISCH_SHIFT );
    return HAL_OK;
}

HAL_StatusTypeDef ADP5360_set_dis_oc(uint16_t oc_mA, float dgt_ms)
{
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_BATPRO_DISCH_OC_SETTING, &v);
    if (st != HAL_OK) return st;

    uint8_t oc_code  = _ocdisch_code_from_ma(oc_mA);
    uint8_t dgt_code = _dgt_dis_code_from_ms(dgt_ms);

    v &= ~(ADP5360_OC_DISCH_MASK | ADP5360_DGT_OC_DISCH_MASK);
    v |= (uint8_t)(oc_code << ADP5360_OC_DISCH_SHIFT);
    v |= (uint8_t)(dgt_code << ADP5360_DGT_OC_DISCH_SHIFT);

    return ADP5360_write_u8(ADP5360_REG_BATPRO_DISCH_OC_SETTING, v);
}


// ----- helpers: 0x14 OV code <-> mV -----
static inline uint8_t _ov_code_from_mv(uint16_t mv)
{
    if (mv < 3550) mv = 3550;
    if (mv > 4800) mv = 4800;
    // map ~50 mV/LSB from 3.55 V. Round to nearest code.
    uint16_t off = (uint16_t)(mv - 3550);
    uint16_t code = (off + 25u) / 50u;   // nearest
    if (code > 31u) code = 31u;
    return (uint8_t)code;
}
static inline uint16_t _ov_mv_from_code(uint8_t code5)
{
    if (code5 > 31) code5 = 31;
    // Datasheet: codes 11001..11111 clamp to 4.80 V.
    uint16_t mv = (uint16_t)(3550u + 50u * code5);
    if (mv > 4800u) mv = 4800u;
    return mv;
}

// hysteresis {2,4,6,8}%
static inline uint8_t _ov_hys_code_from_pct(uint8_t pct)
{
    uint8_t opts[4] = {2,4,6,8};
    uint8_t best = 0, best_err = 255;
    for (uint8_t i=0;i<4;i++){
        uint8_t e = (pct>opts[i])?(pct-opts[i]):(opts[i]-pct);
        if (e < best_err){ best_err = e; best = i; }
    }
    return best; // 0..3
}
static inline uint8_t _ov_hys_pct_from_code(uint8_t code2)
{
    switch (code2 & 0x03u) { case 0: return 2; case 1: return 4; case 2: return 6; default: return 8; }
}

// deglitch {0.5s, 1s} -> return/accept as ms {500, 1000}
static inline uint8_t _ov_dgt_code_from_ms(uint16_t ms)
{
    return (ms >= 750) ? 1u : 0u;  // nearest: <750 -> 0.5s, else 1s
}
static inline uint16_t _ov_dgt_ms_from_code(uint8_t code1)
{
    return (code1 & 1u) ? 1000u : 500u;
}

HAL_StatusTypeDef ADP5360_get_ov_setting(uint16_t *ov_mV,
                                         uint8_t  *hys_pct,
                                         uint16_t *dgt_ms)
{
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_BATPRO_OV_SETTING, &v);
    if (st != HAL_OK) return st;

    if (ov_mV)   *ov_mV   = _ov_mv_from_code( (v & ADP5360_OV_CHG_MASK) >> ADP5360_OV_CHG_SHIFT );
    if (hys_pct) *hys_pct = _ov_hys_pct_from_code( (v & ADP5360_HYS_OV_CHG_MASK) >> ADP5360_HYS_OV_CHG_SHIFT );
    if (dgt_ms)  *dgt_ms  = _ov_dgt_ms_from_code( v & ADP5360_DGT_OV_CHG_MASK );
    return HAL_OK;
}

HAL_StatusTypeDef ADP5360_set_ov_setting(uint16_t ov_mV,
                                         uint8_t  hys_pct,
                                         uint16_t dgt_ms)
{
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_BATPRO_OV_SETTING, &v);
    if (st != HAL_OK) return st;

    uint8_t ov_code  = _ov_code_from_mv(ov_mV);
    uint8_t hys_code = _ov_hys_code_from_pct(hys_pct);
    uint8_t dgt_code = _ov_dgt_code_from_ms(dgt_ms);

    v &= ~(ADP5360_OV_CHG_MASK | ADP5360_HYS_OV_CHG_MASK | ADP5360_DGT_OV_CHG_MASK);
    v |= (uint8_t)(ov_code  << ADP5360_OV_CHG_SHIFT);
    v |= (uint8_t)(hys_code << ADP5360_HYS_OV_CHG_SHIFT);
    v |= (uint8_t)(dgt_code & ADP5360_DGT_OV_CHG_MASK);

    return ADP5360_write_u8(ADP5360_REG_BATPRO_OV_SETTING, v);
}


// --- helpers: charge OC threshold ---
static uint8_t _occhg_code_from_ma(uint16_t ma) {
    const uint16_t vals[8] = {25,50,100,150,200,250,300,400};
    uint8_t best=0; uint16_t best_err=0xFFFF;
    for (uint8_t i=0;i<8;i++){
        uint16_t e = (ma>vals[i])?(ma-vals[i]):(vals[i]-ma);
        if (e<best_err){best_err=e;best=i;}
    }
    return best; // 0..7
}
static uint16_t _occhg_ma_from_code(uint8_t code3) {
    const uint16_t vals[8] = {25,50,100,150,200,250,300,400};
    return vals[(code3 & 0x07u)];
}

// --- helpers: deglitch 5/10/20/40 ms ---
static uint8_t _dgt_chg_code_from_ms(uint16_t ms) {
    // nearest of {5,10,20,40}
    const uint16_t opts[4] = {5,10,20,40};
    uint8_t best=0; uint16_t best_err=0xFFFF;
    for (uint8_t i=0;i<4;i++){
        uint16_t e = (ms>opts[i])?(ms-opts[i]):(opts[i]-ms);
        if (e<best_err){best_err=e;best=i;}
    }
    return best; // 0..3
}
static uint16_t _dgt_chg_ms_from_code(uint8_t code2) {
    switch (code2 & 0x03u) { case 0: return 5; case 1: return 10; case 2: return 20; default: return 40; }
}

HAL_StatusTypeDef ADP5360_get_chg_oc(uint16_t *oc_mA, uint16_t *dgt_ms)
{
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_BATPRO_CHG_OC_SETTING, &v);
    if (st != HAL_OK) return st;

    if (oc_mA)  *oc_mA  = _occhg_ma_from_code( (v & ADP5360_OC_CHG_MASK) >> ADP5360_OC_CHG_SHIFT );
    if (dgt_ms) *dgt_ms = _dgt_chg_ms_from_code( (v & ADP5360_DGT_OC_CHG_MASK) >> ADP5360_DGT_OC_CHG_SHIFT );
    return HAL_OK;
}

HAL_StatusTypeDef ADP5360_set_chg_oc(uint16_t oc_mA, uint16_t dgt_ms)
{
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_BATPRO_CHG_OC_SETTING, &v);
    if (st != HAL_OK) return st;

    uint8_t oc_code  = _occhg_code_from_ma(oc_mA);
    uint8_t dgt_code = _dgt_chg_code_from_ms(dgt_ms);

    v &= ~(ADP5360_OC_CHG_MASK | ADP5360_DGT_OC_CHG_MASK);
    v |= (uint8_t)(oc_code  << ADP5360_OC_CHG_SHIFT);
    v |= (uint8_t)(dgt_code << ADP5360_DGT_OC_CHG_SHIFT);

    return ADP5360_write_u8(ADP5360_REG_BATPRO_CHG_OC_SETTING, v);
}


static inline uint16_t _vsoc_code_to_mv(uint8_t code) {
    // VBAT = 2500 mV + 8 mV * CODE
    return (uint16_t)(2500u + (uint16_t)code * 8u);
}

HAL_StatusTypeDef ADP5360_get_vsoc_point_mv(uint8_t reg_addr, uint16_t *vbatt_mV)
{
    if (!vbatt_mV) return HAL_ERROR;
    uint8_t code = 0;
    HAL_StatusTypeDef st = ADP5360_read_u8(reg_addr, &code);
    if (st != HAL_OK) return st;
    *vbatt_mV = _vsoc_code_to_mv(code);
    return HAL_OK;
}

HAL_StatusTypeDef ADP5360_get_vsoc_table_mv(uint16_t vsoc_mV[10])
{
    if (!vsoc_mV) return HAL_ERROR;
    const uint8_t regs[10] = {
        ADP5360_REG_V_SOC_0,   ADP5360_REG_V_SOC_5,  ADP5360_REG_V_SOC_11,
        ADP5360_REG_V_SOC_19,  ADP5360_REG_V_SOC_28, ADP5360_REG_V_SOC_41,
        ADP5360_REG_V_SOC_55,  ADP5360_REG_V_SOC_69, ADP5360_REG_V_SOC_84,
        ADP5360_REG_V_SOC_100
    };
    for (int i = 0; i < 10; ++i) {
        uint8_t code = 0;
        HAL_StatusTypeDef st = ADP5360_read_u8(regs[i], &code);
        if (st != HAL_OK) return st;
        vsoc_mV[i] = _vsoc_code_to_mv(code);
    }
    return HAL_OK;
}


HAL_StatusTypeDef ADP5360_get_bat_capacity(uint16_t *capacity_mAh)
{
    if (!capacity_mAh) return HAL_ERROR;
    uint8_t code = 0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_BAT_CAP, &code);
    if (st != HAL_OK) return st;
    *capacity_mAh = (uint16_t)code * 2u;
    return HAL_OK;
}

HAL_StatusTypeDef ADP5360_set_bat_capacity(uint16_t capacity_mAh)
{
    // Clamp 0..510 and convert mAh -> code (2 mAh/LSB), rounding to nearest
    if (capacity_mAh > 510u) capacity_mAh = 510u;
    uint8_t code = (uint8_t)((capacity_mAh + 1u) / 2u);
    return ADP5360_write_u8(ADP5360_REG_BAT_CAP, code);
}


HAL_StatusTypeDef ADP5360_get_soc(uint8_t *soc_percent, uint8_t *raw7)
{
    uint8_t v = 0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_BAT_SOC, &v);
    if (st != HAL_OK) return st;

    uint8_t r = (uint8_t)(v & 0x7Fu);   // 7-bit field
    if (raw7)       *raw7 = r;
    if (soc_percent)*soc_percent = (r > 100u) ? 100u : r;  // only valid 0..100

    return HAL_OK;
}


// --- helpers: encode/decode tables for 0x22 ---
static uint8_t _code_from_age_pct(float pct) {
    // nearest of {0.8,1.5,3.1,6.3}
    const float opts[4] = {0.8f,1.5f,3.1f,6.3f};
    uint8_t best=0; float best_err=1e9f;
    for (uint8_t i=0;i<4;i++){ float e=fabsf(pct-opts[i]); if(e<best_err){best_err=e;best=i;} }
    return best;
}
static float _age_pct_from_code(uint8_t code2) {
    switch(code2 & 0x03u){ case 0: return 0.8f; case 1: return 1.5f; case 2: return 3.1f; default: return 6.3f; }
}

static uint8_t _code_from_temp_coeff(float pct_per_C) {
    // nearest of {0.2,0.4,0.6,0.8}
    const float opts[4] = {0.2f,0.4f,0.6f,0.8f};
    uint8_t best=0; float best_err=1e9f;
    for (uint8_t i=0;i<4;i++){ float e=fabsf(pct_per_C-opts[i]); if(e<best_err){best_err=e;best=i;} }
    return best;
}
static float _temp_coeff_from_code(uint8_t code2) {
    switch(code2 & 0x03u){ case 0: return 0.2f; case 1: return 0.4f; case 2: return 0.6f; default: return 0.8f; }
}

HAL_StatusTypeDef ADP5360_get_socaccum_ctl(ADP5360_socacm_ctl_t *ctl)
{
    if (!ctl) return HAL_ERROR;
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_BAT_SOCACM_CTL, &v);
    if (st != HAL_OK) return st;

    ctl->age_reduction_pct    = _age_pct_from_code( (v & ADP5360_BATCAP_AGE_MASK)  >> ADP5360_BATCAP_AGE_SHIFT );
    ctl->temp_coeff_pct_per_C = _temp_coeff_from_code( (v & ADP5360_BATCAP_TEMP_MASK) >> ADP5360_BATCAP_TEMP_SHIFT );
    ctl->en_temp_comp         = (v & ADP5360_EN_BATCAP_TEMP) ? 1u : 0u;
    ctl->en_age_comp          = (v & ADP5360_EN_BATCAP_AGE)  ? 1u : 0u;
    return HAL_OK;
}

HAL_StatusTypeDef ADP5360_set_socaccum_ctl(const ADP5360_socacm_ctl_t *ctl)
{
    if (!ctl) return HAL_ERROR;
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_BAT_SOCACM_CTL, &v);
    if (st != HAL_OK) return st;

    uint8_t age_code  = _code_from_age_pct(ctl->age_reduction_pct);
    uint8_t temp_code = _code_from_temp_coeff(ctl->temp_coeff_pct_per_C);

    v &= ~(ADP5360_BATCAP_AGE_MASK | ADP5360_BATCAP_TEMP_MASK | ADP5360_EN_BATCAP_TEMP | ADP5360_EN_BATCAP_AGE);
    v |= (uint8_t)(age_code  << ADP5360_BATCAP_AGE_SHIFT);
    v |= (uint8_t)(temp_code << ADP5360_BATCAP_TEMP_SHIFT);
    if (ctl->en_temp_comp) v |= ADP5360_EN_BATCAP_TEMP;
    if (ctl->en_age_comp)  v |= ADP5360_EN_BATCAP_AGE;

    return ADP5360_write_u8(ADP5360_REG_BAT_SOCACM_CTL, v);
}



HAL_StatusTypeDef ADP5360_get_soc_accumulator(uint16_t *raw12, float *charge_times)
{
    uint8_t hi = 0, lo = 0;
    HAL_StatusTypeDef st;

    st = ADP5360_read_u8(ADP5360_REG_BAT_SOCACM_H, &hi);
    if (st != HAL_OK) return st;

    st = ADP5360_read_u8(ADP5360_REG_BAT_SOCACM_L, &lo);
    if (st != HAL_OK) return st;

    uint16_t r = (uint16_t)(((uint16_t)hi << 4) | ((lo >> 4) & 0x0Fu)); // 12-bit
    if (raw12) *raw12 = r;
    if (charge_times) *charge_times = (float)r / 100.0f;

    return HAL_OK;
}


HAL_StatusTypeDef ADP5360_get_vbat(uint16_t *vbat_mV, uint16_t *raw12)
{
    uint8_t hi = 0, lo = 0;
    HAL_StatusTypeDef st;

    st = ADP5360_read_u8(ADP5360_REG_VBAT_READ_H, &hi);
    if (st != HAL_OK) return st;

    st = ADP5360_read_u8(ADP5360_REG_VBAT_READ_L, &lo);
    if (st != HAL_OK) return st;

    uint16_t r = (uint16_t)(((uint16_t)hi << 5) | ((lo >> 3) & 0x1Fu)); // 12-bit
    if (raw12)  *raw12  = r;
    if (vbat_mV)*vbat_mV = r;  // units are mV per datasheet

    return HAL_OK;
}


// --- tiny table helpers ---
static uint8_t _code_from_list_u8(uint8_t v, const uint8_t *opts, uint8_t n){
    uint8_t best=0, err=0xFF;
    for(uint8_t i=0;i<n;i++){ uint8_t e = (v>opts[i])?(v-opts[i]):(opts[i]-v); if(e<err){err=e;best=i;} }
    return best;
}
static uint8_t _code_from_list_u16(uint16_t v, const uint16_t *opts, uint8_t n){
    uint8_t best=0; uint16_t err=0xFFFF;
    for(uint8_t i=0;i<n;i++){ uint16_t e = (v>opts[i])?(v-opts[i]):(opts[i]-v); if(e<err){err=e;best=i;} }
    return best;
}

HAL_StatusTypeDef ADP5360_get_fg_mode(ADP5360_fg_mode_t *m)
{
    if(!m) return HAL_ERROR;
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_FUEL_GAUGE_MODE, &v);
    if(st!=HAL_OK) return st;

    // decode
    switch ((v & ADP5360_SOC_LOW_TH_MASK) >> ADP5360_SOC_LOW_TH_SHIFT) {
        case 0: m->soc_low_th_pct = 6;  break;
        case 1: m->soc_low_th_pct = 11; break;
        case 2: m->soc_low_th_pct = 21; break;
        default:m->soc_low_th_pct = 31; break;
    }
    switch ((v & ADP5360_SLP_CURR_MASK) >> ADP5360_SLP_CURR_SHIFT) {
        case 0: m->slp_curr_mA = 5;  break;
        case 1: m->slp_curr_mA = 10; break;
        case 2: m->slp_curr_mA = 20; break;
        default:m->slp_curr_mA = 40; break;
    }
    switch ((v & ADP5360_SLP_TIME_MASK) >> ADP5360_SLP_TIME_SHIFT) {
        case 0: m->slp_time_min = 1;  break;
        case 1: m->slp_time_min = 4;  break;
        case 2: m->slp_time_min = 8;  break;
        default:m->slp_time_min = 16; break;
    }
    m->fg_mode_sleep = (v & ADP5360_FG_MODE_MASK) ? 1u : 0u;
    m->en_fg         = (v & ADP5360_EN_FG_MASK)   ? 1u : 0u;
    return HAL_OK;
}

HAL_StatusTypeDef ADP5360_set_fg_mode(const ADP5360_fg_mode_t *m)
{
    if(!m) return HAL_ERROR;
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_FUEL_GAUGE_MODE, &v);
    if(st!=HAL_OK) return st;

    const uint8_t  th_opts[4] = {6,11,21,31};
    const uint16_t ic_opts[4] = {5,10,20,40};
    const uint16_t it_opts[4] = {1,4,8,16};

    uint8_t th_code = _code_from_list_u8 (m->soc_low_th_pct, th_opts, 4);
    uint8_t ic_code = _code_from_list_u16(m->slp_curr_mA,   ic_opts, 4);
    uint8_t it_code = _code_from_list_u16(m->slp_time_min,  it_opts, 4);

    v &= ~(ADP5360_SOC_LOW_TH_MASK | ADP5360_SLP_CURR_MASK | ADP5360_SLP_TIME_MASK |
           ADP5360_FG_MODE_MASK | ADP5360_EN_FG_MASK);

    v |= (uint8_t)(th_code << ADP5360_SOC_LOW_TH_SHIFT);
    v |= (uint8_t)(ic_code << ADP5360_SLP_CURR_SHIFT);
    v |= (uint8_t)(it_code << ADP5360_SLP_TIME_SHIFT);
    if (m->fg_mode_sleep) v |= ADP5360_FG_MODE_MASK;   // 1=sleep
    if (m->en_fg)         v |= ADP5360_EN_FG_MASK;     // 1=enable

    return ADP5360_write_u8(ADP5360_REG_FUEL_GAUGE_MODE, v);
}

HAL_StatusTypeDef ADP5360_fg_enable_active(void)
{
    ADP5360_fg_mode_t m = { .soc_low_th_pct=11, .slp_curr_mA=10, .slp_time_min=1,
                            .fg_mode_sleep=0, .en_fg=1 };
    return ADP5360_set_fg_mode(&m);
}


HAL_StatusTypeDef ADP5360_fg_refresh(void)
{
    // Write 1 to bit7, then 0. Other bits are reserved/read-only.
    HAL_StatusTypeDef st;
    uint8_t v = ADP5360_SOC_RESET_MASK;
    st = ADP5360_write_u8(ADP5360_REG_SOC_RESET, v);
    if (st != HAL_OK) return st;
    v = 0x00u;
    return ADP5360_write_u8(ADP5360_REG_SOC_RESET, v);
}


static uint8_t _buck_code_from_list_u16(uint16_t v, const uint16_t *opts, uint8_t n){
    uint8_t best=0; uint16_t err=0xFFFF;
    for(uint8_t i=0;i<n;i++){ uint16_t e = (v>opts[i])?(v-opts[i]):(opts[i]-v);
        if(e<err){err=e;best=i;} }
    return best;
}

HAL_StatusTypeDef ADP5360_get_buck(ADP5360_buck_cfg_t *cfg)
{
    if(!cfg) return HAL_ERROR;
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_BUCK_CONFIGURE, &v);
    if(st!=HAL_OK) return st;

    switch ((v & ADP5360_BUCK_SS_MASK) >> ADP5360_BUCK_SS_SHIFT) {
        case 0: cfg->softstart_ms = 1;   break;
        case 1: cfg->softstart_ms = 8;   break;
        case 2: cfg->softstart_ms = 64;  break;
        default:cfg->softstart_ms = 512; break;
    }
    switch ((v & ADP5360_BUCK_ILIM_MASK) >> ADP5360_BUCK_ILIM_SHIFT) {
        case 0: cfg->ilim_mA = 100; break;
        case 1: cfg->ilim_mA = 200; break;
        case 2: cfg->ilim_mA = 300; break;
        default:cfg->ilim_mA = 400; break;
    }
    cfg->fpwm_mode   = (v & ADP5360_BUCK_MODE_MASK) ? 1u : 0u;
    cfg->stop_enable = (v & ADP5360_STP_BUCK_MASK)  ? 1u : 0u;
    cfg->discharge_en= (v & ADP5360_DISCHG_BUCK_MASK)?1u : 0u;
    cfg->enable      = (v & ADP5360_EN_BUCK_MASK)   ? 1u : 0u;
    return HAL_OK;
}

HAL_StatusTypeDef ADP5360_set_buck(const ADP5360_buck_cfg_t *cfg)
{
    if(!cfg) return HAL_ERROR;
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_BUCK_CONFIGURE, &v);
    if(st!=HAL_OK) return st;

    const uint16_t ss_opts[4] = {1,8,64,512};
    const uint16_t il_opts[4] = {100,200,300,400};
    uint8_t ss_code = _buck_code_from_list_u16(cfg->softstart_ms, ss_opts, 4);
    uint8_t il_code = _buck_code_from_list_u16(cfg->ilim_mA,     il_opts, 4);

    v &= ~(ADP5360_BUCK_SS_MASK | ADP5360_BUCK_ILIM_MASK | ADP5360_BUCK_MODE_MASK |
           ADP5360_STP_BUCK_MASK | ADP5360_DISCHG_BUCK_MASK | ADP5360_EN_BUCK_MASK);

    v |= (uint8_t)(ss_code << ADP5360_BUCK_SS_SHIFT);
    v |= (uint8_t)(il_code << ADP5360_BUCK_ILIM_SHIFT);
    if (cfg->fpwm_mode)    v |= ADP5360_BUCK_MODE_MASK;
    if (cfg->stop_enable)  v |= ADP5360_STP_BUCK_MASK;
    if (cfg->discharge_en) v |= ADP5360_DISCHG_BUCK_MASK;
    if (cfg->enable)       v |= ADP5360_EN_BUCK_MASK;

    return ADP5360_write_u8(ADP5360_REG_BUCK_CONFIGURE, v);
}


static inline uint8_t _buck_vout_code_from_mv(uint16_t mv) {
    if (mv < 600)  mv = 600;
    if (mv > 3750) mv = 3750;
    // 50 mV/LSB from 600 mV
    uint16_t off  = (uint16_t)(mv - 600);
    uint16_t code = (off + 25u) / 50u; // round to nearest
    if (code > 63u) code = 63u;
    return (uint8_t)code;
}
static inline uint16_t _buck_vout_mv_from_code(uint8_t code6) {
    if (code6 > 63) code6 = 63;
    return (uint16_t)(600u + 50u * code6);
}
static inline uint8_t _buck_dly_code_from_us(uint16_t us) {
    // nearest of {0,5,10,20}
    const uint16_t opts[4] = {0,5,10,20};
    uint8_t best=0; uint16_t err=0xFFFF;
    for (uint8_t i=0;i<4;i++){
        uint16_t e = (us>opts[i])?(us-opts[i]):(opts[i]-us);
        if (e<err){ err=e; best=i; }
    }
    return best; // 0..3
}
static inline uint16_t _buck_dly_us_from_code(uint8_t code2) {
    switch (code2 & 0x03u) { case 0: return 0; case 1: return 5; case 2: return 10; default: return 20; }
}

HAL_StatusTypeDef ADP5360_get_buck_vout(uint16_t *vout_mV, uint16_t *dly_us)
{
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_BUCK_OUTPUT_VOLTAGE, &v);
    if (st != HAL_OK) return st;

    if (vout_mV) *vout_mV = _buck_vout_mv_from_code(v & ADP5360_VOUT_BUCK_MASK);
    if (dly_us)  *dly_us  = _buck_dly_us_from_code((v & ADP5360_BUCK_DLY_MASK) >> ADP5360_BUCK_DLY_SHIFT);
    return HAL_OK;
}

HAL_StatusTypeDef ADP5360_set_buck_vout(uint16_t vout_mV, uint16_t dly_us)
{
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_BUCK_OUTPUT_VOLTAGE, &v);
    if (st != HAL_OK) return st;

    uint8_t code_v = _buck_vout_code_from_mv(vout_mV);
    uint8_t code_d = _buck_dly_code_from_us(dly_us);

    v &= ~(ADP5360_VOUT_BUCK_MASK | ADP5360_BUCK_DLY_MASK);
    v |= (uint8_t)(code_v & ADP5360_VOUT_BUCK_MASK);
    v |= (uint8_t)(code_d << ADP5360_BUCK_DLY_SHIFT);

    return ADP5360_write_u8(ADP5360_REG_BUCK_OUTPUT_VOLTAGE, v);
}


static uint8_t _code_from_list_u16_bb(uint16_t v, const uint16_t *opts, uint8_t n){
    uint8_t best=0; uint16_t err=0xFFFF;
    for(uint8_t i=0;i<n;i++){ uint16_t e=(v>opts[i])?(v-opts[i]):(opts[i]-v);
        if(e<err){err=e;best=i;} }
    return best;
}

HAL_StatusTypeDef ADP5360_get_buckboost(ADP5360_buckbst_cfg_t *cfg)
{
    if(!cfg) return HAL_ERROR;
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_BUCKBST_CONFIGURE, &v);
    if(st!=HAL_OK) return st;

    switch ((v & ADP5360_BUCKBST_SS_MASK) >> ADP5360_BUCKBST_SS_SHIFT) {
        case 0: cfg->softstart_ms = 1;   break;
        case 1: cfg->softstart_ms = 8;   break;
        case 2: cfg->softstart_ms = 64;  break;
        default:cfg->softstart_ms = 512; break;
    }

    switch ((v & ADP5360_BUCKBST_ILIM_MASK) >> ADP5360_BUCKBST_ILIM_SHIFT) {
        case 0: cfg->ilim_mA = 100; break;
        case 1: cfg->ilim_mA = 200; break;
        case 2: cfg->ilim_mA = 300; break;
        case 3: cfg->ilim_mA = 400; break;
        case 4: cfg->ilim_mA = 500; break;
        case 5: cfg->ilim_mA = 600; break;
        case 6: cfg->ilim_mA = 700; break;
        default:cfg->ilim_mA = 800; break;
    }

    cfg->stop_enable  = (v & ADP5360_STP_BUCKBST_MASK)   ? 1u : 0u;
    cfg->discharge_en = (v & ADP5360_DISCHG_BUCKBST_MASK)? 1u : 0u;
    cfg->enable       = (v & ADP5360_EN_BUCKBST_MASK)    ? 1u : 0u;
    return HAL_OK;
}

HAL_StatusTypeDef ADP5360_set_buckboost(const ADP5360_buckbst_cfg_t *cfg)
{
    if(!cfg) return HAL_ERROR;
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_BUCKBST_CONFIGURE, &v);
    if(st!=HAL_OK) return st;

    const uint16_t ss_opts[4] = {1,8,64,512};
    const uint16_t il_opts[8] = {100,200,300,400,500,600,700,800};
    uint8_t ss_code = _code_from_list_u16_bb(cfg->softstart_ms, ss_opts, 4);
    uint8_t il_code = _code_from_list_u16_bb(cfg->ilim_mA,     il_opts, 8);

    v &= ~(ADP5360_BUCKBST_SS_MASK | ADP5360_BUCKBST_ILIM_MASK |
           ADP5360_STP_BUCKBST_MASK | ADP5360_DISCHG_BUCKBST_MASK | ADP5360_EN_BUCKBST_MASK);

    v |= (uint8_t)(ss_code << ADP5360_BUCKBST_SS_SHIFT);
    v |= (uint8_t)(il_code << ADP5360_BUCKBST_ILIM_SHIFT);
    if (cfg->stop_enable)  v |= ADP5360_STP_BUCKBST_MASK;
    if (cfg->discharge_en) v |= ADP5360_DISCHG_BUCKBST_MASK;
    if (cfg->enable)       v |= ADP5360_EN_BUCKBST_MASK;

    return ADP5360_write_u8(ADP5360_REG_BUCKBST_CONFIGURE, v);
}



// Piecewise code<->mV mapping:
// code 0..11  : V = 1800 + 100*code (mV)
// code 12..63 : V = 2950 +  50*(code-12) (mV)
static inline uint16_t _bb_vout_mv_from_code(uint8_t code6)
{
    uint8_t c = (code6 > 63) ? 63 : code6;
    if (c <= 11) return (uint16_t)(1800u + 100u * c);
    return (uint16_t)(2950u + 50u * (c - 12u));
}

static inline uint8_t _bb_vout_code_from_mv(uint16_t mv)
{
    if (mv < 1800) mv = 1800;
    if (mv > 5500) mv = 5500;

    if (mv <= 2900) {
        // round to nearest 100 mV from 1.8 V
        uint16_t off = (uint16_t)(mv - 1800);
        uint16_t code = (off + 50u) / 100u;   // nearest
        if (code > 11u) code = 11u;
        return (uint8_t)code;                 // 0..11
    } else {
        // round to nearest 50 mV from 2.95 V
        uint16_t off = (uint16_t)(mv - 2950);
        uint16_t step = (off + 25u) / 50u;    // nearest
        uint16_t code = 12u + step;
        if (code > 63u) code = 63u;
        return (uint8_t)code;                 // 12..63
    }
}

static inline uint8_t _bb_dly_code_from_us(uint16_t us){
    const uint16_t opts[4]={0,5,10,20};
    uint8_t best=0; uint16_t err=0xFFFF;
    for(uint8_t i=0;i<4;i++){ uint16_t e=(us>opts[i])?(us-opts[i]):(opts[i]-us);
        if(e<err){err=e;best=i;} }
    return best; // 0..3
}
static inline uint16_t _bb_dly_us_from_code(uint8_t code2){
    switch(code2&0x03u){ case 0: return 0; case 1: return 5; case 2: return 10; default: return 20; }
}

HAL_StatusTypeDef ADP5360_get_buckboost_vout(uint16_t *vout_mV, uint16_t *dly_us)
{
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_BUCKBST_OUTPUT_VOLTAGE, &v);
    if (st != HAL_OK) return st;

    if (vout_mV) *vout_mV = _bb_vout_mv_from_code(v & ADP5360_VOUT_BUCKBST_MASK);
    if (dly_us)  *dly_us  = _bb_dly_us_from_code((v & ADP5360_BUCKBST_DLY_MASK) >> ADP5360_BUCKBST_DLY_SHIFT);
    return HAL_OK;
}

HAL_StatusTypeDef ADP5360_set_buckboost_vout(uint16_t vout_mV, uint16_t dly_us)
{
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_BUCKBST_OUTPUT_VOLTAGE, &v);
    if (st != HAL_OK) return st;

    uint8_t code_v = _bb_vout_code_from_mv(vout_mV);
    uint8_t code_d = _bb_dly_code_from_us(dly_us);

    v &= ~(ADP5360_VOUT_BUCKBST_MASK | ADP5360_BUCKBST_DLY_MASK);
    v |= (uint8_t)(code_v & ADP5360_VOUT_BUCKBST_MASK);
    v |= (uint8_t)(code_d << ADP5360_BUCKBST_DLY_SHIFT);

    return ADP5360_write_u8(ADP5360_REG_BUCKBST_OUTPUT_VOLTAGE, v);
}




HAL_StatusTypeDef ADP5360_get_supervisory(ADP5360_supervisory_t *cfg)
{
    if (!cfg) return HAL_ERROR;
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_SUPERVISORY_SETTING, &v);
    if (st != HAL_OK) return st;

    cfg->buck_rst_en    = (v & ADP5360_VOUT1_RST_MASK) ? 1u : 0u;
    cfg->buckbst_rst_en = (v & ADP5360_VOUT2_RST_MASK) ? 1u : 0u;
    cfg->reset_time_ms  = (v & ADP5360_RESET_TIME_MASK) ? 1600u : 200u;

    switch ((v & ADP5360_WD_TIME_MASK) >> ADP5360_WD_TIME_SHIFT) {
        case 0x0u: cfg->wd_time_s = 12.5f; break;
        case 0x1u: cfg->wd_time_s = 25.6f; break;
        case 0x2u: cfg->wd_time_s = 50.0f; break;
        default:   cfg->wd_time_s = 100.0f; break;
    }

    cfg->wd_enable      = (v & ADP5360_EN_WD_MASK)    ? 1u : 0u;
    cfg->mr_shipment_en = (v & ADP5360_EN_MR_SD_MASK) ? 1u : 0u;
    return HAL_OK;
}

HAL_StatusTypeDef ADP5360_set_supervisory(const ADP5360_supervisory_t *cfg)
{
    if (!cfg) return HAL_ERROR;
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_SUPERVISORY_SETTING, &v);
    if (st != HAL_OK) return st;

    // clear writable control bits (leave RESET_WD alone)
    v &= ~(ADP5360_VOUT1_RST_MASK | ADP5360_VOUT2_RST_MASK | ADP5360_RESET_TIME_MASK |
           ADP5360_WD_TIME_MASK | ADP5360_EN_WD_MASK | ADP5360_EN_MR_SD_MASK);

    if (cfg->buck_rst_en)    v |= ADP5360_VOUT1_RST_MASK;
    if (cfg->buckbst_rst_en) v |= ADP5360_VOUT2_RST_MASK;
    if (cfg->reset_time_ms >= 1000u) v |= ADP5360_RESET_TIME_MASK;  // 1.6s else 200ms

    // choose closest watchdog period
    float t = cfg->wd_time_s;
    uint8_t wd_code = 0; // default 12.5s
    if (t >= 88.0f) wd_code = 0x3u;
    else if (t >= 37.8f) wd_code = 0x2u;
    else if (t >= 19.1f) wd_code = 0x1u;
    // else 0x0u
    v |= (uint8_t)(wd_code << ADP5360_WD_TIME_SHIFT);

    if (cfg->wd_enable)      v |= ADP5360_EN_WD_MASK;
    if (cfg->mr_shipment_en) v |= ADP5360_EN_MR_SD_MASK;

    return ADP5360_write_u8(ADP5360_REG_SUPERVISORY_SETTING, v);
}

HAL_StatusTypeDef ADP5360_watchdog_kick(void)
{
    // RMW to preserve other settings; write bit0=1 (auto clears)
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_SUPERVISORY_SETTING, &v);
    if (st != HAL_OK) return st;
    v |= ADP5360_RESET_WD_MASK;
    return ADP5360_write_u8(ADP5360_REG_SUPERVISORY_SETTING, v);
}


HAL_StatusTypeDef ADP5360_get_fault(uint8_t *mask)
{
    if (!mask) return HAL_ERROR;
    return ADP5360_read_u8(ADP5360_REG_FAULT, mask);
}

HAL_StatusTypeDef ADP5360_clear_fault(uint8_t mask)
{
    // Sticky R/W flags: write '1' to the bits you want to clear, '0' to leave unchanged.
    // Make sure we only touch defined bits.
    uint8_t valid = ADP5360_FLT_BAT_UV | ADP5360_FLT_BAT_OC | ADP5360_FLT_BAT_CHGOC |
                    ADP5360_FLT_BAT_CHGOV | ADP5360_FLT_WD_TIMEOUT | ADP5360_FLT_TSD110;
    uint8_t w = mask & valid;
    if (w == 0) return HAL_OK; // nothing to clear
    return ADP5360_write_u8(ADP5360_REG_FAULT, w);
}

HAL_StatusTypeDef ADP5360_clear_all_faults(void)
{
    return ADP5360_clear_fault(ADP5360_FLT_BAT_UV | ADP5360_FLT_BAT_OC | ADP5360_FLT_BAT_CHGOC |
                               ADP5360_FLT_BAT_CHGOV | ADP5360_FLT_WD_TIMEOUT | ADP5360_FLT_TSD110);
}

HAL_StatusTypeDef ADP5360_get_pgood(ADP5360_pgood_t *pg, uint8_t *raw)
{
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_PGOOD_STATUS, &v);
    if (st != HAL_OK) return st;

    if (raw) *raw = v;
    if (pg) {
        pg->mr_press  = (v & ADP5360_PG_MR_PRESS)  ? 1u : 0u;
        pg->chg_cmplt = (v & ADP5360_PG_CHG_CMPLT) ? 1u : 0u;
        pg->vbus_ok   = (v & ADP5360_PG_VBUSOK)    ? 1u : 0u;
        pg->bat_ok    = (v & ADP5360_PG_BATOK)     ? 1u : 0u;
        pg->vout2_ok  = (v & ADP5360_PG_VOUT2OK)   ? 1u : 0u;
        pg->vout1_ok  = (v & ADP5360_PG_VOUT1OK)   ? 1u : 0u;
    }
    return HAL_OK;
}



HAL_StatusTypeDef ADP5360_get_pgood1_mask(ADP5360_pgood1_mask_t *m, uint8_t *raw)
{
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_PGOOD1_MASK, &v);
    if (st != HAL_OK) return st;
    if (raw) *raw = v;
    if (m) {
        m->active_low = (v & ADP5360_PG1_REV_MASK)   ? 1u : 0u;
        m->chg_cmplt  = (v & ADP5360_CHGCMPLT_MASK1) ? 1u : 0u;
        m->vbus_ok    = (v & ADP5360_VBUSOK_MASK1)   ? 1u : 0u;
        m->bat_ok     = (v & ADP5360_BATOK_MASK1)    ? 1u : 0u;
        m->vout2_ok   = (v & ADP5360_VOUT2OK_MASK1)  ? 1u : 0u;
        m->vout1_ok   = (v & ADP5360_VOUT1OK_MASK1)  ? 1u : 0u;
    }
    return HAL_OK;
}

HAL_StatusTypeDef ADP5360_set_pgood1_mask(const ADP5360_pgood1_mask_t *m)
{
    if (!m) return HAL_ERROR;
    uint8_t v=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_PGOOD1_MASK, &v);
    if (st != HAL_OK) return st;

    v &= ~(ADP5360_PG1_REV_MASK | ADP5360_CHGCMPLT_MASK1 | ADP5360_VBUSOK_MASK1 |
           ADP5360_BATOK_MASK1  | ADP5360_VOUT2OK_MASK1  | ADP5360_VOUT1OK_MASK1);

    if (m->active_low) v |= ADP5360_PG1_REV_MASK;
    if (m->chg_cmplt)  v |= ADP5360_CHGCMPLT_MASK1;
    if (m->vbus_ok)    v |= ADP5360_VBUSOK_MASK1;
    if (m->bat_ok)     v |= ADP5360_BATOK_MASK1;
    if (m->vout2_ok)   v |= ADP5360_VOUT2OK_MASK1;
    if (m->vout1_ok)   v |= ADP5360_VOUT1OK_MASK1;

    return ADP5360_write_u8(ADP5360_REG_PGOOD1_MASK, v);
}

HAL_StatusTypeDef ADP5360_get_irq_enable(ADP5360_irq_enable_t *en, uint8_t *raw1, uint8_t *raw2)
{
    if (!en) return HAL_ERROR;
    uint8_t v1=0, v2=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_INT_ENABLE1, &v1);
    if (st != HAL_OK) return st;
    st = ADP5360_read_u8(ADP5360_REG_INT_ENABLE2, &v2);
    if (st != HAL_OK) return st;

    if (raw1) *raw1 = v1;
    if (raw2) *raw2 = v2;

    en->soc_low   = (v1 & ADP5360_EN_SOCLOW_INT)    ? 1u : 0u;
    en->soc_acm   = (v1 & ADP5360_EN_SOCACM_INT)    ? 1u : 0u;
    en->adpichg   = (v1 & ADP5360_EN_ADPICHG_INT)   ? 1u : 0u;
    en->batpro    = (v1 & ADP5360_EN_BATPRO_INT)    ? 1u : 0u;
    en->thr       = (v1 & ADP5360_EN_THR_INT)       ? 1u : 0u;
    en->bat       = (v1 & ADP5360_EN_BAT_INT)       ? 1u : 0u;
    en->chg       = (v1 & ADP5360_EN_CHG_INT)       ? 1u : 0u;
    en->vbus      = (v1 & ADP5360_EN_VBUS_INT)      ? 1u : 0u;

    en->mr        = (v2 & ADP5360_EN_MR_INT)        ? 1u : 0u;
    en->wd        = (v2 & ADP5360_EN_WD_INT)        ? 1u : 0u;
    en->buck_pg   = (v2 & ADP5360_EN_BUCKPG_INT)    ? 1u : 0u;
    en->buckbst_pg= (v2 & ADP5360_EN_BUCKBSTPG_INT) ? 1u : 0u;

    return HAL_OK;
}

HAL_StatusTypeDef ADP5360_set_irq_enable(const ADP5360_irq_enable_t *en)
{
    if (!en) return HAL_ERROR;

    uint8_t v1=0, v2=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_INT_ENABLE1, &v1);
    if (st != HAL_OK) return st;
    st = ADP5360_read_u8(ADP5360_REG_INT_ENABLE2, &v2);
    if (st != HAL_OK) return st;

    v1 &= ~(ADP5360_EN_SOCLOW_INT | ADP5360_EN_SOCACM_INT | ADP5360_EN_ADPICHG_INT |
            ADP5360_EN_BATPRO_INT | ADP5360_EN_THR_INT | ADP5360_EN_BAT_INT |
            ADP5360_EN_CHG_INT    | ADP5360_EN_VBUS_INT);
    v2 &= ~(ADP5360_EN_MR_INT | ADP5360_EN_WD_INT | ADP5360_EN_BUCKPG_INT | ADP5360_EN_BUCKBSTPG_INT);

    if (en->soc_low)    v1 |= ADP5360_EN_SOCLOW_INT;
    if (en->soc_acm)    v1 |= ADP5360_EN_SOCACM_INT;
    if (en->adpichg)    v1 |= ADP5360_EN_ADPICHG_INT;
    if (en->batpro)     v1 |= ADP5360_EN_BATPRO_INT;
    if (en->thr)        v1 |= ADP5360_EN_THR_INT;
    if (en->bat)        v1 |= ADP5360_EN_BAT_INT;
    if (en->chg)        v1 |= ADP5360_EN_CHG_INT;
    if (en->vbus)       v1 |= ADP5360_EN_VBUS_INT;

    if (en->mr)         v2 |= ADP5360_EN_MR_INT;
    if (en->wd)         v2 |= ADP5360_EN_WD_INT;
    if (en->buck_pg)    v2 |= ADP5360_EN_BUCKPG_INT;
    if (en->buckbst_pg) v2 |= ADP5360_EN_BUCKBSTPG_INT;

    st = ADP5360_write_u8(ADP5360_REG_INT_ENABLE1, v1);
    if (st != HAL_OK) return st;
    return ADP5360_write_u8(ADP5360_REG_INT_ENABLE2, v2);
}


HAL_StatusTypeDef ADP5360_read_irq_flags(uint8_t *flag1, uint8_t *flag2)
{
    uint8_t f1=0, f2=0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_INT_FLAG1, &f1);
    if (st != HAL_OK) return st;
    st = ADP5360_read_u8(ADP5360_REG_INT_FLAG2, &f2);
    if (st != HAL_OK) return st;

    if (flag1) *flag1 = f1;
    if (flag2) *flag2 = f2;
    return HAL_OK;
}


HAL_StatusTypeDef ADP5360_get_shipmode(uint8_t *enabled)
{
    if (!enabled) return HAL_ERROR;
    uint8_t v = 0;
    HAL_StatusTypeDef st = ADP5360_read_u8(ADP5360_REG_SHIPMODE, &v);
    if (st != HAL_OK) return st;
    *enabled = (v & ADP5360_EN_SHIPMODE_MASK) ? 1u : 0u;
    return HAL_OK;
}

HAL_StatusTypeDef ADP5360_set_shipmode(uint8_t enable)
{
    uint8_t v = enable ? ADP5360_EN_SHIPMODE_MASK : 0x00u;
    // Datasheet: writing '1' puts device into shipment mode immediately.
    // Writing '0' keeps it out of shipment mode.
    return ADP5360_write_u8(ADP5360_REG_SHIPMODE, v);
}


#define RET(x) do { HAL_StatusTypeDef _st = (x); if (_st != HAL_OK) return _st; } while(0)

static inline ADP5360_ithr_t _map_ithr_uA(uint16_t uA) {
    if (uA <= 6)  return ADP5360_ITHR_6UA;
    if (uA <= 12) return ADP5360_ITHR_12UA;
    return ADP5360_ITHR_60UA; // default/high
}

HAL_StatusTypeDef ADP5360_power_init(const ADP5360_init_t *c)
{
    if (!c) return HAL_ERROR;

    // --- CHARGER ---
    const ADP5360_vsys_t vsys =
        c->vbus_ilim.vsys_5V ? ADP5360_VSYS_5V : ADP5360_VSYS_VTRM_P200mV;
    RET(ADP5360_set_vbus_ilim(c->vbus_ilim.vadpichg_mV, vsys, c->vbus_ilim.ilim_mA));

    RET(ADP5360_set_chg_term(c->term.vtrm_mV, (uint16_t)(c->term.itrk_dead_mA * 10))); // mA -> dmA
    RET(ADP5360_set_chg_current_dmA((uint16_t)(c->curr.iend_mA * 10),
                                    (uint16_t)(c->curr.ichg_mA * 10)));
    RET(ADP5360_set_voltage_thresholds(c->vth.dis_rch,
                                       c->vth.vrch_mV,
                                       c->vth.vtrk_dead_mV,
                                       c->vth.vweak_mV));
    RET(ADP5360_set_chg_timers(c->tmr.en_tend, c->tmr.en_chg_timer, c->tmr.period_sel));
    {
        // ADP5360_init_t embeds a "func" block; convert to the typed API struct.
        const ADP5360_func_t func = {
            .en_jeita        = c->func.en_jeita,
            .ilim_jeita_cool = c->func.ilim_jeita_cool_10pct,
            .off_isofet      = c->func.off_isofet,
            .en_ldo          = c->func.en_ldo,
            .en_eoc          = c->func.en_eoc,
            .en_adpichg      = c->func.en_adpichg,
            .en_chg          = c->func.en_chg,
        };
        RET(ADP5360_set_chg_function(&func));
    }

    // --- THERMISTOR (NTC) ---
    RET(ADP5360_set_ntc_ctrl(_map_ithr_uA(c->thr_ctrl.ithr_uA), c->thr_ctrl.en_thr));
    RET(ADP5360_set_ntc_thresholds(c->thr_limits.t60_mV, c->thr_limits.t45_mV,
                                   c->thr_limits.t10_mV, c->thr_limits.t0_mV));

    // --- BATTERY PROTECTION ---
    {
        // ADP5360_init_t embeds a "batpro_ctrl" block; convert to the typed API struct.
        const ADP5360_batpro_ctrl_t batpro = {
            .en_batpro     = c->batpro_ctrl.en_batpro,
            .en_chglb      = c->batpro_ctrl.en_chglb,
            .oc_chg_hiccup = c->batpro_ctrl.chg_hiccup,
            .oc_dis_hiccup = c->batpro_ctrl.dis_hiccup,
            .isofet_ovchg  = c->batpro_ctrl.isofet_ovchg,
        };
        RET(ADP5360_set_batpro_ctrl(&batpro));
    }
    RET(ADP5360_set_uv_setting(c->uv_prot.vth_mV, c->uv_prot.hys_pct, c->uv_prot.dgt_ms));
    RET(ADP5360_set_dis_oc(c->dis_oc.oc_mA, c->dis_oc.dgt_ms));
    RET(ADP5360_set_ov_setting(c->ov_prot.vth_mV, c->ov_prot.hys_pct, c->ov_prot.dgt_ms));
    RET(ADP5360_set_chg_oc(c->chg_oc.oc_mA, c->chg_oc.dgt_ms));

    // --- FUEL GAUGE ---
    RET(ADP5360_set_bat_capacity(c->bat_cap_mAh));
    RET(ADP5360_set_socaccum_ctl(&c->socacm));
    RET(ADP5360_set_fg_mode(&c->fg_mode));
    (void)ADP5360_fg_refresh(); // best-effort refresh (ignore error)

    // --- BUCK ---
    RET(ADP5360_set_buck_vout(c->buck_vout.vout_mV, c->buck_vout.dly_us));
    RET(ADP5360_set_buck(&c->buck_cfg));

    // --- BUCK-BOOST ---
    RET(ADP5360_set_buckboost_vout(c->buckbst_vout.vout_mV, c->buckbst_vout.dly_us));
    RET(ADP5360_set_buckboost(&c->buckbst_cfg));

    // --- SUPERVISORY / PGOOD / IRQ ---
    RET(ADP5360_set_supervisory(&c->supv));
    RET(ADP5360_set_pgood1_mask(&c->pg1));
    RET(ADP5360_set_irq_enable(&c->irq_en));

    return HAL_OK;
}






