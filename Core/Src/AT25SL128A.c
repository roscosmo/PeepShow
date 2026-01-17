#include "AT25sl128a.h"
#include <string.h>

/* ---------- AT25SL128A opcodes ---------- */
#define AT25_CMD_RDID       0x9F
#define AT25_CMD_RDSR1      0x05
#define AT25_CMD_RDSR2      0x35
#define AT25_CMD_WRSR2      0x31
#define AT25_CMD_WREN       0x06
#define AT25_CMD_WRDI       0x04
#define AT25_CMD_ENRST      0x66
#define AT25_CMD_RST        0x99
#define AT25_CMD_EQPI       0x38      // Enable QPI (enter 4-4-4)
#define AT25_CMD_RQPI       0xF5      // Exit QPI (send while in QPI)

/* ---------- Status register bits ---------- */
#define AT25_SR1_WIP        0x01
#define AT25_SR1_WEL        0x02
#define AT25_SR2_QE         0x02

/* ---------- Local defaults ---------- */
#define AT25_DEFAULT_TIMEOUT_MS  100


#ifndef AT25_CMD_READ
#define AT25_CMD_READ    0x0B    /* Fast Read (8 dummy cycles) */
#endif
#ifndef AT25_CMD_PP
#define AT25_CMD_PP      0x02    /* Page Program */
#endif
#ifndef AT25_CMD_SE4K
#define AT25_CMD_SE4K    0x20    /* 4 KiB Sector Erase */
#endif

#define AT25_PAGE_SIZE   256u

/* Tracks whether the device is left in 4-4-4 QPI mode by BootInit. */
static bool     s_qpi_mode   = false;
/* Dummy cycles for read (fast read default is 8). You can override in BootInit if desired. */
static uint8_t  s_read_dummy = 8;






/* Small helpers to keep the regular command setup tidy */
static HAL_StatusTypeDef cmd_only(OSPI_HandleTypeDef *h,
                                  uint8_t instr,
                                  uint32_t instr_lines)                        // HAL_OSPI_INSTRUCTION_*_LINE(S)
{
    OSPI_RegularCmdTypeDef c;
    memset(&c, 0, sizeof(c));
    c.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;
    c.FlashId            = HAL_OSPI_FLASH_ID_1;
    c.Instruction        = instr;
    c.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;
    c.InstructionMode    = instr_lines;
    c.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;
    c.AddressMode        = HAL_OSPI_ADDRESS_NONE;
    c.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
    c.DataMode           = HAL_OSPI_DATA_NONE;
    c.DummyCycles        = 0;
    c.NbData             = 0;
    c.DQSMode            = HAL_OSPI_DQS_DISABLE;
    return HAL_OSPI_Command(h, &c, HAL_MAX_DELAY);
}

static HAL_StatusTypeDef read_bytes(OSPI_HandleTypeDef *h,
                                    uint8_t instr,
                                    uint8_t *buf, uint32_t n,
                                    uint32_t instr_lines, uint32_t data_lines,
                                    uint8_t dummy_cycles)
{
    OSPI_RegularCmdTypeDef c;
    memset(&c, 0, sizeof(c));
    c.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;
    c.FlashId            = HAL_OSPI_FLASH_ID_1;
    c.Instruction        = instr;
    c.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;
    c.InstructionMode    = instr_lines;
    c.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;
    c.AddressMode        = HAL_OSPI_ADDRESS_NONE;
    c.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
    c.DataMode           = data_lines;
    c.DummyCycles        = dummy_cycles;
    c.NbData             = n;
    c.DQSMode            = HAL_OSPI_DQS_DISABLE;

    HAL_StatusTypeDef st = HAL_OSPI_Command(h, &c, HAL_MAX_DELAY);
    if (st != HAL_OK) return st;
    return HAL_OSPI_Receive(h, buf, HAL_MAX_DELAY);
}

static HAL_StatusTypeDef write_bytes(OSPI_HandleTypeDef *h,
                                     uint8_t instr,
                                     const uint8_t *buf, uint32_t n,
                                     uint32_t instr_lines, uint32_t data_lines)
{
    OSPI_RegularCmdTypeDef c;
    memset(&c, 0, sizeof(c));
    c.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;
    c.FlashId            = HAL_OSPI_FLASH_ID_1;
    c.Instruction        = instr;
    c.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;
    c.InstructionMode    = instr_lines;
    c.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;
    c.AddressMode        = HAL_OSPI_ADDRESS_NONE;
    c.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
    c.DataMode           = data_lines;
    c.DummyCycles        = 0;
    c.NbData             = n;
    c.DQSMode            = HAL_OSPI_DQS_DISABLE;

    HAL_StatusTypeDef st = HAL_OSPI_Command(h, &c, HAL_MAX_DELAY);
    if (st != HAL_OK) return st;
    return HAL_OSPI_Transmit(h, (uint8_t*)buf, HAL_MAX_DELAY);
}

