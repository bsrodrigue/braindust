// characters/mage.json must reproduce the mage's animation rules. Run from the project root: ./build/character_test
#undef NDEBUG  // asserts must run in Release builds too
#include <cassert>
#include <cstdio>
#include <string>

#include "../src/character.hpp"

static std::string stateAfter(const Character& c, const AnimParams& p) {
  Animator a;
  a.update(c, p, 1 / 60.f);
  return c.states[a.state()].name;
}

int main() {
  SetTraceLogLevel(LOG_WARNING);
  Character c = parseCharacter("characters/mage.json");
  // priority: strike > flare > cast > (land: rise, fall, run) > drift in water > idle
  assert(stateAfter(c, {{"strike", .2f}, {"flare", 1}, {"castHold", .3f}}) == "strike");
  assert(stateAfter(c, {{"flare", 1}, {"castHold", .3f}}) == "flare");
  assert(stateAfter(c, {{"castHold", .3f}, {"inWater", 1}}) == "cast");
  assert(stateAfter(c, {{"vy", -100}}) == "rise");
  assert(stateAfter(c, {{"vy", 100}}) == "fall");
  assert(stateAfter(c, {{"ground", 1}, {"speed", 200}, {"runRatio", .9f}}) == "run");
  assert(stateAfter(c, {{"ground", 1}, {"speed", 20}}) == "idle");
  assert(stateAfter(c, {{"inWater", 1}, {"speed", 200}}) == "drift");

  // driven clips play once over the countdown: strike frames 0,1,4,5 across 0.36 s
  Animator a;
  for (auto [left, frame] : {std::pair{.36f, 0}, {.26f, 1}, {.17f, 4}, {.05f, 5}}) {
    a.update(c, {{"strike", left}}, 0);
    assert(a.frame(c) == frame);
  }
  a.update(c, {{"castHold", .35f}}, 0); assert(a.frame(c) == 2);  // raise the wand...
  a.update(c, {{"castHold", .1f}}, 0); assert(a.frame(c) == 3);   // ...then release
  std::puts("character_test: ok");
}
