#include "app.h"
#include "fonts.h"
#include "library.h"
#include "settings.h"
#include "text.h"
#include "ui.h"

Book book;

namespace {

Wheel wheel;

bool isAddItem(int i) {
    return i == (int)library.size();
}

void label(int i, char* out, size_t cap) {
    copyUtf8(out, isAddItem(i) ? "boeken toevoegen" : library[i].title, cap);
}

void info(int i, char* out, size_t cap) {
    if (isAddItem(i)) {
        copyUtf8(out, library.empty() ? "nog geen boeken \xC2\xB7 via wifi" : "via wifi", cap);
        return;
    }
    const BookInfo& b = library[i];
    if (b.author[0] != '\0') {
        snprintf(out, cap, "%d%% \xC2\xB7 %s", b.percent, b.author);
    } else {
        snprintf(out, cap, "%d%%", b.percent);
    }
}

void draw(uint32_t now) {
    frame.fillScreen(TFT_BLACK);
    drawCorner(frame, Corner::TopLeft, "bibliotheek", grey(kInfoGrey));
    if (!isAddItem(wheel.selected())) {
        char count[16];
        snprintf(count, sizeof(count), "%d/%d", wheel.selected() + 1, (int)library.size());
        drawCorner(frame, Corner::TopRight, count, grey(kInfoGrey));
    }
    wheel.draw(frame, now, label, info);
    frame.pushSprite(0, 0);
}

void enter(uint32_t now) {
    scanLibrary();
    char last[48];
    loadLastBook(last, sizeof(last));
    const int index = libraryIndexOf(last);
    wheel.reset(library.size() + 1, index < 0 ? 0 : index);
    draw(now);
}

void update(const Input& in, uint32_t now) {
    bool redraw = false;
    if (in.clicks != 0) {
        wheel.scroll(in.clicks, now);
        redraw = true;
    }
    if (in.select) {
        if (isAddItem(wheel.selected())) {
            goTo(uploadScreen);
            return;
        }
        const BookInfo& b = library[wheel.selected()];
        if (book.open(b.path)) {
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
