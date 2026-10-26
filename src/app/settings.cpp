#include "settings.h"

#include <Preferences.h>

namespace {

Preferences prefs;
const int kDefaultWpm = 300;

// NVS keys are limited to 15 chars, so use "p" + an FNV-1a hash of the path.
void positionKey(const char* path, char* key) {
    uint32_t h = 2166136261u;
    for (const char* c = path; *c; ++c) {
        h ^= (uint8_t)*c;
        h *= 16777619u;
    }
    snprintf(key, 12, "p%08lx", (unsigned long)h);
}

}

void beginSettings() {
    prefs.begin("reader", false);
}

int loadWpm() {
    return prefs.getInt("wpm", kDefaultWpm);
}

void saveWpm(int wpm) {
    if (loadWpm() != wpm) prefs.putInt("wpm", wpm);
}

uint32_t loadPosition(const char* bookPath) {
    char key[12];
    positionKey(bookPath, key);
    return prefs.getUInt(key, 0);
}

void savePosition(const char* bookPath, uint32_t pos) {
    char key[12];
    positionKey(bookPath, key);
    if (prefs.getUInt(key, 0) != pos) prefs.putUInt(key, pos);
}

void forgetPosition(const char* bookPath) {
    char key[12];
    positionKey(bookPath, key);
    prefs.remove(key);
}

void loadLastBook(char* out, size_t cap) {
    const String s = prefs.getString("last", "");
    strlcpy(out, s.c_str(), cap);
}

void saveLastBook(const char* bookPath) {
    if (prefs.getString("last", "") != bookPath) prefs.putString("last", bookPath);
}
