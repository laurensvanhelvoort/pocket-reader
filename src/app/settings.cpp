#include "settings.h"

#include <Preferences.h>

Options options;

namespace {

Preferences prefs;
const int kDefaultWpm = 300;

// NVS keys are limited to 15 chars, so use a letter + an FNV-1a hash of the path.
void bookKey(char kind, const char* path, char* key) {
    uint32_t h = 2166136261u;
    for (const char* c = path; *c; ++c) {
        h ^= (uint8_t)*c;
        h *= 16777619u;
    }
    snprintf(key, 12, "%c%08lx", kind, (unsigned long)h);
}

void putFlag(const char* key, bool value) {
    if (prefs.getBool(key, !value) != value) prefs.putBool(key, value);
}

}

void beginSettings() {
    prefs.begin("reader", false);
    options.brightness = prefs.getUChar("bright", options.brightness);
    options.progressLine = prefs.getBool("prog", options.progressLine);
    options.chapterTitles = prefs.getBool("chap", options.chapterTitles);
    options.rampUp = prefs.getBool("ramp", options.rampUp);
    options.pauseContext = prefs.getBool("ctx", options.pauseContext);
}

void saveOptions() {
    if (prefs.getUChar("bright", 0) != options.brightness) prefs.putUChar("bright", options.brightness);
    putFlag("prog", options.progressLine);
    putFlag("chap", options.chapterTitles);
    putFlag("ramp", options.rampUp);
    putFlag("ctx", options.pauseContext);
}

int loadWpm() {
    return prefs.getInt("wpm", kDefaultWpm);
}

void saveWpm(int wpm) {
    if (loadWpm() != wpm) prefs.putInt("wpm", wpm);
}

uint32_t loadPosition(const char* bookPath) {
    char key[12];
    bookKey('p', bookPath, key);
    return prefs.getUInt(key, 0);
}

void savePosition(const char* bookPath, uint32_t pos) {
    char key[12];
    bookKey('p', bookPath, key);
    if (prefs.getUInt(key, 0) != pos) prefs.putUInt(key, pos);
}

void touchBook(const char* bookPath) {
    char key[12];
    bookKey('o', bookPath, key);
    const uint32_t seq = prefs.getUInt("seq", 0);
    if (seq != 0 && prefs.getUInt(key, 0) == seq) return;
    prefs.putUInt("seq", seq + 1);
    prefs.putUInt(key, seq + 1);
}

uint32_t bookOrder(const char* bookPath) {
    char key[12];
    bookKey('o', bookPath, key);
    return prefs.getUInt(key, 0);
}

bool isFinished(const char* bookPath) {
    char key[12];
    bookKey('d', bookPath, key);
    return prefs.getBool(key, false);
}

void setFinished(const char* bookPath, bool finished) {
    char key[12];
    bookKey('d', bookPath, key);
    if (finished) {
        putFlag(key, true);
    } else if (prefs.getBool(key, false)) {
        prefs.remove(key);
    }
}

void forgetBook(const char* bookPath) {
    const char kinds[] = "pods";
    char key[12];
    for (int i = 0; kinds[i] != '\0'; ++i) {
        bookKey(kinds[i], bookPath, key);
        prefs.remove(key);
    }
}

bool loadStats(const char* bookPath, uint32_t size, BookStats& out) {
    char key[12];
    bookKey('s', bookPath, key);
    if (prefs.getBytesLength(key) != sizeof(BookStats)) return false;
    prefs.getBytes(key, &out, sizeof(BookStats));
    return out.size == size;
}

void saveStats(const char* bookPath, const BookStats& stats) {
    char key[12];
    bookKey('s', bookPath, key);
    prefs.putBytes(key, &stats, sizeof(BookStats));
}

void loadLastBook(char* out, size_t cap) {
    const String s = prefs.getString("last", "");
    strlcpy(out, s.c_str(), cap);
}

void saveLastBook(const char* bookPath) {
    if (prefs.getString("last", "") != bookPath) prefs.putString("last", bookPath);
}
