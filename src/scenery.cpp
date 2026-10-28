// Above the sea (sky, the far coast), the tileset (assets/tileset, Craftpix "World Tree Roots", toned down by shaders/tile.fs), props.
#include <algorithm>
#include <cmath>

#include "renderer.hpp"
#include "util.hpp"

namespace {
constexpr float TILE = 64;                      // world px per 128 px tile
constexpr Color TILE_TINT{224, 180, 154, 255};  // warm, toward the sunset
std::string tileStyle(const Box& s) {
  if (!s.tile.empty()) return s.tile;
  return s.h <= 48 ? "float" : s.w <= 60 ? "stone" : "ground";
}
}  // namespace

void Renderer::drawSky(const World& w, float waterY) {  // screen space
  const float S = ui.S, t = w.t;
  const int RW = ui.RW;
  float wy = waterY, ty = (w.level.top - w.cam.y) * S, mid = ty + (wy - ty) * .55f;
  if (ty > 0) DrawRectangle(0, 0, RW, int(ty) + 1, hex("#1b1a3a"));
  DrawRectangleGradientV(0, int(ty), RW, int(mid - ty) + 1, hex("#1b1a3a"), hex("#6e3a52"));
  DrawRectangleGradientV(0, int(mid), RW, int(wy - mid) + 1, hex("#6e3a52"), hex("#e08a4a"));
  float sx = RW * .72f - w.cam.x * .05f * S, sy = wy - 42 * S;  // the sun, low over the sea
  DrawCircleGradient(int(sx), int(sy), 190 * S, hex("#f6c27a", .35f), hex("#f6c27a", 0));
  DrawCircleV({sx, sy}, 30 * S, hex("#f8d49a"));
  auto layer = [&](float par, float scale, Color col, uint32_t seedBase, bool city) {
    uint32_t keep = rngSeed;
    rngSeed = seedBase;  // the same random draws per building every frame, so the skyline never reshuffles
    float span = 2600, off = std::fmod(w.cam.x * par, span);
    for (int rep = -1; rep <= 1; rep++)
      for (int i = 0; i < 70; i++) {
        float bx = i * span / 70 + rep * span - off, bw = (14 + rnd() * 34) * scale, h = (city ? 18 + rnd() * 70 : 20 + rnd() * 45) * scale, kind = rnd();
        float win[3] = {rnd(), rnd(), rnd()}, x = bx * S * .55f, base = wy;
        if (x < -120 * S || x > RW + 120 * S) continue;
        DrawRectangleV({x, base - h * S}, {bw * S, h * S + 1}, col);
        if (!city) continue;
        if (kind < .25f) DrawCircleV({x + bw * S / 2, base - h * S}, bw * S * .5f, col);  // domes
        if (kind > .85f) DrawTriangle({x + bw * S * .5f, base - (h + 30 * scale) * S}, {x, base - h * S}, {x + bw * S, base - h * S}, col);  // towers
        for (int k = 0; k < 3; k++)  // warm windows
          if (win[k] < .5f)
            DrawRectangleV({x + bw * S * (.2f + .25f * k), base - h * S * (.3f + .2f * k)}, {2 * S * scale, 2 * S * scale},
                           hex("#f0b060", .55f + .3f * std::sin(t * .7f + i + k)));
      }
    rngSeed = keep;
  };
  layer(.06f, .7f, hex("#3a2a44"), 91, true);  // the far city: the kingdoms on the coast
  for (int i = 0; i < 3; i++) {                // ships under sail on the horizon
    float x = (std::fmod(300.f + i * 830 - w.cam.x * .08f + t * 3, 2600.f)) * S * .55f, b = wy + std::sin(t + i) * S;
    Color c = hex("#2e2238");
    DrawTriangle({x, b - 6 * S}, {x + 46 * S, b - 6 * S}, {x + 38 * S, b}, c);
    DrawTriangle({x, b - 6 * S}, {x + 38 * S, b}, {x + 8 * S, b}, c);
    DrawTriangle({x + 22 * S, b - 50 * S}, {x + 6 * S, b - 10 * S}, {x + 22 * S, b - 10 * S}, c);
    DrawTriangle({x + 24 * S, b - 44 * S}, {x + 24 * S, b - 10 * S}, {x + 40 * S, b - 10 * S}, c);
  }
  layer(.25f, 1.2f, hex("#221a2c"), 57, false);  // nearer headlands
}

