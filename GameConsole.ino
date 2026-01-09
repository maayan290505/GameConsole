#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ===================== Display =====================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ===================== Pins =====================
// Pots (ADC on 1 & 2)
const int POT_LEFT_PIN   = 1;   // GPIO1 (ADC)  (Pong left paddle, Flappy optional)
const int POT_RIGHT_PIN  = 2;   // GPIO2 (ADC)  (Pong right paddle)

// Buttons
const int START_BTN_PIN  = 20;  // "BACK / MENU"
const int SELECT_BTN_PIN = 19;  // "SELECT / ACTION"

// ===================== Input tuning =====================
static const int DEAD_BAND = 10;        // reduce tiny jitter
static const int ADC_MAX_REAL = 3400;   // you reported ~3400 max (adjust if needed)

// ===================== Button events =====================
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
};

Button btnStart;
Button btnSelect;

// ===================== App state =====================
enum ScreenState { MAIN_MENU, GAME_MENU, IN_GAME };
enum GameId { GAME_PONG, GAME_FLAPPY, GAME_COUNT };

ScreenState screenState = MAIN_MENU;
GameId selectedGame = GAME_PONG;
GameId activeGame = GAME_PONG;

// ===================== Pots =====================
int rawL = 0;
int rawR = 0;

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

int mapPotToPaddleFixed(int v) {
  if (v < 0) v = 0;
  if (v > ADC_MAX_REAL) v = ADC_MAX_REAL;

  long y = map(v, 0, ADC_MAX_REAL, 0, SCREEN_HEIGHT - PADDLE_H);
  if (y < 0) y = 0;
  if (y > SCREEN_HEIGHT - PADDLE_H) y = SCREEN_HEIGHT - PADDLE_H;
  return (int)y;
}

void pongResetBall(bool toRight) {
  pongBallX = SCREEN_WIDTH / 2.0f;
  pongBallY = SCREEN_HEIGHT / 2.0f;

  float speedX = 1.9f; // ball speed
  float speedY = 1.2f;

  pongVX = toRight ? speedX : -speedX;
  pongVY = (random(0, 2) == 0) ? speedY : -speedY;
}

void pongStartNewGame() {
  pongScoreL = pongScoreR = 0;
  pongLeftY  = (SCREEN_HEIGHT - PADDLE_H) / 2;
  pongRightY = (SCREEN_HEIGHT - PADDLE_H) / 2;
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

  // optional slight speed-up:
  pongVX *= 1.01f;

  if (isRight) pongBallX = RIGHT_X - BALL_SIZE - 1;
  else         pongBallX = LEFT_X + PADDLE_W + 1;
}

void pongUpdate() {
  // paddles from pots (fixed mapping so you reach top/bottom immediately)
  pongLeftY  = mapPotToPaddleFixed(rawL);
  pongRightY = mapPotToPaddleFixed(rawR);

  // ball
  pongBallX += pongVX;
  pongBallY += pongVY;

  // walls
  if (pongBallY <= 0) { pongBallY = 0; pongVY = -pongVY; }
  if (pongBallY >= SCREEN_HEIGHT - BALL_SIZE) { pongBallY = SCREEN_HEIGHT - BALL_SIZE; pongVY = -pongVY; }

  // left paddle
  if (pongVX < 0 &&
      pongBallX <= (LEFT_X + PADDLE_W) &&
      (pongBallY + BALL_SIZE) >= pongLeftY &&
      pongBallY <= (pongLeftY + PADDLE_H)) {
    pongBounce(pongLeftY, false);
  }

  // right paddle
  if (pongVX > 0 &&
      (pongBallX + BALL_SIZE) >= RIGHT_X &&
      (pongBallY + BALL_SIZE) >= pongRightY &&
      pongBallY <= (pongRightY + PADDLE_H)) {
    pongBounce(pongRightY, true);
  }

  // score
  if (pongBallX < -10) { pongScoreR++; pongResetBall(true); }
  if (pongBallX > SCREEN_WIDTH + 10) { pongScoreL++; pongResetBall(false); }
}

void pongRender() {
  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(40, 0); display.print(pongScoreL);
  display.setCursor(82, 0); display.print(pongScoreR);

  for (int y = 0; y < SCREEN_HEIGHT; y += 6) {
    display.drawFastVLine(SCREEN_WIDTH / 2, y, 3, SSD1306_WHITE);
  }

  display.fillRect(LEFT_X, pongLeftY, PADDLE_W, PADDLE_H, SSD1306_WHITE);
  display.fillRect(RIGHT_X, pongRightY, PADDLE_W, PADDLE_H, SSD1306_WHITE);

  display.fillRect((int)pongBallX, (int)pongBallY, BALL_SIZE, BALL_SIZE, SSD1306_WHITE);

  display.display();
}

// ===================== Flappy (SELECT to flap) =====================
struct Flappy {
  float birdY = 28;
  float vel = 0;
  int score = 0;

