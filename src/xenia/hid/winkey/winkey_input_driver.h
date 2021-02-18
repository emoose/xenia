/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2014 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_HID_WINKEY_WINKEY_INPUT_DRIVER_H_
#define XENIA_HID_WINKEY_WINKEY_INPUT_DRIVER_H_

#include <queue>

#include "xenia/base/mutex.h"
#include "xenia/hid/input_driver.h"
#include "xenia/hid/winkey/hookables/hookable_game.h"

#define XINPUT_BUTTONS_MASK  0xFFFF
#define XINPUT_BIND_LEFT_TRIGGER   (1 << 16)
#define XINPUT_BIND_RIGHT_TRIGGER  (1 << 17)

#define XINPUT_BIND_LS_UP    (1 << 18)
#define XINPUT_BIND_LS_DOWN  (1 << 19)
#define XINPUT_BIND_LS_LEFT  (1 << 20)
#define XINPUT_BIND_LS_RIGHT (1 << 21)

#define XINPUT_BIND_RS_UP    (1 << 22)
#define XINPUT_BIND_RS_DOWN  (1 << 23)
#define XINPUT_BIND_RS_LEFT  (1 << 24)
#define XINPUT_BIND_RS_RIGHT (1 << 25)

#define XINPUT_BIND_MODIFIER (1 << 26)

#define VK_BIND_MWHEELUP     0x10000
#define VK_BIND_MWHEELDOWN   0x20000

namespace xe {
namespace hid {
namespace winkey {

class WinKeyInputDriver : public InputDriver {
 public:

  explicit WinKeyInputDriver(xe::ui::Window* window);
  ~WinKeyInputDriver() override;

  X_STATUS Setup() override;

  X_RESULT GetCapabilities(uint32_t user_index, uint32_t flags,
                           X_INPUT_CAPABILITIES* out_caps) override;
  X_RESULT GetState(uint32_t user_index, X_INPUT_STATE* out_state) override;
  X_RESULT SetState(uint32_t user_index, X_INPUT_VIBRATION* vibration) override;
  X_RESULT GetKeystroke(uint32_t user_index, uint32_t flags,
                        X_INPUT_KEYSTROKE* out_keystroke) override;

 protected:
  struct KeyEvent {
    int vkey = 0;
    int repeat_count = 0;
    bool transition = false;  // going up(false) or going down(true)
    bool prev_state = false;  // down(true) or up(false)
  };

  xe::global_critical_region global_critical_region_;
  std::queue<KeyEvent> key_events_;

  std::mutex mouse_mutex_;
  std::queue<MouseEvent> mouse_events_;

  std::mutex key_mutex_;
  bool key_states_[256];

  uint32_t packet_number_;

  std::vector<std::unique_ptr<HookableGame>> hookable_games_;

  std::unordered_map<uint32_t, std::unordered_map<uint32_t, uint32_t>>
      key_binds_;
};

}  // namespace winkey
}  // namespace hid
}  // namespace xe

#endif  // XENIA_HID_WINKEY_WINKEY_INPUT_DRIVER_H_
