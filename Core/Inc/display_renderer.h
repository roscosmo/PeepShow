#ifndef DISPLAY_RENDERER_H
#define DISPLAY_RENDERER_H

#pragma once

#include "LS013B7DH05.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void renderInit(void);
const uint8_t *renderGetBuffer(void);
bool renderTakeDirtySpan(uint16_t *start_row, uint16_t *end_row);
void renderMarkDirtyRows(uint16_t start_row, uint16_t end_row);

void renderFill(bool fill);
void renderInvert(void);
void renderFillRows(uint16_t start_row, uint16_t end_row, bool fill);
void renderInvertRows(uint16_t start_row, uint16_t end_row);

#ifdef __cplusplus
}
#endif

#endif
