#ifndef LS013B7DH05_H
#define LS013B7DH05_H

#pragma once
#include "stm32u5xx_hal.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Panel resolution */
#define DISPLAY_WIDTH   144u
#define DISPLAY_HEIGHT  168u

/* 1bpp, 8 pixels/byte */
#define LINE_WIDTH      (DISPLAY_WIDTH / 8u)           /* 18 bytes */
#define BUFFER_LENGTH   (DISPLAY_HEIGHT * LINE_WIDTH)  /* 3024 bytes */

typedef struct {
    SPI_HandleTypeDef *Bus;
    GPIO_TypeDef      *dispGPIO;
    uint16_t           LCDcs;     /* SCS pin (ACTIVE HIGH) */
} LS013B7DH05;

/* Init / clear */
HAL_StatusTypeDef LCD_Init(LS013B7DH05 *MemDisp,
                           SPI_HandleTypeDef *Bus,
                           GPIO_TypeDef *dispGPIO,
                           uint16_t LCDcs);

HAL_StatusTypeDef LCD_Clean(LS013B7DH05 *MemDisp);

/* Blocking flush */
HAL_StatusTypeDef LCD_FlushAll(LS013B7DH05 *MemDisp, const uint8_t *buf);
HAL_StatusTypeDef LCD_FlushRows(LS013B7DH05 *MemDisp, const uint8_t *buf,
                                const uint16_t *rows, uint16_t rowCount);

/* DMA chunked flush (LPDMA) */
HAL_StatusTypeDef LCD_FlushAll_DMA(LS013B7DH05 *MemDisp, const uint8_t *buf);
HAL_StatusTypeDef LCD_FlushRows_DMA(LS013B7DH05 *MemDisp, const uint8_t *buf,
                                    const uint16_t *rows, uint16_t rowCount);

bool              LCD_FlushDMA_IsDone(void);
HAL_StatusTypeDef LCD_FlushDMA_WaitWFI(uint32_t timeout_ms);

/* Optional DMA completion hooks (ISR context). */
void LCD_FlushDmaDoneCallback(void);
void LCD_FlushDmaErrorCallback(void);

#endif
