/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_VFS_DEVICES_STFS_CONTAINER_DEVICE_H_
#define XENIA_VFS_DEVICES_STFS_CONTAINER_DEVICE_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_map>

#include "xenia/base/math.h"
#include "xenia/base/string_util.h"
#include "xenia/kernel/util/xex2_info.h"
#include "xenia/vfs/device.h"
#include "xenia/vfs/devices/stfs_xbox.h"

namespace xe {
namespace vfs {

// https://free60project.github.io/wiki/STFS.html

class StfsContainerEntry;

class StfsContainerDevice : public Device {
 public:
  const static uint32_t kBlockSize = 0x1000;

  StfsContainerDevice(const std::string_view mount_path,
                      const std::filesystem::path& host_path,
                      bool read_only = false, bool create = false);
  ~StfsContainerDevice() override;

  bool Initialize() override;

  bool is_read_only() const override {
    return !allow_writing_ ||
           header_.metadata.volume_type != XContentVolumeType::kStfs ||
           header_.metadata.volume_descriptor.stfs.flags.bits.read_only_format;
  }

  void Dump(StringBuffer* string_buffer) override;
  Entry* ResolvePath(const std::string_view path) override;

  const std::string& name() const override { return name_; }
  uint32_t attributes() const override { return 0; }
  uint32_t component_name_max_length() const override { return 40; }

  uint32_t total_allocation_units() const override {
    if (header_.metadata.volume_type == XContentVolumeType::kStfs) {
      return header_.metadata.volume_descriptor.stfs.total_block_count;
    }

    return uint32_t(data_size() / sectors_per_allocation_unit() /
                    bytes_per_sector());
  }
  uint32_t available_allocation_units() const override {
    if (!is_read_only()) {
      auto& descriptor = header_.metadata.volume_descriptor.stfs;
      return kBlocksPerHashLevel[2] -
             (descriptor.total_block_count - descriptor.free_block_count);
    }
    return 0;
  }
  uint32_t sectors_per_allocation_unit() const override { return 8; }
  uint32_t bytes_per_sector() const override { return 0x200; }

  size_t data_size() const {
    if (header_.header.header_size) {
      if (header_.metadata.volume_type == XContentVolumeType::kStfs) {
        return header_.metadata.volume_descriptor.stfs.total_block_count *
               kBlockSize;
      }
      return files_total_size_ -
             xe::round_up(header_.header.header_size, kBlockSize);
    }
    return files_total_size_ - sizeof(StfsHeader);
  }

  static uint32_t bytes_to_stfs_blocks(size_t num_bytes) {
    // xe::round_up doesn't handle 0 how we need it to, so:
    return uint32_t((num_bytes + kBlockSize - 1) / kBlockSize);
  }

  uint32_t STFSMaxHashLevel() const {
    if (header_.metadata.volume_descriptor.stfs.total_block_count <=
        kBlocksPerHashLevel[0]) {
      return 0;
    }
    if (header_.metadata.volume_descriptor.stfs.total_block_count <=
        kBlocksPerHashLevel[1]) {
      return 1;
    }
    return 2;
  }

  StfsHeader& header() { return header_; }

  // Writes updated headers & hash-tables to the file
  bool STFSFlush();

 protected:
  friend class StfsContainerEntry;
  void STFSBlockMarkDirty(uint32_t block_num);
  bool STFSBlockIsMarkedDirty(uint32_t block_num) const;

  uint32_t STFSBlockAllocate();
  void STFSBlockFree(uint32_t block_num);

  // Resets hash tables to an empty state
  bool STFSReset();

  std::vector<uint32_t> STFSGetDataBlockChain(uint32_t block_num,
                                              uint32_t max_count = 0xFFFFFF);
  void STFSSetDataBlockChain(const std::vector<uint32_t>& chain);

  std::vector<uint32_t> STFSResizeDataBlockChain(uint32_t start_block,
                                                 uint32_t num_blocks);

 private:
  const uint32_t kBlocksPerHashLevel[3] = {170, 28900, 4913000};
  const uint32_t kEndOfChain = 0xFFFFFF;
  const uint32_t kEntriesPerDirectoryBlock =
      kBlockSize / sizeof(StfsDirectoryEntry);

  FILE* main_file() { return files_.at(0); }

  enum class Error {
    kSuccess = 0,
    kErrorOutOfMemory = -1,
    kErrorReadError = -10,
    kErrorFileMismatch = -30,
    kErrorDamagedFile = -31,
    kErrorTooSmall = -32,
  };

  enum class SvodLayoutType {
    kUnknown = 0x0,
    kEnhancedGDF = 0x1,
    kXSF = 0x2,
    kSingleFile = 0x4,
  };

  XContentPackageType ReadMagic(const std::filesystem::path& path);
  bool ResolveFromFolder(const std::filesystem::path& path);

  Error OpenFiles();
  void CloseFiles();

  Error ReadHeaderAndVerify(FILE* header_file);

  Error ReadSVOD();
  Error ReadEntrySVOD(uint32_t sector, uint32_t ordinal,
                      StfsContainerEntry* parent);
  void BlockToOffsetSVOD(size_t sector, size_t* address, size_t* file_index);

  bool STFSDirectoryRead();
  void STFSDirectoryWrite();

  uint64_t STFSDataBlockToOffset(uint32_t block_num) const;
  uint32_t STFSDataBlockToHashBlockNum(uint32_t block_num,
                                       uint32_t hash_level) const;
  uint64_t STFSDataBlockToHashBlockOffset(uint32_t block_num,
                                          uint32_t hash_level) const;

  StfsHashTable& STFSGetHashTable(uint32_t block_num, uint32_t hash_level,
                                  bool use_secondary_block = false,
                                  uint8_t* hash_in_out = nullptr,
                                  bool* is_table_invalid = nullptr);

  StfsHashEntry& STFSGetHashEntry(uint32_t block_num, uint32_t hash_level,
                                  bool use_secondary_block = false,
                                  uint8_t* hash_in_out = nullptr);

  // DataHash functions handle secondary block & hash checking for us
  StfsHashTable& STFSGetDataHashTable(uint32_t block_num,
                                      bool* is_table_invalid);
  StfsHashEntry STFSGetDataHashEntry(uint32_t block_num);
  void STFSSetDataHashEntry(uint32_t block_num,
                            const StfsHashEntry& hash_entry);

  std::string name_;
  std::filesystem::path host_path_;
  bool allow_writing_ = false;
  bool allow_creating_ = false;

  std::map<size_t, FILE*> files_;
  size_t files_total_size_;
  std::unique_ptr<Entry> root_entry_;

  size_t svod_base_offset_;
  SvodLayoutType svod_layout_;

  StfsHeader header_;

  std::unordered_map<uint64_t, StfsHashTable> hash_tables_;
  std::vector<uint32_t> dirty_blocks_;
  std::vector<uint64_t> invalid_tables_;

  uint32_t blocks_per_hash_table_;
  uint32_t block_step_[2];
};

}  // namespace vfs
}  // namespace xe

#endif  // XENIA_VFS_DEVICES_STFS_CONTAINER_DEVICE_H_
