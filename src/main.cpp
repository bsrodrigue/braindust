// Braindust: a 2D thalassophobia action game (raylib).
// Run from anywhere; it finds the project root (the folder holding levels/ and shaders/).
//   braindust [levels/name.json] [--spawn x,y] [--shot frames out.png]
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <string>
#include <vector>

#include "character.hpp"
#include "content.hpp"
#include "input.hpp"
#include "level.hpp"
#include "panel.hpp"
#include "renderer.hpp"
#include "scene3d.hpp"
#include "synth.hpp"
#include "tunables.hpp"
#include "world.hpp"

namespace {

const char* TUNABLES_PATH = "tunables.json";
const char* CHARACTER_PATH = "characters/mage.json";
const char* DATA_DIR = "data";

struct Options {
  std::string levelPath = "levels/abyss.json", shotPath;
  int shotFrames = 0;  // > 0: render this many frames in a hidden window, save the last one, quit
  bool spawnSet = false;
  Vector2 spawn{};
};

Options parseArgs(int argc, char** argv) {
  Options o;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--shot" && i + 2 < argc) { o.shotFrames = std::atoi(argv[++i]); o.shotPath = argv[++i]; }
    else if (a == "--spawn" && i + 1 < argc) o.spawnSet = std::sscanf(argv[++i], "%f,%f", &o.spawn.x, &o.spawn.y) == 2;
    else o.levelPath = a;
  }
  // findRoot() moves to the project root: keep paths given from here working (a level path may also be relative to the root)
  std::string here = std::string(GetWorkingDirectory()) + "/";
  if (!o.shotPath.empty() && o.shotPath[0] != '/') o.shotPath = here + o.shotPath;
  if (o.levelPath[0] != '/' && FileExists((here + o.levelPath).c_str())) o.levelPath = here + o.levelPath;
  return o;
}

bool findRoot() {  // the folder that holds levels/ and shaders/
  std::string app = GetApplicationDirectory();
  for (const std::string& d : {std::string(GetWorkingDirectory()), app, app + "/..", app + "/../..", app + "/../../.."})
    if (DirectoryExists((d + "/levels").c_str()) && FileExists((d + "/shaders/post.fs").c_str())) { ChangeDirectory(d.c_str()); return true; }
  return false;
}

Level loadLevelWithSpawn(const Options& o) {
  Level L = loadLevel(o.levelPath);
  if (o.spawnSet) L.spawn = o.spawn;
  return L;
}

// F2: every tunable, live. Edits the file values and keeps this run's wrapper bonuses on top.
Panel tunePanel(World& w) {
  Panel p;
  p.title = "TUNE";
  p.page = "body, movement, light";
  for (const TunableField& f : TUNABLE_FIELDS)
    p.params.push_back({f.name, f.group, f.step, Tunables{}.*f.field, [&w, &f] { return w.base.*f.field; }, [&w, &f](float v) {
                          w.base.*f.field = std::max(0.f, v);
                          w.retune();
                        }});
  p.save = [&w] { return std::string(saveTunables(TUNABLES_PATH, w.base) ? "saved tunables.json" : "save failed"); };
  return p;
}

// F2 again: every number of every entry of a data file, grouped by entry. Edits apply at once (an enemy's health
// on its next spawn); defaults are the values the file had when loaded.
Panel recordsPanel(const char* title, const char* page, RecordSet& set) {
  Panel p;
  p.title = title;
  p.page = page;
  for (auto& r : set.list)
    for (auto& [name, v] : r.nums) {
      float* value = &v, a = std::fabs(v);
      float step = a >= 100 ? 10 : a >= 10 ? 1 : a >= 1 ? .1f : .05f;
      p.params.push_back({name, r.name, step, v, [value] { return *value; }, [value](float x) { *value = x; }});
    }
  p.save = [&set] { return (saveRecords(set) ? "saved " : "save failed: ") + set.path; };
  return p;
}

