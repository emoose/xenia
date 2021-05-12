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
  be<XContentType> content_type;
  union {
    // this should be be<uint16_t>, but that stops copy constructor being
    // generated...
    uint16_t uint[128];
    char16_t chars[128];
  } display_name_raw;
  char file_name_raw[42];
  uint8_t padding[2];

  bool operator==(const XCONTENT_DATA& other) const {
    // Package is located via device_id/content_type/file_name, so only need to
    // compare those
    return device_id == other.device_id && content_type == other.content_type &&
           file_name() == other.file_name();
  }

  std::u16string display_name() const {
    return load_and_swap<std::u16string>(display_name_raw.uint);
  }

  std::string file_name() const {
    return load_and_swap<std::string>(file_name_raw);
  }

  void set_display_name(const std::u16string_view value) {
    // Some games (eg Goldeneye XBLA) require multiple null-terminators for it
    // to read the string properly, blanking the array should take care of that

    std::fill_n(display_name_raw.chars, countof(display_name_raw.chars), 0);
    string_util::copy_and_swap_truncating(display_name_raw.chars, value,
                                          countof(display_name_raw.chars));
  }

  void set_file_name(const std::string_view value) {
    std::fill_n(file_name_raw, countof(file_name_raw), 0);
    string_util::copy_maybe_truncating<string_util::Safety::IKnowWhatIAmDoing>(
        file_name_raw, value, xe::countof(file_name_raw));
  }
};
static_assert_size(XCONTENT_DATA, 308);

struct XCONTENT_AGGREGATE_DATA {
  be<uint32_t> device_id;
  be<XContentType> content_type;
  union {
    // this should be be<uint16_t>, but that stops copy constructor being
    // generated...
    uint16_t uint[128];
    char16_t chars[128];
  } display_name_raw;
  char file_name_raw[42];
  uint8_t padding[2];
  be<uint32_t> title_id;

  bool operator==(const XCONTENT_AGGREGATE_DATA& other) const {
    // Package is located via device_id/title_id/content_type/file_name, so only
    // need to compare those
    return device_id == other.device_id && title_id == other.title_id &&
           content_type == other.content_type &&
           file_name() == other.file_name();
  }

  std::u16string display_name() const {
    return load_and_swap<std::u16string>(display_name_raw.uint);
  }

  std::string file_name() const {
    return load_and_swap<std::string>(file_name_raw);
  }

  void set_display_name(const std::u16string_view value) {
    // Some games (eg Goldeneye XBLA) require multiple null-terminators for it
    // to read the string properly, blanking the array should take care of that

    std::fill_n(display_name_raw.chars, countof(display_name_raw.chars), 0);
    string_util::copy_and_swap_truncating(display_name_raw.chars, value,
                                          countof(display_name_raw.chars));
  }

  void set_file_name(const std::string_view value) {
    std::fill_n(file_name_raw, countof(file_name_raw), 0);
    string_util::copy_maybe_truncating<string_util::Safety::IKnowWhatIAmDoing>(
        file_name_raw, value, xe::countof(file_name_raw));
  }
};
static_assert_size(XCONTENT_AGGREGATE_DATA, 312);

class ContentPackage {
 public:
  ContentPackage(KernelState* kernel_state, const std::string_view root_name,
                 const XCONTENT_DATA& data,
                 const std::filesystem::path& package_path,
                 bool read_only = false, bool create = false);
  ~ContentPackage();

  const XCONTENT_DATA& GetPackageContentData() const { return content_data_; }

  void SetThumbnail(const std::vector<uint8_t>& data);
  void GetThumbnail(std::vector<uint8_t>* data);

 private:
  KernelState* kernel_state_;
  std::string root_name_;
  std::string device_path_;
  XCONTENT_DATA content_data_;
};

class ContentManager {
 public:
  ContentManager(KernelState* kernel_state,
                 const std::filesystem::path& root_path);
  ~ContentManager();

  std::vector<XCONTENT_DATA> ListContent(uint32_t device_id,
                                         XContentType content_type);

  std::unique_ptr<ContentPackage> ResolvePackage(
      const std::string_view root_name, const XCONTENT_DATA& data,
      bool read_only = false, bool create = false);

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
  bool IsContentOpen(const XCONTENT_DATA& data) const;

  void SetTitleIdOverride(uint32_t title_id) { title_id_override_ = title_id; }

 private:
  uint32_t title_id() const;

  std::filesystem::path ResolvePackageRoot(XContentType content_type);
  std::filesystem::path ResolvePackagePath(const XCONTENT_DATA& data);

  KernelState* kernel_state_;
  std::filesystem::path root_path_;

  // TODO(benvanik): remove use of global lock, it's bad here!
  xe::global_critical_region global_critical_region_;
  std::unordered_map<string_key, ContentPackage*> open_packages_;

  // For allowing games to request content for other titles
  // or loading content before executable module is fully ready (eg. title
  // update packages)
  uint32_t title_id_override_ = 0;
};

}  // namespace xam
}  // namespace kernel
}  // namespace xe

#endif  // XENIA_KERNEL_XAM_CONTENT_MANAGER_H_
