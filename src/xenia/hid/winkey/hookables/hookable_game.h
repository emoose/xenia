/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_HID_WINKEY_HOOKABLE_GAME_H_
#define XENIA_HID_WINKEY_HOOKABLE_GAME_H_

#include "xenia/xbox.h"
#include "xenia/hid/input.h"

namespace xe {
namespace hid {
namespace winkey {

struct MouseEvent {
  int32_t x_delta = 0;
  int32_t y_delta = 0;
  int32_t buttons = 0;
  int32_t wheel_delta = 0;
};

struct RawInputState {
  MouseEvent mouse;
  bool* key_states;
};

// For per-game bindings
// (if adding new game, make sure to update WinKeyInputDriver::WinKeyInputDriver!
enum HookableGameIDs : uint32_t {
  Unsupported = 0,
  GoldenEye = 0x584108A9,
  Halo3 = 0x4D5307E6,
  Halo3ODST = 0x4D530877,
  HaloReach = 0x4D53085B,
  Halo4 = 0x4D530919
};

class HookableGame {
 public:
  virtual ~HookableGame() = default;

  virtual bool IsGameSupported() = 0;
  virtual bool DoHooks(uint32_t user_index, RawInputState& input_state) = 0;
};

}  // namespace winkey
}  // namespace hid
}  // namespace xe

#endif  // XENIA_HID_WINKEY_HOOKABLE_GAME_H_
