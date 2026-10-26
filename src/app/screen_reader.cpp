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
const uint32_t kChapterCardMs = 1800;
const int kRampWords = 8;
const float kRampStart = 1.6f;
const int kProgressY = 167;
const int kProgressH = 2;
const int kContextY = 26;
const int kContextH = 118;
const int kContextLineH = 25;
const int kContextLines = 4;
const int kMaxContextWords = 80;

int bandY, bandH, bandBaseline;

LGFX_Sprite band(&lcd);
LGFX_Sprite navLabel(&lcd);
LGFX_Sprite statusBox(&lcd);
LGFX_Sprite infoBox(&lcd);
LGFX_Sprite contextBox(&lcd);
const int kNavW = 100, kBoxW = 200, kBoxH = 22, kBoxBaseline = 16;
bool spritesReady = false;

int wpm = 300;
bool paused = true;
bool ended = false;
bool finished = false;

Word word;
Chunk chunks[kMaxChunks];
int chunkCount = 0;
int chunkIndex = 0;
uint32_t nextAt = 0;
int sinceStart = 0;

uint32_t sentence = 0;
int wordsInSentence = 0;
bool lastEndedSentence = true;

int shownChapter = -1;
bool cardShowing = false;
uint32_t cardUntil = 0;
int progressPx = -1;
bool contextShown = false;
Word contextWords[kMaxContextWords];

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

    contextBox.setColorDepth(16);
    contextBox.setPsram(true);
    if (contextBox.createSprite(kScreenW, kContextH) == nullptr) {
        contextBox.setPsram(false);
        contextBox.createSprite(kScreenW, kContextH);
    }
    contextBox.setFont(&fontSmall.font);
    contextBox.setTextDatum(baseline_left);
    spritesReady = true;
}

void drawWedges() {
    drawWedgeV(lcd, kOrpX, bandY - kTickLen, +1);
    drawWedgeV(lcd, kOrpX, bandY + bandH + kTickLen - 1, -1);
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
    if (contextShown) return;
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
    if (contextShown) return;
    band.fillScreen(TFT_BLACK);
    band.pushSprite(0, bandY);
}

void drawCard(int ch) {
    char prefix[48];
    char fitted[96];
    const char* name;
    const char* title = book.chapter(ch).title;
    if (!splitChapterTitle(title, prefix, sizeof(prefix), &name)) {
        snprintf(prefix, sizeof(prefix), "hoofdstuk %d/%d", ch + 1, book.chapterCount());
        name = title;
    }
    band.fillScreen(TFT_BLACK);
    band.setTextDatum(baseline_center);
    band.setFont(&fontMono.font);
    band.setTextColor(gold());
    fitText(band, prefix, kScreenW - 2 * kMargin, fitted, sizeof(fitted));
    band.drawString(fitted, kScreenW / 2, 17);
    band.setFont(&fontTitle.font);
    band.setTextColor(grey(255));
    fitText(band, name, kScreenW - 2 * kMargin, fitted, sizeof(fitted));
    band.drawString(fitted, kScreenW / 2, 47);
    band.setFont(&fontWord.font);
    band.setTextDatum(baseline_left);
    if (!contextShown) band.pushSprite(0, bandY);
}

// The card already shows the title, so skip the chapter's own heading paragraph.
void skipHeading(int ch) {
    char title[96] = "";
    char seen[96] = "";
    appendLetterKey(book.chapter(ch).title, title, sizeof(title));
    Word w = word;
    for (int i = 0; title[0] != '\0' && i < 40; ++i) {
        appendLetterKey(w.text, seen, sizeof(seen));
        if (strncmp(title, seen, strlen(seen)) != 0) break;
        if (w.paragraphEnd && strcmp(title, seen) == 0) {
            Word after;
            if (book.next(after)) {
                word = after;
                return;
            }
            break;
        }
        if (!book.next(w)) break;
    }
    Word again;
    book.seek(word.offset);
    book.next(again);
}

void updateProgress(uint32_t pos) {
    if (!options.progressLine) return;
    const int ch = book.chapterAt(pos);
    uint32_t start = ch >= 0 ? book.chapter(ch).offset : book.bodyStart();
    uint32_t end = ch + 1 < book.chapterCount() ? book.chapter(ch + 1).offset : book.size();
    const int w = kScreenW - 2 * kMargin;
    int px = end > start && pos > start ? (int)((uint64_t)(pos - start) * w / (end - start)) : 0;
    if (px > w) px = w;
    if (px == progressPx) return;
    if (progressPx < 0 || px < progressPx) lcd.fillRect(kMargin, kProgressY, w, kProgressH, grey(40));
    lcd.fillRect(kMargin, kProgressY, px, kProgressH, gold());
    progressPx = px;
}

