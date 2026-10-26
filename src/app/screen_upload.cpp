#include "app.h"
#include "fonts.h"
#include "text.h"
#include "ui.h"
#include "web.h"

namespace {

uint32_t lastChanges = 0;
int lastStations = -1;
uint32_t lastDrawAt = 0;

void formatMb(char* out, size_t cap, uint32_t bytes) {
    const uint32_t tenths = (bytes + 52428) / 104858;
    snprintf(out, cap, "%lu,%lu MB vrij", (unsigned long)(tenths / 10), (unsigned long)(tenths % 10));
}

void draw() {
    const WebStatus& s = webStatus();
    char text[128];
    char fitted[128];

    frame.fillScreen(TFT_BLACK);
    drawCorner(frame, Corner::TopLeft, "toevoegen", grey(kInfoGrey));
    formatMb(text, sizeof(text), webFreeBytes());
    drawCorner(frame, Corner::TopRight, text, grey(kInfoGrey));
    drawCorner(frame, Corner::BottomLeft, "klaar: lang drukken", grey(90));

    frame.setTextDatum(baseline_center);
    frame.setFont(&fontSmall.font);
    frame.setTextColor(grey(150));
    frame.drawString("verbind met wifi", kScreenW / 2, 56);
    frame.setFont(&fontTitle.font);
    frame.setTextColor(grey(255));
    frame.drawString(kWifiName, kScreenW / 2, 92);
    frame.setFont(&fontMono.font);
    frame.setTextColor(grey(kInfoGrey));
    snprintf(text, sizeof(text), "en open %s", webAddress());
    frame.drawString(text, kScreenW / 2, 114);
    drawWedgeH(frame, 85, kMargin, +1);
    drawWedgeH(frame, 85, kScreenW - kMargin - 1, -1);

    uint16_t color = grey(150);
    if (s.receiving) {
        snprintf(text, sizeof(text), "ontvangen: %lu kB", (unsigned long)(s.received / 1024));
    } else if (s.lastError[0] != '\0') {
        snprintf(text, sizeof(text), "mislukt: %s", s.lastError);
        color = lcd.color565(226, 131, 111);
    } else if (s.lastTitle[0] != '\0') {
        snprintf(text, sizeof(text), "toegevoegd: %s", s.lastTitle);
        color = gold();
    } else if (webStations() > 0) {
        copyUtf8(text, "verbonden", sizeof(text));
    } else {
        text[0] = '\0';
    }
    frame.setFont(&fontSmall.font);
    frame.setTextColor(color);
    fitText(frame, text, kScreenW - 2 * kMargin, fitted, sizeof(fitted));
    frame.drawString(fitted, kScreenW / 2, 140);

    frame.pushSprite(0, 0);
    lastDrawAt = millis();
}

void enter(uint32_t) {
    book.close();
    webBegin();
    setWebProgressHook(draw);
    lastChanges = webStatus().changes;
    lastStations = webStations();
    draw();
}

void update(const Input& in, uint32_t now) {
    if (in.hold || in.prev) {
        goTo(libraryScreen);
        return;
    }
    webLoop();

    const int stations = webStations();
    const WebStatus& s = webStatus();
    if (s.changes != lastChanges || stations != lastStations || (now - lastDrawAt > 2000)) {
        lastChanges = s.changes;
        lastStations = stations;
        draw();
    }
}

void leave() {
    setWebProgressHook(nullptr);
    webEnd();
}

}

const Screen uploadScreen = {enter, update, leave};
