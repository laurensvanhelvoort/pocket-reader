#include "app.h"
#include "fonts.h"
#include "settings.h"
#include "text.h"
#include "ui.h"

uint32_t readerJumpTo = 0;
bool readerAutoPlay = false;

namespace {

const int kOrpX = 106;
const int kBaselineY = 102;
const int kGap = 15;
const int kWpmStep = 25;
const int kWpmMin = 100;
const int kWpmMax = 900;
const uint32_t kSaveEveryMs = 20000;

int bandY, bandH, bandBaseline;

LGFX_Sprite band(&lcd);
LGFX_Sprite navLabel(&lcd);
LGFX_Sprite statusBox(&lcd);
LGFX_Sprite infoBox(&lcd);
const int kNavW = 100, kBoxW = 200, kBoxH = 22, kBoxBaseline = 16;
bool spritesReady = false;

int wpm = 300;
bool paused = true;
bool ended = false;

Word word;
Chunk chunks[kMaxChunks];
int chunkCount = 0;
int chunkIndex = 0;
uint32_t nextAt = 0;

uint32_t sentence = 0;
int wordsInSentence = 0;
bool lastEndedSentence = true;

uint32_t savedPos = 0;
uint32_t lastSaveAt = 0;
bool wpmDirty = false;
uint32_t wpmChangedAt = 0;
uint32_t statusUntil = 0;

void setupSprites() {
    if (spritesReady) return;
    bandY = kBaselineY - kWordAscent - kGap;
    bandH = kWordAscent + 2 * kGap;
    bandBaseline = kWordAscent + kGap;

    band.setColorDepth(16);
    band.createSprite(kScreenW, bandH);
    band.setFont(&fontWord.font);
    band.setTextDatum(baseline_left);

    LGFX_Sprite* boxes[] = {&navLabel, &statusBox, &infoBox};
    for (LGFX_Sprite* s : boxes) {
        s->setColorDepth(16);
        s->createSprite(s == &navLabel ? kNavW : kBoxW, kBoxH);
        s->setFont(&fontMono.font);
    }
    spritesReady = true;
}

int layoutX(const Parts& p) {
    return kOrpX - band.textWidth(p.before) - band.textWidth(p.orp) / 2;
}

bool fits(const char* text) {
    Parts p;
    splitWord(text, p);
    const int x = layoutX(p);
    const int w = band.textWidth(p.before) + band.textWidth(p.orp) + band.textWidth(p.after);
    return x >= kMargin && x + w <= kScreenW - kMargin;
}

void drawChunk(const char* text) {
    Parts p;
    splitWord(text, p);
    int x = layoutX(p);
    if (x < kMargin) x = kMargin;

    band.fillScreen(TFT_BLACK);
    band.setTextColor(grey(255));
    band.drawString(p.before, x, bandBaseline);
    x += band.textWidth(p.before);
    band.setTextColor(gold());
    band.drawString(p.orp, x, bandBaseline);
    x += band.textWidth(p.orp);
    band.setTextColor(grey(255));
    band.drawString(p.after, x, bandBaseline);
    band.pushSprite(0, bandY);
}

void clearWord() {
    band.fillScreen(TFT_BLACK);
    band.pushSprite(0, bandY);
}

void drawBox(LGFX_Sprite& box, int x, int y, const char* text, uint16_t color, bool alignRight) {
    box.fillScreen(TFT_BLACK);
    box.setTextColor(color);
    box.setTextDatum(alignRight ? baseline_right : baseline_left);
    box.drawString(text, alignRight ? box.width() : 0, kBoxBaseline);
    box.pushSprite(x, y - kBoxBaseline);
}

void updateStatus(uint32_t now, bool tempoChanged) {
    char text[40] = "";
    if (paused) {
        snprintf(text, sizeof(text), "%s \xC2\xB7 %d wpm", ended ? "einde" : "pauze", wpm);
        statusUntil = 0;
    } else if (tempoChanged) {
        snprintf(text, sizeof(text), "%d wpm", wpm);
        statusUntil = now + 1500;
    } else {
        statusUntil = 0;
    }
    drawBox(statusBox, kScreenW - kMargin - kBoxW, kBottomTextY, text, grey(kInfoGrey), true);
}

void updateInfo() {
    char text[40] = "";
    if (paused) {
        const int ch = book.chapterAt(sentence);
        const int pct = book.percent(sentence);
        if (book.chapterCount() > 0 && ch >= 0) {
            snprintf(text, sizeof(text), "hfst %d/%d \xC2\xB7 %d%%", ch + 1, book.chapterCount(), pct);
        } else {
            snprintf(text, sizeof(text), "%d%%", pct);
        }
    }
    drawBox(infoBox, kScreenW - kMargin - kBoxW, kTopTextY, text, grey(kInfoGrey), true);
}

int navDir = 0;
uint32_t navStart = 0;
int navFrame = -1;

int navY(int dir) {
    return dir > 0 ? kTopTextY : kBottomTextY;
}

void startNav(int dir, uint32_t now) {
    if (navDir != 0 && navDir != dir) drawBox(navLabel, kMargin, navY(navDir), "", 0, false);
    navDir = dir;
    navStart = now;
    navFrame = -1;
}

void updateNav(uint32_t now) {
    if (navDir == 0) return;
    const uint32_t t = now - navStart;
    const int frame = t < 90 ? 0 : t < 600 ? 1 : t < 900 ? 2 + (t - 600) / 75 : 6;
    if (frame == navFrame) return;
    navFrame = frame;

    const char* text = "";
    if (frame < 6) {
        text = navDir > 0 ? (frame == 0 ? "sent >" : "sent >>") : (frame == 0 ? "sent  <" : "sent <<");
    }
    const uint8_t level = frame < 2 ? kInfoGrey : kInfoGrey * (6 - frame) / 5;
    drawBox(navLabel, kMargin, navY(navDir), text, grey(level), false);
    if (frame == 6) navDir = 0;
}

void save(uint32_t now) {
    if (sentence != savedPos) {
        savePosition(book.path(), sentence);
        savedPos = sentence;
    }
    lastSaveAt = now;
}

void showChunk(uint32_t now) {
    const char* text = chunks[chunkIndex].text;
    drawChunk(text);

    Parts p;
    splitWord(text, p);
    const bool last = chunkIndex == chunkCount - 1;
    const Ending ending = last ? wordEnding(word.text) : Ending::None;
    nextAt = now + durationMs(p.letters, ending, last && word.paragraphEnd, wpm);
}

void startWord(uint32_t now) {
    if (lastEndedSentence) {
        sentence = word.offset;
        wordsInSentence = 0;
    }
    ++wordsInSentence;
    lastEndedSentence = word.paragraphEnd || wordEnding(word.text) == Ending::Sentence;

    chunkCount = chunkWord(word.text, chunks, fits);
    chunkIndex = 0;
    showChunk(now);
}

void showFrom(uint32_t pos, uint32_t now) {
    book.seek(pos);
    lastEndedSentence = true;
    ended = !book.next(word);
    if (ended) {
        clearWord();
        return;
    }
    startWord(now);
}

void setPaused(bool p, uint32_t now) {
    paused = p;
    if (paused) {
        save(now);
    } else {
        nextAt = now + 60000 / wpm;
    }
    updateStatus(now, false);
    updateInfo();
}

void advance(uint32_t now) {
    if (chunkIndex + 1 < chunkCount) {
        ++chunkIndex;
        showChunk(now);
        return;
    }
    if (!book.next(word)) {
        ended = true;
        setPaused(true, now);
        return;
    }
    startWord(now);
    if (wordsInSentence == 1 && now - lastSaveAt >= kSaveEveryMs) save(now);
}

void previousSentence(uint32_t now) {
    const uint32_t target = wordsInSentence < 2 ? book.previousSentence(sentence) : sentence;
    showFrom(target, now);
    if (paused) {
        updateStatus(now, false);
        updateInfo();
    }
}

void nextSentence(uint32_t now) {
    if (ended) return;
    const uint32_t target = book.nextSentence(word.offset);
    if (target != 0) {
        showFrom(target, now);
    } else {
        Word again;
        book.seek(word.offset);
        book.next(again);
    }
    if (paused) updateInfo();
}

void enter(uint32_t now) {
    setupSprites();
    wpm = loadWpm();
    lcd.fillScreen(TFT_BLACK);
    drawWedgeV(lcd, kOrpX, bandY - kTickLen, +1);
    drawWedgeV(lcd, kOrpX, bandY + bandH + kTickLen - 1, -1);
    navDir = 0;
    wpmDirty = false;

    uint32_t start = readerJumpTo != 0 ? readerJumpTo : sentence;
    readerJumpTo = 0;
    if (start < book.bodyStart()) start = book.bodyStart();
    start = book.sentenceStart(start);
    savedPos = loadPosition(book.path());
    lastSaveAt = now;
    saveLastBook(book.path());

    showFrom(start, now);
    paused = ended || !readerAutoPlay;
    readerAutoPlay = false;
    if (!paused) nextAt = now + 600;
    updateStatus(now, false);
    updateInfo();
}

void update(const Input& in, uint32_t now) {
    if (in.hold) {
        goTo(menuScreen);
        return;
    }
    if (in.clicks != 0) {
        wpm = constrain(wpm + in.clicks * kWpmStep, kWpmMin, kWpmMax);
        wpmDirty = true;
        wpmChangedAt = now;
        updateStatus(now, true);
    }
    if (in.select) {
        if (ended) {
            goTo(menuScreen);
            return;
        }
        setPaused(!paused, now);
    }
    if (in.prev) {
        previousSentence(now);
        startNav(-1, now);
    }
    if (in.next) {
        nextSentence(now);
        startNav(+1, now);
    }

    if (wpmDirty && now - wpmChangedAt > 2000) {
        saveWpm(wpm);
        wpmDirty = false;
    }
    updateNav(now);
    if (statusUntil != 0 && (int32_t)(now - statusUntil) >= 0) updateStatus(now, false);

    if (!paused && !ended && (int32_t)(now - nextAt) >= 0) advance(now);
}

void leave() {
    save(millis());
    if (wpmDirty) saveWpm(wpm);
    wpmDirty = false;
}

}

const Screen readerScreen = {enter, update, leave};

uint32_t readerPosition() {
    return sentence;
}
