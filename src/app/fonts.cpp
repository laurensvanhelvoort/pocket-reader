#include "fonts.h"

#include "fontdata/font_mono.h"
#include "fontdata/font_small.h"
#include "fontdata/font_title.h"
#include "fontdata/font_word.h"

SmoothFont fontWord;
SmoothFont fontTitle;
SmoothFont fontSmall;
SmoothFont fontMono;

const int kWordAscent = font_word_ascent;

bool SmoothFont::begin(const uint8_t* array) {
    data.set(array);
    return font.loadFont(&data);
}

bool loadFonts() {
    return fontWord.begin(font_word) && fontTitle.begin(font_title) && fontSmall.begin(font_small) &&
           fontMono.begin(font_mono);
}
