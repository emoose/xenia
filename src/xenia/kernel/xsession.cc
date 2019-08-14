/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2019 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/kernel/xsession.h"

#include "xenia/base/byte_stream.h"
#include "xenia/kernel/kernel_state.h"

namespace xe {
namespace kernel {

XSession::XSession(KernelState* kernel_state)
    : XObject(kernel_state, kType) {}

XSession::XSession() : XObject(kType) {}

XSession::~XSession() {}

void XSession::Initialize() { 
  this->CreateNative<X_SESSION>();
}

bool XSession::Save(ByteStream* stream) {
  if (!SaveObject(stream)) {
    return false;
  }
  return true;
}

object_ref<XSession> XSession::Restore(KernelState* kernel_state,
                                                 ByteStream* stream) {
  auto symlink = new XSession();
  symlink->kernel_state_ = kernel_state;
  if (!symlink->RestoreObject(stream)) {
    delete symlink;
    return nullptr;
  }

  auto path = stream->Read<std::string>();
  auto target = stream->Read<std::string>();
  symlink->Initialize();

  return object_ref<XSession>(symlink);
}

}  // namespace kernel
}  // namespace xe