std::vector<Panel> tunePages(World& w, Content& c) {
  return {tunePanel(w), recordsPanel("MOVES", "data/moves.json", c.moves), recordsPanel("ENEMIES", "data/enemies.json", c.enemies),
          recordsPanel("ITEMS", "data/items.json", c.items)};
}

}  // namespace

int main(int argc, char** argv) {
  Options opt = parseArgs(argc, argv);
  if (!findRoot()) { std::fprintf(stderr, "braindust: run from the project folder (needs levels/ and shaders/)\n"); return 1; }

  Synth synth;
  World world(synth);
  try { world.level = loadLevelWithSpawn(opt); } catch (const std::exception& e) { std::fprintf(stderr, "braindust: %s\n", e.what()); return 1; }
  world.base = loadTunables(TUNABLES_PATH);
  Content content;
  try { content = loadContent(DATA_DIR); } catch (const std::exception& e) { std::fprintf(stderr, "braindust: %s\n", e.what()); return 1; }
  world.content = &content;

  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT | (opt.shotFrames ? FLAG_WINDOW_HIDDEN : 0));
  InitWindow(1280, 720, "Braindust");
  SetWindowMinSize(480, 320);
  SetTargetFPS(60);
  SetExitKey(KEY_NULL);
  if (!opt.shotFrames) synth.start();

  Character character;
  try { character = loadCharacter(CHARACTER_PATH); } catch (const std::exception& e) {
    std::fprintf(stderr, "braindust: %s\n", e.what());
    CloseWindow();
    return 1;
  }
  world.character = &character;

  Renderer renderer;
  Scene3D scene3d;
  renderer.load();
  scene3d.load();
  renderer.resize(world);
  world.reset();
  scene3d.place(world);

  Input input;
  std::vector<Panel> pages = tunePages(world, content);
  int page = -1;  // the open tune page; -1: playing
  bool help = false;

  for (int frame = 0; !WindowShouldClose(); frame++) {
    renderer.resize(world);
    float dt = opt.shotFrames ? 1 / 60.f : std::min(.033f, GetFrameTime());
    input.poll();
    if (IsKeyPressed(KEY_M)) synth.toggleMute();
    if (IsKeyPressed(KEY_F1)) help = !help;
    if (IsKeyPressed(KEY_F2) || IsKeyPressed(KEY_TAB)) {  // the next page, then back to the game
      if (page >= 0) pages[page].open = false;
      page = page + 1 < int(pages.size()) ? page + 1 : -1;
      if (page >= 0) pages[page].toggle();
    }
    if (page >= 0) {
      pages[page].input(renderer.ui);
      if (!pages[page].open) page = -1;
    }
    if (IsKeyPressed(KEY_F5)) {  // reload tunables, data, the character and the level from disk
      world.base = loadTunables(TUNABLES_PATH);
      std::string failed;
      try {
        Content c = loadContent(DATA_DIR);
        content = std::move(c);
        pages = tunePages(world, content);
        if (page >= 0) pages[page].toggle();
      } catch (const std::exception& e) { failed = e.what(); }
      try {
        Character c = loadCharacter(CHARACTER_PATH);
        unloadCharacter(character);
        character = c;
      } catch (const std::exception& e) { failed = e.what(); }
      try { world.level = loadLevelWithSpawn(opt); } catch (const std::exception& e) { failed = e.what(); }
      world.reset();
      scene3d.place(world);
      if (!failed.empty()) world.say("RELOAD FAILED", failed);
    }
    if (opt.shotFrames) world.banner.t = 0;
    if (page < 0) {  // the tune pages pause the game
      world.update(dt, input);
      scene3d.update(dt, world);
    }
    renderer.draw(world, scene3d, page >= 0 ? &pages[page] : nullptr, help);
    if (opt.shotFrames && frame + 1 >= opt.shotFrames) {
      renderer.screenshot(opt.shotPath);
      break;
    }
  }
  synth.stop();
  unloadCharacter(character);
  CloseWindow();
  return 0;
}
