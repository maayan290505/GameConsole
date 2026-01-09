#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ===================== Display =====================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ===================== Pins =====================
// Pots (you said ADC on 1 & 2)
const int POT_LEFT_PIN   = 1;   // GPIO1 = A1 (ADC)  (Flappy uses this)
const int POT_RIGHT_PIN  = 2;   // GPIO2 = A2 (ADC)  (Pong right paddle)
const int START_BTN_PIN  = 20;  // GPIO20, button to GND (INPUT_PULLUP)

// ===================== Common input tuning =====================
static const int ADC_MAX_ASSUMED = 4095;     // we still map with calibration where needed
static const int DEAD_BAND = 10;             // reduce tiny jitter
static const int POT_DELTA_TRIGGER = 500;    // Flappy jump threshold
static const unsigned long FLAP_COOLDOWN_MS = 180;

// ===================== Button events (short/long) =====================
enum BtnEvent { BTN_NONE, BTN_SHORT, BTN_LONG };

struct Button {
  int pin;
  bool lastRaw = true;
  bool stable = true;

  unsigned long lastChangeMs = 0;

  bool isDown = false;
  bool longFired = false;
  unsigned long downStartMs = 0;

  static const unsigned long DEBOUNCE_MS = 25;
  static const unsigned long LONG_MS = 450;

  void begin(int p) {
    pin = p;
    pinMode(pin, INPUT_PULLUP);
  }

  BtnEvent update() {
    BtnEvent ev = BTN_NONE;
    bool raw = digitalRead(pin); // HIGH idle, LOW pressed
    unsigned long now = millis();

    // debounce input
    if (raw != lastRaw) {
      lastRaw = raw;
      lastChangeMs = now;
    }
    if (now - lastChangeMs < DEBOUNCE_MS) return BTN_NONE;

    // stable state changed?
    if (stable != raw) {
      stable = raw;

      if (stable == LOW) {
        // pressed
        isDown = true;
        longFired = false;
        downStartMs = now;
      } else {
        // released
        if (isDown) {
          isDown = false;
          if (!longFired) ev = BTN_SHORT;  // only short if long wasn't fired
        }
      }
    }

    // long press fires immediately when threshold is crossed
    if (isDown && !longFired && (now - downStartMs >= LONG_MS)) {
      longFired = true;
      ev = BTN_LONG;
    }

    return ev;
  }
};

Button btn;

// ===================== App state =====================
enum ScreenState { MAIN_MENU, GAME_MENU, IN_GAME };
enum GameId { GAME_PONG, GAME_FLAPPY, GAME_COUNT };

ScreenState screenState = MAIN_MENU;
GameId selectedGame = GAME_PONG;
GameId activeGame = GAME_PONG;

// Pots raw (kept globally)
int rawL = 0;
int rawR = 0;

// Deadband helper
int applyDeadband(int v, int &last) {
  if (last < 0) { last = v; return v; }
  if (abs(v - last) < DEAD_BAND) return last;
  last = v;
  return v;
}

// ===================== Pong =====================
const int PADDLE_W = 3;
const int PADDLE_H = 16;
const int LEFT_X  = 2;
const int RIGHT_X = SCREEN_WIDTH - 2 - PADDLE_W;

const int BALL_SIZE = 3;
float pongBallX, pongBallY;
float pongVX, pongVY;
int pongLeftY  = (SCREEN_HEIGHT - PADDLE_H) / 2;
int pongRightY = (SCREEN_HEIGHT - PADDLE_H) / 2;
int pongScoreL = 0, pongScoreR = 0;

// Pong pot calibration (so paddles reach bottom even if max is ~3400)
int pongMinL = 4095, pongMaxL = 0;
int pongMinR = 4095, pongMaxR = 0;

int mapPotToPaddle(int v, int vmin, int vmax) {
  if (vmax - vmin < 50) return (SCREEN_HEIGHT - PADDLE_H) / 2;
  long y = map(v, vmin, vmax, 0, SCREEN_HEIGHT - PADDLE_H);
  if (y < 0) y = 0;
  if (y > SCREEN_HEIGHT - PADDLE_H) y = SCREEN_HEIGHT - PADDLE_H;
  return (int)y;
}

