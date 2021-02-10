/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_HID_WINKEY_HALO3_H_
#define XENIA_HID_WINKEY_HALO3_H_

#include "xenia/hid/winkey/hookables/hookable_game.h"

namespace xe {
namespace hid {
namespace winkey {

class Halo3Game : public HookableGame {
 public:
  enum class GameBuild {
    Unknown,
    Release_08172,  // March 8th 2007 (08172.07.03.08.2240.delta)
    Release_11855   // August 20th 2007 (07.08.20.2317.halo3_ship) - TU0
  };

  ~Halo3Game() override;

  bool IsGameSupported();
  X_RESULT GetState(uint32_t user_index, RawInputState& input_state,
                    X_INPUT_STATE* out_state);

 private:
  GameBuild game_build_ = GameBuild::Unknown;
};

}  // namespace winkey
}  // namespace hid
}  // namespace xe

#endif  // XENIA_HID_WINKEY_HALO3_H_
