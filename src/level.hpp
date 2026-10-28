// Level data (levels/*.json, the same files editor.html edits). Pure data: no raylib resources.
#pragma once
#include <string>
#include <vector>

#include "raylib.h"

// tile: ground | float | bridge | stone | box (default picked from shape).
// seal > 0: a stardust seal. Only a star bolt cast with at least that much stardust (%) opens it.
struct Box { float x, y, w, h; std::string tile = ""; float seal = 0; bool open = false, tried = false; };
inline bool overlaps(const Box& a, const Box& b) { return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y; }

struct Decor { std::string model; float x, y, z, scale, rx, ry, rz; };  // model: skull | giant | tower
// reveal >= 0 ("hidden" in the file): seen and taken only inside your light, and only with at least that much
// stardust (%): a mind that far gone sees it. "hidden": true is 0 (light only); absent is -1 (always there).
struct Wrapper { float x, y; std::string kind; float reveal = -1; bool taken = false; };  // kind: mana | focus | protection | dust
struct Pickup { float x, y; std::string kind; float reveal = -1; bool taken = false; };   // kind: an entry of data/items.json
struct Spawn { std::string type; float x, y; };                                          // type: an entry of data/enemies.json
struct DustZone { Box r; float rate; };
struct Inscription { float x, y; std::string text; bool read = false; };
struct LevelLight { float x, y, r; Color c; };
struct Prop { std::string type; float x, y; std::string name = ""; float scale = 1; bool flip = false; };  // hut, villager, fire, post, boat, or sprite (a tileset image)

struct Level {
  std::string name;
  float width = 1000, height = 5200, top = -140;  // top < 0: there is land and sky above the sea (y = 0)
  Vector2 spawn{500, 60}, portal{500, 5095}, empty{765, 5160};
  std::vector<Box> solids;
  std::vector<Spawn> enemies;  // "enemies", plus the older "lurkers" and "thoughts" lists
  std::vector<Decor> decor;
  std::vector<Wrapper> wrappers;
  std::vector<Pickup> items;
  std::vector<DustZone> zones;
  std::vector<Inscription> texts;
  std::vector<LevelLight> lights;
  std::vector<Prop> props;
  Color surface{42, 106, 128, 255}, deep{14, 42, 54, 255};  // fog colour at the surface and in the deep
};

Level loadLevel(const std::string& path);  // throws on a missing or malformed file