void pongResetBall(bool toRight) {
  pongBallX = SCREEN_WIDTH / 2.0f;
  pongBallY = SCREEN_HEIGHT / 2.0f;

  float speedX = 1.9f; // change ball speed here
  float speedY = 1.2f;

  pongVX = toRight ? speedX : -speedX;
  pongVY = (random(0, 2) == 0) ? speedY : -speedY;
}

void pongStartNewGame() {
  pongScoreL = pongScoreR = 0;
  pongLeftY  = (SCREEN_HEIGHT - PADDLE_H) / 2;
  pongRightY = (SCREEN_HEIGHT - PADDLE_H) / 2;

  pongMinL = 4095; pongMaxL = 0;
  pongMinR = 4095; pongMaxR = 0;

  pongResetBall(true);
}

void pongBounce(int paddleY, bool isRight) {
  float paddleCenter = paddleY + PADDLE_H / 2.0f;
  float ballCenter   = pongBallY + BALL_SIZE / 2.0f;
  float rel = (ballCenter - paddleCenter) / (PADDLE_H / 2.0f);
  if (rel < -1.0f) rel = -1.0f;
  if (rel >  1.0f) rel =  1.0f;

  pongVX = -pongVX;
  pongVY = rel * 2.2f;

  // optional speed-up
  pongVX *= 1.02f;

  if (isRight) pongBallX = RIGHT_X - BALL_SIZE - 1;
  else         pongBallX = LEFT_X + PADDLE_W + 1;
}

void pongUpdate() {
  // Calibrate pot ranges
  if (rawL < pongMinL) pongMinL = rawL;
  if (rawL > pongMaxL) pongMaxL = rawL;
  if (rawR < pongMinR) pongMinR = rawR;
  if (rawR > pongMaxR) pongMaxR = rawR;

  // Move paddles
  pongLeftY  = mapPotToPaddle(rawL, pongMinL, pongMaxL);
  pongRightY = mapPotToPaddle(rawR, pongMinR, pongMaxR);

  // Move ball
  pongBallX += pongVX;
  pongBallY += pongVY;

  // Walls
  if (pongBallY <= 0) { pongBallY = 0; pongVY = -pongVY; }
  if (pongBallY >= SCREEN_HEIGHT - BALL_SIZE) { pongBallY = SCREEN_HEIGHT - BALL_SIZE; pongVY = -pongVY; }

  // Paddle collisions
  if (pongVX < 0 &&
      pongBallX <= (LEFT_X + PADDLE_W) &&
      (pongBallY + BALL_SIZE) >= pongLeftY &&
      pongBallY <= (pongLeftY + PADDLE_H)) {
    pongBounce(pongLeftY, false);
  }

  if (pongVX > 0 &&
      (pongBallX + BALL_SIZE) >= RIGHT_X &&
      (pongBallY + BALL_SIZE) >= pongRightY &&
      pongBallY <= (pongRightY + PADDLE_H)) {
    pongBounce(pongRightY, true);
  }

  // Scoring
  if (pongBallX < -10) { pongScoreR++; pongResetBall(true); }
  if (pongBallX > SCREEN_WIDTH + 10) { pongScoreL++; pongResetBall(false); }
}

void pongRender() {
  display.clearDisplay();

  // Scores
  display.setTextSize(1);
  display.setCursor(40, 0); display.print(pongScoreL);
  display.setCursor(82, 0); display.print(pongScoreR);

  // Center line
  for (int y = 0; y < SCREEN_HEIGHT; y += 6) {
    display.drawFastVLine(SCREEN_WIDTH / 2, y, 3, SSD1306_WHITE);
  }

  // Paddles
  display.fillRect(LEFT_X, pongLeftY, PADDLE_W, PADDLE_H, SSD1306_WHITE);
  display.fillRect(RIGHT_X, pongRightY, PADDLE_W, PADDLE_H, SSD1306_WHITE);

  // Ball
  display.fillRect((int)pongBallX, (int)pongBallY, BALL_SIZE, BALL_SIZE, SSD1306_WHITE);

  display.display();
}