/* Poll SR1 until (SR1 & mask) == match */
static HAL_StatusTypeDef poll_sr1(OSPI_HandleTypeDef *h,
                                  uint8_t mask, uint8_t match,
                                  uint32_t instr_lines, uint32_t data_lines,
                                  uint32_t timeout_ms)
{
    OSPI_RegularCmdTypeDef c;
    memset(&c, 0, sizeof(c));
    c.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;
    c.FlashId            = HAL_OSPI_FLASH_ID_1;
    c.Instruction        = AT25_CMD_RDSR1;
    c.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;
    c.InstructionMode    = instr_lines;
    c.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;
    c.AddressMode        = HAL_OSPI_ADDRESS_NONE;
    c.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
    c.DataMode           = data_lines;     // read 1 byte
    c.DummyCycles        = 0;
    c.NbData             = 1;
    c.DQSMode            = HAL_OSPI_DQS_DISABLE;

    OSPI_AutoPollingTypeDef p;
    memset(&p, 0, sizeof(p));
    p.Match            = match;
    p.Mask             = mask;
    p.MatchMode        = HAL_OSPI_MATCH_MODE_AND;
    p.Interval         = 0x10;
    p.AutomaticStop    = HAL_OSPI_AUTOMATIC_STOP_ENABLE;

    HAL_StatusTypeDef st = HAL_OSPI_Command(h, &c, HAL_MAX_DELAY);
    if (st != HAL_OK) return st;
    return HAL_OSPI_AutoPolling(h, &p, timeout_ms);
}

static HAL_StatusTypeDef wait_wip0(OSPI_HandleTypeDef *h,
                                   uint32_t instr_lines, uint32_t data_lines,
                                   uint32_t timeout_ms)
{
    // Wait until (SR1 & WIP) == 0
    return poll_sr1(h, AT25_SR1_WIP, 0x00, instr_lines, data_lines, timeout_ms);
}

static HAL_StatusTypeDef write_enable(OSPI_HandleTypeDef *h,
                                      uint32_t instr_lines, uint32_t data_lines,
                                      uint32_t timeout_ms)
{
    HAL_StatusTypeDef st = cmd_only(h, AT25_CMD_WREN, instr_lines);
    if (st != HAL_OK) return st;
    // Confirm WEL == 1
    return poll_sr1(h, AT25_SR1_WEL, AT25_SR1_WEL,
                    instr_lines, data_lines, timeout_ms);
}

/* ---------- Small public helpers ---------- */
HAL_StatusTypeDef AT25_ReadSR1(OSPI_HandleTypeDef *hospi, uint8_t *sr1)
{
    return read_bytes(hospi, AT25_CMD_RDSR1, sr1, 1,
                      HAL_OSPI_INSTRUCTION_1_LINE, HAL_OSPI_DATA_1_LINE, 0);
}

HAL_StatusTypeDef AT25_ReadSR2(OSPI_HandleTypeDef *hospi, uint8_t *sr2)
{
    return read_bytes(hospi, AT25_CMD_RDSR2, sr2, 1,
                      HAL_OSPI_INSTRUCTION_1_LINE, HAL_OSPI_DATA_1_LINE, 0);
}

HAL_StatusTypeDef AT25_WriteSR2(OSPI_HandleTypeDef *hospi, uint8_t value)
{
    HAL_StatusTypeDef st = write_enable(hospi,
                 HAL_OSPI_INSTRUCTION_1_LINE, HAL_OSPI_DATA_1_LINE, 50);
    if (st != HAL_OK) return st;

    st = write_bytes(hospi, AT25_CMD_WRSR2, &value, 1,
                     HAL_OSPI_INSTRUCTION_1_LINE, HAL_OSPI_DATA_1_LINE);
    if (st != HAL_OK) return st;

    return wait_wip0(hospi,
                     HAL_OSPI_INSTRUCTION_1_LINE, HAL_OSPI_DATA_1_LINE, 200);
}

