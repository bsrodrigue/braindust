// On-screen controls for touch screens: a stick under the left thumb, action buttons under the right one, and a
// menu button at the top right. Positions are HUD units (see ui.hpp), so the layout follows the screen.
// update() takes this frame's touches as data, so the logic runs (and is tested) without a screen.
#pragma once
#include <vector>

#include "input.hpp"
#include "raylib.h"

class Ui;

struct Touch { int id; Vector2 p; };  // p: HUD units

class TouchControls {
 public:
  struct Button { Act act; const char* label; float dx, dy, r; };  // dx, dy: from the anchor near the bottom-right corner
  static const std::vector<Button>& buttons();

  void update(const std::vector<Touch>& touches, float VW, float VH);
  bool held(Act a) const { return held_[a]; }
  bool menuTapped() const { return menu_ && !menuPrev_; }
  void draw(Ui& ui) const;

  // this frame's touches in HUD units; on a desktop, a held left mouse button is one touch
  static std::vector<Touch> read(const Ui& ui);

  // layout, HUD units (public for tests)
  Vector2 buttonPos(const Button& b) const { return {VW_ - 70 + b.dx, VH_ - 70 + b.dy}; }
  Vector2 menuPos() const { return {VW_ - 26, 24}; }
  Vector2 stickRest() const { return {95, VH_ - 95}; }
  static constexpr float STICK_R = 55, MENU_R = 16;

 private:
  bool held_[A_COUNT]{}, menu_ = false, menuPrev_ = false;
  int stickId_ = -1;                // the touch that drives the stick, -1: none
  std::vector<int> seen_;           // last frame's touch ids: only a new touch can become the stick
  Vector2 stickBase_{}, stickKnob_{};
  float VW_ = 600, VH_ = 540;
};
