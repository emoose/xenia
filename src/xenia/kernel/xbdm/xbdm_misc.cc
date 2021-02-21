/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/base/logging.h"
#include "xenia/kernel/kernel_state.h"
#include "xenia/kernel/util/shim_utils.h"
#include "xenia/kernel/xbdm/xbdm_private.h"
#include "xenia/kernel/xthread.h"
#include "xenia/vfs/devices/host_path_device.h"
#include "xenia/emulator.h"
#include "xenia/xbox.h"

DEFINE_string(devkit_root, "", "Root path for devkit: partition", "Storage");

namespace xe {
namespace kernel {
namespace xbdm {

#define MAKE_DUMMY_STUB_PTR(x)     \
  dword_result_t x() { return 0; } \
  DECLARE_XBDM_EXPORT1(x, kDebug, kStub)

#define MAKE_DUMMY_STUB_STATUS(x)                           \
  dword_result_t x() { return X_STATUS_INVALID_PARAMETER; } \
  DECLARE_XBDM_EXPORT1(x, kDebug, kStub)

MAKE_DUMMY_STUB_PTR(DmAllocatePool);

void DmCloseLoadedModules(lpdword_t unk0_ptr) {}
DECLARE_XBDM_EXPORT1(DmCloseLoadedModules, kDebug, kStub);

MAKE_DUMMY_STUB_STATUS(DmFreePool);

dword_result_t DmGetXbeInfo() {
  // TODO(gibbed): Crackdown appears to expect this as success?
  // Unknown arguments -- let's hope things don't explode.
  return 0x02DA0000;
}
DECLARE_XBDM_EXPORT1(DmGetXbeInfo, kDebug, kStub);

MAKE_DUMMY_STUB_STATUS(DmGetXboxName);

dword_result_t DmIsDebuggerPresent() { return 0; }
DECLARE_XBDM_EXPORT1(DmIsDebuggerPresent, kDebug, kStub);

MAKE_DUMMY_STUB_STATUS(DmRegisterCommandProcessor);

void DmSendNotificationString(lpdword_t unk0_ptr) {}
DECLARE_XBDM_EXPORT1(DmSendNotificationString, kDebug, kStub);

dword_result_t DmRegisterCommandProcessorEx(lpdword_t name_ptr,
                                            lpdword_t handler_fn,
                                            dword_t unk3) {
  // Return success to prevent some games from stalling
  return X_STATUS_SUCCESS;
}
DECLARE_XBDM_EXPORT1(DmRegisterCommandProcessorEx, kDebug, kStub);

MAKE_DUMMY_STUB_STATUS(DmStartProfiling);
MAKE_DUMMY_STUB_STATUS(DmStopProfiling);

dword_result_t DmCaptureStackBackTrace(lpdword_t unk0_ptr, lpdword_t unk1_ptr) {
  return X_STATUS_INVALID_PARAMETER;
}
DECLARE_XBDM_EXPORT1(DmCaptureStackBackTrace, kDebug, kStub);

MAKE_DUMMY_STUB_STATUS(DmGetThreadInfoEx);
MAKE_DUMMY_STUB_STATUS(DmSetProfilingOptions);

dword_result_t DmWalkLoadedModules(lpdword_t unk0_ptr, lpdword_t unk1_ptr) {
  // Some games will loop forever unless this code is returned
  return 0x82DA0104;
}
DECLARE_XBDM_EXPORT1(DmWalkLoadedModules, kDebug, kStub);

dword_result_t DmMapDevkitDrive() {
  auto* emulator = kernel_state()->emulator();
  auto* file_system = emulator->file_system();
  std::string existing;

  if (file_system->FindSymbolicLink("devkit:", existing)) {
    return X_STATUS_SUCCESS;  // TODO: check what actual code returned is
  }

  auto path = emulator->executable_path().parent_path();
  if (!cvars::devkit_root.empty()) {
    path = cvars::devkit_root;
  }

  auto device =
      std::make_unique<xe::vfs::HostPathDevice>("\\devkit", path, false);
  if (!device->Initialize()) {
    XELOGE("DmMapDevkitDrive: Unable to scan devkit path");
  } else {
    if (!file_system->RegisterDevice(std::move(device))) {
      XELOGE("DmMapDevkitDrive: Unable to register devkit path");
    } else {
      file_system->RegisterSymbolicLink("devkit:", "\\devkit");
    }
  }

  return X_STATUS_SUCCESS;
}
DECLARE_XBDM_EXPORT1(DmMapDevkitDrive, kDebug, kStub);

dword_result_t DmFindPdbSignature(lpdword_t unk0_ptr, lpdword_t unk1_ptr) {
  return X_STATUS_INVALID_PARAMETER;
}
DECLARE_XBDM_EXPORT1(DmFindPdbSignature, kDebug, kStub);

void RegisterMiscExports(xe::cpu::ExportResolver* export_resolver,
                         KernelState* kernel_state) {}

}  // namespace xbdm
}  // namespace kernel
}  // namespace xe
