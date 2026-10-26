#pragma once

#include <Arduino.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class Display : public lgfx::LGFX_Device {
    lgfx::Bus_Parallel8 bus;
    lgfx::Panel_ST7789 panel;
    lgfx::Light_PWM light;

public:
    Display() {
        auto b = bus.config();
        b.freq_write = 16000000;
        b.pin_wr = 8;
        b.pin_rd = 9;
        b.pin_rs = 7;  // DC
        b.pin_d0 = 39; b.pin_d1 = 40; b.pin_d2 = 41; b.pin_d3 = 42;
        b.pin_d4 = 45; b.pin_d5 = 46; b.pin_d6 = 47; b.pin_d7 = 48;
        bus.config(b);
        panel.setBus(&bus);

        auto p = panel.config();
        p.pin_cs = 6;
        p.pin_rst = 5;
        p.panel_width = 170;
        p.panel_height = 320;
        p.offset_x = 35;
        p.invert = true;
        p.readable = false;
        panel.config(p);

        auto l = light.config();
        l.pin_bl = 38;
        light.config(l);
        panel.setLight(&light);

        setPanel(&panel);
    }
};

inline void startDisplay(Display& lcd) {
    pinMode(15, OUTPUT);  // display power
    digitalWrite(15, HIGH);
    lcd.init();
    lcd.setRotation(3);
    lcd.setBrightness(180);
    lcd.fillScreen(TFT_BLACK);
}