/* ---------- The main init ---------- */
bool AT25_BootInit(OSPI_HandleTypeDef *hospi, const AT25_InitCfg *cfg, uint32_t *jedec_id)
{
    const uint32_t tmo = (cfg && cfg->timeout_ms) ? cfg->timeout_ms : AT25_DEFAULT_TIMEOUT_MS;
    AT25_BusMode target_mode = (cfg) ? cfg->bus_mode : AT25_BUS_SPI;

    /* 0) tiny power-up settle (covers tPUW etc.). */
    HAL_Delay(2);

    /* We start in SPI (1-1-1). Any reset must be sent in SPI. */
    uint32_t instr_lines = HAL_OSPI_INSTRUCTION_1_LINE;
    uint32_t data_lines  = HAL_OSPI_DATA_1_LINE;

    /* 1) Force clean power-up state: 66h + 99h. */
    if (cmd_only(hospi, AT25_CMD_ENRST, instr_lines) != HAL_OK) return false;
    if (cmd_only(hospi, AT25_CMD_RST,   instr_lines) != HAL_OK) return false;

    /* 2) Wait until WIP=0 (device ready after reset). */
    if (wait_wip0(hospi, instr_lines, data_lines, tmo) != HAL_OK) return false;

    /* 3) Read JEDEC ID to verify basic connectivity. */
    uint8_t id[3] = {0};
    if (read_bytes(hospi, AT25_CMD_RDID, id, 3,
                   instr_lines, data_lines, 0) != HAL_OK) return false;
    if (jedec_id) {
        *jedec_id = ((uint32_t)id[0] << 16) | ((uint32_t)id[1] << 8) | id[2];
    }

    /* 4) If staying in SPI, we're done. */
    if (target_mode == AT25_BUS_SPI) return true;

    /* 5) Ensure QE=1 (needed for any Quad I/O). */
    uint8_t sr2 = 0;
    if (AT25_ReadSR2(hospi, &sr2) != HAL_OK) return false;
    if ((sr2 & AT25_SR2_QE) == 0) {
        if (AT25_WriteSR2(hospi, (uint8_t)(sr2 | AT25_SR2_QE)) != HAL_OK) return false;
        // Re-read to confirm
        if (AT25_ReadSR2(hospi, &sr2) != HAL_OK) return false;
        if ((sr2 & AT25_SR2_QE) == 0) return false;
    }

    /* 6) For QPI request, switch bus to 4-4-4. (Send EQPI while still in SPI.) */
    if (target_mode == AT25_BUS_4_4_4) {
        if (cmd_only(hospi, AT25_CMD_EQPI, HAL_OSPI_INSTRUCTION_1_LINE) != HAL_OK)
            return false;

        // From this point, the device expects 4-line instructions/data.
        instr_lines = HAL_OSPI_INSTRUCTION_4_LINES;
        data_lines  = HAL_OSPI_DATA_4_LINES;

        // Optional: sanity read of SR1 in QPI to prove the mode switch worked.
        if (poll_sr1(hospi, AT25_SR1_WIP, 0x00, instr_lines, data_lines, tmo) != HAL_OK)
            return false;
    }

    /* Note about dummy cycles:
       AT25SL128A exposes "Set Read Parameters (C0h)" to change its *internal*
       expectations for some continuous/wrap reads. For standard HAL reads you
       can usually just program c.DummyCycles per command. If you later use a
       continuous 1-4-4 or 4-4-4 XIP-like read and need to change the flash's
       internal dummy count, send 0xC0 with the proper parameter byte. */

    (void)cfg; // read_dummy may be used in your later READ commands
    // inside AT25_BootInit(...) right before the final 'return true;'
    if (cfg) {
        s_read_dummy = cfg->read_dummy ? cfg->read_dummy : 8;
    }
    s_qpi_mode = (target_mode == AT25_BUS_4_4_4);  // reflect the chosen bus mode

    return true;
}


/* Addressed READ (supports SPI 1-1-1 and QPI 4-4-4) */
HAL_StatusTypeDef AT25_Read(OSPI_HandleTypeDef *h, uint32_t addr,
                            uint8_t *dst, uint32_t n)
{
    if (n == 0) return HAL_OK;

    uint32_t il = s_qpi_mode ? HAL_OSPI_INSTRUCTION_4_LINES : HAL_OSPI_INSTRUCTION_1_LINE;
    uint32_t al = s_qpi_mode ? HAL_OSPI_ADDRESS_4_LINES     : HAL_OSPI_ADDRESS_1_LINE;
    uint32_t dl = s_qpi_mode ? HAL_OSPI_DATA_4_LINES        : HAL_OSPI_DATA_1_LINE;

    OSPI_RegularCmdTypeDef c;
    memset(&c, 0, sizeof(c));
    c.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;
    c.FlashId            = HAL_OSPI_FLASH_ID_1;
    c.Instruction        = AT25_CMD_READ;                 // 0x0B fast-read
    c.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;
    c.InstructionMode    = il;
    c.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;
    c.AddressMode        = al;
    c.AddressSize        = HAL_OSPI_ADDRESS_24_BITS;
    c.Address            = addr;
    c.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
    c.DataMode           = dl;
    c.DummyCycles        = s_read_dummy;                  // default 8
    c.NbData             = n;
    c.DQSMode            = HAL_OSPI_DQS_DISABLE;

    HAL_StatusTypeDef st = HAL_OSPI_Command(h, &c, HAL_MAX_DELAY);
    if (st != HAL_OK) return st;
    return HAL_OSPI_Receive(h, dst, HAL_MAX_DELAY);
}

