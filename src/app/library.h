#pragma once

#include <Arduino.h>

#include <vector>

struct BookInfo {
    char path[48];
    char title[84];
    char author[84];
    int percent;
    uint32_t size;
};

extern std::vector<BookInfo> library;

void scanLibrary();
int libraryIndexOf(const char* path);
