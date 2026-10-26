#pragma once

#include "display.h"

struct SmoothFont {
    lgfx::PointerWrapper data;
    lgfx::VLWfont font;
    bool begin(const uint8_t* array);
};

extern SmoothFont fontWord;
extern SmoothFont fontTitle;
extern SmoothFont fontSmall;
extern SmoothFont fontMono;

extern const int kWordAscent;

bool loadFonts();
