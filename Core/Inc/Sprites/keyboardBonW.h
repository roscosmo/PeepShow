#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// 1bpp bitmap for keyboardBonW.png (bit=1 => black pixel), row-major.
// Each row is padded to a whole number of bytes.
// Bit order within each byte: MSB first (x=0 is bit 7).
#define KEYBOARDBONW_WIDTH   ((uint16_t)139)
#define KEYBOARDBONW_HEIGHT  ((uint16_t)142)
#define KEYBOARDBONW_ROW_BYTES ((uint16_t)18)

extern const uint8_t keyboardBonW_bitmap[KEYBOARDBONW_ROW_BYTES * KEYBOARDBONW_HEIGHT];

typedef struct KeyboardBonW_Sprite {
    const char* name;
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
} KeyboardBonW_Sprite;

extern const KeyboardBonW_Sprite keyboardBonW_sprites[];
extern const size_t keyboardBonW_sprite_count;

// Optional helper: linear search by sprite name (case-sensitive). Returns NULL if not found.
const KeyboardBonW_Sprite* keyboardBonW_find_sprite(const char* name);

#ifdef __cplusplus
} // extern "C"
#endif
