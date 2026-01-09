#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------- Display ----------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---------- Pins ----------
const int POT_LEFT_PIN   = 1;   // GPIO1 = A1 (ADC)
const int POT_RIGHT_PIN  = 2;   // GPIO2 = A2 (ADC)
const int START_BTN_PIN  = 20;  // GPIO20 (digital), button to GND

// ---------- Game tuning ----------
const int PADDLE_W = 3;
const int PADDLE_H = 16;

const int LEFT_X  = 2;
const int RIGHT_X = SCREEN_WIDTH - 2 - PADDLE_W;

const int BALL_SIZE = 3;

const int DEAD_BAND = 10; // ignore ADC changes smaller than this

// ADC auto-calibration (starts wide, learns real min/max)
int potMinL = 4095, potMaxL = 0;
int potMinR = 4095, potMaxR = 0;

// Ball
float ballX, ballY;
float ballVX, ballVY;

// Paddles
int leftPaddleY  = (SCREEN_HEIGHT - PADDLE_H) / 2;
int rightPaddleY = (SCREEN_HEIGHT - PADDLE_H) / 2;

// Score
int scoreL = 0;
int scoreR = 0;

// Raw ADC (kept for logic + optional serial)
int rawL = 0;
int rawR = 0;

// ---------- Start button / game state ----------
bool gameStarted = false;

// Debounce + edge detection
bool lastBtnRead = true;      // INPUT_PULLUP => idle HIGH
bool stableBtn   = true;      // debounced stable state
bool prevStableBtn = true;    // for edge detect
unsigned long lastDebounceMs = 0;
const unsigned long DEBOUNCE_MS = 25;

// Optional Serial debug
const bool SERIAL_DEBUG = false;
unsigned long lastRawPrintMs = 0;
const unsigned long RAW_PRINT_EVERY_MS = 200;

// Map pot value to paddle Y using calibrated min/max
int potToPaddleYCal(int val, int vmin, int vmax) {
  if (vmax - vmin < 50) {
    return (SCREEN_HEIGHT - PADDLE_H) / 2;
  }

  long y = map(val, vmin, vmax, 0, SCREEN_HEIGHT - PADDLE_H);
  if (y < 0) y = 0;
  if (y > SCREEN_HEIGHT - PADDLE_H) y = SCREEN_HEIGHT - PADDLE_H;
  return (int)y;
}

void resetBall(bool toRight) {
  ballX = SCREEN_WIDTH / 2.0f;
  ballY = SCREEN_HEIGHT / 2.0f;

  // Ball speed (change here)
  float speedX = 1.92f;
  float speedY = 1.20f;

  ballVX = toRight ? speedX : -speedX;
  ballVY = (random(0, 2) == 0) ? speedY : -speedY;
}

// Read + debounce button. Returns true only on a *new press*.
bool startButtonPressedEdge() {
  bool reading = digitalRead(START_BTN_PIN); // HIGH = not pressed, LOW = pressed
  unsigned long now = millis();

  if (reading != lastBtnRead) {
    lastDebounceMs = now;
    lastBtnRead = reading;
  }

  if ((now - lastDebounceMs) > DEBOUNCE_MS) {
    stableBtn = reading;
  }

  // Edge detect: HIGH -> LOW transition means "pressed"
  bool pressed = (prevStableBtn == HIGH && stableBtn == LOW);
  prevStableBtn = stableBtn;
  return pressed;
}

void goToMenu() {
  gameStarted = false;

  // Optional: keep scores, or reset them — your choice.
  // I'll reset to make it feel like a fresh start.
  scoreL = 0;
  scoreR = 0;

  // Reset calibration so next start learns full range again
  potMinL = 4095; potMaxL = 0;
  potMinR = 4095; potMaxR = 0;

  // Re-center paddles
  leftPaddleY  = (SCREEN_HEIGHT - PADDLE_H) / 2;
  rightPaddleY = (SCREEN_HEIGHT - PADDLE_H) / 2;

  // Put ball back to center (not moving on menu)
  ballX = SCREEN_WIDTH / 2.0f;
  ballY = SCREEN_HEIGHT / 2.0f;
  ballVX = 0;
  ballVY = 0;
}

void startGame() {
  gameStarted = true;

  // Reset game
  scoreL = 0;
  scoreR = 0;

  // Reset calibration at the moment you start
  potMinL = 4095; potMaxL = 0;
  potMinR = 4095; potMaxR = 0;

  resetBall(true);
}

void updatePaddles() {
  rawL = analogRead(POT_LEFT_PIN);
  rawR = analogRead(POT_RIGHT_PIN);

  // Auto-calibrate ranges continuously
  if (rawL < potMinL) potMinL = rawL;
  if (rawL > potMaxL) potMaxL = rawL;

  if (rawR < potMinR) potMinR = rawR;
  if (rawR > potMaxR) potMaxR = rawR;

  // Deadband (kills tiny jitter)
  static int lastRawL = -1, lastRawR = -1;
  if (lastRawL == -1) lastRawL = rawL;
  if (lastRawR == -1) lastRawR = rawR;

  if (abs(rawL - lastRawL) < DEAD_BAND) rawL = lastRawL;
  else lastRawL = rawL;

  if (abs(rawR - lastRawR) < DEAD_BAND) rawR = lastRawR;
  else lastRawR = rawR;

  leftPaddleY  = potToPaddleYCal(rawL, potMinL, potMaxL);
  rightPaddleY = potToPaddleYCal(rawR, potMinR, potMaxR);
}

