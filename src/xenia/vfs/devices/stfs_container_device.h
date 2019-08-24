/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2014 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_VFS_DEVICES_STFS_CONTAINER_DEVICE_H_
#define XENIA_VFS_DEVICES_STFS_CONTAINER_DEVICE_H_

#include <algorithm>
#include <map>
#include <memory>
#include <string>

#include "xenia/base/mapped_memory.h"
#include "xenia/kernel/util/xex2_info.h"
#include "xenia/vfs/device.h"

namespace xe {
namespace vfs {

// https://free60project.github.io/wiki/STFS.html

class StfsContainerEntry;

/* STFS */
struct StfsVolumeFlags {
  uint8_t read_only_format : 1;
  uint8_t root_active_index : 1;
  uint8_t directory_overallocated : 1;
  uint8_t directory_index_bounds_valid : 1;
};
static_assert_size(StfsVolumeFlags, 1);

struct StfsVolumeDescriptor {
  uint8_t descriptor_length;
  uint8_t version;
  union {
    StfsVolumeFlags bits;
    uint8_t as_byte;
  } Flags;
  uint8_t directory_block_count0;
  uint8_t directory_block_count1;
  uint8_t directory_block_num0;
  uint8_t directory_block_num1;
  uint8_t directory_block_num2;
  uint8_t root_hash[0x14];
  xe::be<uint32_t> NumberOfTotalBlocks;
  xe::be<uint32_t> NumberOfFreeBlocks;

  uint32_t directory_block_count() {
    return directory_block_count0 | (directory_block_count1 << 8);
  }

  uint32_t directory_block_num() {
    return directory_block_num0 | (directory_block_num1 << 8) |
           (directory_block_num2 << 16);
  }
};
static_assert_size(StfsVolumeDescriptor, 0x24);

/* SVOD */
struct SvodDeviceFeatures {
  uint8_t must_be_zero_for_future_usage : 6;
  uint8_t enhanced_gdf_layout : 1;
  uint8_t zero_for_downlevel_clients : 1;
};
static_assert_size(SvodDeviceFeatures, 1);

struct SvodDeviceDescriptor {
  uint8_t descriptor_length;
  uint8_t block_cache_element_count;
  uint8_t worker_thread_processor;
  uint8_t worker_thread_priority;
  uint8_t first_fragment_hash_entry[0x14];
  union {
    SvodDeviceFeatures bits;
    uint8_t as_byte;
  } Features;
  uint8_t num_data_blocks2;
  uint8_t num_data_blocks1;
  uint8_t num_data_blocks0;
  uint8_t start_data_block0;
  uint8_t start_data_block1;
  uint8_t start_data_block2;
  uint8_t reserved[5];

  uint32_t num_data_blocks() {
    return num_data_blocks0 | (num_data_blocks1 << 8) |
           (num_data_blocks2 << 16);
  }

