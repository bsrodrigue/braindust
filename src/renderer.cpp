#include "renderer.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "rlgl.h"
#include "util.hpp"

extern "C" void glFinish(void);  // OpenGL and OpenGL ES alike; only called while profiling

namespace {
void premultBlend() {
  rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
  BeginBlendMode(BLEND_CUSTOM_SEPARATE);
}
void addBlend() {
  rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE, RL_ZERO, RL_ONE, RL_FUNC_ADD, RL_FUNC_ADD);
  BeginBlendMode(BLEND_CUSTOM_SEPARATE);
}
bool inFront(const Camera3D& c, Vector3 p) { return Vector3DotProduct(Vector3Subtract(p, c.position), V3(0, 0, -1)) > 0; }
float wrap(float v, float m) { return std::fmod(std::fmod(v, m) + m, m); }
}  // namespace

void fillPoly(const std::vector<Vector2>& p, Color c) {
  Vector2 ctr{0, 0};
  for (auto& v : p) { ctr.x += v.x; ctr.y += v.y; }
  ctr.x /= p.size(); ctr.y /= p.size();
  for (size_t i = 0; i < p.size(); i++) DrawTriangle(ctr, p[i], p[(i + 1) % p.size()], c);
}
void glow(float x, float y, float r, Color c) { DrawCircleGradient(int(x), int(y), r, c, alpha(c, 0)); }

const char* Renderer::qualityName(Quality q) { return q == Quality::Low ? "low" : q == Quality::Medium ? "medium" : "high"; }
Renderer::Quality Renderer::qualityByName(const std::string& name, Quality otherwise) {
  for (Quality q : {Quality::Low, Quality::Medium, Quality::High}) if (name == qualityName(q)) return q;
  return otherwise;
}
void Renderer::setQuality(Quality q) {
  quality_ = q;
  ui.maxHeight = q == Quality::Low ? 540 : q == Quality::Medium ? 720 : 0;
  targetsDirty_ = true;
}

void Renderer::load() {
  ui.loadFonts();
  post_ = platform::loadShader(nullptr, "shaders/post.fs");
  merge_ = platform::loadShader(nullptr, "shaders/merge.fs");
  bloomSh_ = platform::loadShader(nullptr, "shaders/bloom.fs");
  tile_ = platform::loadShader(nullptr, "shaders/tile.fs");
  float sat = .5f;
  SetShaderValue(tile_, GetShaderLocation(tile_, "uSat"), &sat, SHADER_UNIFORM_FLOAT);
  for (int i = 0; i < 700; i++) snow_.push_back({frand() * 2000, frand() * 2000, .05f + frand() * .4f});
  for (int i = 0; i < 260; i++) stars_.push_back({frand() * 2000, frand() * 1200, .05f + frand() * .4f});
}

void Renderer::resize(World& w) {
  bool first = ui.RW == 0;
  if (ui.resize() || targetsDirty_) {
    targetsDirty_ = false;
    auto all = {&bg_, &fg_, &world_, &dark_, &merged_, &bloom_, &final_};
    if (!first) for (auto* r : all) UnloadRenderTexture(*r);
    int div = quality_ == Quality::High ? 1 : 2;  // the 3D background (and near pass) at half size: soft and fogged anyway
    for (auto* r : all) {
      int w = r == &bg_ || r == &fg_ ? ui.RW / div : r == &bloom_ ? ui.RW / 4 : ui.RW;
      int h = r == &bg_ || r == &fg_ ? ui.RH / div : r == &bloom_ ? ui.RH / 4 : ui.RH;
      *r = LoadRenderTexture(std::max(1, w), std::max(1, h));
      SetTextureFilter(r->texture, TEXTURE_FILTER_BILINEAR);
    }
  }
  w.view = {ui.VW, ui.VH};
}

