#include "touch.hpp"

#include <algorithm>

#include "ui.hpp"
#include "util.hpp"

// around an anchor 70 units in from the bottom-right corner: JUMP under the thumb, the rest within reach
const std::vector<TouchControls::Button>& TouchControls::buttons() {
  static const std::vector<Button> b = {
      {A_UP, "JUMP", 0, 0, 34},          {A_STAFF, "WAND", -82, 14, 28},   {A_BOLT, "BOLT", -58, -72, 25},
      {A_FLARE, "FLARE", 12, -88, 25},   {A_DODGE, "DODGE", -150, -40, 23}, {A_PARRY, "PARRY", -152, 30, 23},
      {A_FOCUS, "FOCUS", -130, -112, 22}, {A_ITEM, "VIAL", -200, -100, 22}};
  return b;
}

// A touch on a button holds it (sliding onto another button moves the press, as on a gamepad). A touch that begins
// low on the left, off the buttons, becomes the stick: its base is where it landed, and it stays the stick until lifted.
void TouchControls::update(const std::vector<Touch>& touches, float VW, float VH) {
  VW_ = VW; VH_ = VH;
  std::fill(held_, held_ + A_COUNT, false);
  menuPrev_ = menu_;
  menu_ = false;
  auto near = [](Vector2 p, Vector2 c, float r) { return Vector2Distance(p, c) <= r * 1.2f; };  // forgiving for thumbs
  const Touch* stick = nullptr;
  for (auto& t : touches) if (t.id == stickId_) stick = &t;
  if (!stick) stickId_ = -1;
  std::vector<int> ids;
  for (auto& t : touches) {
    ids.push_back(t.id);
    if (t.id == stickId_) continue;
    if (near(t.p, menuPos(), MENU_R)) { menu_ = true; continue; }
    const Button* hit = nullptr;  // the nearest button in reach
    for (auto& b : buttons())
      if (near(t.p, buttonPos(b), b.r) && (!hit || Vector2Distance(t.p, buttonPos(b)) < Vector2Distance(t.p, buttonPos(*hit)))) hit = &b;
    if (hit) { held_[hit->act] = true; continue; }
    bool fresh = std::find(seen_.begin(), seen_.end(), t.id) == seen_.end();
    if (fresh && stickId_ < 0 && t.p.x < VW * .42f && t.p.y > VH * .3f) { stickId_ = t.id; stickBase_ = t.p; stick = &t; }
  }
  seen_ = ids;
  stickKnob_ = stickBase_;
  if (!stick) return;
  Vector2 d = Vector2Subtract(stick->p, stickBase_);
  if (float len = Vector2Length(d); len > STICK_R) d = Vector2Scale(d, STICK_R / len);
  stickKnob_ = Vector2Add(stickBase_, d);
  float nx = d.x / STICK_R, ny = d.y / STICK_R;
  held_[A_LEFT] = nx < -.35f;
  held_[A_RIGHT] = nx > .35f;
  held_[A_DOWN] = ny > .55f;  // push down to dive
}

std::vector<Touch> TouchControls::read(const Ui& ui) {
  std::vector<Touch> out;
  int n = GetTouchPointCount();
  for (int i = 0; i < n; i++) out.push_back({GetTouchPointId(i), ui.fromScreen(GetTouchPosition(i))});
  if (n == 0 && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) out.push_back({0, ui.mouse()});
  return out;
}

void TouchControls::draw(Ui& ui) const {
  const float S = ui.S;
  auto px = [S](Vector2 p) { return Vector2{p.x * S, p.y * S}; };
  auto disc = [&](Vector2 c, float r, bool on) {
    DrawCircleV(px(c), r * S, on ? hex("#8f7448", .45f) : hex("#0b1216", .35f));
    DrawRing(px(c), (r - 1) * S, r * S, 0, 360, 40, hex("#c8bfae", on ? .75f : .35f));
  };
  bool active = stickId_ >= 0;
  Vector2 base = active ? stickBase_ : stickRest(), knob = active ? stickKnob_ : base;
  DrawCircleV(px(base), STICK_R * S, hex("#0b1216", .2f));
  DrawRing(px(base), (STICK_R - 1) * S, STICK_R * S, 0, 360, 48, hex("#c8bfae", .25f));
  disc(knob, 22, active);
  for (auto& b : buttons()) {
    Vector2 c = buttonPos(b);
    disc(c, b.r, held_[b.act]);
    ui.text(b.label, c.x, c.y - 5, 9, hex("#e8dcc4", held_[b.act] ? 1 : .75f), false, true);
  }
  Vector2 m = menuPos();
  disc(m, MENU_R, menu_);
  for (int i = -1; i <= 1; i++) DrawRectangleV(px({m.x - 6, m.y + i * 4.f - .6f}), {12 * S, 1.4f * S}, hex("#e8dcc4", .8f));
}