  uint32_t start_data_block() {
    return start_data_block0 | (start_data_block1 << 8) |
           (start_data_block2 << 16);
  }
};
static_assert_size(SvodDeviceDescriptor, 0x24);

/* XContent */
struct XContentMediaData {
  uint8_t series_id[0x10];
  uint8_t season_id[0x10];
  xe::be<uint16_t> season_number;
  xe::be<uint16_t> episode_number;
};
static_assert_size(XContentMediaData, 0x24);

#pragma pack(push, 1)
struct XContentAvatarAssetData {
  xe::be<uint32_t> sub_category;
  xe::be<uint32_t> colorizable;
  uint8_t asset_id[0x10];
  uint8_t skeleton_version_mask;
  uint8_t reserved[0xB];
};
static_assert_size(XContentAvatarAssetData, 0x24);

struct XContentAttributes {
  uint8_t profile_transfer : 1;
  uint8_t device_transfer : 1;
  uint8_t move_only_transfer : 1;
  uint8_t kinect_enabled : 1;
  uint8_t disable_network_storage : 1;
  uint8_t deep_link_supported : 1;
  uint8_t reserved : 2;
};
static_assert_size(XContentAttributes, 1);

enum XContentType : uint32_t {
  kSavedGame = 0x00000001,
  kMarketplaceContent = 0x00000002,
  kPublisher = 0x00000003,
  kXbox360Title = 0x00001000,
  kIptvPauseBuffer = 0x00002000,
  kXNACommunity = 0x00003000,
  kInstalledGame = 0x00004000,
  kXboxTitle = 0x00005000,
  kSocialTitle = 0x00006000,
  kGamesOnDemand = 0x00007000,
  kSUStoragePack = 0x00008000,
  kAvatarItem = 0x00009000,
  kProfile = 0x00010000,
  kGamerPicture = 0x00020000,
  kTheme = 0x00030000,
  kCacheFile = 0x00040000,
  kStorageDownload = 0x00050000,
  kXboxSavedGame = 0x00060000,
  kXboxDownload = 0x00070000,
  kGameDemo = 0x00080000,
  kVideo = 0x00090000,
  kGameTitle = 0x000A0000,
  kInstaller = 0x000B0000,
  kGameTrailer = 0x000C0000,
  kArcadeTitle = 0x000D0000,
  kXNA = 0x000E0000,
  kLicenseStore = 0x000F0000,
  kMovie = 0x00100000,
  kTV = 0x00200000,
  kMusicVideo = 0x00300000,
  kGameVideo = 0x00400000,
  kPodcastVideo = 0x00500000,
  kViralVideo = 0x00600000,
  kCommunityGame = 0x02000000,
};

enum class XContentVolumeType : uint32_t {
  kStfs = 0,
  kSvod = 1,
};

struct XContentMetadata {
  xe::be<XContentType> content_type;
  xe::be<uint32_t> metadata_version;
  xe::be<uint64_t> content_size;
  xex2_opt_execution_info execution_info;
  uint8_t console_id[5];
  xe::be<uint64_t> profile_id;
  union {
    StfsVolumeDescriptor stfs_volume_descriptor;
    SvodDeviceDescriptor svod_volume_descriptor;
  };
  xe::be<uint32_t> data_file_count;
  xe::be<uint64_t> data_file_size;
  xe::be<XContentVolumeType> volume_type;
  xe::be<uint64_t> online_creator;
  xe::be<uint32_t> category;
  uint8_t reserved2[0x20];
  union {
    XContentMediaData media_data;
    XContentAvatarAssetData avatar_asset_data;
  };
  uint8_t device_id[0x14];
  uint16_t display_name[9][0x80];
  uint16_t description[9][0x80];
  uint16_t publisher[0x40];
  uint16_t title_name[0x40];
  union {
    XContentAttributes bits;
    uint8_t as_byte;
  } flags;
  xe::be<uint32_t> thumbnail_size;
  xe::be<uint32_t> title_thumbnail_size;
  uint8_t thumbnail[0x3D00];
  uint16_t display_name_ex[3][0x80];
  uint8_t title_thumbnail[0x3D00];
  uint16_t description_ex[3][0x80];

