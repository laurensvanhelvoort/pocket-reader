#pragma once

#include "display.h"

extern Display lcd;
extern LGFX_Sprite frame;

const int kScreenW = 320;
const int kScreenH = 170;
const int kMargin = 8;
const int kTopTextY = 20;
const int kBottomTextY = 160;

const uint8_t kTickGrey = 100;
const uint8_t kInfoGrey = 140;
uint16_t grey(uint8_t level);
uint16_t gold(float level = 1.0f);

void initUi();
void setBacklight(int percent);

const int kTickLen = 15;
// Anti-aliased against black. dir = +1 points the tip down (V) or right (H).
void drawWedgeV(LovyanGFX& g, int x, int yWide, int dir);
void drawWedgeH(LovyanGFX& g, int y, int xWide, int dir);

void fitText(LovyanGFX& g, const char* text, int maxWidth, char* out, size_t cap);

enum class Corner { TopLeft, TopRight, BottomLeft, BottomRight };
void drawCorner(LovyanGFX& g, Corner corner, const char* text, uint16_t color);

const uint8_t kStyleAccent = 1;
const uint8_t kStyleDim = 2;
const uint8_t kStyleInfoAccent = 4;

class Wheel {
public:
    using TextFn = void (*)(int index, char* out, size_t cap);
    using StyleFn = uint8_t (*)(int index);

    void reset(int count, int selected);
    void scroll(int clicks, uint32_t now);
    int selected() const { return selected_; }
    int count() const { return count_; }
    bool animating(uint32_t now) const;

    void draw(LovyanGFX& g, uint32_t now, TextFn label, TextFn info, StyleFn style = nullptr);

private:
    float position(uint32_t now) const;

    int count_ = 0;
    int selected_ = 0;
    float from_ = 0;
    uint32_t animStart_ = 0;
};
