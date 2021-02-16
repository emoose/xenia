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
    Debug_08172,    // Mar  8 2007 (08172.07.03.08.2240.delta__cache_debug)
    Play_08172,     // Mar  8 2007 (08172.07.03.08.2240.delta__cache_play)
    Profile_08172,  // Mar  8 2007 (08172.07.03.08.2240.delta__cache_profile)
    Release_08172,  // Mar  8 2007 (08172.07.03.08.2240.delta)
    Test_08172,     // Mar  8 2007 (08172.07.03.08.2240.delta__cache_test)
    Release_699E0227_11855,  // Aug 20 2007, TU0, media ID 699E0227, v0.0.0.42
                             // (11855.07.08.20.2317.halo3_ship__cache_release)
    Release_699E0227_12070,  // Sep  5 2008, TU3, media ID 699E0227, v0.0.3.42
                             // (12070.08.09.05.2031.halo3_ship__cache_release)

    // Halo 3: ODST
    Release_152AB680_13895,  // Apr 27 2009, TU0, media ID 152AB680, v0.0.0.11
                             // (13895.09.04.27.2201.atlas_relea__cache_release)

    // Halo: Reach
    Release_566C10D3_11860,  // Jul 24 2010, TU0, media ID 566C10D3, v0.0.0.1
                             // (11860.10.07.24.0147.omaha_relea)
    Release_566C10D3_12065,  // Aug 24 2011, TU1, media ID 566C10D3, v0.0.1.1
                             // (12065.11.08.24.1738.tu1actual)

    // Halo 4
    Release_1C9D20BC_20810,  // Sep 22 2012, TU0, media ID 1C9D20BC, v0.0.0.15
                             // (20810.12.09.22.1647.main)
    Release_1C9D20BC_21522,  // Oct 17 2013, TU8/TU10? media ID 1C9D20BC
                             // v0.0.10.15 (21522.13.10.17.1936.main)
  };

  ~Halo3Game() override;

  bool IsGameSupported();
  bool DoHooks(uint32_t user_index, RawInputState& input_state,
               X_INPUT_STATE* out_state);

 private:
  GameBuild game_build_ = GameBuild::Unknown;
};

}  // namespace winkey
}  // namespace hid
}  // namespace xe

#endif  // XENIA_HID_WINKEY_HALO3_H_