  static const int BIRD_X = 28;
  static const int BIRD_SIZE = 4;

  float gravity = 0.35f;
  float flapVel = -3.8f;

  static const int PIPE_W = 10;
  static const int GAP_H  = 20;

  int pipeX1 = 128;
  int pipeX2 = 128 + 64;
  int gapY1 = 30;
  int gapY2 = 26;

  bool passed1 = false;
  bool passed2 = false;

  void reset() {
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

  void flap() { vel = flapVel; }

  void stepPipe(int &pipeX, int &gapY, bool &passed) {
    pipeX -= 2;
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

  void update() {
    vel += gravity;
    birdY += vel;

    stepPipe(pipeX1, gapY1, passed1);
    stepPipe(pipeX2, gapY2, passed2);

    if (birdY < 0) birdY = 0;

    if (birdY > SCREEN_HEIGHT - BIRD_SIZE) {
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

  void drawPipe(int pipeX, int gapY) {
    int topH = gapY - (GAP_H / 2);
    int botY = gapY + (GAP_H / 2);
    if (topH < 0) topH = 0;
    if (botY > SCREEN_HEIGHT) botY = SCREEN_HEIGHT;

    display.fillRect(pipeX, 0, PIPE_W, topH, SSD1306_WHITE);
    display.fillRect(pipeX, botY, PIPE_W, SCREEN_HEIGHT - botY, SSD1306_WHITE);
  }

  void render() {
    display.clearDisplay();

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("Score:");
    display.print(score);

    drawPipe(pipeX1, gapY1);
    drawPipe(pipeX2, gapY2);

    display.fillRect(BIRD_X, (int)birdY, BIRD_SIZE, BIRD_SIZE, SSD1306_WHITE);

    display.display();
  }
};

Flappy flappy;

void flappyStartNewGame() { flappy.reset(); }

// ===================== Menus =====================
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
  display.print("Selected: ");
  display.print(gameName(selectedGame));

  display.setCursor(10, 44);
  display.print("SELECT: next game");
  display.setCursor(10, 54);
  display.print("START : open menu");

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
  display.print("SELECT: START game");
  display.setCursor(10, 44);
  display.print("START : MAIN menu");

  if (g == GAME_FLAPPY) {
    display.setCursor(10, 56);
    display.print("In game: SELECT = flap");
  }

  display.display();
}

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

// ===================== Timing =====================
unsigned long lastFrameMs = 0;
const unsigned long FRAME_MS = 16;

// ===================== Setup/Loop =====================
void setup() {
  randomSeed((uint32_t)micros());

  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;) delay(10);
  }

  btnStart.begin(START_BTN_PIN);
  btnSelect.begin(SELECT_BTN_PIN);

  screenState = MAIN_MENU;
  selectedGame = GAME_PONG;
  activeGame = GAME_PONG;
}

void loop() {
  BtnEvent evStart  = btnStart.update();
  BtnEvent evSelect = btnSelect.update();

  // read pots
  rawL = analogRead(POT_LEFT_PIN);
  rawR = analogRead(POT_RIGHT_PIN);

  static int lastL = -1, lastR = -1;
  rawL = applyDeadband(rawL, lastL);
  rawR = applyDeadband(rawR, lastR);

  // MAIN MENU
  if (screenState == MAIN_MENU) {
    if (evSelect == BTN_SHORT) {
      selectedGame = (GameId)((selectedGame + 1) % GAME_COUNT);
    }
    if (evStart == BTN_SHORT || evStart == BTN_LONG) {
      enterGameMenu(selectedGame);
    }
    renderMainMenu();
    delay(20);
    return;
  }

  // GAME MENU
  if (screenState == GAME_MENU) {
    if (evStart == BTN_SHORT || evStart == BTN_LONG) {
      enterMainMenu();
      renderMainMenu();
      delay(80);
      return;
    }
    if (evSelect == BTN_SHORT || evSelect == BTN_LONG) {
      startGame(activeGame);
      delay(80);
      return;
    }
    renderGameMenu(activeGame);
    delay(20);
    return;
  }

  // IN GAME
  if (screenState == IN_GAME) {
    // START long => main menu, START short => game menu
    if (evStart == BTN_LONG) {
      enterMainMenu();
      renderMainMenu();
      delay(120);
      return;
    }
    if (evStart == BTN_SHORT) {
      enterGameMenu(activeGame);
      renderGameMenu(activeGame);
      delay(120);
      return;
    }

    // frame timing
    unsigned long now = millis();
    if (now - lastFrameMs < FRAME_MS) return;
    lastFrameMs = now;

    if (activeGame == GAME_PONG) {
      pongUpdate();
      pongRender();
    } else if (activeGame == GAME_FLAPPY) {
      // SELECT flaps
      if (evSelect == BTN_SHORT || evSelect == BTN_LONG) {
        flappy.flap();
      }
      flappy.update();
      flappy.render();
    }
    return;
  }
}
