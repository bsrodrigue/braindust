#include "input.hpp"

#include <algorithm>

#include "raylib.h"
#include "touch.hpp"

Act actByName(const std::string& name) {
  static const char* names[A_COUNT] = {"left", "right", "up", "down", "focus", "staff", "bolt", "flare", "dodge", "parry", "item", "restart"};
  for (int a = 0; a < A_COUNT; a++) if (name == names[a]) return Act(a);
  return A_COUNT;
}

void Input::poll(const TouchControls* touch) {
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
  cur_[A_DODGE] = k({KEY_LEFT_SHIFT, KEY_RIGHT_SHIFT, KEY_SEMICOLON}) || b(GAMEPAD_BUTTON_RIGHT_TRIGGER_2);
  cur_[A_PARRY] = k({KEY_U, KEY_V}) || b(GAMEPAD_BUTTON_LEFT_TRIGGER_1);
  cur_[A_ITEM] = k({KEY_Q, KEY_E}) || b(GAMEPAD_BUTTON_LEFT_FACE_UP);
  cur_[A_RESTART] = k({KEY_R}) || b(GAMEPAD_BUTTON_MIDDLE_RIGHT);
  if (touch) for (int a = 0; a < A_COUNT; a++) cur_[a] = cur_[a] || touch->held(Act(a));
}

void Input::feed(std::initializer_list<Act> down) {
  std::copy(cur_, cur_ + A_COUNT, prev_);
  std::fill(cur_, cur_ + A_COUNT, false);
  for (Act a : down) cur_[a] = true;
}
