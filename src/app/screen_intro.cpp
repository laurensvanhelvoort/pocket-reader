#include "app.h"
#include "fonts.h"
#include "settings.h"
#include "text.h"
#include "ui.h"

namespace {

const int kAxisX = kScreenW / 2;
const int kBaselineY = 102;
const int kGap = 15;
const float kPi = 3.14159265f;

const uint32_t kFadeInMs = 200;
const uint32_t kWordMs = 250;
const uint32_t kHoldMs = 800;
const uint32_t kMoveMs = 400;
const uint32_t kPocketAt = kFadeInMs;
const uint32_t kReaderAt = kPocketAt + kWordMs;
const uint32_t kLogoAt = kReaderAt + kWordMs;
const uint32_t kMoveAt = kLogoAt + kHoldMs;
const uint32_t kEndAt = kMoveAt + kMoveMs;

uint32_t startAt = 0;
int shownPhase = -1;
bool lightOn = false;

float easeInOut(float t) {
    return t < 0.5f ? 4 * t * t * t : 1 - powf(-2 * t + 2, 3) / 2;
}

void drawWord(const char* before, const char* mid, const char* after, float level) {
    frame.setFont(&fontWord.font);
    frame.setTextDatum(baseline_left);
    int x = kAxisX - frame.textWidth(before) - frame.textWidth(mid) / 2;
    frame.setTextColor(grey((uint8_t)(255 * level)));
    frame.drawString(before, x, kBaselineY);
    x += frame.textWidth(before);
    frame.setTextColor(gold(level));
    frame.drawString(mid, x, kBaselineY);
    x += frame.textWidth(mid);
    frame.setTextColor(grey((uint8_t)(255 * level)));
    frame.drawString(after, x, kBaselineY);
}

void drawReading(const char* word) {
    Parts p;
    splitWord(word, p);
    drawWord(p.before, p.orp, p.after, 1.0f);
}

void drawLogo(float level) {
    char version[16];
    snprintf(version, sizeof(version), "v%s", kVersion);
    drawWord("pocket", "-", "reader", level);
    drawCorner(frame, Corner::BottomRight, version, grey((uint8_t)(100 * level)));
}

void render(uint32_t t) {
    const float topY = kBaselineY - kWordAscent - kGap - kTickLen;
    const float bottomY = kBaselineY + kGap + kTickLen;
    const float sideY = 92 - 7;

    frame.fillScreen(TFT_BLACK);
    if (t < kMoveAt) {
        drawWedge(frame, kAxisX, topY, kPi / 2);
        drawWedge(frame, kAxisX, bottomY, -kPi / 2);
        if (t >= kLogoAt) {
            drawLogo(1.0f);
        } else if (t >= kReaderAt) {
            drawReading("reader");
        } else if (t >= kPocketAt) {
            drawReading("pocket");
        }
    } else {
        const float m = fminf(1.0f, (t - kMoveAt) / (float)kMoveMs);
        const float k = easeInOut(m);
        drawLogo(1.0f - fminf(1.0f, m * 1.6f));
        drawWedge(frame, kAxisX + (kMargin - kAxisX) * k, topY + (sideY - topY) * k, kPi / 2 * (1 - k));
        drawWedge(frame, kAxisX + (kScreenW - kMargin - kAxisX) * k, bottomY + (sideY - bottomY) * k,
                  -kPi / 2 - kPi / 2 * k);
    }
    frame.pushSprite(0, 0);
}

int phase(uint32_t t) {
    if (t < kPocketAt) return 0;
    if (t < kReaderAt) return 1;
    if (t < kLogoAt) return 2;
    if (t < kMoveAt) return 3;
    return 4;
}

void enter(uint32_t now) {
    startAt = now;
    shownPhase = -1;
    lightOn = false;
    lcd.setBrightness(0);
    render(0);
}

void update(const Input& in, uint32_t now) {
    const uint32_t t = now - startAt;
    if (in.select || in.hold || in.next || in.prev || in.clicks != 0 || t >= kEndAt) {
        setBacklight(options.brightness);
        goTo(libraryScreen);
        return;
    }
    if (!lightOn) {
        const float f = fminf(1.0f, t / (float)kFadeInMs);
        lcd.setBrightness((uint8_t)(f * options.brightness * 255 / 100));
        lightOn = f >= 1.0f;
    }
    const int p = phase(t);
    if (p != shownPhase || p == 4) {
        shownPhase = p;
        render(t);
    }
}

}

const Screen introScreen = {enter, update, nullptr};
