// Draws a frame: 3D background (bg) + 2D world with darkness (world) + 3D near pass (fg) -> post shader -> HUD.
// renderer.cpp: passes and compositing. scenery.cpp: sky, tiles, props. actors.cpp: player, enemies, pickups. hud.cpp: HUD.
#pragma once
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

  void load();
  void resize(World& w);  // render targets follow the window; also sets the world's view size
  void draw(const World& w, Scene3D& scene3d, Panel* panel, bool help);  // panel: drawn over the HUD when open
  bool screenshot(const std::string& path) const;

 private:
  RenderTexture2D bg_{}, fg_{}, world_{}, dark_{}, final_{};
  Shader post_{}, tile_{};
  std::unordered_map<std::string, Texture2D> tiles_;
  struct Mote { float x, y, d; };
  std::vector<Mote> snow_;
  std::vector<Vector3> stars_;
  float darkNow_ = 0;  // darkness overlay alpha this frame (the post shader needs it)

  // renderer.cpp
  void worldPass(const World& w, const Scene3D& s);
  void darknessPass(const World& w, const Scene3D& s);
  void lightsPass(const World& w, const Scene3D& s, Camera2D c2);
  void inscriptions(const World& w);
  void composite(const World& w, Panel* panel);
  Vector2 toScreen(const World& w, float wx, float wy) const;
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
  void drawHelp();
};

Vector2 lurePos(const Enemy& e);
void fillPoly(const std::vector<Vector2>& p, Color c);
void glow(float x, float y, float r, Color c);
