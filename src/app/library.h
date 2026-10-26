#pragma once

#include <Arduino.h>

#include <vector>

struct BookInfo {
    char path[48];
    char title[84];
    char author[84];
    int percent;
    uint32_t size;
    uint32_t order;
    bool finished;
};

extern std::vector<BookInfo> library;

// Recently read or added first, finished books last.
void scanLibrary();
int libraryIndexOf(const char* path);
