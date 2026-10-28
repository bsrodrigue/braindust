#include "input.hpp"

#include <algorithm>
#include <initializer_list>

#include "raylib.h"

void Input::poll() {
  std::copy(cur_, cur_ + A_COUNT, prev_);
  auto k = [](std::initializer_list<int> keys) { return std::any_of(keys.begin(), keys.end(), [](int key) { return IsKeyDown(key); }); };
  bool pad = IsGamepadAvailable(0);
  auto b = [&](int btn) { return pad && IsGamepadButtonDown(0, btn); };
  float ax = pad ? GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X) : 0, ay = pad ? GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y) : 0;
  cur_[A_LEFT] = k({KEY_LEFT, KEY_A}) || b(GAMEPAD_BUTTON_LEFT_FACE_LEFT) || ax < -.4f;
  cur_[A_RIGHT] = k({KEY_RIGHT, KEY_D}) || b(GAMEPAD_BUTTON_LEFT_FACE_RIGHT) || ax > .4f;
  cur_[A_UP] = k({KEY_SPACE, KEY_UP, KEY_W}) || b(GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
  cur_[A_DOWN] = k({KEY_DOWN, KEY_S}) || b(GAMEPAD_BUTTON_LEFT_FACE_DOWN) || ay > .6f;
  cur_[A_FOCUS] = k({KEY_F}) || b(GAMEPAD_BUTTON_RIGHT_TRIGGER_1);
  cur_[A_STAFF] = k({KEY_J, KEY_Z}) || b(GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
  cur_[A_BOLT] = k({KEY_K, KEY_X}) || b(GAMEPAD_BUTTON_RIGHT_FACE_UP);
  cur_[A_FLARE] = k({KEY_L, KEY_C}) || b(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
  cur_[A_RESTART] = k({KEY_R}) || b(GAMEPAD_BUTTON_MIDDLE_RIGHT);
}
