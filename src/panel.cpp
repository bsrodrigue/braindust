#include "panel.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "util.hpp"

namespace {
constexpr float MINUS_DX = 112, PLUS_DX = 26, BUTTON_W = 14;  // -/+ buttons, from the panel's right edge
float stepMul() {
  if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) return 10;
  if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) return .1f;
  return 1;
}
}  // namespace

// one layout shared by input and drawing, in HUD units
Panel::Layout Panel::layout(const Ui& ui) {
  Layout L;
  L.w = 300; L.x0 = ui.VW - L.w - 14; L.rowH = 15; L.listTop = 14 + 30; L.listH = ui.VH - 28 - 30 - 46;
  std::vector<std::pair<int, const char*>> all;  // rows: params, with a header row where the group changes
  const std::string* group = nullptr;
  for (int i = 0; i < int(params.size()); i++) {
    if (!group || *group != params[i].group) { group = &params[i].group; all.push_back({-1, group->c_str()}); }
    all.push_back({i, nullptr});
  }
  int rows = std::max(4, int(L.listH / L.rowH)), sel = 0;
  for (int i = 0; i < int(all.size()); i++) if (all[i].first == sel_) sel = i;
  if (follow_) {
    if (sel < top_ + 1) top_ = std::max(0, sel - 1);
    if (sel >= top_ + rows) top_ = sel - rows + 1;
  }
  top_ = std::clamp(top_, 0, std::max(0, int(all.size()) - rows));
  for (int i = top_; i < int(all.size()) && i < top_ + rows; i++) L.lines.push_back({all[i].first, all[i].second, L.listTop + (i - top_) * L.rowH});
  int nb = showNext ? 4 : 3;
  float by = ui.VH - 14 - 22, bw = (L.w - 10 * (nb + 1)) / nb;
  auto button = [&](int i) { return Rectangle{L.x0 + 10 + i * (bw + 10), by, bw, 16}; };
  L.save = button(0); L.defaults = button(1); L.next = showNext ? button(2) : Rectangle{}; L.close = button(nb - 1);
  return L;
}

void Panel::set(int i, float v) { params[i].set(v); msg_.clear(); }

void Panel::input(const Ui& ui) {
  int n = int(params.size());
  if (!n) return;
  sel_ = std::clamp(sel_, 0, n - 1);
  auto pressed = [](int k) { return IsKeyPressed(k) || IsKeyPressedRepeat(k); };
  bool pad = IsGamepadAvailable(0);
  auto padPressed = [&](int b) { return pad && IsGamepadButtonPressed(0, b); };
  if (pressed(KEY_DOWN) || padPressed(GAMEPAD_BUTTON_LEFT_FACE_DOWN)) { sel_ = (sel_ + 1) % n; follow_ = true; }
  if (pressed(KEY_UP) || padPressed(GAMEPAD_BUTTON_LEFT_FACE_UP)) { sel_ = (sel_ + n - 1) % n; follow_ = true; }
  int d = (pressed(KEY_RIGHT) || padPressed(GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) - (pressed(KEY_LEFT) || padPressed(GAMEPAD_BUTTON_LEFT_FACE_LEFT));
  if (d) set(sel_, params[sel_].get() + d * params[sel_].step * stepMul());
  if (IsKeyPressed(KEY_BACKSPACE)) set(sel_, params[sel_].def);
  if (IsKeyPressed(KEY_S) && save) msg_ = save();
  if (IsKeyPressed(KEY_ESCAPE)) open = false;

  Layout L = layout(ui);
  Vector2 m = ui.mouse();
  float wheel = GetMouseWheelMove();
  if (wheel != 0) { top_ -= int(wheel * 2); follow_ = false; }
  hover_ = -1;
  for (auto& ln : L.lines)
    if (ln.param >= 0 && m.x >= L.x0 && m.x <= L.x0 + L.w && m.y >= ln.y - 1 && m.y < ln.y - 1 + L.rowH) hover_ = ln.param;
  float minusX = L.x0 + L.w - MINUS_DX, plusX = L.x0 + L.w - PLUS_DX, valX = minusX + 18;
  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    auto in = [&](Rectangle r) { return CheckCollisionPointRec(m, r); };
    if (in(L.save)) { if (save) msg_ = save(); }
    else if (in(L.defaults)) { for (int i = 0; i < n; i++) set(i, params[i].def); msg_ = "all values back to defaults (not saved)"; }
    else if (in(L.close)) open = false;
    else if (showNext && in(L.next)) nextRequested = true;
    else if (hover_ >= 0) {
      sel_ = hover_; follow_ = false;
      const Param& p = params[sel_];
      if (m.x >= minusX && m.x < minusX + BUTTON_W) set(sel_, p.get() - p.step * stepMul());
      else if (m.x >= plusX && m.x < plusX + BUTTON_W) set(sel_, p.get() + p.step * stepMul());
      else if (m.x >= valX) { drag_ = sel_; dragX0_ = m.x; dragV0_ = p.get(); }
      else { scrolling_ = true; scrollY0_ = m.y; scrollTop0_ = top_; }
    } else if (m.x >= L.x0 && m.x <= L.x0 + L.w && m.y >= L.listTop && m.y < L.listTop + L.listH) {
      scrolling_ = true; scrollY0_ = m.y; scrollTop0_ = top_;
    }
  }
  if (scrolling_) {  // one row per row height dragged
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) { top_ = scrollTop0_ + int((scrollY0_ - m.y) / L.rowH); follow_ = false; }
    else scrolling_ = false;
  }
  if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && hover_ >= 0) { sel_ = hover_; set(sel_, params[sel_].def); }
  if (drag_ >= 0) {  // 6 px of drag = one step
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) set(drag_, dragV0_ + std::round((m.x - dragX0_) / 6) * params[drag_].step * stepMul());
    else drag_ = -1;
  }
}