void drawContext() {
    int n = 0;
    int current = -1;
    Word w;
    book.seek(sentence);
    while (n < kMaxContextWords && book.next(w)) {
        if (w.offset == word.offset) current = n;
        contextWords[n++] = w;
        if (w.offset >= word.offset && (w.paragraphEnd || wordEnding(w.text) == Ending::Sentence)) break;
    }
    book.seek(word.offset);
    book.next(w);

    static int lineOf[kMaxContextWords];
    static int xOf[kMaxContextWords];
    const int maxW = kScreenW - 2 * kMargin;
    const int space = contextBox.textWidth(" ");
    int line = 0, x = 0;
    for (int i = 0; i < n; ++i) {
        const int ww = contextBox.textWidth(contextWords[i].text);
        if (x > 0 && x + ww > maxW) {
            ++line;
            x = 0;
        }
        lineOf[i] = line;
        xOf[i] = x;
        x += ww + space;
    }
    const int lines = n > 0 ? line + 1 : 0;
    const int shown = lines < kContextLines ? lines : kContextLines;
    int first = 0;
    if (lines > kContextLines) first = constrain((current >= 0 ? lineOf[current] : 0) - 1, 0, lines - kContextLines);
    const int top = (kContextH - shown * kContextLineH) / 2;

    contextBox.fillScreen(TFT_BLACK);
    for (int i = 0; i < n; ++i) {
        if (lineOf[i] < first || lineOf[i] >= first + shown) continue;
        contextBox.setTextColor(i == current ? gold() : grey(185));
        contextBox.drawString(contextWords[i].text, kMargin + xOf[i], top + (lineOf[i] - first) * kContextLineH + 18);
    }
    contextBox.pushSprite(0, kContextY);
    contextShown = true;
}

void showWordView() {
    if (!contextShown) return;
    contextShown = false;
    lcd.fillRect(0, kContextY, kScreenW, kContextH, TFT_BLACK);
    drawWedges();
    if (ended && chunkCount == 0) {
        clearWord();
    } else {
        drawChunk(chunks[chunkIndex].text);
    }
}

void refreshPausedView() {
    if (paused && options.pauseContext) {
        drawContext();
    } else {
        showWordView();
    }
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

float rampFactor() {
    if (!options.rampUp || sinceStart >= kRampWords) return 1.0f;
    return 1.0f + (kRampStart - 1.0f) * (kRampWords - sinceStart) / kRampWords;
}

void showChunk(uint32_t now) {
    const char* text = chunks[chunkIndex].text;
    drawChunk(text);

    Parts p;
    splitWord(text, p);
    const bool last = chunkIndex == chunkCount - 1;
    const Ending ending = last ? wordEnding(word.text) : Ending::None;
    nextAt = now + (uint32_t)(durationMs(p.letters, ending, last && word.paragraphEnd, wpm) * rampFactor());
}

void startWord(uint32_t now) {
    if (lastEndedSentence) {
        sentence = word.offset;
        wordsInSentence = 0;
    }
    ++wordsInSentence;
    ++sinceStart;
    lastEndedSentence = word.paragraphEnd || wordEnding(word.text) == Ending::Sentence;

    chunkCount = chunkWord(word.text, chunks, fits);
    chunkIndex = 0;
    showChunk(now);
    updateProgress(word.offset);
}

void beginWord(uint32_t now) {
    startWord(now);
    if (wordsInSentence == 1 && now - lastSaveAt >= kSaveEveryMs) save(now);
}

void showFrom(uint32_t pos, uint32_t now) {
    cardShowing = false;
    book.seek(pos);
    lastEndedSentence = true;
    ended = !book.next(word);
    if (ended) {
        chunkCount = 0;
        clearWord();
        return;
    }
    shownChapter = book.chapterAt(word.offset);
    startWord(now);
}

void startPlaying(uint32_t now, uint32_t delayMs) {
    sinceStart = 0;
    nextAt = now + (uint32_t)(delayMs * rampFactor());
    if (finished && !ended) {
        setFinished(book.path(), false);
        finished = false;
    }
}

void setPaused(bool p, uint32_t now) {
    paused = p;
    if (paused) {
        if (cardShowing) {
            cardShowing = false;
            beginWord(now);
        }
        save(now);
    } else {
        showWordView();
        startPlaying(now, 60000 / wpm);
    }
    refreshPausedView();
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
        if (!finished) {
            setFinished(book.path(), true);
            finished = true;
        }
        setPaused(true, now);
        return;
    }
    const int ch = book.chapterAt(word.offset);
    if (ch > shownChapter && options.chapterTitles) {
        shownChapter = ch;
        cardShowing = true;
        cardUntil = now + kChapterCardMs;
        drawCard(ch);
        skipHeading(ch);
        updateProgress(word.offset);
        return;
    }
    shownChapter = ch;
    beginWord(now);
}

void previousSentence(uint32_t now) {
    const uint32_t target = wordsInSentence < 2 ? book.previousSentence(sentence) : sentence;
    showFrom(target, now);
    if (paused) {
        refreshPausedView();
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
    if (paused) {
        refreshPausedView();
        updateInfo();
    }
}

void enter(uint32_t now) {
    setupSprites();
    wpm = loadWpm();
    lcd.fillScreen(TFT_BLACK);
    contextShown = false;
    cardShowing = false;
    progressPx = -1;
    drawWedges();
    navDir = 0;
    wpmDirty = false;
    finished = isFinished(book.path());

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
    if (!paused) startPlaying(now, 600);
    refreshPausedView();
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

    if (cardShowing && !paused && (int32_t)(now - cardUntil) >= 0) {
        cardShowing = false;
        sinceStart = 0;
        beginWord(now);
    }
    if (!paused && !ended && !cardShowing && (int32_t)(now - nextAt) >= 0) advance(now);
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
