#ifndef DISPLAY_RENDERER_H
#define DISPLAY_RENDERER_H

#pragma once

#include "LS013B7DH05.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  RENDER_LAYER_UI = 0,
  RENDER_LAYER_GAME = 1,
  RENDER_LAYER_BG = 2
} render_layer_t;

typedef enum
{
  RENDER_STATE_TRANSPARENT = 0,
  RENDER_STATE_BLACK = 1,
  RENDER_STATE_WHITE = 2
} render_state_t;

typedef enum
{
  RENDER_ROTATION_0 = 0,
  RENDER_ROTATION_90_CW = 1,
  RENDER_ROTATION_180 = 2,
  RENDER_ROTATION_270_CW = 3
} render_rotation_t;

void renderInit(void);
const uint8_t *renderGetBuffer(void);
bool renderTakeDirtyRows(uint16_t *rows, uint16_t max_rows, uint16_t *out_count, bool *out_full);
void renderMarkDirtyRows(uint16_t start_row, uint16_t end_row);
void renderMarkDirtyList(const uint16_t *rows, uint16_t row_count);

void renderSetRotation(render_rotation_t rotation);
render_rotation_t renderGetRotation(void);
uint16_t renderGetWidth(void);
uint16_t renderGetHeight(void);

void renderFill(bool fill);
void renderInvert(void);
void renderFillRows(uint16_t start_row, uint16_t end_row, bool fill);
void renderInvertRows(uint16_t start_row, uint16_t end_row);
void renderSetPixel(uint16_t x, uint16_t y, render_layer_t layer, render_state_t state);
void renderDrawHLine(uint16_t x, uint16_t y, uint16_t length, render_layer_t layer, render_state_t state);
void renderDrawVLine(uint16_t x, uint16_t y, uint16_t length, render_layer_t layer, render_state_t state);
void renderFillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, render_layer_t layer, render_state_t state);
void renderDrawRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, render_layer_t layer, render_state_t state);
void renderDrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, render_layer_t layer, render_state_t state);
void renderDrawLineThick(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t thickness,
                         render_layer_t layer, render_state_t state);
void renderDrawCircle(uint16_t x0, uint16_t y0, uint16_t radius, render_layer_t layer, render_state_t state);
void renderDrawCircleThick(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t thickness, render_layer_t layer,
                           render_state_t state);
void renderFillCircle(uint16_t x0, uint16_t y0, uint16_t radius, render_layer_t layer, render_state_t state);
void renderDrawChar(uint16_t x, uint16_t y, char ch, render_layer_t layer, render_state_t fg);
void renderDrawText(uint16_t x, uint16_t y, const char *text, render_layer_t layer, render_state_t fg);
void renderBlit1bpp(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t *data,
                    uint16_t stride_bytes, render_layer_t layer, render_state_t fg);

#ifdef __cplusplus
}
#endif

#endif
