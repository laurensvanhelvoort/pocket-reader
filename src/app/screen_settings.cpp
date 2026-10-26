#include "app.h"
#include "settings.h"
#include "text.h"
#include "ui.h"

const Screen* settingsBack = &libraryScreen;

namespace {

enum class Item { Brightness, ProgressLine, ChapterTitles, RampUp, PauseContext };
const Item kItems[] = {Item::Brightness, Item::ProgressLine, Item::ChapterTitles, Item::RampUp, Item::PauseContext};
const int kCount = sizeof(kItems) / sizeof(kItems[0]);
const int kBrightnessStep = 10;

Wheel wheel;
bool editing = false;

bool* flag(Item item) {
    switch (item) {
        case Item::ProgressLine: return &options.progressLine;
        case Item::ChapterTitles: return &options.chapterTitles;
        case Item::RampUp: return &options.rampUp;
        case Item::PauseContext: return &options.pauseContext;
        default: return nullptr;
    }
}

void label(int i, char* out, size_t cap) {
    switch (kItems[i]) {
        case Item::Brightness: copyUtf8(out, "Helderheid", cap); break;
        case Item::ProgressLine: copyUtf8(out, "Voortgangslijn", cap); break;
        case Item::ChapterTitles: copyUtf8(out, "Hoofdstuktitel", cap); break;
        case Item::RampUp: copyUtf8(out, "Aanloop", cap); break;
        case Item::PauseContext: copyUtf8(out, "Context bij pauze", cap); break;
    }
}

void info(int i, char* out, size_t cap) {
    if (kItems[i] == Item::Brightness) {
        snprintf(out, cap, editing ? "< %d%% >" : "%d%%", options.brightness);
    } else {
        copyUtf8(out, *flag(kItems[i]) ? "aan" : "uit", cap);
    }
}

uint8_t style(int i) {
    if (kItems[i] == Item::Brightness) return editing ? kStyleInfoAccent : 0;
    return *flag(kItems[i]) ? kStyleInfoAccent : 0;
}

void draw(uint32_t now) {
    frame.fillScreen(TFT_BLACK);
    drawCorner(frame, Corner::TopLeft, "instellingen", grey(kInfoGrey));
    if (editing) drawCorner(frame, Corner::TopRight, "draaien", grey(kInfoGrey));
    wheel.draw(frame, now, label, info, style);
    frame.pushSprite(0, 0);
}

void enter(uint32_t now) {
    editing = false;
    wheel.reset(kCount, 0);
    draw(now);
}

void update(const Input& in, uint32_t now) {
    if (editing) {
        if (in.clicks != 0) {
            options.brightness = constrain(options.brightness + in.clicks * kBrightnessStep, 10, 100);
            setBacklight(options.brightness);
            draw(now);
        }
        if (in.select || in.hold || in.prev) {
            editing = false;
            saveOptions();
            draw(now);
        }
        return;
    }
    if (in.hold || in.prev) {
        goTo(*settingsBack);
        return;
    }
    if (in.select) {
        const Item item = kItems[wheel.selected()];
        if (item == Item::Brightness) {
            editing = true;
        } else {
            *flag(item) = !*flag(item);
            saveOptions();
        }
        draw(now);
        return;
    }
    bool redraw = false;
    if (in.clicks != 0) {
        wheel.scroll(in.clicks, now);
        redraw = true;
    }
    if (redraw || wheel.animating(now)) draw(now);
}

}

const Screen settingsScreen = {enter, update, nullptr};
