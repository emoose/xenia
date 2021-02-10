/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2014 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/hid/winkey/hookables/goldeneye.h"

#include "xenia/base/platform_win.h"
#include "xenia/hid/hid_flags.h"
#include "xenia/hid/input_system.h"
#include "xenia/xbox.h"
#include "xenia/kernel/util/shim_utils.h"

using namespace xe::kernel;

DECLARE_bool(swap_buttons);
DECLARE_double(sensitivity);
DECLARE_bool(invert_y);

namespace xe {
namespace hid {
namespace winkey {

GoldeneyeGame::~GoldeneyeGame() = default;

bool GoldeneyeGame::IsGameSupported() {
  auto* check =
      kernel_memory()->TranslateVirtual<xe::be<uint32_t>*>(0x8200336C);
  if (!check) {
    return false;
  }

  return *check == 0x676f6c64;
}

#define IS_KEY_DOWN(x) (input_state.key_states[x])

X_RESULT GoldeneyeGame::GetState(uint32_t user_index,
                                 RawInputState& input_state,
                                 X_INPUT_STATE* out_state) {
  if (!IsGameSupported()) {
    return 1;
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
    left_trigger = 0xFF;
  }

  if (input_state.mouse.wheel_delta != 0) {
    if (input_state.mouse.wheel_delta > 0) {
      buttons |= 0x8000;  // XINPUT_GAMEPAD_Y + RT
      right_trigger = 0xFF;
    } else {
      buttons |= 0x8000;  // XINPUT_GAMEPAD_Y
    }
  }

  // GE007 mousehook hax
  // Read addr of player base
  auto players_addr =
      *kernel_memory()->TranslateVirtual<xe::be<uint32_t>*>(0x82F1FA98);

  if (players_addr) {
    // Add xenia console memory base to the addr
    auto* player = kernel_memory()->TranslateVirtual(players_addr);

    xe::be<float>* player_cam_x = (xe::be<float>*)(player + 0x254);
    xe::be<float>* player_cam_y = (xe::be<float>*)(player + 0x264);

    // Have to do weird things converting it to normal float otherwise
    // xe::be += treats things as int?
    float camX = (float)*player_cam_x;
    float camY = (float)*player_cam_y;

    camX += (((float)input_state.mouse.x_delta) / 10.f) * (float)cvars::sensitivity;

    if (!cvars::invert_y) {
      camY -= (((float)input_state.mouse.y_delta) / 10.f) * (float)cvars::sensitivity;
    } else {
      camY += (((float)input_state.mouse.y_delta) / 10.f) *
              (float)cvars::sensitivity;
    }

    *player_cam_x = camX;
    *player_cam_y = camY;
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

  if (IS_KEY_DOWN('R')) {
    // R
    buttons |= 0x4000;  // XINPUT_GAMEPAD_X
  }
  if (IS_KEY_DOWN('F')) {
    // Q
    buttons |= 0x2000;  // XINPUT_GAMEPAD_B
  }
  if (IS_KEY_DOWN('E')) {
    // E
    buttons |= 0x1000;  // XINPUT_GAMEPAD_A
  }
  if (IS_KEY_DOWN('Q')) {
    // Space
    buttons |= 0x8000;  // XINPUT_GAMEPAD_Y
  }

  if (IS_KEY_DOWN('V')) {
    // V
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
    // Z
    buttons |= 0x0020;  // XINPUT_GAMEPAD_BACK
  }
  if (IS_KEY_DOWN(VK_RETURN)) {
    // X
    buttons |= 0x0010;  // XINPUT_GAMEPAD_START
  }
  if (IS_KEY_DOWN('1')) {
    // 1
    buttons |= 0x0100;  // XINPUT_GAMEPAD_LEFT_SHOULDER
  }
  if (IS_KEY_DOWN('3')) {
    // 3
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
