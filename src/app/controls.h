#pragma once

#include <Arduino.h>

struct Input {
    int clicks = 0;
    bool select = false;
    bool hold = false;
    bool next = false;
    bool prev = false;
};

void beginControls();
Input readControls(uint32_t now);
