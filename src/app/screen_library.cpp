#include "app.h"
#include "fonts.h"
#include "library.h"
#include "settings.h"
#include "text.h"
#include "ui.h"

Book book;

namespace {

Wheel wheel;

bool isBook(int i) {
    return i < (int)library.size();
}

bool isAdd(int i) {
    return i == (int)library.size();
}

void label(int i, char* out, size_t cap) {
    if (isBook(i)) {
        copyUtf8(out, library[i].title, cap);
    } else {
        copyUtf8(out, isAdd(i) ? "Boeken toevoegen" : "Instellingen", cap);
    }
}

void info(int i, char* out, size_t cap) {
    if (isAdd(i)) {
        copyUtf8(out, library.empty() ? "nog geen boeken \xC2\xB7 via wifi" : "via wifi", cap);
        return;
    }
    if (!isBook(i)) {
        copyUtf8(out, "helderheid en lezen", cap);
        return;
    }
    const BookInfo& b = library[i];
    char state[16];
    if (b.finished) {
        copyUtf8(state, "uitgelezen", sizeof(state));
    } else {
        snprintf(state, sizeof(state), "%d%%", b.percent);
    }
    if (b.author[0] != '\0') {
        snprintf(out, cap, "%s \xC2\xB7 %s", state, b.author);
    } else {
        copyUtf8(out, state, cap);
    }
}

uint8_t style(int i) {
    if (!isBook(i)) return kStyleAccent;
    return library[i].finished ? kStyleDim : 0;
}

void draw(uint32_t now) {
    frame.fillScreen(TFT_BLACK);
    drawCorner(frame, Corner::TopLeft, "bibliotheek", grey(kInfoGrey));
    if (isBook(wheel.selected())) {
        char count[16];
        snprintf(count, sizeof(count), "%d/%d", wheel.selected() + 1, (int)library.size());
        drawCorner(frame, Corner::TopRight, count, grey(kInfoGrey));
    }
    wheel.draw(frame, now, label, info, style);
    frame.pushSprite(0, 0);
}

void enter(uint32_t now) {
    scanLibrary();
    int index = 0;
    if (previousScreen() == &settingsScreen) {
        index = library.size() + 1;
    } else if (previousScreen() == &uploadScreen) {
        index = 0;
    } else {
        char last[48];
        loadLastBook(last, sizeof(last));
        index = libraryIndexOf(last);
    }
    wheel.reset(library.size() + 2, index < 0 ? 0 : index);
    draw(now);
}

void update(const Input& in, uint32_t now) {
    bool redraw = false;
    if (in.clicks != 0) {
        wheel.scroll(in.clicks, now);
        redraw = true;
    }
    if (in.select) {
        const int i = wheel.selected();
        if (isAdd(i)) {
            goTo(uploadScreen);
            return;
        }
        if (!isBook(i)) {
            settingsBack = &libraryScreen;
            goTo(settingsScreen);
            return;
        }
        const BookInfo& b = library[i];
        if (book.open(b.path)) {
            touchBook(b.path);
            const uint32_t pos = loadPosition(b.path);
            readerJumpTo = pos > book.bodyStart() ? pos : book.bodyStart();
            readerAutoPlay = false;
            goTo(readerScreen);
            return;
        }
        Serial.printf("bibliotheek: kan %s niet openen\n", b.path);
    }
    if (redraw || wheel.animating(now)) draw(now);
}

}

const Screen libraryScreen = {enter, update, nullptr};
