// Touch controls and the GLSL ES rewrite, without a screen. Run from the project root: ./build/platform_test
#undef NDEBUG  // asserts must run in Release builds too
#include <cassert>
#include <cstdio>
#include <string>

#include "../src/platform.hpp"
#include "raymath.h"
#include "../src/touch.hpp"

namespace {

const float VW = 960, VH = 540;  // a 16:9 screen in HUD units

Vector2 at(Act a, const TouchControls& t) {
  for (auto& b : TouchControls::buttons()) if (b.act == a) return t.buttonPos(b);
  assert(false);
  return {};
}

void stick() {  // a thumb landing low on the left becomes the stick; pushing it moves, pushing down dives
  TouchControls t;
  t.update({}, VW, VH);
  t.update({{1, {120, 420}}}, VW, VH);
  assert(!t.held(A_LEFT) && !t.held(A_RIGHT));  // where it landed is the centre
  t.update({{1, {120 - 40, 420}}}, VW, VH);
  assert(t.held(A_LEFT) && !t.held(A_RIGHT) && !t.held(A_DOWN));
  t.update({{1, {120 + 300, 420 + 10}}}, VW, VH);  // far past the rim: still just right
  assert(t.held(A_RIGHT) && !t.held(A_DOWN));
  t.update({{1, {120, 420 + 50}}}, VW, VH);
  assert(t.held(A_DOWN));
  t.update({}, VW, VH);  // lifted
  assert(!t.held(A_DOWN) && !t.held(A_RIGHT));
}

void buttons() {  // each button holds its action; two thumbs at once; sliding to another button moves the press
  TouchControls t;
  t.update({}, VW, VH);  // the layout follows the last screen size it was given
  for (auto& b : TouchControls::buttons()) {
    t.update({{7, t.buttonPos(b)}}, VW, VH);
    for (auto& o : TouchControls::buttons()) assert(t.held(o.act) == (o.act == b.act));
    t.update({}, VW, VH);
  }
  t.update({{1, {120, 420}}, {2, at(A_UP, t)}}, VW, VH);
  t.update({{1, {60, 420}}, {2, at(A_UP, t)}}, VW, VH);
  assert(t.held(A_LEFT) && t.held(A_UP));
  t.update({{2, at(A_STAFF, t)}}, VW, VH);
  assert(t.held(A_STAFF) && !t.held(A_UP));
}

void stickKeepsItsTouch() {  // the stick's thumb sliding over a button does not press it; a held button never becomes the stick
  TouchControls t;
  t.update({}, VW, VH);
  t.update({{1, {300, 420}}}, VW, VH);
  t.update({{1, at(A_PARRY, t)}}, VW, VH);
  assert(!t.held(A_PARRY) && t.held(A_RIGHT));
  t.update({}, VW, VH);
  t.update({{3, at(A_ITEM, t)}}, VW, VH);
  t.update({{3, {120, 420}}}, VW, VH);  // slid off the button into the stick's area: not a new touch, so no stick
  t.update({{3, {60, 420}}}, VW, VH);
  assert(!t.held(A_LEFT) && !t.held(A_RIGHT) && !t.held(A_ITEM));
}

void menuButton() {  // a tap opens the menu once, not every frame it is held
  TouchControls t;
  t.update({}, VW, VH);
  t.update({{5, t.menuPos()}}, VW, VH);
  assert(t.menuTapped());
  t.update({{5, t.menuPos()}}, VW, VH);
  assert(!t.menuTapped());
}

void layoutFits() {  // on 16:9 and 4:3, every control is on screen, clear of the others and of the stick's area
  for (float vw : {960.f, 720.f}) {
    TouchControls t;
    t.update({}, vw, VH);
    auto& bs = TouchControls::buttons();
    for (size_t i = 0; i < bs.size(); i++) {
      Vector2 p = t.buttonPos(bs[i]);
      assert(p.x - bs[i].r >= 0 && p.x + bs[i].r <= vw && p.y - bs[i].r >= 0 && p.y + bs[i].r <= VH);
      assert(p.x - bs[i].r * 1.2f > vw * .42f);
      for (size_t j = i + 1; j < bs.size(); j++) assert(Vector2Distance(p, t.buttonPos(bs[j])) > (bs[i].r + bs[j].r) * 1.2f);
    }
  }
}

void glsl100() {  // GLSL 330 in, GLSL ES 100 out
  std::string vs = platform::toGlsl100("#version 330\nin vec3 vertexPosition;\nout vec2 fragTexCoord;\nvoid main() {}\n", true);
  assert(vs.rfind("#version 100\n", 0) == 0 && vs.find("#version 330") == std::string::npos);
  assert(vs.find("attribute vec3 vertexPosition;") != std::string::npos && vs.find("varying vec2 fragTexCoord;") != std::string::npos);
  std::string fs = platform::toGlsl100("#version 330\n  in vec2 fragTexCoord;\nout vec4 finalColor;\nvoid main() { finalColor = texture(t, fragTexCoord); }\n", false);
  assert(fs.find("varying vec2 fragTexCoord;") != std::string::npos && fs.find("\nout vec4") == std::string::npos);
  assert(fs.find("#define texture texture2D") != std::string::npos && fs.find("#define finalColor gl_FragColor") != std::string::npos);
  assert(fs.find("precision highp float;") != std::string::npos);
}

}  // namespace

int main() {
  SetTraceLogLevel(LOG_WARNING);
  stick();
  buttons();
  stickKeepsItsTouch();
  menuButton();
  layoutFits();
  glsl100();
  std::puts("platform_test: ok");
}
