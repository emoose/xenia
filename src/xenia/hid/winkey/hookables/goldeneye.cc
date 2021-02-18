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
const uint32_t kTitleIdPerfectDark = 0x584109C2;

namespace xe {
namespace hid {
namespace winkey {

GoldeneyeGame::~GoldeneyeGame() = default;

struct RareGameBuildAddrs {
  uint32_t check_addr;   // addr to check
  uint32_t check_value;  // value to look for, if matches this value we know
                         // it's this game

  uint32_t menu_addr;  // addr of menu x/y
  uint32_t game_pause_addr;

  uint32_t settings_addr;

  uint32_t player_addr;  // addr to pointer of player data

  uint32_t player_offset_disabled;  // offset to "is control disabled" flag
  uint32_t player_offset_active_uint8;  // offset to uint8 "is control active/enabled" flag
  uint32_t player_offset_cam_x;   // offset to camera X pos
  uint32_t player_offset_cam_y;   // offset to camera Y pos
  uint32_t player_offset_crosshair_x;
  uint32_t player_offset_crosshair_y;
  uint32_t player_offset_gun_x;
  uint32_t player_offset_gun_y;
  uint32_t player_offset_aim_mode;
  uint32_t player_offset_aim_multiplier;
};

std::map<GoldeneyeGame::GameBuild, RareGameBuildAddrs> supported_builds = {
    {
      GoldeneyeGame::GameBuild::GoldenEye_Aug2007,
        {0x8200336C, 0x676f6c64, 0x8272B37C, 0x82F1E70C, 0x83088228, 0x82F1FA98,
          0x80, 0, 0x254, 0x264, 0x10A8, 0x10AC, 0x10BC, 0x10C0, 0x22C, 0x11AC}
    },
    {
      GoldeneyeGame::GameBuild::PerfectDark_Devkit_33,
        {0x825CBC59, 0x30303333, 0, 0, 0, 0x826284C4, 0, 0x668, 0x14C, 0x15C, 0, 0, 0,
          0, 0, 0}
    },
    {
      GoldeneyeGame::GameBuild::PerfectDark_Release_52,
        {0x825EC0E5, 0x30303532, 0, 0, 0, 0x8264909C, 0, 0x668, 0x14C, 0x15C, 0, 0, 0,
          0, 0, 0}
    },
    {
      GoldeneyeGame::GameBuild::PerfectDark_Devkit_102,
        {0x825EC0E5, 0x30313032, 0, 0, 0, 0x82649274, 0, 0x668, 0x14C, 0x15C, 0, 0, 0,
          0, 0, 0}
    },
    // TODO: test these!
    /*
    {
      GoldeneyeGame::GameBuild::PerfectDark_Release_104,
        {0x825EC0D5, 0x30313034, 0, 0, 0, 0x82649264, 0, 0x668, 0x14C, 0x15C, 0, 0, 0,
          0, 0, 0}
    },
    {
      GoldeneyeGame::GameBuild::PerfectDark_Release_107,
        {0x825FC25D, 0x30313037, 0, 0, 0, 0x826619E4, 0, 0x668, 0x14C, 0x15C, 0, 0, 0,
          0, 0, 0}
    },*/
};

bool GoldeneyeGame::IsGameSupported() {
  auto title_id = kernel_state()->title_id();

  if (title_id != kTitleIdGoldenEye && title_id != kTitleIdPerfectDark) {
    return false;
  }

  for (auto& build : supported_builds) {
    auto* build_ptr = kernel_memory()->TranslateVirtual<xe::be<uint32_t>*>(
        build.second.check_addr);

    if (*build_ptr == build.second.check_value) {
      game_build_ = build.first;
      return true;
    }
  }

  return false;
}

#define IS_KEY_DOWN(x) (input_state.key_states[x])

bool GoldeneyeGame::DoHooks(uint32_t user_index, RawInputState& input_state,
                            X_INPUT_STATE* out_state) {
  if (!IsGameSupported()) {
    return false;
  }

  auto& game_addrs = supported_builds[game_build_];

  auto time = xe::kernel::XClock::now();

  // Move menu selection crosshair
  // TODO: detect if we're actually in the menu first
  if (game_addrs.menu_addr) {
    auto menuX_ptr =
        kernel_memory()->TranslateVirtual<xe::be<float>*>(game_addrs.menu_addr);
    auto menuY_ptr =
        kernel_memory()->TranslateVirtual<xe::be<float>*>(game_addrs.menu_addr + 4);
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
  }

  // Read addr of player base
  auto players_addr =
      *kernel_memory()->TranslateVirtual<xe::be<uint32_t>*>(game_addrs.player_addr);

  if (!players_addr) {
    return true;
  }

  auto* player = kernel_memory()->TranslateVirtual(players_addr);

  uint32_t game_pause_flag = 0;
  if (game_addrs.game_pause_addr) {
    game_pause_flag = *kernel_memory()->TranslateVirtual<xe::be<uint32_t>*>(
        game_addrs.game_pause_addr);
  }

  // TODO: find better game_control_disabled byte, this doesn't handle the delay
  // when watch is brought up/down
  // TODO: +0x2E8 = some kind of watch-status dword?
  uint32_t game_control_disabled = 0;
  if (game_addrs.player_offset_disabled) {
    game_control_disabled =
        *(xe::be<uint32_t>*)(player + game_addrs.player_offset_disabled);
  }
  if (game_addrs.player_offset_active_uint8) {
    // TODO: find the disabled flag in PD
    game_control_disabled =
        !*(uint8_t*)(player + game_addrs.player_offset_active_uint8);
  }
  
  // TODO: remove this once we can detect fade-in instead
  if (cvars::ge_always_allow_inputs) {
    game_control_disabled = 0;
  }

  // Disable auto-aim & lookahead
  // (only if we detect game_control_active or game_pause_flag values are changing)
  if (game_pause_flag != prev_game_pause_flag_ ||
      game_control_disabled != prev_game_control_disabled_) {

    if (game_addrs.settings_addr) {
      auto settings_addr =
          *kernel_memory()->TranslateVirtual<xe::be<uint32_t>*>(
              game_addrs.settings_addr);

      if (settings_addr) {
        auto* settings_ptr =
            kernel_memory()->TranslateVirtual<xe::be<uint32_t>*>(settings_addr +
                                                                 0x298);
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
      }

      prev_game_pause_flag_ = game_pause_flag;
      prev_game_control_disabled_ = game_control_disabled;
    }
  }

  if (game_control_disabled) {
    return true;
  }

  // GE007 mousehook hax
  xe::be<float>* player_cam_x = (xe::be<float>*)(player + game_addrs.player_offset_cam_x);
  xe::be<float>* player_cam_y =
      (xe::be<float>*)(player + game_addrs.player_offset_cam_y);

  xe::be<float>* player_crosshair_x =
      (xe::be<float>*)(player + game_addrs.player_offset_crosshair_x);
  xe::be<float>* player_crosshair_y =
      (xe::be<float>*)(player + game_addrs.player_offset_crosshair_y);
  xe::be<float>* player_gun_x =
      (xe::be<float>*)(player + game_addrs.player_offset_gun_x);
  xe::be<float>* player_gun_y =
      (xe::be<float>*)(player + game_addrs.player_offset_gun_y);

  uint32_t player_aim_mode = 0;

  if (game_addrs.player_offset_aim_mode) {
    player_aim_mode =
        *(xe::be<uint32_t>*)(player + game_addrs.player_offset_aim_mode);

    if (player_aim_mode != prev_aim_mode_) {
      if (player_aim_mode != 0) {
        // Entering aim mode, reset gun position
        if (game_addrs.player_offset_gun_x && game_addrs.player_offset_gun_y) {
          *player_gun_x = 0;
          *player_gun_y = 0;
        }
      }
      // Always reset crosshair after entering/exiting aim mode
      // Otherwise non-aim-mode will still fire toward it...
      if (game_addrs.player_offset_crosshair_x &&
          game_addrs.player_offset_crosshair_y) {
        *player_crosshair_x = 0;
        *player_crosshair_y = 0;
      }
      prev_aim_mode_ = player_aim_mode;
    }
  }

  float gX = game_addrs.player_offset_gun_x ? *player_gun_x : 0.f;
  float gY = game_addrs.player_offset_gun_y ? *player_gun_y : 0.f;

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
    float aim_multiplier = 1;
    if (game_addrs.player_offset_aim_multiplier) {
      aim_multiplier = *reinterpret_cast<xe::be<float>*>(
          player + game_addrs.player_offset_aim_multiplier);
    }

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

    start_centering_ = true;
    disable_sway_ = true;     // skip weapon sway until we've centered
    centering_speed_ = 0.05f; // speed up centering from aim-mode
  } else {
    // Apply gun-centering
    if (start_centering_) {
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
        start_centering_ = true;
        centering_speed_ = 0.0125f;
      }
    }

