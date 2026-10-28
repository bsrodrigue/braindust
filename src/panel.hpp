// A side panel of named numbers, edited live with keyboard, mouse or gamepad.
// The F2 tune menu is one; anything that can list get/set pairs (a platform, a light...) can have its own.
// The tune pages are four of them (F2 / Tab moves to the next page).
//   keys: Up/Down select, Left/Right adjust (Shift x10, Ctrl x0.1), Backspace resets, S saves, Esc closes
//   mouse or touch: click a row, drag the value (6 px per step), -/+ buttons, drag the names to scroll, wheel scrolls,
//   right-click resets
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "ui.hpp"

struct Param {
  std::string name, group;
  float step = 1, def = 0;  // def: what reset and Defaults restore
  std::function<float()> get;
  std::function<void(float)> set;
};

class Panel {
 public:
  std::string title, page;  // page: a subtitle (which file it edits)
  std::vector<Param> params;
  std::function<std::string()> save;  // returns the status line to show
  bool open = false;
  bool showNext = false, nextRequested = false;  // a Next button (touch has no F2); the owner turns the page

  void toggle() { open = !open; msg_.clear(); }
  void input(const Ui& ui);
  void draw(Ui& ui);

 private:
  struct Line { int param; const char* header; float y; };
  struct Layout { float x0, w, rowH, listTop, listH; std::vector<Line> lines; Rectangle save, defaults, next, close; };
  Layout layout(const Ui& ui);
  void set(int i, float v);

  int sel_ = 0, top_ = 0, hover_ = -1, drag_ = -1;
  bool follow_ = true;  // keep the keyboard selection in view; the mouse wheel scrolls freely
  float dragX0_ = 0, dragV0_ = 0;
  bool scrolling_ = false;  // a drag on the names scrolls the list
  float scrollY0_ = 0;
  int scrollTop0_ = 0;
  std::string msg_;
};
