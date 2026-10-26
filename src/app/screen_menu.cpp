#include "app.h"
#include "fonts.h"
#include "library.h"
#include "settings.h"
#include "text.h"
#include "ui.h"

namespace {

enum class Item { Continue, Chapters, Info, Settings, Start, Library };
Item items[6];
int itemCount = 0;
Wheel menuWheel;

void menuLabel(int i, char* out, size_t cap) {
    switch (items[i]) {
        case Item::Continue: copyUtf8(out, "Verder lezen", cap); break;
        case Item::Chapters: copyUtf8(out, "Hoofdstukken", cap); break;
        case Item::Info: copyUtf8(out, "Info", cap); break;
        case Item::Settings: copyUtf8(out, "Instellingen", cap); break;
        case Item::Start: copyUtf8(out, "Naar het begin", cap); break;
        case Item::Library: copyUtf8(out, "Bibliotheek", cap); break;
    }
}

void menuInfo(int i, char* out, size_t cap) {
    const uint32_t pos = readerPosition();
    const int ch = book.chapterAt(pos);
    switch (items[i]) {
        case Item::Continue:
            if (ch >= 0) {
                snprintf(out, cap, "hfst %d/%d \xC2\xB7 %d%%", ch + 1, book.chapterCount(), book.percent(pos));
            } else {
                snprintf(out, cap, "%d%%", book.percent(pos));
            }
            break;
        case Item::Chapters: snprintf(out, cap, "%d hoofdstukken", book.chapterCount()); break;
        case Item::Info: {
            BookStats stats;
            if (loadStats(book.path(), book.size(), stats) && book.size() > book.bodyStart()) {
                const uint64_t left = (uint64_t)stats.units * (book.size() - pos) / (book.size() - book.bodyStart());
                char time[24];
                formatMinutes((uint32_t)(left / (100ull * loadWpm())), time, sizeof(time));
                snprintf(out, cap, "nog %s", time);
            } else {
                copyUtf8(out, "leestijd en meer", cap);
            }
            break;
        }
        case Item::Settings: copyUtf8(out, "helderheid en lezen", cap); break;
        case Item::Start: copyUtf8(out, book.title(), cap); break;
        case Item::Library: snprintf(out, cap, "%d %s", (int)library.size(), library.size() == 1 ? "boek" : "boeken"); break;
    }
}

void drawMenu(uint32_t now) {
    frame.fillScreen(TFT_BLACK);
    drawCorner(frame, Corner::TopLeft, "menu", grey(kInfoGrey));
    frame.setFont(&fontMono.font);
    char title[96];
    fitText(frame, book.title(), 190, title, sizeof(title));
    drawCorner(frame, Corner::TopRight, title, grey(kInfoGrey));
    menuWheel.draw(frame, now, menuLabel, menuInfo);
    frame.pushSprite(0, 0);
}

void menuEnter(uint32_t now) {
    itemCount = 0;
    items[itemCount++] = Item::Continue;
    if (book.chapterCount() > 0) items[itemCount++] = Item::Chapters;
    items[itemCount++] = Item::Info;
    items[itemCount++] = Item::Settings;
    items[itemCount++] = Item::Start;
    items[itemCount++] = Item::Library;

    Item back = Item::Continue;
    if (previousScreen() == &chaptersScreen) back = Item::Chapters;
    if (previousScreen() == &infoScreen) back = Item::Info;
    if (previousScreen() == &settingsScreen) back = Item::Settings;
    int selected = 0;
    for (int i = 0; i < itemCount; ++i) {
        if (items[i] == back) selected = i;
    }
    menuWheel.reset(itemCount, selected);
    drawMenu(now);
}

void menuUpdate(const Input& in, uint32_t now) {
    if (in.hold || in.prev) {
        goTo(readerScreen);
        return;
    }
    if (in.select) {
        switch (items[menuWheel.selected()]) {
            case Item::Continue:
                readerAutoPlay = true;
                goTo(readerScreen);
                break;
            case Item::Chapters:
                goTo(chaptersScreen);
                break;
            case Item::Info:
                goTo(infoScreen);
                break;
            case Item::Settings:
                settingsBack = &menuScreen;
                goTo(settingsScreen);
                break;
            case Item::Start:
                readerJumpTo = book.bodyStart();
                goTo(readerScreen);
                break;
            case Item::Library:
                goTo(libraryScreen);
                break;
        }
        return;
    }
    bool redraw = false;
    if (in.clicks != 0) {
        menuWheel.scroll(in.clicks, now);
        redraw = true;
    }
    if (redraw || menuWheel.animating(now)) drawMenu(now);
}

Wheel chapterWheel;

void chapterLabel(int i, char* out, size_t cap) {
    char prefix[48];
    const char* rest;
    const char* title = book.chapter(i).title;
    copyUtf8(out, splitChapterTitle(title, prefix, sizeof(prefix), &rest) ? rest : title, cap);
}

void chapterInfo(int i, char* out, size_t cap) {
    char prefix[48];
    const char* rest;
    const int pct = book.percent(book.chapter(i).offset);
    if (splitChapterTitle(book.chapter(i).title, prefix, sizeof(prefix), &rest)) {
        snprintf(out, cap, "%s \xC2\xB7 %d%%", prefix, pct);
    } else {
        snprintf(out, cap, "hfst %d \xC2\xB7 %d%%", i + 1, pct);
    }
}

void drawChapters(uint32_t now) {
    frame.fillScreen(TFT_BLACK);
    drawCorner(frame, Corner::TopLeft, "hoofdstukken", grey(kInfoGrey));
    char count[16];
    snprintf(count, sizeof(count), "%d/%d", chapterWheel.selected() + 1, book.chapterCount());
    drawCorner(frame, Corner::TopRight, count, grey(kInfoGrey));
    chapterWheel.draw(frame, now, chapterLabel, chapterInfo);
    frame.pushSprite(0, 0);
}

void chaptersEnter(uint32_t now) {
    const int current = book.chapterAt(readerPosition());
    chapterWheel.reset(book.chapterCount(), current < 0 ? 0 : current);
    drawChapters(now);
}

void chaptersUpdate(const Input& in, uint32_t now) {
    if (in.hold || in.prev) {
        goTo(menuScreen);
        return;
    }
    if (in.select && book.chapterCount() > 0) {
        readerJumpTo = book.chapter(chapterWheel.selected()).offset;
        goTo(readerScreen);
        return;
    }
    bool redraw = false;
    if (in.clicks != 0) {
        chapterWheel.scroll(in.clicks, now);
        redraw = true;
    }
    if (redraw || chapterWheel.animating(now)) drawChapters(now);
}

}

const Screen menuScreen = {menuEnter, menuUpdate, nullptr};
const Screen chaptersScreen = {chaptersEnter, chaptersUpdate, nullptr};
