// Bodies must stop at the nearest face where solids overlap (levels do this a lot), never get pushed through.
// Run from the project root: ./build/collision_test
#undef NDEBUG  // asserts must run in Release builds too
#include <cassert>
#include <cmath>
#include <cstdio>

#include "../src/world.hpp"

// one frame with no input, from a free position
static void stepFrom(World& w, float x, float y, float vx, float vy) {
  w.reset();
  w.player.x = x; w.player.y = y; w.player.vx = vx; w.player.vy = vy;
  w.update(1 / 60.f, Input{});
}

int main() {
  SetTraceLogLevel(LOG_WARNING);
  Synth audio;  // never started: sounds are queued and dropped
  World w(audio);
  w.level.width = 2000;

  // sinking onto two blocks whose tops differ by 3 px (levels/corpses.json, solids 125 and 126): land on the higher one
  w.level.solids = {{1595, 3288, 140, 70}, {1488, 3285, 140, 70}};
  stepFrom(w, 1600, 3285 - 70 - 1, 0, 400);
  assert(w.player.ground && w.player.y + w.player.h == 3285);

  // swimming right into a wall of two blocks whose faces are 2 px apart: stop at the nearer face
  w.level.solids = {{502, 3000, 100, 200}, {500, 3000, 100, 200}};
  stepFrom(w, 500 - 28 - 1, 3050, 200, 0);
  assert(w.player.x + w.player.w == 500);

  // tuning a value keeps this run's wrapper bonuses on top, multiplicative ones included
  w.level.solids = {};
  w.level.wrappers = {{500, 300, "focus"}};
  w.reset();
  w.level.wrappers[0].taken = true;
  w.base.focusTime = .3f;
  w.retune();
  assert(std::fabs(w.tun.focusTime - .3f * .6f) < 1e-6f);
  std::puts("collision_test: ok");
}
