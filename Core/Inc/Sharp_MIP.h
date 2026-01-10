#ifndef SHARP_MIP_H
#define SHARP_MIP_H

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

extern uint8_t *DispBuf;

typedef struct {
    SPI_HandleTypeDef *Bus;
    GPIO_TypeDef      *dispGPIO;
    uint16_t           LCDcs;     /* SCS pin (ACTIVE HIGH) */
} Sharp_MIP;

/* Init / clear */
HAL_StatusTypeDef LCD_Init(Sharp_MIP *MemDisp,
                           SPI_HandleTypeDef *Bus,
                           GPIO_TypeDef *dispGPIO,
                           uint16_t LCDcs);

HAL_StatusTypeDef LCD_Clean(Sharp_MIP *MemDisp);

/* Blocking flush */
HAL_StatusTypeDef LCD_FlushAll(Sharp_MIP *MemDisp);
HAL_StatusTypeDef LCD_FlushRows(Sharp_MIP *MemDisp, const uint16_t *rows, uint16_t rowCount);

/* DMA chunked flush (LPDMA) */
HAL_StatusTypeDef LCD_FlushAll_DMA(Sharp_MIP *MemDisp);
HAL_StatusTypeDef LCD_FlushRows_DMA(Sharp_MIP *MemDisp, const uint16_t *rows, uint16_t rowCount);

bool              LCD_FlushDMA_IsDone(void);
HAL_StatusTypeDef LCD_FlushDMA_WaitWFI(uint32_t timeout_ms);

/* Buffer ops */
void LCD_LoadFull(const uint8_t *BMP);
void LCD_BufClean(void);
void LCD_Invert(void);
void LCD_Fill(bool fill);

#endif
