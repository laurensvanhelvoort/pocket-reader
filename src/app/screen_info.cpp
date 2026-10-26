#include "app.h"
#include "fonts.h"
#include "settings.h"
#include "text.h"
#include "ui.h"

namespace {

const int kWordsPerStep = 400;

BookStats stats;
bool ready = false;
bool scanning = false;
int scanPercent = 0;
int shownPercent = -1;

const char* languageName(const char* code) {
    static const char* const names[][2] = {
        {"nl", "nederlands"}, {"en", "engels"}, {"de", "duits"}, {"fr", "frans"},
        {"es", "spaans"},     {"it", "italiaans"}, {"pt", "portugees"},
    };
    for (const auto& n : names) {
        if (strncmp(code, n[0], 2) == 0 && (code[2] == '\0' || code[2] == '-' || code[2] == '_')) return n[1];
    }
    return code;
}

struct Segment {
    const char* text;
    uint16_t color;
};

void drawSegments(const Segment* parts, int count, int y) {
    int width = 0;
    for (int i = 0; i < count; ++i) width += frame.textWidth(parts[i].text);
    int x = (kScreenW - width) / 2;
    frame.setTextDatum(baseline_left);
    for (int i = 0; i < count; ++i) {
        frame.setTextColor(parts[i].color);
        frame.drawString(parts[i].text, x, y);
        x += frame.textWidth(parts[i].text);
    }
    frame.setTextDatum(baseline_center);
}

void draw() {
    char text[128];
    char fitted[128];
    const uint32_t pos = readerPosition();
    const int wpm = loadWpm();

    frame.fillScreen(TFT_BLACK);
    drawCorner(frame, Corner::TopLeft, "info", grey(kInfoGrey));
    snprintf(text, sizeof(text), "%d%%", book.percent(pos));
    drawCorner(frame, Corner::TopRight, text, grey(kInfoGrey));

    frame.setTextDatum(baseline_center);
    frame.setFont(&fontTitle.font);
    frame.setTextColor(grey(255));
    fitText(frame, book.title(), 236, fitted, sizeof(fitted));
    frame.drawString(fitted, kScreenW / 2, 50);
    drawWedgeH(frame, 43, kMargin, +1);
    drawWedgeH(frame, 43, kScreenW - kMargin - 1, -1);

    frame.setFont(&fontSmall.font);
    frame.setTextColor(grey(150));
    fitText(frame, book.author(), kScreenW - 2 * kMargin, fitted, sizeof(fitted));
    frame.drawString(fitted, kScreenW / 2, 74);

    if (ready) {
        char total[24];
        formatMinutes(stats.units / (100 * wpm), total, sizeof(total));
        const Segment line[] = {{"Leestijd ", grey(150)}, {total, gold()}};
        drawSegments(line, 2, 108);

        if (pos > book.bodyStart() && book.size() > book.bodyStart()) {
            const uint64_t left = (uint64_t)stats.units * (book.size() - pos) / (book.size() - book.bodyStart());
            char time[24];
            formatMinutes((uint32_t)(left / (100ull * wpm)), time, sizeof(time));
            snprintf(text, sizeof(text), "nog %s \xC2\xB7 %d wpm", time, wpm);
        } else {
            snprintf(text, sizeof(text), "bij %d wpm", wpm);
        }
    } else {
        frame.setTextColor(grey(110));
        snprintf(text, sizeof(text), "Leestijd berekenen\xE2\x80\xA6 %d%%", scanPercent);
        frame.drawString(text, kScreenW / 2, 108);
        text[0] = '\0';
    }
    frame.setFont(&fontMono.font);
    frame.setTextColor(grey(kInfoGrey));
    frame.drawString(text, kScreenW / 2, 132);

    char words[32] = "";
    if (ready) {
        formatCount(stats.words, text, sizeof(text));
        snprintf(words, sizeof(words), "%s woorden", text);
    }
    const char* language = book.language()[0] != '\0' ? languageName(book.language()) : "";
    if (book.year()[0] != '\0' && language[0] != '\0') {
        snprintf(text, sizeof(text), "%s \xC2\xB7 %s", book.year(), language);
    } else {
        copyUtf8(text, book.year()[0] != '\0' ? book.year() : language, sizeof(text));
    }
    if (frame.textWidth(text) + frame.textWidth(words) + 2 * kMargin > kScreenW - 2 * kMargin) {
        copyUtf8(text, book.year(), sizeof(text));
    }
    drawCorner(frame, Corner::BottomLeft, text, grey(kInfoGrey));
    drawCorner(frame, Corner::BottomRight, words, grey(kInfoGrey));

    frame.pushSprite(0, 0);
    shownPercent = scanPercent;
}

void scanStep() {
    Word w;
    Parts p;
    for (int k = 0; k < kWordsPerStep; ++k) {
        if (!book.next(w)) {
            scanning = false;
            ready = true;
            saveStats(book.path(), stats);
            draw();
            return;
        }
        splitWord(w.text, p);
        ++stats.words;
        stats.units += (uint32_t)lroundf(durationFactor(p.letters, wordEnding(w.text), w.paragraphEnd) * 100);
        scanPercent = book.percent(w.offset);
    }
    if (scanPercent >= shownPercent + 5) draw();
}

void enter(uint32_t) {
    ready = loadStats(book.path(), book.size(), stats);
    scanning = !ready;
    scanPercent = 0;
    if (scanning) {
        stats.size = book.size();
        stats.words = 0;
        stats.units = 0;
        book.seek(book.bodyStart());
    }
    draw();
}

void update(const Input& in, uint32_t) {
    if (in.hold || in.prev || in.select) {
        goTo(menuScreen);
        return;
    }
    if (scanning) scanStep();
}

}

const Screen infoScreen = {enter, update, nullptr};