Vector2 Renderer::toScreen(const World& w, float wx, float wy, bool shaken) const {
  Vector2 k = shaken ? shake_ : Vector2{0, 0};
  return {(wx - w.cam.x + k.x) * ui.S, (wy - w.cam.y + k.y) * ui.S};
}
Vector2 Renderer::project(const World& w, const Camera3D& c, Vector3 p) const {
  Vector2 s = GetWorldToScreenEx(p, c, ui.RW, ui.RH);
  return {s.x / ui.S + w.cam.x, s.y / ui.S + w.cam.y};
}

void Renderer::draw(const World& w, Scene3D& scene3d, const std::function<void()>& overlay) {
  double t0 = GetTime();
  auto lap = [&](double& avgMs) {  // the time since the last lap, GPU work included
    if (!profiling) return;
    rlDrawRenderBatchActive();
    glFinish();
    double t = GetTime(), ms = (t - t0) * 1000;
    avgMs += (ms - avgMs) * (profiled ? .05 : 1);
    t0 = t;
  };
  scene3d.render(w, bg_, fg_);
  lap(avg.scene3d);
  worldPass(w, scene3d);
  lap(avg.world);
  composite(w, [&] {
    if (overlay) overlay();
    if (profiling) drawTimings();
  });
  lap(avg.post);
  BeginDrawing();
  ClearBackground(BLACK);
  DrawTexturePro(final_.texture, {0, 0, float(ui.RW), -float(ui.RH)}, {0, 0, float(GetScreenWidth()), float(GetScreenHeight())}, {0, 0}, 0, WHITE);
  EndDrawing();
  lap(avg.present);
  if (profiling) profiled++;
}

void Renderer::drawTimings() {
  const char* s = TextFormat("%d fps   3D %.1f   world %.1f   post+HUD %.1f   present %.1f   sim %.1f ms   %dx%d %s", GetFPS(), avg.scene3d,
                             avg.world, avg.post, avg.present, avg.sim, ui.RW, ui.RH, qualityName(quality_));
  ui.text(s, ui.VW / 2, 6, 10, hex("#f0c060"), false, true);
}

bool Renderer::screenshot(const std::string& path) const {
  Image img = LoadImageFromTexture(final_.texture);
  ImageFlipVertical(&img);
  bool ok = ExportImage(img, path.c_str());
  UnloadImage(img);
  return ok;
}

