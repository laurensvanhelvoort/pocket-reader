#include "controls.h"

namespace {

const int kPinNext = 14;
const int kPinPrev = 0;
const int kPinEncA = 1;
const int kPinEncB = 2;
const int kPinPush = 10;

const uint32_t kDebounceMs = 20;
const uint32_t kHoldMs = 600;
const int kStepsPerClick = 4;

struct Button {
    int pin;
    bool down = false;
    bool raw = false;
    uint32_t changedAt = 0;
    uint32_t downAt = 0;
    bool holdSent = false;

    explicit Button(int p) : pin(p) {}

    int update(uint32_t now) {
        const bool r = digitalRead(pin) == LOW;
        if (r != raw) {
            raw = r;
            changedAt = now;
        }
        if (raw != down && now - changedAt >= kDebounceMs) {
            down = raw;
            if (down) {
                downAt = now;
                holdSent = false;
            }
            return down ? 1 : -1;
        }
        return 0;
    }
};

Button buttonNext{kPinNext};
Button buttonPrev{kPinPrev};
Button push{kPinPush};

const int8_t DRAM_ATTR kEncTable[16] = {0, -1, +1, 0, +1, 0, 0, -1, -1, 0, 0, +1, 0, +1, -1, 0};
volatile int32_t encSteps = 0;
volatile uint8_t encPrev = 0;
int32_t encUsed = 0;

void IRAM_ATTR onEncoder() {
    const uint8_t now = (digitalRead(kPinEncA) << 1) | digitalRead(kPinEncB);
    encSteps += kEncTable[(encPrev << 2) | now];
    encPrev = now;
}

}

void beginControls() {
    pinMode(kPinNext, INPUT_PULLUP);
    pinMode(kPinPrev, INPUT_PULLUP);
    pinMode(kPinPush, INPUT_PULLUP);
    pinMode(kPinEncA, INPUT_PULLUP);
    pinMode(kPinEncB, INPUT_PULLUP);
    encPrev = (digitalRead(kPinEncA) << 1) | digitalRead(kPinEncB);
    attachInterrupt(kPinEncA, onEncoder, CHANGE);
    attachInterrupt(kPinEncB, onEncoder, CHANGE);
}

Input readControls(uint32_t now) {
    Input in;

    noInterrupts();
    const int32_t steps = encSteps;
    interrupts();
    in.clicks = (steps - encUsed) / kStepsPerClick;
    encUsed += in.clicks * kStepsPerClick;

    in.next = buttonNext.update(now) > 0;
    in.prev = buttonPrev.update(now) > 0;

    const int edge = push.update(now);
    if (push.down && !push.holdSent && now - push.downAt >= kHoldMs) {
        push.holdSent = true;
        in.hold = true;
    }
    if (edge < 0 && !push.holdSent) in.select = true;

    return in;
}
