// Player actions from keyboard + gamepad, edge-detected per frame.
#pragma once

enum Act { A_LEFT, A_RIGHT, A_UP, A_DOWN, A_FOCUS, A_STAFF, A_BOLT, A_FLARE, A_RESTART, A_COUNT };

class Input {
 public:
  void poll();
  bool held(Act a) const { return cur_[a]; }
  bool tap(Act a) const { return cur_[a] && !prev_[a]; }

 private:
  bool cur_[A_COUNT]{}, prev_[A_COUNT]{};
};
