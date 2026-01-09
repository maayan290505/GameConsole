#pragma once
#include <Adafruit_SSD1306.h>
#include "Input.h"

class IGame {
public:
  virtual const char* name() const = 0;
  virtual const char* hint() const = 0;
  virtual uint16_t frameMs() const = 0; // how often this game wants update/render

  virtual void reset() = 0;
  virtual void update(const Inputs& in) = 0;
  virtual void render(Adafruit_SSD1306& d) = 0;

  virtual ~IGame() {}
};
