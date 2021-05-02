/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_KERNEL_XAM_CONTENT_MANAGER_H_
#define XENIA_KERNEL_XAM_CONTENT_MANAGER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "xenia/base/memory.h"
#include "xenia/base/mutex.h"
#include "xenia/base/string_key.h"
#include "xenia/base/string_util.h"
#include "xenia/xbox.h"

namespace xe {
namespace kernel {
class KernelState;
}  // namespace kernel
}  // namespace xe

namespace xe {
namespace kernel {
namespace xam {

struct XCONTENT_DATA {
  be<uint32_t> device_id;
  be<uint32_t> content_type;
  union {
    uint16_t display_name_raw[128];  // should be be<uint16_t>, but that stops
                                     // copy constructor being generated...
    char16_t display_name_chars[128];
  };
  char file_name_raw[42];
  uint8_t padding[2];

  std::u16string display_name() const {
    return load_and_swap<std::u16string>(display_name_raw);
  }

  std::string file_name() const {
    return load_and_swap<std::string>(file_name_raw);
  }

  bool display_name(const std::u16string_view value) {
    string_util::copy_and_swap_truncating(display_name_chars, value,
                                          countof(display_name_chars));
    return true;
  }

  bool file_name(const std::string_view value) {
    string_util::copy_maybe_truncating<string_util::Safety::IKnowWhatIAmDoing>(
        file_name_raw, value, xe::countof(file_name_raw));
    return true;
  }
};
static_assert_size(XCONTENT_DATA, 308);

struct XCONTENT_AGGREGATE_DATA {
  be<uint32_t> device_id;
  be<uint32_t> content_type;
  union {
    uint16_t display_name_raw[128];  // should be be<uint16_t>, but that stops
                                     // copy constructor being generated...
    char16_t display_name_chars[128];
  };
  char file_name_raw[42];
  uint8_t padding[2];
  be<uint32_t> title_id;

  std::u16string display_name() const {
    return load_and_swap<std::u16string>(display_name_raw);
  }

  std::string file_name() const {
    return load_and_swap<std::string>(file_name_raw);
  }

  bool display_name(const std::u16string_view value) {
    string_util::copy_and_swap_truncating(display_name_chars, value,
                                          countof(display_name_chars));
    return true;
  }

  bool file_name(const std::string_view value) {
    string_util::copy_maybe_truncating<string_util::Safety::IKnowWhatIAmDoing>(
        file_name_raw, value, xe::countof(file_name_raw));
    return true;
  }
};
static_assert_size(XCONTENT_AGGREGATE_DATA, 312);

class ContentPackage {
 public:
  ContentPackage(KernelState* kernel_state, const std::string_view root_name,
                 const XCONTENT_DATA& data,
                 const std::filesystem::path& package_path);
  ~ContentPackage();

 private:
  KernelState* kernel_state_;
  std::string root_name_;
  std::string device_path_;
};

class ContentManager {
 public:
  ContentManager(KernelState* kernel_state,
                 const std::filesystem::path& root_path);
  ~ContentManager();

  std::vector<XCONTENT_DATA> ListContent(uint32_t device_id,
                                         uint32_t content_type);

  std::unique_ptr<ContentPackage> ResolvePackage(
      const std::string_view root_name, const XCONTENT_DATA& data);

  bool ContentExists(const XCONTENT_DATA& data);
  X_RESULT CreateContent(const std::string_view root_name,
                         const XCONTENT_DATA& data);
  X_RESULT OpenContent(const std::string_view root_name,
                       const XCONTENT_DATA& data);
  X_RESULT CloseContent(const std::string_view root_name);
  X_RESULT GetContentThumbnail(const XCONTENT_DATA& data,
                               std::vector<uint8_t>* buffer);
  X_RESULT SetContentThumbnail(const XCONTENT_DATA& data,
                               std::vector<uint8_t> buffer);
  X_RESULT DeleteContent(const XCONTENT_DATA& data);
  std::filesystem::path ResolveGameUserContentPath();

 private:
  std::filesystem::path ResolvePackageRoot(uint32_t content_type);
  std::filesystem::path ResolvePackagePath(const XCONTENT_DATA& data);

  KernelState* kernel_state_;
  std::filesystem::path root_path_;

  // TODO(benvanik): remove use of global lock, it's bad here!
  xe::global_critical_region global_critical_region_;
  std::unordered_map<string_key, ContentPackage*> open_packages_;
};

}  // namespace xam
}  // namespace kernel
}  // namespace xe

#endif  // XENIA_KERNEL_XAM_CONTENT_MANAGER_H_
