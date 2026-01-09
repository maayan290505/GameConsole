#pragma once
#include <Arduino.h>

enum BtnEvent { BTN_NONE, BTN_SHORT, BTN_LONG };

class Button {
public:
  void begin(int p) {
    pin = p;
    pinMode(pin, INPUT_PULLUP);
  }

  BtnEvent update() {
    BtnEvent ev = BTN_NONE;
    bool raw = digitalRead(pin); // HIGH idle, LOW pressed
    unsigned long now = millis();

    // debounce
    if (raw != lastRaw) {
      lastRaw = raw;
      lastChangeMs = now;
    }
    if (now - lastChangeMs < DEBOUNCE_MS) return BTN_NONE;

    // stable changed?
    if (stable != raw) {
      stable = raw;

      if (stable == LOW) {
        isDown = true;
        longFired = false;
        downStartMs = now;
      } else {
        if (isDown) {
          isDown = false;
          if (!longFired) ev = BTN_SHORT;
        }
      }
    }

    // long fires immediately (no need to release)
    if (isDown && !longFired && (now - downStartMs >= LONG_MS)) {
      longFired = true;
      ev = BTN_LONG;
    }

    return ev;
  }

private:
  int pin = -1;

  bool lastRaw = true;
  bool stable = true;
  unsigned long lastChangeMs = 0;

  bool isDown = false;
  bool longFired = false;
  unsigned long downStartMs = 0;

  static const unsigned long DEBOUNCE_MS = 25;
  static const unsigned long LONG_MS = 450;
};