// ---------------------------------------------------------------- the 2D world, then darkness and the lights that pierce it
void Renderer::worldPass(const World& w, const Scene3D& s) {
  const float S = ui.S, VW = ui.VW, VH = ui.VH;
  const int RW = ui.RW, RH = ui.RH;
  shake_ = {(frand() - .5f) * w.shake * 24, (frand() - .5f) * w.shake * 24};
  Camera2D c2{{0, 0}, {w.cam.x - shake_.x, w.cam.y - shake_.y}, 0, S};
  BeginTextureMode(world_);
  ClearBackground(BLANK);
  rlDisableBackfaceCulling();
  premultBlend();
  if (w.inPocket) {  // the cosmos inside the crack
    DrawRectangle(0, 0, RW, RH, hex("#040308"));
    float gs = VH * 1.2f * S;
    DrawTexturePro(s.galaxy, {0, 0, 512, 512}, {RW * .6f - gs / 2 - w.cam.x * .04f * S, RH * .45f - gs / 2 - w.cam.y * .04f * S, gs, gs}, {0, 0}, 0,
                   hex("#ffffff", .7f));
    for (auto& st : stars_)
      DrawRectangleV({wrap(st.x - w.cam.x * st.z, VW) * S, wrap(st.y - w.cam.y * st.z, VH) * S}, {1.5f * S, 1.5f * S},
                     hex("#c8c0d4", .2f + .6f * std::fabs(std::sin(w.t * st.z * 3 + st.x))));
  } else {
    for (auto& m : snow_)  // marine snow drifting behind the playfield
      DrawRectangleV({wrap(m.x - w.cam.x * m.d, VW) * S, wrap(m.y - w.cam.y * m.d - w.t * 6 * m.d, VH) * S}, {S * (.6f + m.d), S * (.6f + m.d)},
                     hex("#7d8a8e", .15f + m.d * .3f));
    float waterY = (0 - w.cam.y) * S;
    if (waterY > 0) drawSky(w, waterY);
  }
  BeginMode2D(c2);
  if (w.inPocket) {
    for (auto& b : *w.solids)
      if (!b.open) { DrawRectangleRec({b.x, b.y, b.w, b.h}, hex("#16121e")); DrawRectangleRec({b.x, b.y, b.w, 1.5f}, hex("#5a4f6a")); }
    drawCrack(pocket::exitCrack, hex("#071a22"), hex("#5d7f8f"), nullptr);
    const Rectangle& a = pocket::altar;
    DrawRectangleRec({a.x, a.y + 24, a.width, a.height - 24}, hex("#5e4c30"));
  } else {
    BeginShaderMode(tile_);
    for (auto& p : w.level.props) if (p.type == "sprite") drawSpriteProp(p);
    EndShaderMode();
    for (auto& p : w.level.props) if (p.type != "sprite") drawProp(w, p);
    std::vector<Vector2> surf;  // the sea surface: a lit skin over the dark water
    for (float x = -300; x <= w.W + 300; x += 20) surf.push_back({x, std::sin(x * .03f + w.t * 2) * 3});
    for (size_t i = 1; i < surf.size(); i++) {
      DrawTriangle(surf[i - 1], {surf[i - 1].x, 18}, surf[i], hex("#8fc0cc", .18f));
      DrawTriangle(surf[i], {surf[i - 1].x, 18}, {surf[i].x, 18}, hex("#8fc0cc", .18f));
      DrawLineEx(surf[i - 1], surf[i], 2, hex("#e8c8a0", .7f));
    }
    if (!s.hasDreamer()) {  // 2D fallback when the skull model is missing
      DrawEllipse(int(w.level.portal.x), int(w.H), 190, 152, hex("#26251f"));
      drawCrack(w.cracks[0].r, hex("#05030c"), hex("#6a5f8a"), &s.galaxy);
      drawCrack(w.cracks[1].r, hex("#010101"), hex("#3a382f"), nullptr);
    }
    for (auto& z : w.level.zones) {  // where a mage's skull burst: the water itself is wrong here
      for (int i = 0; i < 14; i++) {
        float u = std::fmod(i * .618f + w.t * .05f, 1.f), v = std::fmod(i * .382f - w.t * .08f * (1 + i % 3), 1.f);
        if (v < 0) v += 1;
        DrawRectangleV({z.r.x + u * z.r.w, z.r.y + v * z.r.h}, {2, 2}, hex(i % 2 ? "#8c7aa8" : "#8f7448", .5f));
      }
    }
    for (auto& b : *w.solids)  // rock under the sea stays dark
      if (b.y >= 0 && !b.open) { DrawRectangleRec({b.x, b.y, b.w, b.h}, hex("#030506")); DrawRectangleRec({b.x, b.y, b.w, 1.5f}, hex("#152229")); }
    BeginShaderMode(tile_);  // land is tiled
    for (auto& b : *w.solids) if (b.y < 0 && !b.open) drawLandSolid(w, b);
    EndShaderMode();
    for (auto& b : *w.solids) if (b.seal > 0 && !b.open) drawSeal(w, b);
    for (auto& wr : w.level.wrappers) if (!wr.taken && w.seen(wr.x, wr.y, wr.reveal)) drawWrapper(w, wr);
    for (auto& it : w.level.items) if (!it.taken && w.seen(it.x, it.y, it.reveal)) drawItem(w, it);
  }
  for (auto& e : w.enemies) if (e.alive) drawEnemy(w, e);
  if (w.outcome == Outcome::Playing) drawPlayer(w);
  for (auto& p : w.particles) DrawRectangleV({p.x, p.y}, {p.size, p.size}, alpha(p.c, p.life / p.max));
  EndMode2D();
  EndBlendMode();
  EndTextureMode();

  darknessPass(w, s);

  BeginTextureMode(world_);
  premultBlend();
  DrawTextureRec(dark_.texture, {0, 0, float(RW), -float(RH)}, {0, 0}, WHITE);
  EndBlendMode();
  lightsPass(w, s, c2);
  inscriptions(w);
  EndTextureMode();
  rlEnableBackfaceCulling();
}

