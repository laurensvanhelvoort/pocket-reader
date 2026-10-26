#pragma once

#include <Arduino.h>

struct Options {
    int brightness = 70;
    bool progressLine = true;
    bool chapterTitles = true;
    bool rampUp = true;
    bool pauseContext = false;
};

extern Options options;

void beginSettings();
void saveOptions();

int loadWpm();
void saveWpm(int wpm);

uint32_t loadPosition(const char* bookPath);
void savePosition(const char* bookPath, uint32_t pos);

void touchBook(const char* bookPath);
uint32_t bookOrder(const char* bookPath);
bool isFinished(const char* bookPath);
void setFinished(const char* bookPath, bool finished);
void forgetBook(const char* bookPath);

struct BookStats {
    uint32_t size;
    uint32_t words;
    uint32_t units;  // sum of durationFactor() x 100
};
bool loadStats(const char* bookPath, uint32_t size, BookStats& out);
void saveStats(const char* bookPath, const BookStats& stats);

void loadLastBook(char* out, size_t cap);
void saveLastBook(const char* bookPath);
