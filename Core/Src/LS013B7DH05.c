/*
 * LS013B7DH05.c
 *
 * Sharp Memory LCD driver for LS013B7DH05 (144x168 class panel).
 *
 * Buffer contract (important)
 * ---------------------------
 * This driver expects the framebuffer payload to already be in panel byte order:
 *   - LINE_WIDTH bytes per row
 *   - within each byte, bit (x&7) corresponds to pixel x (LSB-left)
 *
 * That matches the packing performed by display_renderer.c (pack_row()).
 *
 * EXTCOM
 * ------
 * EXTCOM toggling (to prevent DC bias) is handled externally by the application.
 */

#include "LS013B7DH05.h"
#include <string.h>
#include <stdbool.h>

/*
 * Panel: 144 x 168 (LS013B7DH05 class)
 *
 * Stream format (EXTCOM handled externally):
 *   [WRITE_CMD = 0x01]
 *   repeat per line:
 *     [GATE_ADDR (1..168)]
 *     [LINE_DATA (LINE_WIDTH bytes)]
 *     [DUMMY 0x00]  (8 clocks)
 *   end:
 *     [DUMMY 0x00]  (extra 8 clocks; total 16 after last line)
 *
 * IMPORTANT CHANGE:
 *   Buffer is stored in PANEL BYTE ORDER already.
 *   => flush does NOT do rev8() on pixel bytes.
 *
 * SPI:
 *   - 8-bit
 *   - CPOL=0, CPHA=1Edge
 *   - FirstBit = LSB (recommended, matches Sharp MIP examples)
 *   - CS is ACTIVE HIGH and must stay HIGH during the whole update stream
 */

#define MLCD_CMD_WRITE   (0x01u)
#define MLCD_CMD_CLEAR   (0x04u)

#define SPI_TIMEOUT_MS   (150u)

/* STM32U5 SPI TSIZE practical limit per HAL transmit/DMA chunk */
#define SPI_TX_CHUNK_MAX (255u)

/* Full-screen stream bytes:
 * 1 + H*(addr + data + dummy) + 1
 * = 1 + 168*(1+18+1) + 1 = 3362
 */
#define TXBUF_MAX (1u + (DISPLAY_HEIGHT * (LINE_WIDTH + 2u)) + 1u)

/* Put txBuf in SRAM4 for LPDMA */
#if defined(__GNUC__)
  #define SRAM4_BUF_ATTR __attribute__((section(".sram4"))) __attribute__((aligned(4)))
#elif defined(__ICCARM__)
  #define SRAM4_BUF_ATTR __attribute__((section(".sram4"))) __attribute__((aligned(4)))
#else
  #define SRAM4_BUF_ATTR
#endif

static uint8_t txBuf[TXBUF_MAX] SRAM4_BUF_ATTR;

static inline void SCS_High(LS013B7DH05 *d) { HAL_GPIO_WritePin(d->dispGPIO, d->LCDcs, GPIO_PIN_SET); }
static inline void SCS_Low (LS013B7DH05 *d) { HAL_GPIO_WritePin(d->dispGPIO, d->LCDcs, GPIO_PIN_RESET); }

/* --------------------------- Blocking chunked TX --------------------------- */
/**
 * @brief Transmit a buffer over SPI in smaller chunks.
 *
 * Some STM32 HAL configurations (and some DMA engines) have practical limits on
 * a single transmit length. This helper splits the transfer into
 * SPI_TX_CHUNK_MAX sized pieces and performs blocking transmits.
 */

/*=============================================================================
 * Internal helpers
 *============================================================================*/

static HAL_StatusTypeDef spi_tx_chunked(LS013B7DH05 *d, const uint8_t *buf, uint32_t len)
{
    while (len > 0u) {
        uint16_t chunk = (len > SPI_TX_CHUNK_MAX) ? (uint16_t)SPI_TX_CHUNK_MAX : (uint16_t)len;
        HAL_StatusTypeDef st = HAL_SPI_Transmit(d->Bus, (uint8_t*)buf, chunk, SPI_TIMEOUT_MS);
        if (st != HAL_OK) return st;
        buf += chunk;
        len -= chunk;
    }
    return HAL_OK;
}

