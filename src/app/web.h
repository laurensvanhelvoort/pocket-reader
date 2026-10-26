#pragma once

#include <Arduino.h>

extern const char* const kWifiName;

void webBegin();
void webEnd();
void webLoop();

const char* webAddress();
int webStations();
uint32_t webFreeBytes();

struct WebStatus {
    bool receiving = false;
    uint32_t received = 0;
    char lastTitle[84] = "";
    char lastError[64] = "";
    uint32_t changes = 0;
};
const WebStatus& webStatus();

// Called during an upload, while webLoop() is blocked.
void setWebProgressHook(void (*hook)());
