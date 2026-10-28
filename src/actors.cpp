// The player (a character sprite), enemies, cracks and wrappers. World px, inside BeginMode2D.
#include <cmath>
#include <tuple>

#include "renderer.hpp"
#include "rlgl.h"
#include "util.hpp"

Vector2 lurePos(const Enemy& e) { return {e.x + e.w / 2 + e.face * 30, e.y + e.h / 2 - 22 + std::sin(e.ph * 3) * 2}; }

void Renderer::drawPlayer(const World& w) {
  if (!w.character) return;
  const Character& c = *w.character;
  const Player& P = w.player;
  const SheetAnim& an = P.anim.anim(c);
  int f = P.anim.frame(c);
  float k = c.scale();
  float ga = (P.inv > 0 && P.kb <= 0 && int(w.t * 20) % 2) ? .45f : 1;  // blink while invulnerable
  Color tint = P.kb > 0 ? hex("#ffb0b0", ga) : alpha(c.tint, ga);
  rlPushMatrix();
  rlTranslatef(P.x + P.w / 2, P.y + P.h + std::sin(w.t * 1.6f) * 1.2f, 0);  // feet
  rlScalef(1, P.squash, 1);                                                // squash from the feet
  Vector2 head = an.head[f];
  rlPushMatrix();
  rlTranslatef(head.x * k * P.face, head.y * k, 0);
  drawCranium(w, ga);
  rlPopMatrix();
  Rectangle src{f * c.cellW, an.row * c.cellH, P.face > 0 ? c.cellW : -c.cellW, c.cellH};
  DrawTexturePro(c.sheet, src, {-c.anchorX * k, -c.anchorY * k, c.cellW * k, c.cellH * k}, {0, 0}, 0, tint);
  rlPopMatrix();
}

// the swelling cranium: veined, translucent, pushing out of the headwrap as stardust grows (origin: top of the head)
void Renderer::drawCranium(const World& w, float ga) {
  const Player& P = w.player;
  float cs = 1 + P.dust / 100 * 1.4f;
  if (cs <= 1.01f) return;
  float r = 8 * cs, cy = 6 - (r - 8) * 1.6f;  // hidden in the wrap at first, rising as it grows
  DrawEllipse(0, int(cy), r * .92f, r, alpha(hex("#1f1724"), ga));
  DrawEllipse(int(-r * .25f), int(cy - r * .25f), r * .55f, r * .6f, alpha(hex("#6d5a72"), ga * .7f));
  DrawEllipseLines(0, int(cy), r * .92f, r, hex("#8a7a8c", .4f * ga));
  for (int i = 0; i < 4; i++) {
    float a = -2.6f + i * .7f;
    DrawSplineSegmentBezierQuadratic({std::cos(a) * r * .9f, cy + std::sin(a) * r * .9f}, {std::cos(a + .4f) * r * .4f, cy + std::sin(a + .3f) * r * .5f},
                                     {0, cy + r * .2f}, 1, hex("#2b1d30", ga));
  }
  for (int i = 0; i < 4 + P.dust / 6; i++) {
    float a = i * 2.399f + w.t * .15f, d = r * .75f * std::sqrt(std::fmod(i * .618f, 1.f));
    DrawRectangleV({std::cos(a) * d - .5f, cy + std::sin(a) * d - .5f}, {1, 1}, hex("#b8b0c4", (.2f + .3f * std::pow(std::sin(w.t * 2 + i), 2.f)) * ga));
  }
}

