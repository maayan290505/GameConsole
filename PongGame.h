#pragma once
#include "IGame.h"

class PongGame : public IGame {
public:
  PongGame(int adcMaxReal) : ADC_MAX_REAL(adcMaxReal) {}

  const char* name() const override { return "PONG"; }
  const char* hint() const override { return "Pots = paddles"; }

  void reset() override {
    scoreL = scoreR = 0;
    leftY  = (SCREEN_HEIGHT - PADDLE_H) / 2;
    rightY = (SCREEN_HEIGHT - PADDLE_H) / 2;
    resetBall(true);
  }

  void update(const Inputs& in) override {
    leftY  = mapPotToPaddle(in.potL);
    rightY = mapPotToPaddle(in.potR);

    ballX += vx;
    ballY += vy;

    if (ballY <= 0) { ballY = 0; vy = -vy; }
    if (ballY >= SCREEN_HEIGHT - BALL_SIZE) { ballY = SCREEN_HEIGHT - BALL_SIZE; vy = -vy; }

    // left paddle
    if (vx < 0 &&
        ballX <= (LEFT_X + PADDLE_W) &&
        (ballY + BALL_SIZE) >= leftY &&
        ballY <= (leftY + PADDLE_H)) {
      bounce(leftY, false);
    }

    // right paddle
    if (vx > 0 &&
        (ballX + BALL_SIZE) >= RIGHT_X &&
        (ballY + BALL_SIZE) >= rightY &&
        ballY <= (rightY + PADDLE_H)) {
      bounce(rightY, true);
    }

    if (ballX < -10) { scoreR++; resetBall(true); }
    if (ballX > SCREEN_WIDTH + 10) { scoreL++; resetBall(false); }
  }

  void render(Adafruit_SSD1306& d) override {
    d.clearDisplay();

    d.setTextSize(1);
    d.setCursor(40, 0); d.print(scoreL);
    d.setCursor(82, 0); d.print(scoreR);

    for (int y = 0; y < SCREEN_HEIGHT; y += 6) {
      d.drawFastVLine(SCREEN_WIDTH / 2, y, 3, SSD1306_WHITE);
    }

    d.fillRect(LEFT_X, leftY, PADDLE_W, PADDLE_H, SSD1306_WHITE);
    d.fillRect(RIGHT_X, rightY, PADDLE_W, PADDLE_H, SSD1306_WHITE);
    d.fillRect((int)ballX, (int)ballY, BALL_SIZE, BALL_SIZE, SSD1306_WHITE);

    d.display();
  }

  // Change speed anytime (optional API)
  void setBallSpeed(float sx, float sy) { baseSpeedX = sx; baseSpeedY = sy; }

private:
  // Screen constants
  static const int SCREEN_WIDTH  = 128;
  static const int SCREEN_HEIGHT = 64;

  static const int PADDLE_W = 3;
  static const int PADDLE_H = 16;
  static const int LEFT_X   = 2;
  static const int RIGHT_X  = SCREEN_WIDTH - 2 - PADDLE_W;
  static const int BALL_SIZE = 3;

  const int ADC_MAX_REAL;

  float ballX = SCREEN_WIDTH / 2.0f;
  float ballY = SCREEN_HEIGHT / 2.0f;
  float vx = 1.9f;
  float vy = 1.2f;

  float baseSpeedX = 1.9f;
  float baseSpeedY = 1.2f;

  int leftY  = (SCREEN_HEIGHT - PADDLE_H) / 2;
  int rightY = (SCREEN_HEIGHT - PADDLE_H) / 2;

  int scoreL = 0, scoreR = 0;

  uint16_t frameMs() const override { return 16; } // ~60fps

  int mapPotToPaddle(int v) {
    if (v < 0) v = 0;
    if (v > ADC_MAX_REAL) v = ADC_MAX_REAL;

    long y = map(v, 0, ADC_MAX_REAL, 0, SCREEN_HEIGHT - PADDLE_H);
    if (y < 0) y = 0;
    if (y > SCREEN_HEIGHT - PADDLE_H) y = SCREEN_HEIGHT - PADDLE_H;
    return (int)y;
  }

  void resetBall(bool toRight) {
    ballX = SCREEN_WIDTH / 2.0f;
    ballY = SCREEN_HEIGHT / 2.0f;

    vx = toRight ? baseSpeedX : -baseSpeedX;
    vy = (random(0, 2) == 0) ? baseSpeedY : -baseSpeedY;
  }

  void bounce(int paddleY, bool isRight) {
    float paddleCenter = paddleY + PADDLE_H / 2.0f;
    float ballCenter   = ballY + BALL_SIZE / 2.0f;
    float rel = (ballCenter - paddleCenter) / (PADDLE_H / 2.0f);
    if (rel < -1.0f) rel = -1.0f;
    if (rel >  1.0f) rel =  1.0f;

    vx = -vx;
    vy = rel * 2.2f;

    vx *= 1.01f;

    if (isRight) ballX = RIGHT_X - BALL_SIZE - 1;
    else         ballX = LEFT_X + PADDLE_W + 1;
  }
};
