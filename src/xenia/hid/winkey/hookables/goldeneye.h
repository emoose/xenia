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
#include "xenia/kernel/xclock.h"

namespace xe {
namespace hid {
namespace winkey {

class GoldeneyeGame : public HookableGame {
 public:
  enum class GameBuild {
    Unknown = 0,
    GoldenEye_Aug2007,
    PerfectDark_Devkit_33,   // 09.12.03.0033
    PerfectDark_Release_52,  // 10.02.16.0052
    PerfectDark_Devkit_102,  // 10.03.04.0102
    PerfectDark_Release_104, // 10.03.07.0104
    PerfectDark_Release_107, // 10.04.13.0107
  };

  ~GoldeneyeGame() override;

  bool IsGameSupported();
  bool DoHooks(uint32_t user_index, RawInputState& input_state,
               X_INPUT_STATE* out_state);
  bool ModifierKeyHandler(uint32_t user_index, RawInputState& input_state,
                          X_INPUT_STATE* out_state);

 private:
  GameBuild game_build_ = GameBuild::Unknown;

  uint32_t prev_aim_mode_ = 0;
  uint32_t prev_game_pause_flag_ = -1;
  uint32_t prev_game_control_disabled_ = -1;

  float centering_speed_ = 0.0125f;
  bool start_centering_ = false;
  bool disable_sway_ = false;  // temporarily prevents sway being applied
};

}  // namespace winkey
}  // namespace hid
}  // namespace xe

#endif  // XENIA_HID_WINKEY_GOLDENEYE_H_
