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
#include "xenia/kernel/util/shim_utils.h"
#include "xenia/xbox.h"

using namespace xe::kernel;

DECLARE_bool(swap_buttons);
DECLARE_double(sensitivity);
DECLARE_bool(invert_y);
DECLARE_bool(disable_autoaim);

DEFINE_double(
    aim_turn_distance, 0.4f,
    "(GE) Distance crosshair can move in aim-mode before turning the camera",
    "MouseHook");

namespace xe {
namespace hid {
namespace winkey {

GoldeneyeGame::~GoldeneyeGame() = default;

bool GoldeneyeGame::IsGameSupported() {
  if (kernel_state()->title_id() != 0x584108A9) {
    return false;
  }

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

  // Move menu selection crosshair
  // TODO: detect if we're actually in the menu first
  auto menuX_ptr =
      kernel_memory()->TranslateVirtual<xe::be<float>*>(0x8272B37C);
  auto menuY_ptr =
      kernel_memory()->TranslateVirtual<xe::be<float>*>(0x8272B380);
  if (menuX_ptr && menuY_ptr) {
    float menuX = *menuX_ptr;
    float menuY = *menuY_ptr;

    menuX +=
        (((float)input_state.mouse.x_delta) / 5.f) * (float)cvars::sensitivity;
    menuY +=
        (((float)input_state.mouse.y_delta) / 5.f) * (float)cvars::sensitivity;

    *menuX_ptr = menuX;
    *menuY_ptr = menuY;
  }

  // Read addr of player base
  auto players_addr =
      *kernel_memory()->TranslateVirtual<xe::be<uint32_t>*>(0x82F1FA98);

  if (players_addr) {
    auto* player = kernel_memory()->TranslateVirtual(players_addr);

    auto game_pause_flag =
        *kernel_memory()->TranslateVirtual<xe::be<uint32_t>*>(0x82F1E70C);

    // TODO: find better game_control_active byte, this doesn't handle the delay
    // when watch is brought up/down...
    auto game_control_active = *(xe::be<uint32_t>*)(player + 0x908);

    // Disable auto-aim & lookahead
    // (only if we detect game_control_active or game_pause_flag values are changing)
    if (game_pause_flag != prev_game_pause_flag_ ||
        game_control_active != prev_game_control_active_) {

      auto settings_addr =
          *kernel_memory()->TranslateVirtual<xe::be<uint32_t>*>(0x83088228);

      if (settings_addr) {
        auto* settings_ptr = kernel_memory()->TranslateVirtual<xe::be<uint32_t>*>(
            settings_addr + 0x298);
        uint32_t settings = *settings_ptr;

        enum GESettingFlag {
          LookUpright = 0x8,  // non-inverted
          AutoAim = 0x10,
          AimControlToggle = 0x20,
          ShowAimCrosshair = 0x40,
          LookAhead = 0x80,
          ShowAmmoCounter = 0x100,
          ShowAimBorder = 0x200,
          ScreenLetterboxWide = 0x400,
          ScreenLetterboxCinema = 0x800,
          ScreenRatio16_9 = 0x1000,
          ScreenRatio16_10 = 0x2000,
          CameraRoll = 0x40000
        };

        // Disable AutoAim & LookAhead
        settings = settings & ~((uint32_t)GESettingFlag::LookAhead);
        if (cvars::disable_autoaim) {
          settings = settings & ~((uint32_t)GESettingFlag::AutoAim);
        }

        *settings_ptr = settings;

        prev_game_pause_flag_ = game_pause_flag;
        prev_game_control_active_ = game_control_active;
      }
    }

    // GE007 mousehook hax
    if (game_control_active) {
      xe::be<float>* player_cam_x = (xe::be<float>*)(player + 0x254);
      xe::be<float>* player_cam_y = (xe::be<float>*)(player + 0x264);

      xe::be<float>* player_crosshair_x = (xe::be<float>*)(player + 0x10A8);
      xe::be<float>* player_crosshair_y = (xe::be<float>*)(player + 0x10AC);
      xe::be<float>* player_gun_x = (xe::be<float>*)(player + 0x10BC);
      xe::be<float>* player_gun_y = (xe::be<float>*)(player + 0x10C0);

      uint32_t player_aim_mode = *(xe::be<uint32_t>*)(player + 0x22C);

      if (player_aim_mode != prev_aim_mode_) {
        // aim mode changed, reset it
        *player_crosshair_x = 0;
        *player_crosshair_y = 0;
        *player_gun_x = 0;
        *player_gun_y = 0;
        prev_aim_mode_ = player_aim_mode;
      }

      // Have to do weird things converting it to normal float otherwise
      // xe::be += treats things as int?
      if (player_aim_mode == 1) {
        float chX = *player_crosshair_x;
        float chY = *player_crosshair_y;
        float gX = *player_gun_x;
        float gY = *player_gun_y;

        chX += (((float)input_state.mouse.x_delta) / 500.f) *
               (float)cvars::sensitivity;
        gX += (((float)input_state.mouse.x_delta) / 500.f) *
              (float)cvars::sensitivity;

        if (!cvars::invert_y) {
          chY += (((float)input_state.mouse.y_delta) / 500.f) *
                 (float)cvars::sensitivity;
          gY += (((float)input_state.mouse.y_delta) / 500.f) *
                (float)cvars::sensitivity;
        } else {
          chY -= (((float)input_state.mouse.y_delta) / 500.f) *
                 (float)cvars::sensitivity;
          gY -= (((float)input_state.mouse.y_delta) / 500.f) *
                (float)cvars::sensitivity;
        }

        // Keep the gun/crosshair in-bounds [1:-1]

        if (chX > 1) {
          chX = 1;
        }
        if (chX < -1) {
          chX = -1;
        }
        if (chY > 1) {
          chY = 1;
        }
        if (chY < -1) {
          chY = -1;
        }

        if (gX > 1) {
          gX = 1;
        }
        if (gX < -1) {
          gX = -1;
        }
        if (gY > 1) {
          gY = 1;
        }
        if (gY < -1) {
          gY = -1;
        }

        *player_crosshair_x = chX;
        *player_crosshair_y = chY;
        *player_gun_x = gX;
        *player_gun_y = gY;

        // Turn camera when crosshair is past a certain point
        float camX = (float)*player_cam_x;
        float camY = (float)*player_cam_y;

        // this multiplier lets us slow down the movement when zoomed in
        // value from this addr works but doesn't seem correct, seems too slow
        // when zoomed, might be better to calculate it from FOV instead?
        // (current_fov seems to be at 0x115C)
        float aim_multiplier =
            *reinterpret_cast<xe::be<float>*>(player + 0x11AC);

        // TODO: This almost matches up with "show aim border" perfectly
        // Except 0.4f will make Y move a little earlier than it should
        // > 0.5f seems to work for Y, but then X needs to be moved further than
        // it should need to...

        // TODO: see if we can find the algo the game itself uses
        float ch_distance = sqrtf((chX * chX) + (chY * chY));
        if (ch_distance > cvars::aim_turn_distance) {
          camX += (chX * aim_multiplier);
          *player_cam_x = camX;
          camY -= (chY * aim_multiplier);
          *player_cam_y = camY;
        }
      } else {
        float camX = (float)*player_cam_x;
        float camY = (float)*player_cam_y;

        camX += (((float)input_state.mouse.x_delta) / 10.f) *
                (float)cvars::sensitivity;

        if (!cvars::invert_y) {
          camY -= (((float)input_state.mouse.y_delta) / 10.f) *
                  (float)cvars::sensitivity;
        } else {
          camY += (((float)input_state.mouse.y_delta) / 10.f) *
                  (float)cvars::sensitivity;
        }

        *player_cam_x = camX;
        *player_cam_y = camY;
      }
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
