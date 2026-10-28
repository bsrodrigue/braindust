// Screen metrics and crisp text. HUD units: screen px / S, so 600 x 540 units always fit the window.
#pragma once
#include <string>
#include <unordered_map>

#include "raylib.h"

class Ui {
 public:
  int RW = 0, RH = 0;               // window size, px
  float S = 1, VW = 600, VH = 540;  // px per unit; view size in units

  void loadFonts();  // system fonts; raylib's default font when none is found
  bool resize();     // true when the window size changed

  // text is rasterised at its exact pixel size and drawn on whole pixels so it stays sharp
  Font font(bool serif, int px);
  int fontPx(float size) const;
  float textW(const char* s, float size, bool serif = false);
  void text(const char* s, float x, float y, float size, Color c, bool serif = false, bool center = false);  // with a shadow
  Vector2 mouse() const;  // in units
  Rectangle px(float x, float y, float w, float h) const;  // units -> whole screen px

 private:
  std::string monoPath_, serifPath_;
  std::unordered_map<int, Font> cache_;
};
