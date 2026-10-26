#include <LittleFS.h>

#include "app.h"
#include "fonts.h"
#include "settings.h"
#include "ui.h"

namespace {

const Screen* current = nullptr;
const Screen* pending = nullptr;

}

void goTo(const Screen& screen) {
    pending = &screen;
}

void setup() {
    Serial.begin(115200);
    startDisplay(lcd);
    if (!loadFonts()) Serial.println("pocket-reader: fonts laden mislukt");
    initUi();
    beginControls();
    beginSettings();
    if (!LittleFS.begin(true)) Serial.println("pocket-reader: LittleFS start niet");
    goTo(libraryScreen);
}

void loop() {
    const uint32_t now = millis();
    if (pending != nullptr) {
        if (current != nullptr && current->leave != nullptr) current->leave();
        current = pending;
        pending = nullptr;
        lcd.fillScreen(TFT_BLACK);
        current->enter(now);
    }
    const Input in = readControls(now);
    current->update(in, now);
    delay(1);
}
