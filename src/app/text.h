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

float durationFactor(int letters, Ending ending, bool paragraphEnd);
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

// "Chapter V — Family Affairs" -> "Chapter V" and "Family Affairs". False if there is no dash.
bool splitChapterTitle(const char* title, char* prefix, size_t cap, const char** rest);

// Appends only the letters and digits of s, lowercased, to out.
void appendLetterKey(const char* s, char* out, size_t cap);

void formatMinutes(uint32_t minutes, char* out, size_t cap);
void formatCount(uint32_t n, char* out, size_t cap);