void Renderer::drawEnemy(const World& w, const Enemy& e) {
  const float t = w.t;
  rlPushMatrix();
  rlTranslatef(e.x + e.w / 2, e.y + e.h / 2, 0);
  if (e.thought) {  // a creature that escaped from a cranium: pale flesh, too many eyes
    for (int i = 0; i < 6; i++) {
      float a = i * 1.047f + t * .7f;
      DrawSplineSegmentBezierQuadratic({0, 0}, {std::cos(a) * 18, std::sin(a) * 18}, {std::cos(a + .8f) * 28, std::sin(a + .8f) * 28}, 2, hex("#2a2026"));
    }
    DrawEllipse(0, 0, 13, 11, e.flash > 0 ? WHITE : hex("#9a9088"));
    float look = w.player.x > e.x ? 1.f : -1.f;
    for (auto [ex, ey, er] : {std::tuple{-5.f, -3.f, 3.2f}, {4.f, -4.f, 2.4f}, {1.f, 4.f, 2.8f}, {-6.f, 4.f, 1.6f}}) {
      DrawCircleV({ex, ey}, er, hex("#d8d2c4"));
      DrawCircleV({ex + look * er * .35f, ey}, er * .5f, hex("#0a0607"));
    }
    rlPopMatrix();
    return;
  }
  rlScalef(float(e.face), 1, 1);  // a lurker: jaws open as it stalks and lunges
  float open = e.mode == EnemyMode::Lunge ? 1 : e.mode == EnemyMode::Stalk ? .35f + .15f * std::sin(t * 9) : .08f;
  Color body = e.flash > 0 ? WHITE : hex("#0e1113");
  fillPoly({{10, -8}, {-8, -9}, {-24, -3}, {-34, -10}, {-30, 0}, {-34, 10}, {-24, 3}, {-8, 8}, {10, 7}}, body);
  fillPoly({{8, -9}, {27, -4 - open * 5}, {10, 0}}, body);
  fillPoly({{8, 2}, {25, 3 + open * 12}, {8, 8}}, body);
  for (int i = 0; i < 6; i++) {
    float x = 12 + i * 2.5f;
    DrawLineEx({x, -4 - open * i * .7f}, {x + 1, 1 + open * 4}, 1, hex("#c9c4b6"));
    DrawLineEx({x, 3 + open * (2 + i * 1.6f)}, {x + 1, -1 + open * 2}, 1, hex("#c9c4b6"));
  }
  DrawRectangleV({13, -7}, {1.3f, 1.3f}, hex("#d8e4e6"));
  DrawSplineSegmentBezierQuadratic({12, -9}, {18, -26}, {30, -22 + std::sin(e.ph * 3) * 2}, 1, hex("#1d2628"));
  rlPopMatrix();
}

void Renderer::drawCrack(Rectangle k, Color fill, Color stroke, const Texture2D* galaxy) {
  static const float pts[][2] = {{0, 0}, {.7f, .15f}, {.3f, .35f}, {1, .55f}, {.4f, .75f}, {.6f, 1}, {.1f, .8f}, {-.3f, .55f}, {.2f, .4f}, {-.4f, .2f}};
  std::vector<Vector2> p;
  for (auto& q : pts) p.push_back({k.x + k.width * (.5f + q[0] * .5f), k.y + q[1] * k.height});
  fillPoly(p, fill);
  if (galaxy) DrawTexturePro(*galaxy, {0, 0, 512, 512}, {k.x + k.width / 2 - 40, k.y + k.height / 2 - 40, 80, 80}, {0, 0}, 0, hex("#ffffff", .8f));
  for (size_t i = 0; i < p.size(); i++) DrawLineEx(p[i], p[(i + 1) % p.size()], 1.5f, stroke);
}

void Renderer::drawWrapper(const World& w, const Wrapper& wr) {  // a cloth-bound bundle, a drowned mage's wrapper
  float bob = std::sin(w.t * 1.5f + wr.x) * 3;
  DrawEllipse(int(wr.x), int(wr.y + bob), 11, 9, hex("#4d463a"));
  for (int i = -1; i <= 1; i++) DrawLineEx({wr.x - 10, wr.y + bob + i * 4 - 2}, {wr.x + 10, wr.y + bob + i * 4 + 2}, 2, hex("#8f7448"));
  for (int i = 0; i < 3; i++) DrawCircleV({wr.x - 4.f + i * 4, wr.y + bob - 7}, 1.6f, hex("#e8dcc4"));  // cowrie-like beads
}
