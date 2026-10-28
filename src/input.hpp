// Player actions from keyboard + gamepad, edge-detected per frame.
#pragma once
#include <initializer_list>
#include <string>

enum Act { A_LEFT, A_RIGHT, A_UP, A_DOWN, A_FOCUS, A_STAFF, A_BOLT, A_FLARE, A_DODGE, A_PARRY, A_ITEM, A_RESTART, A_COUNT };

// the names data/*.json uses for "input" ("staff", "bolt", "up", ...); A_COUNT when unknown
Act actByName(const std::string& name);

class Input {
 public:
  void poll();
  void feed(std::initializer_list<Act> down);  // one frame with exactly these held (tests, replays)
  bool held(Act a) const { return cur_[a]; }
  bool tap(Act a) const { return cur_[a] && !prev_[a]; }
  bool released(Act a) const { return !cur_[a] && prev_[a]; }

 private:
  bool cur_[A_COUNT]{}, prev_[A_COUNT]{};
};