/* Page Program with automatic page-splitting (WREN + wait WIP=0 each chunk) */
HAL_StatusTypeDef AT25_PageProgram(OSPI_HandleTypeDef *h, uint32_t addr,
                                   const uint8_t *src, uint32_t n)
{
    if (n == 0) return HAL_OK;

    uint32_t il = s_qpi_mode ? HAL_OSPI_INSTRUCTION_4_LINES : HAL_OSPI_INSTRUCTION_1_LINE;
    uint32_t al = s_qpi_mode ? HAL_OSPI_ADDRESS_4_LINES     : HAL_OSPI_ADDRESS_1_LINE;
    uint32_t dl = s_qpi_mode ? HAL_OSPI_DATA_4_LINES        : HAL_OSPI_DATA_1_LINE;

    while (n) {
        uint32_t page_off = addr & (AT25_PAGE_SIZE - 1u);
        uint32_t chunk    = AT25_PAGE_SIZE - page_off;
        if (chunk > n) chunk = n;

        HAL_StatusTypeDef st = write_enable(h, il, dl, 50);
        if (st != HAL_OK) return st;

        OSPI_RegularCmdTypeDef c;
        memset(&c, 0, sizeof(c));
        c.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;
        c.FlashId            = HAL_OSPI_FLASH_ID_1;
        c.Instruction        = AT25_CMD_PP;               // 0x02
        c.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;
        c.InstructionMode    = il;
        c.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;
        c.AddressMode        = al;
        c.AddressSize        = HAL_OSPI_ADDRESS_24_BITS;
        c.Address            = addr;
        c.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
        c.DataMode           = dl;
        c.DummyCycles        = 0;
        c.NbData             = chunk;
        c.DQSMode            = HAL_OSPI_DQS_DISABLE;

        st = HAL_OSPI_Command(h, &c, HAL_MAX_DELAY);
        if (st != HAL_OK) return st;
        st = HAL_OSPI_Transmit(h, (uint8_t*)src, HAL_MAX_DELAY);
        if (st != HAL_OK) return st;

        // Program time can vary; be generous.
        st = wait_wip0(h, il, dl, 400);   // ~ms per page; adjust if needed
        if (st != HAL_OK) return st;

        addr += chunk;
        src  += chunk;
        n    -= chunk;
    }
    return HAL_OK;
}

/* 4 KiB sector erase (addr should be 4K-aligned) */
HAL_StatusTypeDef AT25_Erase4K(OSPI_HandleTypeDef *h, uint32_t addr)
{
    uint32_t il = s_qpi_mode ? HAL_OSPI_INSTRUCTION_4_LINES : HAL_OSPI_INSTRUCTION_1_LINE;
    uint32_t al = s_qpi_mode ? HAL_OSPI_ADDRESS_4_LINES     : HAL_OSPI_ADDRESS_1_LINE;
    uint32_t dl = s_qpi_mode ? HAL_OSPI_DATA_4_LINES        : HAL_OSPI_DATA_1_LINE;

    HAL_StatusTypeDef st = write_enable(h, il, dl, 50);
    if (st != HAL_OK) return st;

    OSPI_RegularCmdTypeDef c;
    memset(&c, 0, sizeof(c));
    c.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;
    c.FlashId            = HAL_OSPI_FLASH_ID_1;
    c.Instruction        = AT25_CMD_SE4K;                 // 0x20
    c.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;
    c.InstructionMode    = il;
    c.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;
    c.AddressMode        = al;
    c.AddressSize        = HAL_OSPI_ADDRESS_24_BITS;
    c.Address            = addr;
    c.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
    c.DataMode           = HAL_OSPI_DATA_NONE;            // no data payload
    c.DummyCycles        = 0;
    c.NbData             = 0;
    c.DQSMode            = HAL_OSPI_DQS_DISABLE;

    st = HAL_OSPI_Command(h, &c, HAL_MAX_DELAY);
    if (st != HAL_OK) return st;

    // 4K erase is slow; several hundred ms up to a few seconds. Use a roomy timeout.
    return wait_wip0(h, il, dl, 3000);
}