// ===================== Flappy Bird (pot-jump) =====================
struct Flappy {
  float birdY = 28;
  float vel = 0;
  int score = 0;

  // Bird
  static const int BIRD_X = 28;
  static const int BIRD_SIZE = 4;

  // Physics
  float gravity = 0.35f;
  float flapVel = -3.6f;

  // Pipes
  static const int PIPE_W = 10;
  static const int GAP_H  = 20;
  int pipeX1 = 128;
  int pipeX2 = 128 + 64;
  int gapY1 = 22;
  int gapY2 = 30;

  bool passed1 = false;
  bool passed2 = false;

  // Pot trigger
  int lastPot = -1;
  unsigned long lastFlapMs = 0;

  void reset() {
    birdY = 28;
    vel = 0;
    score = 0;
    pipeX1 = 128;
    pipeX2 = 128 + 64;
    gapY1 = 18 + (random(0, 28)); // keeps gap inside 64px
    gapY2 = 18 + (random(0, 28));
    passed1 = passed2 = false;
    lastPot = -1;
    lastFlapMs = 0;
  }

  void flapIfPotMoved(int potVal) {
    unsigned long now = millis();

    if (lastPot < 0) {  // first sample
      lastPot = potVal;
      return;
    }

    int d = abs(potVal - lastPot);

    // Always update lastPot every frame (so delta is between consecutive samples)
    lastPot = potVal;

    if (d >= POT_DELTA_TRIGGER && (now - lastFlapMs) >= FLAP_COOLDOWN_MS) {
      vel = flapVel;          // jump
      lastFlapMs = now;
    }
  }


  void stepPipes(int &pipeX, int &gapY, bool &passed) {
    pipeX -= 2;
    if (pipeX < -PIPE_W) {
      pipeX = SCREEN_WIDTH;
      gapY = 18 + random(0, 28);
      passed = false;
    }
  }

  bool collidePipe(int pipeX, int gapY) {
    // Pipe body rectangles (top and bottom)
    int topH = gapY - (GAP_H / 2);
    int botY = gapY + (GAP_H / 2);
    int botH = SCREEN_HEIGHT - botY;

    int bx = BIRD_X;
    int by = (int)birdY;

    // Bird AABB
    int bL = bx;
    int bR = bx + BIRD_SIZE;
    int bT = by;
    int bB = by + BIRD_SIZE;

    int pL = pipeX;
    int pR = pipeX + PIPE_W;

    // If x overlaps pipe
    if (bR >= pL && bL <= pR) {
      // collide with top pipe
      if (bT < topH) return true;
      // collide with bottom pipe
      if (bB > botY) return true;
    }
    return false;
  }

  void update(int potVal) {
    // Pot-controlled flap
    flapIfPotMoved(potVal);

    // Physics
    vel += gravity;
    birdY += vel;

    // Pipes
    stepPipes(pipeX1, gapY1, passed1);
    stepPipes(pipeX2, gapY2, passed2);

    // Ground / ceiling
    if (birdY < 0) birdY = 0;
    if (birdY > SCREEN_HEIGHT - BIRD_SIZE) {
      // hit ground => reset like flappy
      reset();
      return;
    }

    // Score when passing pipes
    if (!passed1 && pipeX1 + PIPE_W < BIRD_X) { score++; passed1 = true; }
    if (!passed2 && pipeX2 + PIPE_W < BIRD_X) { score++; passed2 = true; }

    // Collisions
    if (collidePipe(pipeX1, gapY1) || collidePipe(pipeX2, gapY2)) {
      reset();
      return;
    }
  }

  void drawPipe(int pipeX, int gapY) {
    int topH = gapY - (GAP_H / 2);
    int botY = gapY + (GAP_H / 2);

    if (topH < 0) topH = 0;
    if (botY > SCREEN_HEIGHT) botY = SCREEN_HEIGHT;

    // top pipe
    display.fillRect(pipeX, 0, PIPE_W, topH, SSD1306_WHITE);
    // bottom pipe
    display.fillRect(pipeX, botY, PIPE_W, SCREEN_HEIGHT - botY, SSD1306_WHITE);
  }

