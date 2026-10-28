// The 3D background: rocks, seafloor, Blender skulls and tower, the leviathans. 1 unit = 100 world px, y up.
#pragma once
#include <vector>

#include "raylib.h"
#include "world.hpp"

class Scene3D {
 public:
  Texture2D galaxy{};  // the cosmos seen through cracks; the 2D pass uses it too
  Camera3D camera{};

  void load();                        // shaders, meshes, models; once, after the window exists
  void place(const World& w);         // level-dependent layout: call after World::reset
  void update(float dt, World& w);    // camera, leviathan passes, the portal's first surge, the drone
  void render(const World& w, RenderTexture2D& bg, RenderTexture2D& fg);

  // what the 2D pass needs to light and glow
  struct TowerLight { Vector3 p; float ph; bool dead, crown; };
  bool hasDreamer() const { return dreamer_.ok; }  // the portal skull model loaded
  const std::vector<Vector3>& crackAnchors() const { return crackAnchors_; }
  const std::vector<TowerLight>& towerLights() const { return towerLights_; }
  bool farLeviathanVisible(const World& w) const { return !w.nearActive && w.cam.y > 1200; }
  void farLeviathanEyes(Vector3 out[2]) const;

 private:
  struct Seg { Vector3 p; float r; };
  enum class Kind { Rock, Skull, UniverseSkull, Tower, ProceduralSkull, Floor };
  struct Static { Kind kind; int aux; Matrix m; };  // aux: rock mesh index, or 1 for the playfield skulls

  Shader sceneSh_{};
  Material rock_{}, bone_{}, floor_{}, skin_{}, fin_{}, tooth_{}, eye_{}, galaxyMat_{}, black_{};
  Mesh seg_{}, spine_{}, fang_{}, sphere_{}, floorMesh_{};
  std::vector<Mesh> rocks_;
  Model skull_{}, tower_{};
  bool skullOK_ = false, towerOK_ = false;
  Texture2D sclera_{};

  std::vector<Static> statics_;
  std::vector<TowerLight> towerLights_;
  std::vector<Vector3> crackAnchors_;  // universes visible through skull cracks: light + gap in the darkness
  struct { bool ok = false; Matrix m; Vector3 eye; } dreamer_;
  float levT_ = 0;
  int lastPass_ = -1;
  std::vector<Seg> farLev_ = std::vector<Seg>(48), nearLev_ = std::vector<Seg>(48);

  Material sceneMat(Color c);
  void setUniforms(Color fog, float density, Vector3 amb, Vector3 sun, const std::vector<std::pair<Vector3, Vector4>>& lights);
  void drawLeviathan(const std::vector<Seg>& L, bool withEye);
  void drawStatic(const Static& s);
};