void Panel::draw(Ui& ui) {
  Layout L = layout(ui);
  Vector2 m = ui.mouse();
  float y0 = 14, h = ui.VH - 28;
  DrawRectangleRec(ui.px(L.x0, y0, L.w, h), hex("#070b0e", .95f));
  DrawRectangleLinesEx(ui.px(L.x0, y0, L.w, h), 1, hex("#8f7448"));
  ui.text(title.c_str(), L.x0 + 10, y0 + 7, 15, hex("#d8b45a"), true);
  std::string sub = page.empty() ? "game paused" : page + "  |  F2 next";
  ui.text(sub.c_str(), L.x0 + L.w - 10 - ui.textW(sub.c_str(), 10), y0 + 10, 10, hex("#8a9a9e"));
  float minusX = L.x0 + L.w - MINUS_DX, plusX = L.x0 + L.w - PLUS_DX;
  for (auto& ln : L.lines) {
    if (ln.param < 0) { ui.text(ln.header, L.x0 + 10, ln.y + 2, 10, hex("#c8a868")); continue; }
    const Param& p = params[ln.param];
    float v = p.get();
    bool sel = ln.param == sel_, hov = ln.param == hover_, changed = std::fabs(v - p.def) > 1e-4f;
    if (sel || hov) DrawRectangleRec(ui.px(L.x0 + 4, ln.y - 1, L.w - 8, L.rowH), sel ? hex("#2a2414") : hex("#151a1e"));
    ui.text(p.name.c_str(), L.x0 + 16, ln.y, 12, sel ? hex("#f2e6c8") : hex("#d0d8da"));
    if (sel || hov) {
      for (float bx : {minusX, plusX}) {
        bool over = m.x >= bx && m.x < bx + BUTTON_W && m.y >= ln.y - 1 && m.y < ln.y - 1 + L.rowH;
        DrawRectangleRec(ui.px(bx, ln.y, BUTTON_W, L.rowH - 2), over ? hex("#4a3c22") : hex("#222a30"));
        ui.text(bx == minusX ? "-" : "+", bx + 7 - ui.textW("-", 12) / 2, ln.y, 12, hex("#f2e6c8"));
      }
    }
    const char* vs = TextFormat("%g", v);
    ui.text(vs, plusX - 6 - ui.textW(vs, 12), ln.y, 12, changed ? hex("#f0c060") : hex("#9fd0e0"));
  }
  const char* help = msg_.empty() ? "click a row, drag the value, drag the names to scroll, right-click resets" : msg_.c_str();
  ui.text(help, L.x0 + 10, ui.VH - 14 - 40, 9, msg_.empty() ? hex("#8a9a9e") : hex("#f0c060"));
  for (auto [r, label] : {std::pair{L.save, "Save"}, {L.defaults, "Defaults"}, {L.next, "Next"}, {L.close, "Close"}}) {
    if (r.width <= 0) continue;
    bool over = CheckCollisionPointRec(m, r);
    DrawRectangleRec(ui.px(r.x, r.y, r.width, r.height), over ? hex("#4a3c22") : hex("#1a2024"));
    DrawRectangleLinesEx(ui.px(r.x, r.y, r.width, r.height), 1, hex("#8f7448"));
    ui.text(label, r.x + r.width / 2 - ui.textW(label, 11) / 2, r.y + 2, 11, hex("#f2e6c8"));
  }
}
