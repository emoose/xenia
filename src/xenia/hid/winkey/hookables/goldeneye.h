/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_HID_WINKEY_GOLDENEYE_H_
#define XENIA_HID_WINKEY_GOLDENEYE_H_

#include "xenia/hid/winkey/hookables/hookable_game.h"

namespace xe {
namespace hid {
namespace winkey {

class GoldeneyeGame : public HookableGame {
 public:
  ~GoldeneyeGame() override;

  bool IsGameSupported();
  bool DoHooks(uint32_t user_index, RawInputState& input_state);

 private:
  uint32_t prev_aim_mode_ = 0;
  uint32_t prev_game_pause_flag_ = -1;
  uint32_t prev_game_control_active_ = -1;
  bool player_exiting_aim_mode_ = false;
};

}  // namespace winkey
}  // namespace hid
}  // namespace xe

#endif  // XENIA_HID_WINKEY_GOLDENEYE_H_
