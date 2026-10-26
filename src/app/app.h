#pragma once

#include <Arduino.h>

#include "book.h"
#include "controls.h"

struct Screen {
    void (*enter)(uint32_t now);
    void (*update)(const Input& in, uint32_t now);
    void (*leave)();
};

extern const Screen libraryScreen;
extern const Screen readerScreen;
extern const Screen menuScreen;
extern const Screen chaptersScreen;
extern const Screen uploadScreen;

void goTo(const Screen& screen);

extern Book book;

extern uint32_t readerJumpTo;  // 0 = resume the saved position
extern bool readerAutoPlay;

uint32_t readerPosition();
