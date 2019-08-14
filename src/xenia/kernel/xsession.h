/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2019 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_KERNEL_XSESSION_H_
#define XENIA_KERNEL_XSESSION_H_

#include <memory>
#include <unordered_map>

#include "xenia/base/mutex.h"
#include "xenia/base/threading.h"
#include "xenia/kernel/xobject.h"
#include "xenia/xbox.h"

namespace xe {
namespace kernel {

struct X_SESSION {
  X_DISPATCH_HEADER header;
  uint32_t pointer;
};
static_assert_size(X_SESSION, 0x14);

class XSession : public XObject {
 public:
  static const Type kType = kTypeSession;

  explicit XSession(KernelState* kernel_state);
  ~XSession() override;

  void Initialize();

  bool Save(ByteStream* stream) override;
  static object_ref<XSession> Restore(KernelState* kernel_state,
                                           ByteStream* stream);

 private:
  XSession();
};

}  // namespace kernel
}  // namespace xe

#endif  // XENIA_KERNEL_XSESSION_H_
