// HUD: mana vessel, blood and stardust bars, depth gauge, banners, death/win screen, help. HUD units (see ui.hpp).
#include <algorithm>
#include <cmath>

#include "renderer.hpp"
#include "util.hpp"

void Renderer::drawHud(const World& w) {
  const Player& P = w.player;
  const Tunables& C = w.tun;
  const float S = ui.S, VW = ui.VW, VH = ui.VH, t = w.t;
  bool playing = w.outcome == Outcome::Playing;
  float x = 22, y = 22, r = 22, vx = x + r, vy = y + r;
  bool blood = P.mana < w.cheapestSpell();
  DrawCircleV({vx * S, vy * S}, r * S, hex("#030607"));
  float f = P.mana / C.maxMana;  // mana vessel, filled from the bottom
  for (float yy = vy + r - f * 2 * r; yy < vy + r; yy += 1 / S) {
    float half = std::sqrt(std::max(0.f, r * r - (yy - vy) * (yy - vy)));
    DrawRectangleV({(vx - half) * S, yy * S}, {half * 2 * S, 1.2f}, hex("#4d6a78"));
  }
  DrawRing({vx * S, vy * S}, (r - 1) * S, (r + 1) * S, 0, 360, 48, blood ? hex("#8e1f2b", .6f + .4f * std::sin(t * 6)) : hex("#5e4c30"));
  if (P.focus > 0) DrawRing({vx * S, vy * S}, (r + 3) * S, (r + 5) * S, -90, -90 + 360 * P.focus / C.focusTime, 48, hex("#7a98a8"));
  // carried items: one flask per item, under the vessel
  for (size_t i = 0, n = 0; i < P.items.size(); i++)
    for (int k = 0; k < P.items[i]; k++, n++) {
      float ix = x + 4 + n * 11, iy = y + 2 * r + 8;
      DrawRectangleRounded(ui.px(ix, iy, 8, 10), .5f, 4, hex("#1c1822"));
      DrawRectangleRoundedLines(ui.px(ix, iy, 8, 10), .5f, 4, hex("#b89cff", .8f));
      DrawRectangleV({(ix + 2) * S, (iy - 2) * S}, {4 * S, 2 * S}, hex("#8f7448"));
    }
  float bx = vx + r + 12, bw = 170;
  DrawRectangleV({bx * S, (y + 6) * S}, {bw * S, 8 * S}, hex("#100506"));
  DrawRectangleV({bx * S, (y + 6) * S}, {bw * S * std::max(0.f, P.hp) / C.maxHp, 8 * S}, P.bleed > 0 ? hex("#b8323f") : hex("#6e1a22"));
  DrawRectangleV({bx * S, (y + 20) * S}, {bw * S, 4 * S}, hex("#0b0910"));
  DrawRectangleGradientH(int(bx * S), int((y + 20) * S), int(bw * S * P.dust / 100), int(4 * S), hex("#5a4f6a"), hex("#8f7448"));
  ui.text(TextFormat("%d", std::max(0, int(std::ceil(P.hp)))), bx + bw + 8, y + 4, 10, hex("#59666a"));
  ui.text(TextFormat("stardust %d%%  |  power x%.2f  |  sight %d", int(P.dust), w.power(), int(w.lightR())), bx, y + 30, 10, hex("#59666a"));
  if (!w.inPocket && w.warp > .3f && playing) ui.text("a mage's mind burst here: the dust is in you", bx, y + 44, 10, hex("#8c7aa8"));
  else if (!w.inPocket && w.depthM() > C.pressureDepth && playing) ui.text("the deep presses on your skull", bx, y + 44, 10, hex("#6d5f80"));
  else if (blood && playing) ui.text("mana dry: spells will drink blood", bx, y + 44, 10, hex("#8e1f2b"));
  if (!w.inPocket) {  // depth gauge; the pressure zone below pressureDepth is tinted
    float gx = VW - 34, gy = 70, gh = std::min(260.f, VH - 160), my = gy + gh * std::min(1.f, P.y / w.H);
    DrawRectangleV({gx * S, gy * S}, {2 * S, gh * S}, hex("#ffffff", .05f));
    float pd = std::min(1.f, C.pressureDepth * 10 / w.H);
    DrawRectangleV({gx * S, (gy + gh * pd) * S}, {2 * S, gh * (1 - pd) * S}, hex("#5a4f6a", .33f));
    DrawRectangleV({(gx - 4) * S, (my - 1) * S}, {10 * S, 1.5f * S}, hex("#9aa6a8"));
    const char* m = TextFormat("%d m", w.depthM());
    ui.text(m, gx - 8 - ui.textW(m, 10), my - 6, 10, hex("#8a9a9e"));
  }
  if (w.banner.t > 0 && playing) {
    float a = std::min(1.f, w.banner.t) * .85f;
    ui.text(w.banner.title.c_str(), VW / 2, VH * .3f, 36, hex("#9aa6a8", a), true, true);
    ui.text(w.banner.line.c_str(), VW / 2, VH * .3f + 46, 13, hex("#7a6a4a", a), true, true);
  }
  if (!playing) {
    DrawRectangle(0, 0, ui.RW, ui.RH, hex("#000000", .73f));
    ui.text(w.msg.c_str(), VW / 2, VH * .42f, 22, hex("#9aa6a8"), true, true);
    ui.text(w.outcome == Outcome::Won ? TextFormat("stardust spent: %d%%  |  press R to dive again", int(P.dust)) : "press R (or stroke) to dive again",
            VW / 2, VH * .42f + 38, 12, hex("#7a6a4a"), false, true);
  }
  if (w.fade > 0) DrawRectangle(0, 0, ui.RW, ui.RH, hex("#968caa", w.fade));
}

void Renderer::drawHelp() {
  DrawRectangle(0, 0, ui.RW, ui.RH, hex("#000000", .7f));
  const char* lines[] = {"Arrows / WASD  move      Space  jump / stroke up (in the air: star leap)      Down  dive",
                         "J  wand (+mana; hold and release: thrust)      Shift  dodge      U  parry      Q  dust vial",
                         "K  star bolt      L  flare      F (hold)  focus      spells you cannot afford drink blood",
                         "R  restart   M  mute   F2  tune pages   F5  reload level, data, character + tunables   F1  this help",
                         "Gamepad: stick move, A jump, X wand, Y bolt, B flare, RB focus, RT dodge, LB parry, D-pad up vial"};
  for (int i = 0; i < 5; i++) ui.text(lines[i], ui.VW / 2, ui.VH * .36f + i * 20, 12, hex("#b9c2c4"), false, true);
}
