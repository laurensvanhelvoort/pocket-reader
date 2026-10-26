#include "ui.h"

#include <math.h>

#include "fonts.h"
#include "text.h"

Display lcd;
LGFX_Sprite frame(&lcd);

namespace {

const float kTickWide = 5.0f;
const float kTickNarrow = 1.5f;
const uint32_t kWheelAnimMs = 140;

const float kRowD[] = {-2, -1, 0, 1, 2};
const float kRowY[] = {6, 50, 92, 146, 190};
const int kInfoY = 114;
const int kCenterX = kScreenW / 2;

float rowY(float d) {
    if (d <= kRowD[0]) return kRowY[0];
    for (int i = 1; i < 5; ++i) {
        if (d <= kRowD[i]) {
            const float t = (d - kRowD[i - 1]) / (kRowD[i] - kRowD[i - 1]);
            return kRowY[i - 1] + t * (kRowY[i] - kRowY[i - 1]);
        }
    }
    return kRowY[4];
}

void blendPixel(LovyanGFX& g, int x, int y, float cover) {
    const uint8_t v = (uint8_t)(kTickGrey * cover + 0.5f);
    if (v) g.drawPixel(x, y, grey(v));
}

}

uint16_t grey(uint8_t level) {
    return lcd.color565(level, level, level);
}

uint16_t gold(float level) {
    return lcd.color565((uint8_t)(240 * level), (uint8_t)(205 * level), (uint8_t)(120 * level));
}

void initUi() {
    frame.setColorDepth(16);
    frame.setPsram(true);
    if (frame.createSprite(kScreenW, kScreenH) == nullptr) {
        frame.setPsram(false);
        if (frame.createSprite(kScreenW, kScreenH) == nullptr) Serial.println("ui: te weinig geheugen voor het scherm");
    }
}

void setBacklight(int percent) {
    lcd.setBrightness((uint8_t)(constrain(percent, 5, 100) * 255 / 100));
}

void drawWedgeV(LovyanGFX& g, int x, int yWide, int dir) {
    for (int i = 0; i < kTickLen; ++i) {
        const float t = (i + 0.5f) / kTickLen;
        const float half = (kTickWide + (kTickNarrow - kTickWide) * t) / 2;
        const float left = x - half, right = x + half;
        for (int px = (int)floorf(left); px < (int)ceilf(right); ++px) {
            blendPixel(g, px, yWide + dir * i, fminf(right, px + 1.0f) - fmaxf(left, (float)px));
        }
    }
}

void drawWedgeH(LovyanGFX& g, int y, int xWide, int dir) {
    for (int i = 0; i < kTickLen; ++i) {
        const float t = (i + 0.5f) / kTickLen;
        const float half = (kTickWide + (kTickNarrow - kTickWide) * t) / 2;
        const float top = y - half, bottom = y + half;
        for (int py = (int)floorf(top); py < (int)ceilf(bottom); ++py) {
            blendPixel(g, xWide + dir * i, py, fminf(bottom, py + 1.0f) - fmaxf(top, (float)py));
        }
    }
}

void fitText(LovyanGFX& g, const char* text, int maxWidth, char* out, size_t cap) {
    copyUtf8(out, text, cap);
    if (g.textWidth(out) <= maxWidth) return;
    const char* ellipsis = "\xE2\x80\xA6";
    const int ellipsisW = g.textWidth(ellipsis);
    int len = strlen(out);
    while (len > 0) {
        --len;
        while (len > 0 && ((uint8_t)out[len] & 0xC0) == 0x80) --len;
        out[len] = '\0';
        if (g.textWidth(out) + ellipsisW <= maxWidth) break;
    }
    while (len > 0 && out[len - 1] == ' ') out[--len] = '\0';
    if (len + 4 <= (int)cap) strcat(out, ellipsis);
}

void drawCorner(LovyanGFX& g, Corner corner, const char* text, uint16_t color) {
    g.setFont(&fontMono.font);
    g.setTextColor(color);
    const bool top = corner == Corner::TopLeft || corner == Corner::TopRight;
    const bool left = corner == Corner::TopLeft || corner == Corner::BottomLeft;
    g.setTextDatum(left ? baseline_left : baseline_right);
    g.drawString(text, left ? kMargin : kScreenW - kMargin, top ? kTopTextY : kBottomTextY);
}

void Wheel::reset(int count, int selected) {
    count_ = count;
    selected_ = count > 0 ? constrain(selected, 0, count - 1) : 0;
    from_ = selected_;
    animStart_ = 0;
}

float Wheel::position(uint32_t now) const {
    if (animStart_ == 0) return selected_;
    float t = (now - animStart_) / (float)kWheelAnimMs;
    if (t >= 1) return selected_;
    t = 1 - (1 - t) * (1 - t) * (1 - t);
    return from_ + (selected_ - from_) * t;
}

bool Wheel::animating(uint32_t now) const {
    return animStart_ != 0 && now - animStart_ < kWheelAnimMs + 20;
}

void Wheel::scroll(int clicks, uint32_t now) {
    if (count_ == 0 || clicks == 0) return;
    from_ = position(now);
    selected_ = constrain(selected_ + clicks, 0, count_ - 1);
    animStart_ = now ? now : 1;
}

void Wheel::draw(LovyanGFX& g, uint32_t now, TextFn label, TextFn info, StyleFn style) {
    const float pos = position(now);
    char text[128];
    char fitted[128];

    g.setTextDatum(baseline_center);
    for (int i = (int)floorf(pos) - 2; i <= (int)ceilf(pos) + 2; ++i) {
        if (i < 0 || i >= count_) continue;
        const float d = i - pos;
        const float ad = fabsf(d);
        if (ad > 1.75f) continue;

        const bool big = ad < 0.5f;
        float level = 255;
        if (ad > 1.0f) {
            level = 150 * (1.75f - ad) / 0.75f;
        } else if (ad > 0.5f) {
            level = 255 - (ad - 0.5f) * 2 * 105;
        }
        const uint8_t st = style != nullptr ? style(i) : 0;
        g.setFont(big ? &fontTitle.font : &fontSmall.font);
        if (st & kStyleAccent) {
            g.setTextColor(gold(level / 255));
        } else {
            g.setTextColor(grey((uint8_t)(st & kStyleDim ? level * 0.5f : level)));
        }
        label(i, text, sizeof(text));
        fitText(g, text, big ? 236 : 256, fitted, sizeof(fitted));
        g.drawString(fitted, kCenterX, (int)lroundf(rowY(d)));
    }

    if (info != nullptr && count_ > 0 && fabsf(pos - selected_) < 0.01f) {
        info(selected_, text, sizeof(text));
        const uint8_t st = style != nullptr ? style(selected_) : 0;
        g.setFont(&fontMono.font);
        g.setTextColor(st & kStyleInfoAccent ? gold() : grey(kInfoGrey));
        fitText(g, text, 256, fitted, sizeof(fitted));
        g.drawString(fitted, kCenterX, kInfoY);
    }

    const int y = (int)kRowY[2] - 7;
    drawWedgeH(g, y, kMargin, +1);
    drawWedgeH(g, y, kScreenW - kMargin - 1, -1);
}