// darkness deepens with depth; light comes from your cranium, bolts, flares, level lights and universes in skulls
void Renderer::darknessPass(const World& w, const Scene3D& s) {
  float dark = w.inPocket ? 0 : clampf((w.cam.y + ui.VH / 2 - 60) / 4000, 0, .68f);
  darkNow_ = dark;
  BeginTextureMode(dark_);
  ClearBackground({0, 1, 2, (unsigned char)(dark * 255)});
  rlSetBlendFactors(RL_ZERO, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD);  // destination-out
  BeginBlendMode(BLEND_CUSTOM);
  auto hole = [&](float x, float y, float r, bool shaken = true) {  // 3D sources do not shake: their holes stay on them
    Vector2 p = toScreen(w, x, y, shaken);
    DrawCircleGradient(int(p.x), int(p.y), r * ui.S, WHITE, BLANK);
  };
  if (!w.inPocket) {
    const Player& P = w.player;
    if (w.outcome == Outcome::Playing) hole(P.x + P.w / 2, w.headY() + 20, w.lightR());
    for (auto& sh : w.shots) hole(sh.x, sh.y, 80 + sh.r * 4);
    Vector2 pc = w.portalCentre();
    hole(pc.x, pc.y, s.hasDreamer() ? 300 + std::sin(w.t * 1.3f) * 25 : 130, !s.hasDreamer());  // the dreamer is 3D
    for (auto& a : s.crackAnchors()) if (inFront(s.camera, a)) { Vector2 p = project(w, s.camera, a); hole(p.x, p.y, 220, false); }
    for (auto& l : w.level.lights) hole(l.x, l.y, l.r);
    for (auto& wr : w.level.wrappers) if (!wr.taken && w.seen(wr.x, wr.y, wr.reveal)) hole(wr.x, wr.y, 50);
    for (auto& it : w.level.items) if (!it.taken && w.seen(it.x, it.y, it.reveal)) hole(it.x, it.y, 40);
  }
  EndBlendMode();
  EndTextureMode();
}