/* --------------------------- Build write burst ----------------------------- */
/* rows[] are 1-based gate lines (1..DISPLAY_HEIGHT). */
/**
 * @brief Build a Sharp Memory LCD "write" stream into the internal txBuf.
 *
 * The resulting buffer contains the protocol framing bytes (WRITE command,
 * per-row gate address, per-row trailing dummy, final dummy) wrapped around the
 * caller-provided packed framebuffer rows.
 *
 * @param buf      Packed framebuffer base pointer (row-major, LINE_WIDTH bytes/row).
 * @param rows     List of 1-based gate line numbers to include.
 * @param rowCount Number of entries in @p rows.
 * @param[out] outLen Length in bytes of the constructed stream.
 */

/*=============================================================================
 * Write-stream construction
 *============================================================================*/

static HAL_StatusTypeDef BuildWriteBurst(const uint8_t *buf, const uint16_t *rows,
                                         uint16_t rowCount, uint16_t *outLen)
{
    if (!outLen || !buf || !rows || rowCount == 0u) return HAL_ERROR;
    *outLen = 0;

    uint32_t needed = 1u + ((uint32_t)rowCount * (1u + LINE_WIDTH + 1u)) + 1u;
    if (needed > TXBUF_MAX) return HAL_ERROR;

    uint32_t w = 0u;
    txBuf[w++] = MLCD_CMD_WRITE;

    for (uint16_t i = 0; i < rowCount; i++) {
        uint16_t r = rows[i];
        if (r < 1u || r > DISPLAY_HEIGHT) return HAL_ERROR;

        txBuf[w++] = (uint8_t)r;

        uint32_t offset = (uint32_t)(r - 1u) * LINE_WIDTH;

        /* NO rev8(): buffer is already in panel order */
        memcpy(&txBuf[w], &buf[offset], LINE_WIDTH);
        w += LINE_WIDTH;

        txBuf[w++] = 0x00u; /* per-line dummy */
    }

    txBuf[w++] = 0x00u; /* final dummy */
    *outLen = (uint16_t)w;
    return HAL_OK;
}

/* --------------------------- Public: init/clean ---------------------------- */
/**
 * @brief Initialize an LS013B7DH05 device instance.
 *
 * This does not start EXTCOM (handled externally per the file header note).
 * It configures the device handle and performs any required one-time GPIO/SPI
 * setup that belongs to this driver.
 */
HAL_StatusTypeDef LCD_Init(LS013B7DH05 *MemDisp,
                           SPI_HandleTypeDef *Bus,
                           GPIO_TypeDef *dispGPIO,
                           uint16_t LCDcs)
{
    if (!MemDisp || !Bus || !dispGPIO) return HAL_ERROR;

    MemDisp->Bus      = Bus;
    MemDisp->dispGPIO = dispGPIO;
    MemDisp->LCDcs    = LCDcs;

    return LCD_Clean(MemDisp);
}

/**
 * @brief Issue the panel "clear" command (all pixels white).
 *
 * Uses MLCD_CMD_CLEAR followed by a dummy byte, per Sharp Memory LCD protocol.
 * This is a direct panel command; it does not use the framebuffer.
 */

/*=============================================================================
 * Public API: blocking (polling) transfers
 *============================================================================*/

HAL_StatusTypeDef LCD_Clean(LS013B7DH05 *MemDisp)
{
    if (!MemDisp) return HAL_ERROR;

    uint8_t clearSeq[2] = { MLCD_CMD_CLEAR, 0x00u };

    SCS_High(MemDisp);
    HAL_StatusTypeDef st = HAL_SPI_Transmit(MemDisp->Bus, clearSeq, sizeof(clearSeq), SPI_TIMEOUT_MS);
    SCS_Low(MemDisp);

    return st;
}

/* --------------------------- Public: blocking flush ------------------------ */
/**
 * @brief Flush the entire framebuffer to the panel using blocking SPI.
 *
 * @param MemDisp Device handle.
 * @param buf     Pointer to packed framebuffer payload: DISPLAY_HEIGHT rows
 *                of LINE_WIDTH bytes each, already in panel byte order.
 */
