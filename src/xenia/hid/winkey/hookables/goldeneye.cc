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

DECLARE_double(sensitivity);
DECLARE_bool(invert_y);
DECLARE_bool(disable_autoaim);

DEFINE_double(aim_turn_distance, 0.4f,
              "(GoldenEye) Distance crosshair can move in aim-mode before "
              "turning the camera [range 0 - 1]",
              "MouseHook");

DEFINE_double(ge_menu_sensitivity, 0.5f,
              "(GoldenEye) Mouse sensitivity when in menus", "MouseHook");

DEFINE_bool(ge_gun_sway, true, "(GoldenEye) Enable gun sway as camera is turned",
            "MouseHook");

DEFINE_bool(ge_always_allow_inputs, false,
            "(GoldenEye) (TEMP) Skip the 'is-allowed-to-move' checks and "
            "always allow camera movement, might break cutscenes, will be "
            "removed once I find a way to check for fade-in instead.", "MouseHook");

const uint32_t kTitleIdGoldenEye = 0x584108A9;

namespace xe {
namespace hid {
namespace winkey {

GoldeneyeGame::~GoldeneyeGame() = default;

bool GoldeneyeGame::IsGameSupported() {
  if (kernel_state()->title_id() != kTitleIdGoldenEye) {
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

bool GoldeneyeGame::DoHooks(uint32_t user_index, RawInputState& input_state,
                            X_INPUT_STATE* out_state) {
  if (!IsGameSupported()) {
    return false;
  }

  auto time = xe::kernel::XClock::now();

  // Move menu selection crosshair
  // TODO: detect if we're actually in the menu first
  auto menuX_ptr =
      kernel_memory()->TranslateVirtual<xe::be<float>*>(0x8272B37C);
  auto menuY_ptr =
      kernel_memory()->TranslateVirtual<xe::be<float>*>(0x8272B380);
  if (menuX_ptr && menuY_ptr) {
    float menuX = *menuX_ptr;
    float menuY = *menuY_ptr;

    menuX += (((float)input_state.mouse.x_delta) / 5.f) *
             (float)cvars::ge_menu_sensitivity;
    menuY += (((float)input_state.mouse.y_delta) / 5.f) *
             (float)cvars::ge_menu_sensitivity;

    *menuX_ptr = menuX;
    *menuY_ptr = menuY;
  }

  // Read addr of player base
  auto players_addr =
      *kernel_memory()->TranslateVirtual<xe::be<uint32_t>*>(0x82F1FA98);

  if (!players_addr) {
    return true;
  }

  auto* player = kernel_memory()->TranslateVirtual(players_addr);

  auto game_pause_flag =
      *kernel_memory()->TranslateVirtual<xe::be<uint32_t>*>(0x82F1E70C);

  // TODO: find better game_control_active byte, this doesn't handle the delay
  // when watch is brought up/down
  // also apparently doesn't get set until fade-in is complete (while controller
  // is active during the fade instead)
  auto game_control_active = *(xe::be<uint32_t>*)(player + 0x908);
  
  // TODO: remove this once we can detect fade-in instead
  if (cvars::ge_always_allow_inputs) {
    game_control_active = 1;
  }

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

  if (!game_control_active) {
    return true;
  }

  // GE007 mousehook hax
  xe::be<float>* player_cam_x = (xe::be<float>*)(player + 0x254);
  xe::be<float>* player_cam_y = (xe::be<float>*)(player + 0x264);

  xe::be<float>* player_crosshair_x = (xe::be<float>*)(player + 0x10A8);
  xe::be<float>* player_crosshair_y = (xe::be<float>*)(player + 0x10AC);
  xe::be<float>* player_gun_x = (xe::be<float>*)(player + 0x10BC);
  xe::be<float>* player_gun_y = (xe::be<float>*)(player + 0x10C0);

  uint32_t player_aim_mode = *(xe::be<uint32_t>*)(player + 0x22C);

  if (player_aim_mode != prev_aim_mode_) {
    if (player_aim_mode != 0) {
      // Entering aim mode, reset gun position
      *player_gun_x = 0;
      *player_gun_y = 0;
    }
    // Always reset crosshair after entering/exiting aim mode
    // Otherwise non-aim-mode will still fire toward it...
    *player_crosshair_x = 0;
    *player_crosshair_y = 0;
    prev_aim_mode_ = player_aim_mode;
  }

  float gX = *player_gun_x;
  float gY = *player_gun_y;

  // Have to do weird things converting it to normal float otherwise
  // xe::be += treats things as int?
  if (player_aim_mode == 1) {
    float chX = *player_crosshair_x;
    float chY = *player_crosshair_y;

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
    chX = std::min(chX, 1.f);
    chX = std::max(chX, -1.f);
    chY = std::min(chY, 1.f);
    chY = std::max(chY, -1.f);
    gX = std::min(gX, 1.f);
    gX = std::max(gX, -1.f);
    gY = std::min(gY, 1.f);
    gY = std::max(gY, -1.f);

    *player_crosshair_x = chX;
    *player_crosshair_y = chY;
    *player_gun_x = gX;
    *player_gun_y = gY;

    // Turn camera when crosshair is past a certain point
    float camX = (float)*player_cam_x;
    float camY = (float)*player_cam_y;

    // this multiplier lets us slow down the camera-turn when zoomed in
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

    time_start_center_ = time + std::chrono::milliseconds(50);
    start_centering_ = true;
    disable_sway_ = true;     // skip weapon sway until we've centered
    centering_speed_ = 0.05f; // speed up centering from aim-mode
  } else {
    // Apply gun-centering once enough time has passed
    if (start_centering_ && time >= time_start_center_) {
      if (gX != 0 || gY != 0) {
        if (gX > 0) {
          gX -= std::min(centering_speed_, gX);
        }
        if (gX < 0) {
          gX += std::min(centering_speed_, -gX);
        }
        if (gY > 0) {
          gY -= std::min(centering_speed_, gY);
        }
        if (gY < 0) {
          gY += std::min(centering_speed_, -gY);
        }
      }
      if (gX == 0 && gY == 0) {
        centering_speed_ = 0.0125f;
        start_centering_ = false;
        disable_sway_ = false;
      }
    }

    // Camera hax
    if (input_state.mouse.x_delta || input_state.mouse.y_delta) {
      float camX = *player_cam_x;
      float camY = *player_cam_y;

      camX += (((float)input_state.mouse.x_delta) / 10.f) *
              (float)cvars::sensitivity;
          
      // Add 'sway' to gun
      float gun_sway_x = (((float)input_state.mouse.x_delta) / 4000.f) *
                          (float)cvars::sensitivity;
      float gun_sway_y = (((float)input_state.mouse.y_delta) / 4000.f) *
                          (float)cvars::sensitivity;

      float gun_sway_x_changed = gX + gun_sway_x;
      float gun_sway_y_changed = gY + gun_sway_y;

      if (!cvars::invert_y) {
        camY -= (((float)input_state.mouse.y_delta) / 10.f) *
                (float)cvars::sensitivity;
      } else {
        camY += (((float)input_state.mouse.y_delta) / 10.f) *
                (float)cvars::sensitivity;
        gun_sway_y_changed = gY - gun_sway_y;
      }

      *player_cam_x = camX;
      *player_cam_y = camY;

      if (cvars::ge_gun_sway && !disable_sway_) {
        // Bound the 'sway' movement to [0.2:-0.2] to make it look a bit
        // better (but only if the sway would make it go further OOB)
        if (gun_sway_x_changed > 0.2f && gun_sway_x > 0) {
          gun_sway_x_changed = gX;
        }
        if (gun_sway_x_changed < -0.2f && gun_sway_x < 0) {
          gun_sway_x_changed = gX;
        }
        if (gun_sway_y_changed > 0.2f && gun_sway_y > 0) {
          gun_sway_y_changed = gY;
        }
        if (gun_sway_y_changed < -0.2f && gun_sway_y < 0) {
          gun_sway_y_changed = gY;
        }

        gX = gun_sway_x_changed;
        gY = gun_sway_y_changed;
      }
    } else {
      if (!start_centering_) {
        time_start_center_ = time + std::chrono::milliseconds(100);
        start_centering_ = true;
        centering_speed_ = 0.0125f;
      }
    }

    if (out_state->gamepad.right_trigger != 0) {
      // firing, force gun to center
      gX = gY = 0;
    }

    gX = std::min(gX, 1.f);
    gX = std::max(gX, -1.f);
    gY = std::min(gY, 1.f);
    gY = std::max(gY, -1.f);

    *player_gun_x = gX;
    *player_gun_y = gY;
  }

  return true;
}

}  // namespace winkey
}  // namespace hid
}  // namespace xe
