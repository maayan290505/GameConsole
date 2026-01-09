#pragma once
#include "IGame.h"

class FlappyGame : public IGame {
public:
  const char* name() const override { return "FLAPPY"; }
  const char* hint() const override { return "SELECT = flap"; }

  void reset() override {
    birdY = 28;
    vel = 0;
    score = 0;

    pipeX1 = 128;
    pipeX2 = 128 + 64;

    int minC = (GAP_H / 2) + 6;
    int maxC = SCREEN_HEIGHT - (GAP_H / 2) - 6;
    gapY1 = random(minC, maxC + 1);
    gapY2 = random(minC, maxC + 1);

    passed1 = passed2 = false;
  }

  void update(const Inputs& in) override {
    if (in.selectEv == BTN_SHORT || in.selectEv == BTN_LONG) {
      vel = flapVel;
    }

    vel += gravity;
    birdY += vel;

    stepPipe(pipeX1, gapY1, passed1);
    stepPipe(pipeX2, gapY2, passed2);

    if (birdY < 0) birdY = 0;

    if (birdY + BIRD_SIZE >= SCREEN_HEIGHT) {
      reset();
      return;
    }

    if (!passed1 && pipeX1 + PIPE_W < BIRD_X) { score++; passed1 = true; }
    if (!passed2 && pipeX2 + PIPE_W < BIRD_X) { score++; passed2 = true; }

    if (collidePipe(pipeX1, gapY1) || collidePipe(pipeX2, gapY2)) {
      reset();
      return;
    }
  }

  void render(Adafruit_SSD1306& d) override {
    d.clearDisplay();

    d.setTextSize(1);
    d.setCursor(0, 0);
    d.print("Score:");
    d.print(score);

    drawPipe(d, pipeX1, gapY1);
    drawPipe(d, pipeX2, gapY2);

    d.fillRect(BIRD_X, (int)birdY, BIRD_SIZE, BIRD_SIZE, SSD1306_WHITE);

    // ground line
    d.drawFastHLine(0, SCREEN_HEIGHT - 1, SCREEN_WIDTH, SSD1306_WHITE);

    d.display();
  }

private:
  static const int SCREEN_WIDTH  = 128;
  static const int SCREEN_HEIGHT = 64;

  float birdY = 28;
  float vel = 0;
  int score = 0;

  static const int BIRD_X = 28;
  static const int BIRD_SIZE = 4;

  float gravity = 0.22f; 
  float flapVel = -3.0f;   

  static const int PIPE_W = 10;
  static const int GAP_H  = 28;

  int pipeX1 = 128;
  int pipeX2 = 128 + 64;
  int gapY1 = 30;
  int gapY2 = 26;

  bool passed1 = false;
  bool passed2 = false;

  uint16_t frameMs() const override { return 24; } // ~60fps

  void stepPipe(int &pipeX, int &gapY, bool &passed) {
    pipeX -= 1;
    if (pipeX < -PIPE_W) {
      pipeX = SCREEN_WIDTH;
      int minC = (GAP_H / 2) + 6;
      int maxC = SCREEN_HEIGHT - (GAP_H / 2) - 6;
      gapY = random(minC, maxC + 1);
      passed = false;
    }
  }

  bool collidePipe(int pipeX, int gapY) {
    int topH = gapY - (GAP_H / 2);
    int botY = gapY + (GAP_H / 2);

    int bx = BIRD_X;
    int by = (int)birdY;

    int bL = bx;
    int bR = bx + BIRD_SIZE;
    int bT = by;
    int bB = by + BIRD_SIZE;

    int pL = pipeX;
    int pR = pipeX + PIPE_W;

    if (bR >= pL && bL <= pR) {
      if (bT < topH) return true;
      if (bB > botY) return true;
    }
    return false;
  }

  void drawPipe(Adafruit_SSD1306& d, int pipeX, int gapY) {
    int topH = gapY - (GAP_H / 2);
    int botY = gapY + (GAP_H / 2);
    if (topH < 0) topH = 0;
    if (botY > SCREEN_HEIGHT) botY = SCREEN_HEIGHT;

    d.fillRect(pipeX, 0, PIPE_W, topH, SSD1306_WHITE);
    d.fillRect(pipeX, botY, PIPE_W, SCREEN_HEIGHT - botY, SSD1306_WHITE);
  }
};