  std::wstring get_display_name(XLanguage locale) {
    uint32_t locale_id = (uint32_t)locale;
    locale_id--;

    wchar_t* str = 0;
    if (locale_id >= 0 && locale_id < 9) {
      str = (wchar_t*)display_name[locale_id];
    } else if (locale_id >= 9 && locale_id < 12 && metadata_version >= 2) {
      str = (wchar_t*)display_name_ex[locale_id - 9];
    }
    if (!str) {
      return L"";
    }

    std::vector<wchar_t> wstr;
    wstr.resize(wcslen(str) + 1);  // add 1 in case wcslen returns 0
    xe::copy_and_swap<wchar_t>((wchar_t*)wstr.data(), str, wcslen(str));

    return std::wstring(wstr.data());
  }
  std::wstring get_description(XLanguage locale) {
    uint32_t locale_id = (uint32_t)locale;
    locale_id--;

    wchar_t* str = 0;
    if (locale_id >= 0 && locale_id < 9) {
      str = (wchar_t*)display_name[locale_id];
    } else if (locale_id >= 9 && locale_id < 12 && metadata_version >= 2) {
      str = (wchar_t*)display_name_ex[locale_id - 9];
    }
    if (!str) {
      return L"";
    }

    std::vector<wchar_t> wstr;
    wstr.resize(wcslen(str) + 1);  // add 1 in case wcslen returns 0
    xe::copy_and_swap<wchar_t>(wstr.data(), str, wcslen(str));

    return std::wstring(wstr.data());
  }
  std::wstring get_publisher() {
    wchar_t* value = (wchar_t*)publisher;
    std::vector<wchar_t> wstr;
    wstr.resize(wcslen(value) + 1);  // add 1 in case wcslen returns 0
    xe::copy_and_swap<wchar_t>(wstr.data(), value, wcslen(value));

    return std::wstring(wstr.data());
  }
  std::wstring get_title_name() {
    wchar_t* value = (wchar_t*)title_name;
    std::vector<wchar_t> wstr;
    wstr.resize(wcslen(value) + 1);  // add 1 in case wcslen returns 0
    xe::copy_and_swap<wchar_t>(wstr.data(), value, wcslen(value));

    return std::wstring(wstr.data());
  }

  bool set_display_name(const std::wstring& value, XLanguage locale) {
    uint32_t locale_id = (uint32_t)locale;
    locale_id--;

    wchar_t* str = 0;
    if (locale_id >= 0 && locale_id < 9) {
      str = (wchar_t*)display_name[locale_id];
    } else if (locale_id >= 9 && locale_id < 12 && metadata_version >= 2) {
      str = (wchar_t*)display_name_ex[locale_id - 9];
    }
    if (!str) {
      return false;
    }

    xe::copy_and_swap<wchar_t>(str, value.c_str(),
                               std::min(value.length(), (size_t)128));
    return true;
  }
  bool set_description(const std::wstring& value, XLanguage locale) {
    uint32_t locale_id = (uint32_t)locale;
    locale_id--;

    wchar_t* str = 0;
    if (locale_id >= 0 && locale_id < 9) {
      str = (wchar_t*)description[locale_id];
    } else if (locale_id >= 9 && locale_id < 12 && metadata_version >= 2) {
      str = (wchar_t*)description_ex[locale_id - 9];
    }
    if (!str) {
      return false;
    }

    xe::copy_and_swap<wchar_t>(str, value.c_str(),
                               std::min(value.length(), (size_t)128));
    return true;
  }
  bool set_publisher(const std::wstring& value) {
    xe::copy_and_swap<wchar_t>((wchar_t*)publisher, value.c_str(),
                               std::min(value.length(), (size_t)128));
    return true;
  }
  bool set_title_name(const std::wstring& value) {
    xe::copy_and_swap<wchar_t>((wchar_t*)title_name, value.c_str(),
                               std::min(value.length(), (size_t)128));
    return true;
  }
};
static_assert_size(XContentMetadata, 0x93D6);
#pragma pack(pop)

struct XContentInstallerUpdate {
  xe::be<uint32_t> base_version;
  xe::be<uint32_t> new_version;
  uint8_t reserved[0x15E8];
};
static_assert_size(XContentInstallerUpdate, 0x15F0);

struct XOnlineContentResumeHeader {
  xe::be<uint32_t> resume_state;
  xe::be<uint32_t> current_file_index;
  xe::be<uint64_t> current_file_offset;
  xe::be<uint64_t> bytes_processed;
  xe::be<uint64_t> last_modified;
};
static_assert_size(XOnlineContentResumeHeader, 0x20);

struct XContentInstallerProgressCache {
  XOnlineContentResumeHeader resume_header;
  uint8_t cab_resume_data[0x15D0];
};
static_assert_size(XContentInstallerProgressCache, 0x15F0);

struct XContentLicense {
  xe::be<uint64_t> licensee_id;
  xe::be<uint32_t> license_bits;
  xe::be<uint32_t> license_flags;
};
static_assert_size(XContentLicense, 0x10);

enum XContentPackageType : uint32_t {
  kPackageTypeCon = 0x434F4E20,
  kPackageTypePirs = 0x50495253,
  kPackageTypeLive = 0x4C495645,
};

#pragma pack(push, 1)
struct XContentHeader {
  xe::be<XContentPackageType> magic;
  uint8_t signature[0x228];
  XContentLicense licenses[0x10];
  uint8_t content_id[0x14];
  xe::be<uint32_t> header_size;
};
static_assert_size(XContentHeader, 0x344);

struct XContentInstaller {
  xe::be<uint32_t> type;
  union {
    XContentInstallerUpdate update;
    XContentInstallerProgressCache install_progress_cache;
  } metadata;
};
static_assert_size(XContentInstaller, 0x15F4);

struct StfsHeader {
  XContentHeader header;
  XContentMetadata metadata;
  XContentInstaller installer;
  uint8_t padding[0x2F2];

