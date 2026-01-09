#pragma once
#include <Arduino.h>
#include "Buttons.h"

struct Inputs {
  int potL = 0;
  int potR = 0;
  BtnEvent startEv = BTN_NONE;
  BtnEvent selectEv = BTN_NONE;
};

class InputManager {
public:
  InputManager(int potLPin, int potRPin, int deadBand)
  : potLPin(potLPin), potRPin(potRPin), deadBand(deadBand) {}

  void begin(int startBtnPin, int selectBtnPin) {
    btnStart.begin(startBtnPin);
    btnSelect.begin(selectBtnPin);
  }

  Inputs read() {
    Inputs in;
    in.startEv = btnStart.update();
    in.selectEv = btnSelect.update();

    int l = analogRead(potLPin);
    int r = analogRead(potRPin);

    l = applyDeadband(l, lastL);
    r = applyDeadband(r, lastR);

    in.potL = l;
    in.potR = r;
    return in;
  }

private:
  int potLPin, potRPin;
  int deadBand;
  Button btnStart, btnSelect;

  int lastL = -1;
  int lastR = -1;

  int applyDeadband(int v, int &last) {
    if (last < 0) { last = v; return v; }
    if (abs(v - last) < deadBand) return last;
    last = v;
    return v;
  }
};
