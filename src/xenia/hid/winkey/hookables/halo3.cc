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

DECLARE_bool(swap_buttons);
DECLARE_double(sensitivity);
DECLARE_bool(invert_y);

namespace xe {
namespace hid {
namespace winkey {

Halo3Game::~Halo3Game() = default;

struct GameBuildAddrs {
  const char* build_string;
  uint32_t build_string_addr;
  uint32_t input_globals_offset;
  uint32_t camera_x_offset;
  uint32_t camera_y_offset;
};

std::map<Halo3Game::GameBuild, GameBuildAddrs> supported_builds{
  {
    Halo3Game::GameBuild::Release_08172,
    {"08172.07.03.08.2240.delta", 0x8205D39C, 0xC4, 0x1C, 0x20}
  },
  {
    Halo3Game::GameBuild::Release_11855,
    {"11855.07.08.20.2317.halo3_ship", 0x8203ADC8, 0x78, 0x1C, 0x20}
  }
};

bool Halo3Game::IsGameSupported() {
  if (kernel_state()->title_id() != 0x4D5307E6) {
    return false;
  }

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

X_RESULT Halo3Game::GetState(uint32_t user_index,
                                 RawInputState& input_state,
                                 X_INPUT_STATE* out_state) {
  if (!IsGameSupported()) {
    return 1;
  }

  if (supported_builds.count(game_build_)) {
    // Doesn't seem to be any way to get tls_static_address_ besides this
    // (XThread::GetTLSValue only returns tls_dynamic_address_, and doesn't seem
    // to be any functions to get static_addr...)
    uint32_t pcr_addr =
        static_cast<uint32_t>(xe::kernel::XThread::GetCurrentThread()
                                  ->thread_state()
                                  ->context()
                                  ->r[13]);

    auto tls_addr =
        kernel_memory()->TranslateVirtual<X_KPCR*>(pcr_addr)->tls_ptr;

    uint32_t global_addr =
        *kernel_memory()->TranslateVirtual<xe::be<uint32_t>*>(
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

  uint16_t buttons = 0;
  uint8_t left_trigger = 0;
  uint8_t right_trigger = 0;
  int16_t thumb_lx = 0;
  int16_t thumb_ly = 0;
  int16_t thumb_rx = 0;
  int16_t thumb_ry = 0;

  if ((!cvars::swap_buttons && input_state.mouse_left_click) ||
      (cvars::swap_buttons && input_state.mouse_right_click)) {
    right_trigger = 0xFF;
  }
  if ((!cvars::swap_buttons && input_state.mouse_right_click) ||
      (cvars::swap_buttons && input_state.mouse_left_click)) {
    buttons |= 0x0080;  // XINPUT_GAMEPAD_RIGHT_THUMB
  }

  if (input_state.mouse.wheel_delta != 0) {
    if (input_state.mouse.wheel_delta > 0) {
      buttons |= 0x8000;  // XINPUT_GAMEPAD_Y
    } else {
      buttons |= 0x8000;  // XINPUT_GAMEPAD_Y
    }
  }

  if (IS_KEY_DOWN(VK_SHIFT)) {
    // Right stick toggled
    if (IS_KEY_DOWN('W')) {
      // Up
      thumb_ry += SHRT_MAX;
    }
    if (IS_KEY_DOWN('S')) {
      // Down
      thumb_ry += SHRT_MIN;
    }
    if (IS_KEY_DOWN('D')) {
      // Right
      thumb_rx += SHRT_MAX;
    }
    if (IS_KEY_DOWN('A')) {
      // Left
      thumb_rx += SHRT_MIN;
    }
  } else {
    // left stick
    if (IS_KEY_DOWN('A')) {
      // A
      thumb_lx += SHRT_MIN;
    }
    if (IS_KEY_DOWN('D')) {
      // D
      thumb_lx += SHRT_MAX;
    }
    if (IS_KEY_DOWN('S')) {
      // S
      thumb_ly += SHRT_MIN;
    }
    if (IS_KEY_DOWN('W')) {
      // W
      thumb_ly += SHRT_MAX;
    }
  }

  if (IS_KEY_DOWN(VK_CONTROL) || IS_KEY_DOWN('C')) {
    // CTRL/C
    buttons |= 0x0040;  // XINPUT_GAMEPAD_LEFT_THUMB
  }

  // DPad
  if (IS_KEY_DOWN(VK_UP)) {
    // Up
    buttons |= 0x0001;  // XINPUT_GAMEPAD_DPAD_UP
  }
  if (IS_KEY_DOWN(VK_DOWN)) {
    // Down
    buttons |= 0x0002;  // XINPUT_GAMEPAD_DPAD_DOWN
  }
  if (IS_KEY_DOWN(VK_RIGHT)) {
    // Right
    buttons |= 0x0008;  // XINPUT_GAMEPAD_DPAD_RIGHT
  }
  if (IS_KEY_DOWN(VK_LEFT)) {
    // Left
    buttons |= 0x0004;  // XINPUT_GAMEPAD_DPAD_LEFT
  }

  if (IS_KEY_DOWN('F')) {
    buttons |= 0x4000;  // XINPUT_GAMEPAD_X
  }
  if (IS_KEY_DOWN('Q') || IS_KEY_DOWN('B')) {
    buttons |= 0x2000;  // XINPUT_GAMEPAD_B
  }
  if (IS_KEY_DOWN('E') || IS_KEY_DOWN(VK_SPACE)) {
    buttons |= 0x1000;  // XINPUT_GAMEPAD_A
  }
  if (IS_KEY_DOWN('3')) {
    buttons |= 0x8000;  // XINPUT_GAMEPAD_Y
  }
  if (IS_KEY_DOWN('G')) {
    left_trigger = 0xFF;
  }

  if (IS_KEY_DOWN('V')) {
    buttons |= 0x0080;  // XINPUT_GAMEPAD_RIGHT_THUMB
  }

  /*if (IS_KEY_DOWN('Q') || IS_KEY_DOWN('I')) {
    // Q / I
    left_trigger = 0xFF;
  }

  if (IS_KEY_DOWN('E') || IS_KEY_DOWN('O')) {
    // E / O
    right_trigger = 0xFF;
  }*/

  if (IS_KEY_DOWN(VK_TAB)) {
    buttons |= 0x0020;  // XINPUT_GAMEPAD_BACK
  }
  if (IS_KEY_DOWN(VK_RETURN)) {
    buttons |= 0x0010;  // XINPUT_GAMEPAD_START
  }
  if (IS_KEY_DOWN('1')) {
    buttons |= 0x0100;  // XINPUT_GAMEPAD_LEFT_SHOULDER
  }
  if (IS_KEY_DOWN('R')) {
    buttons |= 0x0200;  // XINPUT_GAMEPAD_RIGHT_SHOULDER
  }

  // Apply our inputs
  out_state->gamepad.buttons = buttons;
  out_state->gamepad.left_trigger = left_trigger;
  out_state->gamepad.right_trigger = right_trigger;
  out_state->gamepad.thumb_lx = thumb_lx;
  out_state->gamepad.thumb_ly = thumb_ly;
  out_state->gamepad.thumb_rx = thumb_rx;
  out_state->gamepad.thumb_ry = thumb_ry;

  return X_ERROR_SUCCESS;
}


}  // namespace winkey
}  // namespace hid
}  // namespace xe
