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
bool renderTakeDirtyRows(uint16_t *rows, uint16_t max_rows, uint16_t *out_count, bool *out_full);
void renderMarkDirtyRows(uint16_t start_row, uint16_t end_row);
void renderMarkDirtyList(const uint16_t *rows, uint16_t row_count);

void renderFill(bool fill);
void renderInvert(void);
void renderFillRows(uint16_t start_row, uint16_t end_row, bool fill);
void renderInvertRows(uint16_t start_row, uint16_t end_row);

#ifdef __cplusplus
}
#endif

#endif