Texture2D Renderer::tile(const std::string& name) {
  if (auto it = tiles_.find(name); it != tiles_.end()) return it->second;
  Texture2D tx{};
  for (const char* dir : {"platformer", "environment", "collectable"}) {
    std::string path = std::string("assets/tileset/") + dir + "/" + name + ".png";
    if (FileExists(path.c_str())) { tx = LoadTexture(path.c_str()); GenTextureMipmaps(&tx); SetTextureFilter(tx, TEXTURE_FILTER_TRILINEAR); break; }
  }
  if (!tx.id) TraceLog(LOG_WARNING, "tileset: no image named %s", name.c_str());
  return tiles_[name] = tx;
}

void Renderer::tileAt(const char* name, float x, float y, float w, float h, Rectangle clip) {  // a tile, cropped to clip
  Texture2D tx = tile(name);
  if (!tx.id) return;
  float x0 = std::max(x, clip.x), y0 = std::max(y, clip.y), x1 = std::min(x + w, clip.x + clip.width), y1 = std::min(y + h, clip.y + clip.height);
  if (x1 <= x0 || y1 <= y0) return;
  Rectangle src{(x0 - x) / w * tx.width, (y0 - y) / h * tx.height, (x1 - x0) / w * tx.width, (y1 - y0) / h * tx.height};
  DrawTexturePro(tx, src, {x0, y0, x1 - x0, y1 - y0}, {0, 0}, 0, TILE_TINT);
}

void Renderer::drawLandSolid(const World& w, const Box& s) {
  Rectangle view{w.cam.x - 80, w.cam.y - 80, ui.VW + 160, ui.VH + 160}, r{s.x, s.y, s.w, s.h};
  if (!CheckCollisionRecs(view, r)) return;
  std::string st = tileStyle(s);
  if (st == "box") { tileAt("platformer_wooden_box", s.x, s.y, s.w, s.h, r); return; }
  if (st == "float" || st == "bridge") {  // one row, drawn a little thicker than its collider
    bool bridge = st == "bridge";
    float tw = bridge ? 32 : TILE, th = bridge ? 32 : TILE;
    int n = std::max(1, int(std::ceil(s.w / tw)));
    Rectangle clip{s.x, s.y - 8, s.w, th + 8};
    for (int i = 0; i < n; i++) {
      const char* nm = bridge ? (i % 2 ? "platformer_bridge_part_02" : "platformer_bridge_part_01")
                       : n == 1     ? "platformer_ground_11"
                       : i == 0     ? "platformer_ground_10"
                       : i == n - 1 ? "platformer_ground_12"
                                    : "platformer_ground_11";
      float x = (!bridge && i == n - 1 && n > 1) ? s.x + s.w - tw : s.x + i * tw;  // right cap flush with the edge
      tileAt(nm, x, s.y - (bridge ? 6 : 8), tw, th, clip);
    }
    return;
  }
  // ground and stone: a grid of tiles, only where visible
  float T = st == "stone" ? std::min(TILE, s.w) : TILE;
  float vx0 = std::max(s.x, view.x), vy0 = std::max(s.y, view.y), vx1 = std::min(s.x + s.w, view.x + view.width), vy1 = std::min(s.y + s.h, view.y + view.height);
  int c0 = int((vx0 - s.x) / T), c1 = int(std::ceil((vx1 - s.x) / T)), r0 = int((vy0 - s.y) / T), r1 = int(std::ceil((vy1 - s.y) / T));
  int cols = std::max(1, int(std::ceil(s.w / T)));
  for (int ry = r0; ry < r1; ry++)
    for (int cx = c0; cx < c1; cx++) {
      // where another solid continues the ground, no grass lip or rocky edge: neighbours read as one mass
      float tx = s.x + cx * T, ty = s.y + ry * T;
      bool covered = w.solidAt(tx + T / 2, s.y - 2), leftOpen = cx == 0 && !w.solidAt(s.x - 2, ty + T / 2),
           rightOpen = cx == cols - 1 && !w.solidAt(s.x + s.w + 2, ty + T / 2);
      const char* nm;
      if (st == "stone") nm = "platformer_stone";
      else if (ry == 0 && !covered) nm = leftOpen ? "platformer_ground_08" : rightOpen ? "platformer_ground_04" : "platformer_ground_02";
      else nm = leftOpen ? "platformer_ground_09" : rightOpen ? "platformer_ground_13" : "platformer_ground_06";
      tileAt(nm, tx, ty, T, T, r);
    }
}

