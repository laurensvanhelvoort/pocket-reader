#pragma once

#include <Arduino.h>

void beginSettings();

int loadWpm();
void saveWpm(int wpm);

uint32_t loadPosition(const char* bookPath);
void savePosition(const char* bookPath, uint32_t pos);
void forgetPosition(const char* bookPath);

void loadLastBook(char* out, size_t cap);
void saveLastBook(const char* bookPath);
