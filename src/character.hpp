// A character is data (characters/*.json): a sprite sheet and an animation state machine.
//
// The game sets named parameters every frame (strike, inWater, vy, ...). Each frame the transitions are
// tried in file order and the first whose conditions all hold picks the state ("from": "any" or a state
// name). A state shows frames of one sheet row, either looping on a clock or driven by a countdown param.
#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include "raylib.h"

struct SheetAnim { int row = 0, frames = 1; std::vector<Vector2> head; };  // one sheet row; head: top of the head per frame, from the feet

struct AnimState {
  std::string name, anim;   // anim: a row of the sheet
  std::vector<int> frames;  // frame indices into that row (default: all of them, in order)
  float fps = 0;            // looping clips: frames per second, plus fpsGain * param(fpsParam)
  std::string fpsParam;
  float fpsGain = 0;
  std::string drive;        // driven clips: a countdown param going from driveLength to 0 plays the frames once
  float driveLength = 1;
};

struct Condition {
  enum Op { GT, GE, LT, LE, EQ, NE } op = NE;
  std::string param;
  float value = 0;
};

struct Transition {
  int from = -1;  // -1: from any state
  int to = 0;
  std::vector<Condition> when;  // all must hold; empty = always
};

using AnimParams = std::unordered_map<std::string, float>;

struct Character {
  std::string path;
  Texture2D sheet{};
  float cellW = 0, cellH = 0, anchorX = 0, anchorY = 0;  // sheet px; the anchor is the feet
  float height = 96;                                     // world px from the feet to the top of the head (first state, first frame)
  Color tint = WHITE;
  std::unordered_map<std::string, SheetAnim> anims;
  std::vector<AnimState> states;  // the first one is the initial state
  std::vector<Transition> transitions;

  float scale() const;  // sheet px -> world px
};

// Reads the character and its sheet; throws with a readable message. Call after the window exists.
Character loadCharacter(const std::string& path);
Character parseCharacter(const std::string& path);  // the same without the texture (no window needed)
void unloadCharacter(Character& c);

// Runs a character's state machine for one actor.
class Animator {
 public:
  void reset() { state_ = 0; clock_ = stateTime_ = 0; frame_ = -1; }
  void update(const Character& c, const AnimParams& p, float dt);
  int state() const { return state_; }
  float stateTime() const { return stateTime_; }
  // The sheet anim and frame index to draw (the initial state's first frame before any update).
  const SheetAnim& anim(const Character& c) const;
  int frame(const Character& c) const;

 private:
  int state_ = 0, frame_ = -1;
  float clock_ = 0, stateTime_ = 0;  // clock_ never resets on a state change, so loops don't restart
};
