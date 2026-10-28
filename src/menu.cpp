#include "menu.hpp"

#include "ui.hpp"
#include "util.hpp"

namespace {
constexpr float ROW_W = 280, ROW_H = 26, GAP = 6;
}

Rectangle Menu::row(const Ui& ui, int i) const {
  float total = items.size() * (ROW_H + GAP) - GAP, y0 = ui.VH / 2 - total / 2 + 14;
  return {ui.VW / 2 - ROW_W / 2, y0 + i * (ROW_H + GAP), ROW_W, ROW_H};
}

void Menu::input(const Ui& ui) {
  int n = int(items.size());
  if (!n) return;
  bool pad = IsGamepadAvailable(0);
  auto pressed = [&](int key, int button) { return IsKeyPressed(key) || IsKeyPressedRepeat(key) || (pad && IsGamepadButtonPressed(0, button)); };
  if (pressed(KEY_DOWN, GAMEPAD_BUTTON_LEFT_FACE_DOWN)) sel_ = (sel_ + 1) % n;
  if (pressed(KEY_UP, GAMEPAD_BUTTON_LEFT_FACE_UP)) sel_ = (sel_ + n - 1) % n;
  int chosen = -1;
  if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || (pad && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN))) chosen = sel_;
  Vector2 m = ui.mouse();  // a touch moves the mouse on Android
  for (int i = 0; i < n; i++)
    if (CheckCollisionPointRec(m, row(ui, i))) {
      if (GetMouseDelta().x != 0 || GetMouseDelta().y != 0) sel_ = i;
      if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) chosen = i;
    }
  if (chosen < 0) return;
  open = false;  // an entry may open the menu again (show), so close first
  auto run = items[chosen].run;
  if (run) run();
}

void Menu::draw(Ui& ui) const {
  DrawRectangle(0, 0, ui.RW, ui.RH, hex("#000000", .6f));
  Rectangle first = row(ui, 0);
  ui.text(title.c_str(), ui.VW / 2, first.y - 40, 22, hex("#d8b45a"), true, true);
  for (int i = 0; i < int(items.size()); i++) {
    Rectangle r = row(ui, i);
    bool sel = i == sel_;
    DrawRectangleRec(ui.px(r.x, r.y, r.width, r.height), sel ? hex("#2a2414", .95f) : hex("#070b0e", .9f));
    DrawRectangleLinesEx(ui.px(r.x, r.y, r.width, r.height), 1, hex("#8f7448", sel ? 1 : .5f));
    ui.text(items[i].label.c_str(), r.x + r.width / 2, r.y + 6, 13, sel ? hex("#f2e6c8") : hex("#c8c0b0"), false, true);
  }
}