void Renderer::lightsPass(const World& w, const Scene3D& s, Camera2D c2) {
  const Player& P = w.player;
  const float t = w.t;
  addBlend();
  BeginMode2D(c2);
  for (auto& e : w.enemies) {
    if (!e.alive) continue;
    const std::string kind = w.enemyType(e).str("draw", "lurker");
    float cx = e.x + e.w / 2, cy = e.y + e.h / 2;
    if (kind == "jelly") glow(cx, cy - 2, 22 + 4 * std::sin(e.ph * 3), hex("#7fd8d0", .35f));
    if (kind == "leech" && P.dust > 0) glow(cx, cy, 10 + P.dust * .2f, hex("#8c7aa8", std::min(.5f, P.dust / 120)));
    if (kind != "lurker" || e.mode != EnemyMode::Drift) continue;  // a lure that goes out means it has seen you
    Vector2 l = lurePos(e);
    glow(l.x, l.y, 14, hex("#5fbfa4", .67f));
    DrawRectangleV({l.x - 1, l.y - 1}, {2, 2}, hex("#cfe8df"));
  }
  for (auto& sh : w.shots) glow(sh.x, sh.y, sh.r * 3, hex("#b8accc", .87f));
  if (w.outcome == Outcome::Playing && P.dust > 0) glow(P.x + P.w / 2, w.headY() - P.dust * .2f, 8 + P.dust * .25f, hex("#8c7aa8", .15f + P.dust / 300));
  if (w.inPocket) {
    glow(pocket::altar.x + 15, pocket::altar.y + 8 + std::sin(t * 2) * 3, 30, hex("#e8dcc4", .67f));
  } else {
    Vector2 pc = w.portalCentre();
    glow(pc.x, pc.y, 50, hex("#6a5f8a", .27f));
    for (auto& l : w.level.lights) glow(l.x, l.y, l.r * .35f, alpha(l.c, .35f + .1f * std::sin(t * .9f + l.x)));
    for (auto& p : w.level.props) {
      if (p.type == "fire") glow(p.x, p.y - 14, 80 + 6 * std::sin(t * 11), hex("#f2a03a", .45f));
      if (p.type == "hut") glow(p.x, p.y - 14, 34, hex("#e0903a", .35f));
    }
    for (auto& wr : w.level.wrappers)
      if (!wr.taken && w.seen(wr.x, wr.y, wr.reveal)) glow(wr.x, wr.y, 30, hex("#c8a868", .25f + .1f * std::sin(t * 2)));
    for (auto& it : w.level.items)
      if (!it.taken && w.seen(it.x, it.y, it.reveal)) glow(it.x, it.y, 24, hex("#b89cff", .25f + .1f * std::sin(t * 2.4f)));
    for (auto& L : s.towerLights()) {  // windows of the drowned tower: dim, flickering, some long dead
      if (L.dead || !inFront(s.camera, L.p)) continue;
      Vector2 p = project(w, s.camera, L.p);
      if (p.y < 0) continue;  // the tower is drowned: none of its light shows in the sky
      float f = L.crown ? .35f + .2f * std::sin(t * .7f) : (.55f + .45f * std::sin(t * .9f + L.ph)) * (std::sin(t * 7 + L.ph * 3) > -.92f ? 1 : .2f);
      glow(p.x, p.y, L.crown ? 40 : 8, L.crown ? hex("#7a6aa8", f * .5f) : hex("#c89a50", f * .6f));
    }
    if (s.farLeviathanVisible(w)) {
      Vector3 eyes[2];
      s.farLeviathanEyes(eyes);
      for (auto e : eyes) if (inFront(s.camera, e)) { Vector2 p = project(w, s.camera, e); glow(p.x, p.y, 9, hex("#c8c0a0", .47f)); }
    }
    if (w.cam.y > 1500 && w.photoOff <= 0) {  // photophores far below: a row of lights on something enormous
      const Vector3& cp = s.camera.position;
      for (int i = 0; i < 14; i++) {
        float b = std::pow(std::max(0.f, std::sin(t * 1.6f - i * .45f)), 3.f);
        Vector3 p3 = V3(cp.x - 8 + std::sin(t * .04f) * 14 + i * 1.3f, cp.y - 9 + std::sin(t * .1f) * 1.5f + std::sin(i * .5f + t * .2f) * 1.1f, -70);
        if (b > .02f) { Vector2 p = project(w, s.camera, p3); glow(p.x, p.y, 5, hex("#6fe3d0", b * .55f * clampf((w.cam.y - 1500) / 1000, 0, 1))); }
      }
    }
  }
  EndMode2D();
  EndBlendMode();
}

// read by coming close, no hand holding (screen space so they stay crisp)
void Renderer::inscriptions(const World& w) {
  premultBlend();
  if (!w.inPocket) {
    Vector2 me = w.playerCentre();
    for (auto& x : w.level.texts) {
      float d = std::hypot(me.x - x.x, me.y - x.y), a = clampf((220 - d) / 100, 0, 1) * .9f;
      if (a <= 0) continue;
      int px = ui.fontPx(14);
      Font f = ui.font(true, px);
      Vector2 m = MeasureTextEx(f, x.text.c_str(), float(px), 0), sp = toScreen(w, x.x, x.y);
      DrawTextEx(f, x.text.c_str(), {std::round(sp.x - m.x / 2), std::round(sp.y - m.y / 2)}, float(px), 0, hex("#d8cfbc", a));
    }
  }
  EndBlendMode();
}