void Renderer::drawSpriteProp(const Prop& p) {  // any tileset image, standing on (x, y)
  Texture2D tx = tile(p.name);
  if (!tx.id) return;
  float w = tx.width * .5f * p.scale, h = tx.height * .5f * p.scale;
  DrawTexturePro(tx, {0, 0, p.flip ? -float(tx.width) : float(tx.width), float(tx.height)}, {p.x - w / 2, p.y - h, w, h}, {0, 0}, 0, TILE_TINT);
}

void Renderer::drawProp(const World& w, const Prop& p) {
  float x = p.x, y = p.y, t = w.t;
  if (p.type == "hut") {  // round mud house, thatched cone
    DrawRectangleRounded({x - 34, y - 40, 68, 40}, .35f, 6, hex("#3b2a22"));
    DrawTriangle({x, y - 92}, {x - 48, y - 36}, {x + 48, y - 36}, hex("#4a3820"));
    for (int i = -3; i <= 3; i++) DrawLineEx({x, y - 90}, {x + i * 14.f, y - 37}, 1, hex("#2c2014"));
    DrawRectangleV({x - 8, y - 26}, {16, 26}, hex("#e0903a", .85f));
  } else if (p.type == "villager") {  // they are not only mages: a figure watching the sea
    float sw = std::sin(t * 1.3f + x) * 1.5f;
    Color robe = int(x) % 3 == 0 ? hex("#5a3a6a") : int(x) % 3 == 1 ? hex("#8a5a2a") : hex("#2f4a6a");
    fillPoly({{x - 9, y}, {x - 6 + sw, y - 40}, {x + 6 + sw, y - 40}, {x + 10, y}}, robe);
    DrawCircleV({x + sw, y - 47}, 6, hex("#3a2418"));
    DrawEllipse(int(x + sw), int(y - 54), 8, 6, int(x) % 2 ? hex("#c8a050") : hex("#a8402a"));  // headwrap
    DrawLineEx({x + 9 + sw, y - 34}, {x + 13, y + 2}, 2, hex("#5a4632"));                         // a spear or a staff
  } else if (p.type == "fire") {
    DrawLineEx({x - 12, y - 2}, {x + 12, y - 6}, 4, hex("#3a2414"));
    DrawLineEx({x - 12, y - 6}, {x + 12, y - 2}, 4, hex("#3a2414"));
    for (int i = 0; i < 3; i++) {
      float h = 18 + 8 * std::sin(t * 9 + i * 2.1f), fw = 7 - i * 1.5f;
      DrawTriangle({x - fw + i * 3 - 3, y - 4}, {x + fw + i * 3 - 3, y - 4}, {x + i * 3 - 3, y - 4 - h}, i ? hex("#f2a03a") : hex("#e0602a"));
    }
    for (int i = 0; i < 5; i++) {
      float k = std::fmod(t * .6f + i * .2f, 1.f);
      DrawRectangleV({x + std::sin(i * 7 + t) * 8, y - 20 - k * 60}, {2, 2}, hex("#f6c27a", 1 - k));
    }
  } else if (p.type == "post") {
    DrawRectangleV({x - 5, y}, {10, 260}, hex("#2e2216"));
  } else if (p.type == "boat") {  // a pirogue riding the swell
    float b = std::sin(t * 1.2f + x) * 2;
    fillPoly({{x - 70, b - 7}, {x + 70, b - 7}, {x + 52, b + 6}, {x - 52, b + 6}}, hex("#3a2618"));
    DrawLineEx({x - 70, b - 7}, {x + 70, b - 7}, 2, hex("#5a4028"));
  }
}
