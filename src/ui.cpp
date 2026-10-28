#include "ui.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>

#include "util.hpp"

namespace {
std::string firstFile(std::initializer_list<const char*> paths) {
  for (auto p : paths) if (FileExists(p)) return p;
  return "";
}
}  // namespace

void Ui::loadFonts() {
  monoPath_ = firstFile({"/usr/share/fonts/dejavu-sans-mono-fonts/DejaVuSansMono.ttf", "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
                         "/usr/share/fonts/liberation-mono-fonts/LiberationMono-Regular.ttf", "C:/Windows/Fonts/consola.ttf"});
  serifPath_ = firstFile({"/usr/share/fonts/google-noto-vf/NotoSerif[wght].ttf", "/usr/share/fonts/liberation-serif-fonts/LiberationSerif-Regular.ttf",
                          "/usr/share/fonts/TTF/DejaVuSerif.ttf", "C:/Windows/Fonts/georgia.ttf"});
}

bool Ui::resize() {
  int w = GetScreenWidth(), h = GetScreenHeight();
  if (w == RW && h == RH) return false;
  RW = w; RH = h;
  S = std::min(h / 540.f, w / 600.f); VW = w / S; VH = h / S;
  return true;
}

Font Ui::font(bool serif, int px) {
  int key = px * 2 + serif;
  if (auto it = cache_.find(key); it != cache_.end()) return it->second;
  const std::string& path = serif ? serifPath_ : monoPath_;
  Font f = path.empty() ? GetFontDefault() : LoadFontEx(path.c_str(), px, nullptr, 0);
  SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
  return cache_[key] = f;
}

int Ui::fontPx(float size) const { return std::max(8, int(std::lround(size * S))); }

float Ui::textW(const char* s, float size, bool serif) {
  int p = fontPx(size);
  return MeasureTextEx(font(serif, p), s, float(p), 0).x / S;
}

void Ui::text(const char* s, float x, float y, float size, Color c, bool serif, bool center) {
  int p = fontPx(size);
  Font f = font(serif, p);
  if (center) x -= textW(s, size, serif) / 2;
  Vector2 at{std::round(x * S), std::round(y * S)};
  DrawTextEx(f, s, {at.x + 1, at.y + 1}, float(p), 0, hex("#05070a", c.a / 255.f * .8f));  // shadow: readable on the bright sky too
  DrawTextEx(f, s, at, float(p), 0, c);
}

Vector2 Ui::mouse() const { return Vector2Scale(GetMousePosition(), 1 / S); }

Rectangle Ui::px(float x, float y, float w, float h) const {
  return {std::round(x * S), std::round(y * S), std::round(w * S), std::round(h * S)};
}