HAL_StatusTypeDef LCD_FlushAll(LS013B7DH05 *MemDisp, const uint8_t *buf)
{
    if (!MemDisp || !buf) return HAL_ERROR;

    static uint16_t allRows[DISPLAY_HEIGHT];
    for (uint16_t i = 0; i < DISPLAY_HEIGHT; i++) allRows[i] = (uint16_t)(i + 1u);

    uint16_t len = 0;
    HAL_StatusTypeDef st = BuildWriteBurst(buf, allRows, DISPLAY_HEIGHT, &len);
    if (st != HAL_OK) return st;

    SCS_High(MemDisp);
    st = spi_tx_chunked(MemDisp, txBuf, len);
    SCS_Low(MemDisp);

    return st;
}

/**
 * @brief Flush a subset of rows using blocking SPI.
 *
 * Rows are specified as 1-based gate lines (1..DISPLAY_HEIGHT). The caller
 * provides the full packed framebuffer; only the selected rows are serialized.
 */
HAL_StatusTypeDef LCD_FlushRows(LS013B7DH05 *MemDisp, const uint8_t *buf,
                                const uint16_t *rows, uint16_t rowCount)
{
    if (!MemDisp || !buf) return HAL_ERROR;

    uint16_t len = 0;
    HAL_StatusTypeDef st = BuildWriteBurst(buf, rows, rowCount, &len);
    if (st != HAL_OK) return st;

    SCS_High(MemDisp);
    st = spi_tx_chunked(MemDisp, txBuf, len);
    SCS_Low(MemDisp);

    return st;
}

/* --------------------------- DMA chunk chaining ---------------------------- */
typedef struct {
    LS013B7DH05     *dev;
    const uint8_t *p;
    uint32_t       remaining;
    HAL_StatusTypeDef last;
} lcd_dma_chain_t;

static volatile bool g_dma_done = true;
static lcd_dma_chain_t g_chain = {0};

bool LCD_FlushDMA_IsDone(void) { return g_dma_done; }

/**
 * @brief Weak hook called when a DMA flush finishes successfully.
 *
 * Override this in your application if you want an ISR-safe notification.
 */

/*=============================================================================
 * Public API hooks (weak callbacks)
 *============================================================================*/

__weak void LCD_FlushDmaDoneCallback(void)
{
}

/**
 * @brief Weak hook called if a DMA flush fails (SPI error callback path).
 *
 * Override this in your application if you want error reporting/telemetry.
 */
__weak void LCD_FlushDmaErrorCallback(void)
{
}

/**
 * @brief Start (or continue) the next DMA chunk in the current flush chain.
 *
 * Uses HAL_SPI_Transmit_DMA() on the active device and advances the chain.
 */

/*=============================================================================
 * DMA flush implementation (chunked chaining)
 *============================================================================*/

static HAL_StatusTypeDef lcd_dma_kick_next(void)
{
    if (!g_chain.dev) return HAL_ERROR;
    if (g_chain.remaining == 0u) return HAL_OK;

    uint16_t n = (g_chain.remaining > SPI_TX_CHUNK_MAX) ? (uint16_t)SPI_TX_CHUNK_MAX
                                                        : (uint16_t)g_chain.remaining;
    return HAL_SPI_Transmit_DMA(g_chain.dev->Bus, (uint8_t*)g_chain.p, n);
}

/**
 * @brief Begin a chunked DMA transmit sequence.
 *
 * Sets up global state used by HAL_SPI_TxCpltCallback()/HAL_SPI_ErrorCallback()
 * to chain multiple DMA transfers back-to-back until the full buffer is sent.
 */
static HAL_StatusTypeDef lcd_dma_start(LS013B7DH05 *dev, const uint8_t *buf, uint32_t len)
{
    if (!dev || !buf || len == 0u) return HAL_ERROR;
    if (!g_dma_done) return HAL_BUSY;

    g_chain.dev = dev;
    g_chain.p = buf;
    g_chain.remaining = len;
    g_chain.last = HAL_OK;

    g_dma_done = false;

    SCS_High(dev);

    HAL_StatusTypeDef st = lcd_dma_kick_next();
    if (st != HAL_OK) {
        SCS_Low(dev);
        g_chain.dev = NULL;
        g_dma_done = true;
        return st;
    }
    return HAL_OK;
}


