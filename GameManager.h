#pragma once
#include <Adafruit_SSD1306.h>
#include "IGame.h"

enum ScreenState { MAIN_MENU, GAME_MENU, IN_GAME };

class GameManager {
public:
  GameManager(Adafruit_SSD1306& disp, IGame** games, int gameCount)
  : d(disp), games(games), gameCount(gameCount) {}

  void begin() {
    state = MAIN_MENU;
    selected = 0;
    active = 0;
  }

  void update(const Inputs& in) {
    if (state == MAIN_MENU) {
      if (in.selectEv == BTN_SHORT) selected = (selected + 1) % gameCount;
      if (in.startEv == BTN_SHORT || in.startEv == BTN_LONG) {
        active = selected;
        state = GAME_MENU;
      }
      renderMainMenu();
      return;
    }

    if (state == GAME_MENU) {
      if (in.startEv == BTN_SHORT || in.startEv == BTN_LONG) {
        state = MAIN_MENU;
        delay(80);
        return;
      }
      if (in.selectEv == BTN_SHORT || in.selectEv == BTN_LONG) {
        games[active]->reset();
        state = IN_GAME;
        delay(80);
        return;
      }
      renderGameMenu();
      return;
    }

    if (state == IN_GAME) {
      if (in.startEv == BTN_LONG) { state = MAIN_MENU; delay(120); return; }
      if (in.startEv == BTN_SHORT) { state = GAME_MENU; delay(120); return; }

      games[active]->update(in);
      games[active]->render(d);
      return;
    }

    
  uint16_t currentFrameMs() const {
    if (state == IN_GAME)
    {
      return games[active]->frameMs();
    } 
      return 20; // menu refresh rate
    }
  }

private:
  Adafruit_SSD1306& d;
  IGame** games;
  int gameCount;

  ScreenState state = MAIN_MENU;
  int selected = 0;
  int active = 0;

  void renderMainMenu() {
    d.clearDisplay();
    d.setTextColor(SSD1306_WHITE);

    d.setTextSize(2);
    d.setCursor(18, 6);
    d.print("GAMES");

    d.setTextSize(1);
    d.setCursor(10, 30);
    d.print("Selected: ");
    d.print(games[selected]->name());

    d.setCursor(10, 44);
    d.print("SELECT: next");
    d.setCursor(10, 54);
    d.print("START : open");

    d.display();
    delay(20);
  }

  void renderGameMenu() {
    d.clearDisplay();
    d.setTextColor(SSD1306_WHITE);

    d.setTextSize(2);
    d.setCursor(10, 6);
    d.print(games[active]->name());

    d.setTextSize(1);
    d.setCursor(10, 30);
    d.print("SELECT: start");
    d.setCursor(10, 44);
    d.print("START : main");

    d.setCursor(10, 56);
    d.print(games[active]->hint());

    d.display();
    delay(20);
  }

};
