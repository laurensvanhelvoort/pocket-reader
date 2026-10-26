#pragma once

#include <Arduino.h>

struct Parts {
    char before[72];
    char orp[8];
    char after[72];
    int letters;
};

void splitWord(const char* w, Parts& p);

enum class Ending { None, Clause, Sentence };
Ending wordEnding(const char* w);

uint32_t durationMs(int letters, Ending ending, bool paragraphEnd, int wpm);

const int kMaxChunks = 4;
struct Chunk {
    char text[72];
};
int chunkWord(const char* w, Chunk* out, bool (*fits)(const char*));

uint32_t decodeUtf8(const char* s, int& i);
void truncateUtf8(char* s, int maxBytes);
void copyUtf8(char* dst, const char* src, size_t cap);
void trimBrokenUtf8(char* s);
