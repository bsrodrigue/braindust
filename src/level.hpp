// Level data (levels/*.json, the same files editor.html edits). Pure data: no raylib resources.
#pragma once
#include <string>
#include <vector>

#include "raylib.h"

struct Box { float x, y, w, h; std::string tile = ""; };  // tile: ground | float | bridge | stone | box (default picked from shape)
inline bool overlaps(const Box& a, const Box& b) { return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y; }

struct Decor { std::string model; float x, y, z, scale, rx, ry, rz; };  // model: skull | giant | tower
struct Wrapper { float x, y; std::string kind; bool taken = false; };    // kind: mana | focus | protection | dust
struct DustZone { Box r; float rate; };
struct Inscription { float x, y; std::string text; bool read = false; };
struct LevelLight { float x, y, r; Color c; };
struct Prop { std::string type; float x, y; std::string name = ""; float scale = 1; bool flip = false; };  // hut, villager, fire, post, boat, or sprite (a tileset image)

struct Level {
  std::string name;
  float width = 1000, height = 5200, top = -140;  // top < 0: there is land and sky above the sea (y = 0)
  Vector2 spawn{500, 60}, portal{500, 5095}, empty{765, 5160};
  std::vector<Box> solids;
  std::vector<Vector2> lurkers, thoughts;
  std::vector<Decor> decor;
  std::vector<Wrapper> wrappers;
  std::vector<DustZone> zones;
  std::vector<Inscription> texts;
  std::vector<LevelLight> lights;
  std::vector<Prop> props;
  Color surface{42, 106, 128, 255}, deep{14, 42, 54, 255};  // fog colour at the surface and in the deep
};

Level loadLevel(const std::string& path);  // throws on a missing or malformed file
