/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2014 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/hid/winkey/hookables/halo3.h"

#include "xenia/base/platform_win.h"
#include "xenia/hid/hid_flags.h"
#include "xenia/hid/input_system.h"
#include "xenia/xbox.h"
#include "xenia/kernel/util/shim_utils.h"
#include "xenia/cpu/processor.h"
#include "xenia/kernel/xthread.h"

using namespace xe::kernel;

DECLARE_double(sensitivity);
DECLARE_bool(invert_y);

namespace xe {
namespace hid {
namespace winkey {

Halo3Game::~Halo3Game() = default;

struct GameBuildAddrs {
  const char* build_string;
  uint32_t build_string_addr;
  uint32_t input_globals_offset; // can be found near usage of "player control globals" string
  uint32_t camera_x_offset;
  uint32_t camera_y_offset;
};

std::map<Halo3Game::GameBuild, GameBuildAddrs> supported_builds{
  {
    Halo3Game::GameBuild::Debug_08172,
    {"08172.07.03.08.2240.delta__cache_debug", 0x820BA40C, 0x1A30, 0x1C, 0x20}
  },
  {
    Halo3Game::GameBuild::Play_08172,
    {"08172.07.03.08.2240.delta__cache_play", 0x820A1108, 0x1928, 0x1C, 0x20}
  },
  {
    Halo3Game::GameBuild::Profile_08172,
    {"08172.07.03.08.2240.delta__cache_profile", 0x8201979C, 0x12C, 0x1C, 0x20}
  },
  {
    Halo3Game::GameBuild::Release_08172,
    {"08172.07.03.08.2240.delta", 0x8205D39C, 0xC4, 0x1C, 0x20}
  },
  {
    Halo3Game::GameBuild::Test_08172,
    {"08172.07.03.08.2240.delta__cache_test", 0x820A8744, 0x1928, 0x1C, 0x20}
  },
  {
    Halo3Game::GameBuild::Release_699E0227_11855,
    {"11855.07.08.20.2317.halo3_ship__cache_release", 0x8203ADE8, 0x78, 0x1C,
      0x20}
  },
  {
    Halo3Game::GameBuild::Release_699E0227_12070,
    {"12070.08.09.05.2031.halo3_ship__cache_release", 0x8203B3E4, 0x78, 0x1C,
      0x20}
  },
  {
    Halo3Game::GameBuild::Release_152AB680_13895,
    {"13895.09.04.27.2201.atlas_relea__cache_release", 0x82048E38, 0xA8, 0x8C,
      0x90}
  },
  {
    Halo3Game::GameBuild::Release_566C10D3_11860,
    {"11860.10.07.24.0147.omaha_relea", 0x82048A54, 0x74, 0x94, 0x98}
  },
  {
    Halo3Game::GameBuild::Release_566C10D3_12065,
    {"12065.11.08.24.1738.tu1actual", 0x82048BCC, 0x74, 0x94, 0x98}
  }
};

bool Halo3Game::IsGameSupported() {
  auto title_id = kernel_state()->title_id();

  if (title_id != HookableGameIDs::Halo3 &&
      title_id != HookableGameIDs::Halo3ODST &&
      title_id != HookableGameIDs::HaloReach &&
      title_id != HookableGameIDs::Halo4) {
    return false;
  }

  bool supported = false;

  for (auto& build : supported_builds) {
    auto* build_ptr = kernel_memory()->TranslateVirtual<const char*>(
        build.second.build_string_addr);

    if (strcmp(build_ptr, build.second.build_string) == 0) {
      game_build_ = build.first;
      return true;
    }
  }

  return false;
}

#define IS_KEY_DOWN(x) (input_state.key_states[x])

bool Halo3Game::DoHooks(uint32_t user_index, RawInputState& input_state) {
  if (!IsGameSupported()) {
    return false;
  }

  if (supported_builds.count(game_build_)) {
    // Doesn't seem to be any way to get tls_static_address_ besides this
    // (XThread::GetTLSValue only returns tls_dynamic_address_, and doesn't
    // seem to be any functions to get static_addr...)

    uint32_t pcr_addr =
        static_cast<uint32_t>(xe::kernel::XThread::GetCurrentThread()
                                  ->thread_state()
                                  ->context()
                                  ->r[13]);

    auto tls_addr =
        kernel_memory()->TranslateVirtual<X_KPCR*>(pcr_addr)->tls_ptr;

    auto global_addr = *kernel_memory()->TranslateVirtual<xe::be<uint32_t>*>(
        tls_addr + supported_builds[game_build_].input_globals_offset);

    if (global_addr) {
      auto* input_globals = kernel_memory()->TranslateVirtual(global_addr);

      auto* player_cam_x = reinterpret_cast<xe::be<float>*>(
          input_globals + supported_builds[game_build_].camera_x_offset);
      auto* player_cam_y = reinterpret_cast<xe::be<float>*>(
          input_globals + supported_builds[game_build_].camera_y_offset);

      // Have to do weird things converting it to normal float otherwise
      // xe::be += treats things as int?
      float camX = (float)*player_cam_x;
      float camY = (float)*player_cam_y;

      camX -= (((float)input_state.mouse.x_delta) / 1000.f) *
              (float)cvars::sensitivity;

      if (!cvars::invert_y) {
        camY -= (((float)input_state.mouse.y_delta) / 1000.f) *
                (float)cvars::sensitivity;
      } else {
        camY += (((float)input_state.mouse.y_delta) / 1000.f) *
                (float)cvars::sensitivity;
      }

      *player_cam_x = camX;
      *player_cam_y = camY;
    }
  }

  return true;
}


}  // namespace winkey
}  // namespace hid
}  // namespace xe
