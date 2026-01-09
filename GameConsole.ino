#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "Buttons.h"
#include "Input.h"
#include "IGame.h"
#include "PongGame.h"
#include "FlappyGame.h"
#include "GameManager.h"

// ===================== Display =====================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ===================== Pins =====================
const int POT_LEFT_PIN   = 1;   // GPIO1 (ADC)
const int POT_RIGHT_PIN  = 2;   // GPIO2 (ADC)

const int START_BTN_PIN  = 20;  // BACK / MENU
const int SELECT_BTN_PIN = 19;  // SELECT / ACTION (change to 21 if rewired)

// ===================== Tuning =====================
static const int DEAD_BAND = 10;
static const int ADC_MAX_REAL = 3400;

// ===================== Managers / Games =====================
InputManager inputMgr(POT_LEFT_PIN, POT_RIGHT_PIN, DEAD_BAND);

PongGame pong(ADC_MAX_REAL);
FlappyGame flappy;

IGame* gameList[] = { &pong, &flappy };
GameManager gameMgr(display, gameList, sizeof(gameList)/sizeof(gameList[0]));

void setup() {
  randomSeed((uint32_t)micros());

  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;) delay(10);
  }

  inputMgr.begin(START_BTN_PIN, SELECT_BTN_PIN);
  gameMgr.begin();
}

static unsigned long lastTickMs = 0;

void loop() {
  Inputs in = inputMgr.read();

  // Ask manager which game is active and what tick rate it wants:
  uint16_t ms = gameMgr.currentFrameMs();   // we’ll add this function
  unsigned long now = millis();
  if (now - lastTickMs < ms) return;
  lastTickMs = now;

  gameMgr.update(in);
}

