// Braindust: a 2D thalassophobia action game (raylib), for the desktop and Android.
// On the desktop, run from anywhere; it finds the project root (the folder holding levels/ and shaders/).
//   braindust [levels/name.json] [--spawn x,y] [--shot frames out.png] [--touch] [--scale 0.5]
// --touch shows the on-screen controls (the mouse is a finger). On Android they show unless a gamepad is connected,
// and the files are the APK's assets (see platform.hpp). --scale renders at that fraction of the screen (weak GPUs).
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <string>
#include <vector>

#include "character.hpp"
#include "content.hpp"
#include "input.hpp"
#include "json.hpp"
#include "level.hpp"
#include "menu.hpp"
#include "panel.hpp"
#include "platform.hpp"
#include "renderer.hpp"
#include "scene3d.hpp"
#include "synth.hpp"
#include "touch.hpp"
#include "tunables.hpp"
#include "util.hpp"
#include "world.hpp"

namespace {

const char* TUNABLES_PATH = "tunables.json";
const char* CHARACTER_PATH = "characters/mage.json";
const char* DATA_DIR = "data";

struct Options {
  std::string levelPath = "levels/abyss.json", shotPath;
  int shotFrames = 0;  // > 0: render this many frames in a hidden window, save the last one, quit
  bool spawnSet = false, touch = false;
  Vector2 spawn{};
};

Options parseArgs(int argc, char** argv) {
  Options o;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--shot" && i + 2 < argc) { o.shotFrames = std::atoi(argv[++i]); o.shotPath = argv[++i]; }
    else if (a == "--spawn" && i + 1 < argc) o.spawnSet = std::sscanf(argv[++i], "%f,%f", &o.spawn.x, &o.spawn.y) == 2;
    else if (a == "--touch") o.touch = true;
    else if (a == "--scale" && i + 1 < argc) platform::setRenderScale(float(std::atof(argv[++i])));
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
  std::vector<Panel> pages{tunePanel(w), recordsPanel("MOVES", "data/moves.json", c.moves),
                           recordsPanel("ENEMIES", "data/enemies.json", c.enemies), recordsPanel("ITEMS", "data/items.json", c.items)};
  for (auto& p : pages) p.showNext = true;
  return pages;
}

int fail(const std::string& what) {  // stderr on the desktop, the log (logcat) on Android
  std::fprintf(stderr, "braindust: %s\n", what.c_str());
  TraceLog(LOG_ERROR, "braindust: %s", what.c_str());
  if (IsWindowReady()) CloseWindow();
  return 1;
}

}  // namespace

int main(int argc, char** argv) {
  Options opt = parseArgs(argc, argv);
  if (!platform::android() && !findRoot()) return fail("run from the project folder (needs levels/ and shaders/)");

  // the window first: on Android the game's files can only be read once raylib has started
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT | (opt.shotFrames ? FLAG_WINDOW_HIDDEN : 0));
  InitWindow(1280, 720, "Braindust");
  SetWindowMinSize(480, 320);
  SetTargetFPS(60);
  SetExitKey(KEY_NULL);

  Synth synth;
  World world(synth);
  try { world.level = loadLevelWithSpawn(opt); } catch (const std::exception& e) { return fail(e.what()); }
  world.base = loadTunables(TUNABLES_PATH);
  Content content;
  try { content = loadContent(DATA_DIR); } catch (const std::exception& e) { return fail(e.what()); }
  world.content = &content;
  Character character;
  try { character = loadCharacter(CHARACTER_PATH); } catch (const std::exception& e) { return fail(e.what()); }
  world.character = &character;
  if (!opt.shotFrames) synth.start();

  Renderer renderer;
  Scene3D scene3d;
  renderer.load();
  scene3d.load();
  renderer.resize(world);
  world.reset();
  scene3d.place(world);

  Input input;
  TouchControls touch;
  Menu menu;
  std::vector<Panel> pages = tunePages(world, content);
  int page = -1;  // the open tune page; -1: none
  bool help = false, quit = false, touchMode = platform::android() || opt.touch;

  auto openPage = [&](int p) {
    if (page >= 0) pages[page].open = false;
    page = p >= 0 && p < int(pages.size()) ? p : -1;
    if (page >= 0) pages[page].toggle();
  };
  auto reloadLevel = [&] {
    try { world.level = loadLevelWithSpawn(opt); } catch (const std::exception& e) { world.say("LOAD FAILED", e.what()); return; }
    world.reset();
    scene3d.place(world);
  };
  auto menuItems = [&] {  // what the keyboard's F-keys do, for touch screens and gamepads
    std::vector<MenuItem> items{{"Resume", nullptr}, {"Restart", [&] { world.reset(); }}};
    for (const std::string& f : platform::listFiles("levels", ".json")) {
      std::string path = "levels/" + f, name = f;
      try { name = Json::parse(readFile(path)).strOr("name", f); } catch (const std::exception&) {}
      items.push_back({"Play: " + name, [&, path] { opt.levelPath = path; opt.spawnSet = false; reloadLevel(); }});
    }
    items.push_back({"Tune pages", [&] { openPage(0); }});
    items.push_back({"Help", [&] { help = true; }});
    items.push_back({synth.isMuted() ? "Sound: off" : "Sound: on", [&] { synth.toggleMute(); }});
    items.push_back({"Quit", [&] { quit = true; }});
    return items;
  };

  for (int frame = 0; !WindowShouldClose() && !quit; frame++) {
    renderer.resize(world);
    Ui& ui = renderer.ui;
    float dt = opt.shotFrames ? 1 / 60.f : std::min(.033f, GetFrameTime());
    bool showTouch = touchMode && !IsGamepadAvailable(0), paused = page >= 0 || menu.open;
    touch.update(showTouch && !paused ? TouchControls::read(ui) : std::vector<Touch>{}, ui.VW, ui.VH);
    input.poll(showTouch && !paused ? &touch : nullptr);
    if (IsKeyPressed(KEY_M)) synth.toggleMute();
    if (IsKeyPressed(KEY_F1)) help = !help;
    else if (help && touchMode && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) help = false;  // a tap closes it
    bool menuKey = IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACK) || touch.menuTapped() ||
                   (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_LEFT));
    if (IsKeyPressed(KEY_F2) || IsKeyPressed(KEY_TAB)) { menu.open = false; openPage(page + 1); }  // the next page, then back
    if (page >= 0) {
      if (IsKeyPressed(KEY_BACK)) pages[page].open = false;
      pages[page].input(ui);  // Esc closes it
      if (pages[page].nextRequested) { pages[page].nextRequested = false; openPage(page + 1); }
      else if (!pages[page].open) page = -1;
    } else if (menuKey) {
      if (menu.open) menu.open = false;
      else menu.show(menuItems());
    } else if (menu.open) {
      menu.input(ui);
    }
    if (IsKeyPressed(KEY_F5)) {  // reload tunables, data, the character and the level from disk
      world.base = loadTunables(TUNABLES_PATH);
      std::string failed;
      try {
        Content c = loadContent(DATA_DIR);
        content = std::move(c);
        int open = page;
        pages = tunePages(world, content);
        page = -1;
        openPage(open);
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
    if (page < 0 && !menu.open) {  // menus and tune pages pause the game
      world.update(dt, input);
      scene3d.update(dt, world);
    }
    renderer.draw(world, scene3d, [&] {
      if (showTouch && page < 0 && !menu.open) touch.draw(ui);
      if (page >= 0) pages[page].draw(ui);
      if (menu.open) menu.draw(ui);
      if (help) renderer.drawHelp(touchMode);
    });
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