/**
 * @brief HAL callback: called by STM32 HAL when a DMA SPI TX completes.
 *
 * If this completion belongs to the active LS013B7DH05 flush chain, the driver:
 *   - advances the buffer pointer
 *   - kicks the next DMA chunk (if any)
 *   - otherwise deasserts CS, marks done, and calls LCD_FlushDmaDoneCallback()
 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (!g_chain.dev || !g_chain.dev->Bus) return;
    if (hspi != g_chain.dev->Bus) return;

    uint32_t sent = (g_chain.remaining > SPI_TX_CHUNK_MAX) ? (uint32_t)SPI_TX_CHUNK_MAX : g_chain.remaining;

    g_chain.p += sent;
    g_chain.remaining -= sent;

    if (g_chain.remaining == 0u) {
        SCS_Low(g_chain.dev);
        g_chain.dev = NULL;
        g_dma_done = true;
        LCD_FlushDmaDoneCallback();
        return;
    }

    HAL_StatusTypeDef st = lcd_dma_kick_next();
    if (st != HAL_OK) {
        SCS_Low(g_chain.dev);
        g_chain.last = st;
        g_chain.dev = NULL;
        g_dma_done = true;
        LCD_FlushDmaErrorCallback();
    }
}


/**
 * @brief HAL callback: called by STM32 HAL on SPI error during DMA flush.
 *
 * If the error belongs to the active flush chain, CS is deasserted, the chain
 * is aborted, and LCD_FlushDmaErrorCallback() is invoked.
 */
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if (!g_chain.dev || !g_chain.dev->Bus) return;
    if (hspi != g_chain.dev->Bus) return;

    SCS_Low(g_chain.dev);
    g_chain.last = HAL_ERROR;
    g_chain.dev = NULL;
    g_dma_done = true;
    LCD_FlushDmaErrorCallback();
}

/**
 * @brief Flush the entire framebuffer to the panel using chunked DMA.
 *
 * Non-blocking: returns once the first DMA transfer has been kicked.
/**
 * @brief Wait for the current DMA flush to complete, sleeping with WFI.
 *
 * @param timeout_ms Timeout in milliseconds.
 * @return HAL_OK on completion, HAL_TIMEOUT if the timeout expires.
 */

/*=============================================================================
 * Public API: DMA (non-blocking) transfers
 *============================================================================*/

HAL_StatusTypeDef LCD_FlushAll_DMA(LS013B7DH05 *MemDisp, const uint8_t *buf)
{
    if (!MemDisp || !buf) return HAL_ERROR;

    static uint16_t allRows[DISPLAY_HEIGHT];
    for (uint16_t i = 0; i < DISPLAY_HEIGHT; i++) allRows[i] = (uint16_t)(i + 1u);

    uint16_t len = 0;
    HAL_StatusTypeDef st = BuildWriteBurst(buf, allRows, DISPLAY_HEIGHT, &len);
    if (st != HAL_OK) return st;

    return lcd_dma_start(MemDisp, txBuf, len);
}

/**
 * @brief Flush a subset of rows using chunked DMA.
 *
 * Builds the write stream into txBuf and then starts a chunked DMA chain.
 * Non-blocking: completion is reported via HAL callbacks and g_dma_done.
 */
HAL_StatusTypeDef LCD_FlushRows_DMA(LS013B7DH05 *MemDisp, const uint8_t *buf,
                                    const uint16_t *rows, uint16_t rowCount)
{
    if (!MemDisp || !buf) return HAL_ERROR;

    uint16_t len = 0;
    HAL_StatusTypeDef st = BuildWriteBurst(buf, rows, rowCount, &len);
    if (st != HAL_OK) return st;

    return lcd_dma_start(MemDisp, txBuf, len);
}


/*=============================================================================
 * Synchronization helpers
 *============================================================================*/

HAL_StatusTypeDef LCD_FlushDMA_WaitWFI(uint32_t timeout_ms)
{
    uint32_t t0 = HAL_GetTick();
    while (!g_dma_done) {
        __WFI();
        if ((HAL_GetTick() - t0) > timeout_ms) return HAL_TIMEOUT;
    }
    return HAL_OK;
}

