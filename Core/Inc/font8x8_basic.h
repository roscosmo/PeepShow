#ifndef FONT8X8_BASIC_H
#define FONT8X8_BASIC_H

// Font size definitions
#define FONT8X8_WIDTH 8
#define FONT8X8_HEIGHT 8
#define FONT8X8_START_CHAR 0x20  // Starting character (U+0020, space)
#define FONT8X8_END_CHAR 0x7F    // Ending character (U+007F)

// External declaration of the font array
extern const unsigned char font8x8_basic[128][8];

#endif /* FONT8X8_BASIC_H */
