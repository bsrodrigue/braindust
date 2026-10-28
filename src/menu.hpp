// The pause menu: a column of entries, picked by touch, mouse, keyboard (Up/Down, Enter) or gamepad (D-pad, A).
// Esc, Android's Back button, the gamepad's Select button and the on-screen menu button open and close it.
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "raylib.h"

class Ui;

struct MenuItem {
  std::string label;
  std::function<void()> run;
};

class Menu {
 public:
  bool open = false;
  std::string title = "PAUSED";
  std::vector<MenuItem> items;

  void show(std::vector<MenuItem> entries) { items = std::move(entries); sel_ = 0; open = true; }
  void input(const Ui& ui);  // runs the chosen entry
  void draw(Ui& ui) const;

 private:
  int sel_ = 0;
  Rectangle row(const Ui& ui, int i) const;  // HUD units
};