// one pass of shader sh over a whole target, reading src; blending off, so what it writes (alpha too) is what is stored
static void fullscreen(RenderTexture2D& target, Shader sh, Texture2D src, const std::function<void()>& uniforms) {
  BeginTextureMode(target);
  rlDisableColorBlend();
  BeginShaderMode(sh);
  if (uniforms) uniforms();
  DrawTexturePro(src, {0, 0, float(src.width), -float(src.height)}, {0, 0, float(target.texture.width), float(target.texture.height)}, {0, 0}, 0, WHITE);
  EndShaderMode();
  rlEnableColorBlend();
  EndTextureMode();
}

// the layers merged once (so the post pass reads one texture, not three), bloom at a quarter size, then the post pass
void Renderer::composite(const World& w, const std::function<void()>& overlay) {
  Vector2 res{float(ui.RW), float(ui.RH)};
  fullscreen(merged_, merge_, bg_.texture, [&] {
    SetShaderValue(merge_, GetShaderLocation(merge_, "uRes"), &res, SHADER_UNIFORM_VEC2);
    SetShaderValueTexture(merge_, GetShaderLocation(merge_, "texWorld"), world_.texture);
    SetShaderValueTexture(merge_, GetShaderLocation(merge_, "texFg"), fg_.texture);
  });
  // bloom at every quality: cheap at a quarter size, and the sunset's glow is mostly bloom
  fullscreen(bloom_, bloomSh_, merged_.texture, [&] {
    Vector2 out{float(bloom_.texture.width), float(bloom_.texture.height)};
    SetShaderValue(bloomSh_, GetShaderLocation(bloomSh_, "uRes"), &res, SHADER_UNIFORM_VEC2);
    SetShaderValue(bloomSh_, GetShaderLocation(bloomSh_, "uOut"), &out, SHADER_UNIFORM_VEC2);
  });
  BeginTextureMode(final_);
  BeginShaderMode(post_);
  auto set = [&](const char* name, const void* v, int type) { SetShaderValue(post_, GetShaderLocation(post_, name), v, type); };
  float shallow = w.inPocket ? 0 : clampf(1 - w.cam.y / 1500, 0, 1), deep = w.inPocket ? 0 : clampf(w.cam.y / 3500, 0, 1), pk = w.inPocket;
  float waterY = w.inPocket ? -1e5f : (0 - w.cam.y) * ui.S;
  set("uRes", &res, SHADER_UNIFORM_VEC2);
  set("uCam", &w.cam, SHADER_UNIFORM_VEC2);
  set("uS", &ui.S, SHADER_UNIFORM_FLOAT);
  set("uTime", &w.t, SHADER_UNIFORM_FLOAT);
  set("uShallow", &shallow, SHADER_UNIFORM_FLOAT);
  set("uDeep", &deep, SHADER_UNIFORM_FLOAT);
  set("uPocket", &pk, SHADER_UNIFORM_FLOAT);
  set("uWarp", &w.warp, SHADER_UNIFORM_FLOAT);
  set("uWaterY", &waterY, SHADER_UNIFORM_FLOAT);
  set("uDark", &darkNow_, SHADER_UNIFORM_FLOAT);
  float on = quality_ != Quality::Low, rays = quality_ == Quality::High ? 16.f : quality_ == Quality::Medium ? 8.f : 0.f, bl = 1;
  set("uChroma", &on, SHADER_UNIFORM_FLOAT);
  set("uCaustics", &on, SHADER_UNIFORM_FLOAT);
  set("uBloom", &bl, SHADER_UNIFORM_FLOAT);
  set("uRays", &rays, SHADER_UNIFORM_FLOAT);
  SetShaderValueTexture(post_, GetShaderLocation(post_, "texBloom"), bloom_.texture);
  DrawTexturePro(merged_.texture, {0, 0, res.x, -res.y}, {0, 0, res.x, res.y}, {0, 0}, 0, WHITE);
  EndShaderMode();
  drawHud(w);
  if (overlay) overlay();
  EndTextureMode();
}
