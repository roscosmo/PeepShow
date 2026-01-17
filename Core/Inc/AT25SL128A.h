#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "stm32u5xx_hal.h"     // brings in stm32u5xx_hal_ospi.h

#ifdef __cplusplus
extern "C" {
#endif

// --- Public configuration ----------------------------------------------------

typedef enum {
    AT25_BUS_SPI    = 0,   // 1-1-1 (default power-up state)
    AT25_BUS_1_4_4  = 1,   // 1-4-4 (Quad I/O commands – QE must be 1)
    AT25_BUS_4_4_4  = 2    // QPI (4-4-4) – QE=1 then 38h Enable QPI
} AT25_BusMode;

typedef struct {
    AT25_BusMode bus_mode;     // desired bus mode after init
    uint8_t      read_dummy;   // how many dummy cycles you'll use in your READ cmd
    uint32_t     timeout_ms;   // generic timeout for command/poll helpers
} AT25_InitCfg;

// --- Public API --------------------------------------------------------------

/**
 * Initializes AT25SL128A to a known state and optionally switches to Quad/QPI.
 * Sequence:
 *  1) small power-up delay
 *  2) 66h/99h reset
 *  3) wait until WIP=0
 *  4) read JEDEC ID (9Fh) and return it
 *  5) if cfg.bus_mode != AT25_BUS_SPI: set QE (SR2.QE=1)
 *  6) if cfg.bus_mode == AT25_BUS_4_4_4: send 38h (Enable QPI)
 *
 * @param hospi      HAL OSPI handle already initialized/clocks+IOs configured
 * @param cfg        init options (bus mode, dummy, timeout)
 * @param jedec_id   OUT: 0x00MMTTCC = [manufacturer|memtype|capacity]
 * @return true on success, false otherwise
 */
bool AT25_BootInit(OSPI_HandleTypeDef *hospi, const AT25_InitCfg *cfg, uint32_t *jedec_id);

// Optional helpers you may find handy later:
HAL_StatusTypeDef AT25_ReadSR1(OSPI_HandleTypeDef *hospi, uint8_t *sr1);
HAL_StatusTypeDef AT25_ReadSR2(OSPI_HandleTypeDef *hospi, uint8_t *sr2);
HAL_StatusTypeDef AT25_WriteSR2(OSPI_HandleTypeDef *hospi, uint8_t value);

HAL_StatusTypeDef AT25_Read(OSPI_HandleTypeDef*, uint32_t addr, uint8_t *dst, uint32_t n);
HAL_StatusTypeDef AT25_PageProgram(OSPI_HandleTypeDef*, uint32_t addr, const uint8_t *src, uint32_t n);
HAL_StatusTypeDef AT25_Erase4K(OSPI_HandleTypeDef*, uint32_t addr);


#ifdef __cplusplus
}
#endif