  void render() {
    display.clearDisplay();

    // Score
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("Score:");
    display.print(score);

    // Pipes
    drawPipe(pipeX1, gapY1);
    drawPipe(pipeX2, gapY2);

    // Bird
    display.fillRect(BIRD_X, (int)birdY, BIRD_SIZE, BIRD_SIZE, SSD1306_WHITE);

    display.display();
  }
};

Flappy flappy;

void flappyStartNewGame() {
  flappy.reset();
}

// ===================== Menu rendering =====================
const char* gameName(GameId g) {
  switch (g) {
    case GAME_PONG:   return "PONG";
    case GAME_FLAPPY: return "FLAPPY";
    default:          return "GAME";
  }
}

void renderMainMenu() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(18, 6);
  display.print("GAMES");

  display.setTextSize(1);
  display.setCursor(10, 30);
  display.print("Select: ");
  display.print(gameName(selectedGame));

  display.setCursor(10, 44);
  display.print("Short: next game");
  display.setCursor(10, 54);
  display.print("Long : open menu");

  display.display();
}

void renderGameMenu(GameId g) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(10, 6);
  display.print(gameName(g));

  display.setTextSize(1);
  display.setCursor(10, 30);
display.print("Short: START game");
display.setCursor(10, 44);
display.print("Long : MAIN menu");

  // small hint
  if (g == GAME_FLAPPY) {
    display.setCursor(10, 56);
    display.print("Jump: pot delta >=500");
  }

  display.display();
}

// ===================== App flow =====================
void enterMainMenu() {
  screenState = MAIN_MENU;
  activeGame = selectedGame;
}

void enterGameMenu(GameId g) {
  activeGame = g;
  screenState = GAME_MENU;
}

void startGame(GameId g) {
  activeGame = g;
  screenState = IN_GAME;

  if (g == GAME_PONG) pongStartNewGame();
  if (g == GAME_FLAPPY) flappyStartNewGame();
}

// ===================== Setup/Loop =====================
unsigned long lastFrameMs = 0;
const unsigned long FRAME_MS = 16;

void setup() {
  randomSeed((uint32_t)micros());

  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;) delay(10);
  }

  btn.begin(START_BTN_PIN);

  // Start at main menu
  screenState = MAIN_MENU;
  selectedGame = GAME_PONG;
  activeGame = GAME_PONG;
}

void loop() {
  // ---- Read inputs
  BtnEvent ev = btn.update();

  rawL = analogRead(POT_LEFT_PIN);
  rawR = analogRead(POT_RIGHT_PIN);

  static int lastL = -1, lastR = -1;
  rawL = applyDeadband(rawL, lastL);
  rawR = applyDeadband(rawR, lastR);

  // ---- Handle events based on state
  if (screenState == MAIN_MENU) {
    if (ev == BTN_SHORT) {
      selectedGame = (GameId)((selectedGame + 1) % GAME_COUNT);
    } else if (ev == BTN_LONG) {
      enterGameMenu(selectedGame);
    }

    renderMainMenu();
    delay(20);
    return;
  }

  if (screenState == GAME_MENU) {
    if (ev == BTN_SHORT) {
      startGame(activeGame);   // SHORT starts the game
      delay(80);
      return;
    } else if (ev == BTN_LONG) {
      enterMainMenu();         // LONG returns to main menu
      renderMainMenu();
      delay(80);
      return;
    }
    renderGameMenu(activeGame);
    delay(20);
    return;
  }

  // IN_GAME:
  // - short: go to game menu
  // - long : go to main menu
  if (screenState == IN_GAME) {
    if (ev == BTN_SHORT) {
      enterGameMenu(activeGame);
      renderGameMenu(activeGame);
      delay(80);
      return;
    } else if (ev == BTN_LONG) {
      enterMainMenu();
      renderMainMenu();
      delay(80);
      return;
    }

    // frame timing
    unsigned long now = millis();
    if (now - lastFrameMs < FRAME_MS) return;
    lastFrameMs = now;

    // update + render per game
    if (activeGame == GAME_PONG) {
      pongUpdate();
      pongRender();
    } else if (activeGame == GAME_FLAPPY) {
      // Flappy uses LEFT pot only
      flappy.update(rawL);
      flappy.render();
    }

    return;
  }
}