    if (game_addrs.player_offset_gun_x && game_addrs.player_offset_gun_y) {
      gX = std::min(gX, 1.f);
      gX = std::max(gX, -1.f);
      gY = std::min(gY, 1.f);
      gY = std::max(gY, -1.f);

      if (out_state->gamepad.right_trigger != 0) {
        // Make crosshair match gun rotation, so it'll fire where gun is facing
        if (game_addrs.player_offset_crosshair_x &&
            game_addrs.player_offset_crosshair_y) {
          *player_crosshair_x = gX;
          *player_crosshair_y = gY;
        }
      }

      *player_gun_x = gX;
      *player_gun_y = gY;
    }
  }

  return true;
}

// GE modifier reduces LS-movement, to allow for walk speed to be reduced
// (ie a 'walk' button)
bool GoldeneyeGame::ModifierKeyHandler(uint32_t user_index,
  RawInputState& input_state,
  X_INPUT_STATE* out_state) {

  float thumb_lx = (int16_t)out_state->gamepad.thumb_lx;
  float thumb_ly = (int16_t)out_state->gamepad.thumb_ly;

  // Work out angle from the current stick values
  float angle = atan2f(thumb_ly, thumb_lx);

  // Sticks get set to SHRT_MAX if key pressed, use half of that
  float distance = (float)SHRT_MAX;
  distance /= 2;

  out_state->gamepad.thumb_lx = (int16_t)(distance * cosf(angle));
  out_state->gamepad.thumb_ly = (int16_t)(distance * sinf(angle));

  // Return true to signal that we've handled the modifier, so default modifier won't be used
  return true;
}

}  // namespace winkey
}  // namespace hid
}  // namespace xe