  void set_defaults() {
    memset(this, 0, sizeof(StfsHeader));

    header.magic = xe::vfs::XContentPackageType::kPackageTypeCon;
    header.licenses[0].licensee_id =
        -1;  // X360 gamesaves seem to set licenses like this
    header.header_size = 0x971A;

    metadata.metadata_version = 2;
    memcpy(metadata.console_id, "XENIA", 5);

    // Saves always have this set? How do titles choose this?
    metadata.flags.bits.device_transfer = true;
  }
};
static_assert_size(StfsHeader, 0xB000);
#pragma pack(pop)

class StfsContainerDevice : public Device {
 public:
  StfsContainerDevice(const std::string& mount_path,
                      const std::wstring& local_path);
  ~StfsContainerDevice() override;

  bool Initialize() override;
  void Dump(StringBuffer* string_buffer) override;
  Entry* ResolvePath(const std::string& path) override;

  uint32_t total_allocation_units() const override {
    return uint32_t(mmap_total_size_ / sectors_per_allocation_unit() /
                    bytes_per_sector());
  }
  uint32_t available_allocation_units() const override { return 0; }
  uint32_t sectors_per_allocation_unit() const override { return 1; }
  uint32_t bytes_per_sector() const override { return 4 * 1024; }

  StfsHeader& header() { return header_; }

 private:
  enum class Error {
    kSuccess = 0,
    kErrorOutOfMemory = -1,
    kErrorReadError = -10,
    kErrorFileMismatch = -30,
    kErrorDamagedFile = -31,
  };

  enum class SvodLayoutType {
    kUnknown = 0x0,
    kEnhancedGDF = 0x1,
    kXSF = 0x2,
    kSingleFile = 0x4,
  };

  struct BlockHash {
    uint32_t next_block_index;
    uint32_t info;
  };

  const uint32_t kSTFSHashSpacing = 170;

  const char* ReadMagic(const std::wstring& path);
  bool ResolveFromFolder(const std::wstring& path);

  Error MapFiles();
  Error ReadHeaderAndVerify(const uint8_t* map_ptr);

  Error ReadSVOD();
  Error ReadEntrySVOD(uint32_t sector, uint32_t ordinal,
                      StfsContainerEntry* parent);
  void BlockToOffsetSVOD(size_t sector, size_t* address, size_t* file_index);

  Error ReadSTFS();
  size_t BlockToOffsetSTFS(uint64_t block);
  size_t BlockToOffsetSTFS_Old(uint64_t block);

  BlockHash GetBlockHash(const uint8_t* map_ptr, uint32_t block_index,
                         uint32_t table_offset);

  std::wstring local_path_;
  std::map<size_t, std::unique_ptr<MappedMemory>> mmap_;
  size_t mmap_total_size_ = 0;

  size_t base_offset_ = 0;
  size_t magic_offset_ = 0;
  std::unique_ptr<Entry> root_entry_;
  StfsHeader header_;
  SvodLayoutType svod_layout_ = SvodLayoutType::kUnknown;
  uint32_t table_size_shift_ = 0;
  bool use_old_algorithm_ = false;
};

}  // namespace vfs
}  // namespace xe

#endif  // XENIA_VFS_DEVICES_STFS_CONTAINER_DEVICE_H_