void bounceOffPaddle(int paddleY, bool isRightPaddle) {
  float paddleCenter = paddleY + PADDLE_H / 2.0f;
  float ballCenter   = ballY + BALL_SIZE / 2.0f;
  float rel = (ballCenter - paddleCenter) / (PADDLE_H / 2.0f);

  if (rel < -1.0f) rel = -1.0f;
  if (rel >  1.0f) rel =  1.0f;

  ballVX = -ballVX;
  ballVY = rel * 2.2f;

  // Speed-up per hit (set to 1.0f to disable)
  ballVX *= 1.03f;

  // Nudge out of paddle
  if (isRightPaddle) ballX = RIGHT_X - BALL_SIZE - 1;
  else               ballX = LEFT_X + PADDLE_W + 1;
}

void updateBall() {
  ballX += ballVX;
  ballY += ballVY;

  // Top/bottom bounce
  if (ballY <= 0) {
    ballY = 0;
    ballVY = -ballVY;
  }
  if (ballY >= SCREEN_HEIGHT - BALL_SIZE) {
    ballY = SCREEN_HEIGHT - BALL_SIZE;
    ballVY = -ballVY;
  }

  // Left paddle collision
  if (ballVX < 0 &&
      ballX <= (LEFT_X + PADDLE_W) &&
      (ballY + BALL_SIZE) >= leftPaddleY &&
      ballY <= (leftPaddleY + PADDLE_H)) {
    bounceOffPaddle(leftPaddleY, false);
  }

  // Right paddle collision
  if (ballVX > 0 &&
      (ballX + BALL_SIZE) >= RIGHT_X &&
      (ballY + BALL_SIZE) >= rightPaddleY &&
      ballY <= (rightPaddleY + PADDLE_H)) {
    bounceOffPaddle(rightPaddleY, true);
  }

  // Scoring
  if (ballX < -10) {
    scoreR++;
    resetBall(true);
  }
  if (ballX > SCREEN_WIDTH + 10) {
    scoreL++;
    resetBall(false);
  }
}

void drawCenterLine() {
  for (int y = 0; y < SCREEN_HEIGHT; y += 6) {
    display.drawFastVLine(SCREEN_WIDTH / 2, y, 3, SSD1306_WHITE);
  }
}

void renderGame() {
  display.clearDisplay();

  // Score
  display.setTextSize(1);
  display.setCursor(40, 0);
  display.print(scoreL);
  display.setCursor(82, 0);
  display.print(scoreR);

  drawCenterLine();

  // Paddles
  display.fillRect(LEFT_X, leftPaddleY, PADDLE_W, PADDLE_H, SSD1306_WHITE);
  display.fillRect(RIGHT_X, rightPaddleY, PADDLE_W, PADDLE_H, SSD1306_WHITE);

  // Ball
  display.fillRect((int)ballX, (int)ballY, BALL_SIZE, BALL_SIZE, SSD1306_WHITE);

  display.display();
}

void renderStartScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(44, 8);
  display.print("PONG");

  display.setTextSize(1);
  display.setCursor(10, 38);
  display.print("Press START to play");

  display.display();
}

void setup() {
  if (SERIAL_DEBUG) Serial.begin(115200);

  randomSeed((uint32_t)micros());
  Wire.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;) { delay(10); }
  }

  pinMode(START_BTN_PIN, INPUT_PULLUP);

  goToMenu();
}

void loop() {
  // Edge press works both on menu + in game
  bool pressed = startButtonPressedEdge();

  // Always read pots (menu included, for calibration/movement feel)
  updatePaddles();

  if (SERIAL_DEBUG) {
    unsigned long now = millis();
    if (now - lastRawPrintMs >= RAW_PRINT_EVERY_MS) {
      lastRawPrintMs = now;
      Serial.print("rawL=");
      Serial.print(rawL);
      Serial.print(" rawR=");
      Serial.print(rawR);
      Serial.print(" | Lmin/max=");
      Serial.print(potMinL);
      Serial.print("/");
      Serial.print(potMaxL);
      Serial.print(" Rmin/max=");
      Serial.print(potMinR);
      Serial.print("/");
      Serial.println(potMaxR);
    }
  }

  if (!gameStarted) {
    renderStartScreen();

    if (pressed) {
      startGame();
      delay(120); // small guard
    }

    delay(20);
    return;
  }

  // In game: pressing START returns to menu
  if (pressed) {
    goToMenu();
    delay(120);
    return;
  }

  updateBall();
  renderGame();
  delay(16);
}
