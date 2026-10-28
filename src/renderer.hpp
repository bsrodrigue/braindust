// Draws a frame: 3D background (bg) + 2D world with darkness (world) + 3D near pass (fg) -> post shader -> HUD.
// renderer.cpp: passes and compositing. scenery.cpp: sky, tiles, props. actors.cpp: player, enemies, pickups. hud.cpp: HUD.
#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "panel.hpp"
#include "scene3d.hpp"
#include "ui.hpp"
#include "world.hpp"

class Renderer {
 public:
  Ui ui;

  // What the GPU spends per frame. The post pass and the render size dominate on phones (see drawTimings / F3).
  //   Low:    at most 540 px tall, no god rays, no caustics, no chromatic aberration
  //   Medium: at most 720 px tall, god rays in 8 steps (Android's default)
  //   High:   full size, everything (the desktop's default)
  // All three draw the 3D background at half size except High: it is soft and fogged by design.
  enum class Quality { Low, Medium, High };
  static const char* qualityName(Quality q);
  static Quality qualityByName(const std::string& name, Quality otherwise);
  void setQuality(Quality q);
  Quality quality() const { return quality_; }

  void load();
  void resize(World& w);  // render targets follow the window and the quality; also sets the world's view size
  // overlay: drawn over the HUD, in HUD units (menus, tune pages, help, touch controls)
  void draw(const World& w, Scene3D& scene3d, const std::function<void()>& overlay);
  void drawHelp(bool touch);

  // F3 (or the menu): frame timings, ms. While on, the renderer waits for the GPU after each pass, so a pass's time
  // includes its GPU work (and the frame gets a little slower). present includes waiting for the screen's refresh.
  struct Timings { double scene3d = 0, world = 0, post = 0, present = 0, sim = 0; };
  bool profiling = false;
  Timings avg;      // smoothed over about half a second
  int profiled = 0; // frames measured
  void addSim(double ms) { avg.sim += (ms - avg.sim) * .05; }
  void drawTimings();
  bool screenshot(const std::string& path) const;

 private:
  RenderTexture2D bg_{}, fg_{}, world_{}, dark_{}, merged_{}, bloom_{}, final_{};
  Shader post_{}, tile_{}, merge_{}, bloomSh_{};
  Quality quality_ = Quality::High;
  bool targetsDirty_ = false;  // the quality changed: rebuild the render targets
  std::unordered_map<std::string, Texture2D> tiles_;
  struct Mote { float x, y, d; };
  std::vector<Mote> snow_;
  std::vector<Vector3> stars_;
  float darkNow_ = 0;  // darkness overlay alpha this frame (the post shader needs it)
  Vector2 shake_{};    // this frame's screen shake, world px: it moves the 2D world, not the 3D background

  // renderer.cpp
  void worldPass(const World& w, const Scene3D& s);
  void darknessPass(const World& w, const Scene3D& s);
  void lightsPass(const World& w, const Scene3D& s, Camera2D c2);
  void inscriptions(const World& w);
  void composite(const World& w, const std::function<void()>& overlay);
  Vector2 toScreen(const World& w, float wx, float wy, bool shaken = true) const;  // shaken: moves with the 2D world's shake
  Vector2 project(const World& w, const Camera3D& c, Vector3 p) const;  // a 3D point to world px

  // scenery.cpp
  void drawSky(const World& w, float waterY);
  Texture2D tile(const std::string& name);
  void tileAt(const char* name, float x, float y, float w, float h, Rectangle clip);
  void drawLandSolid(const World& w, const Box& s);
  void drawSpriteProp(const Prop& p);
  void drawProp(const World& w, const Prop& p);

  // actors.cpp
  void drawPlayer(const World& w);
  void drawCranium(const World& w, float ga);
  void drawEnemy(const World& w, const Enemy& e);
  void drawCrack(Rectangle k, Color fill, Color stroke, const Texture2D* galaxy);
  void drawWrapper(const World& w, const Wrapper& wr);
  void drawItem(const World& w, const Pickup& it);
  void drawSeal(const World& w, const Box& b);

  // hud.cpp
  void drawHud(const World& w);
};

Vector2 lurePos(const Enemy& e);
void fillPoly(const std::vector<Vector2>& p, Color c);
void glow(float x, float y, float r, Color c);
