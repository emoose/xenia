/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2014 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/vfs/devices/stfs_container_file.h"

#include <algorithm>
#include <cmath>

#include "xenia/base/math.h"
#include "xenia/vfs/devices/stfs_container_device.h"
#include "xenia/vfs/devices/stfs_container_entry.h"

namespace xe {
namespace vfs {

StfsContainerFile::StfsContainerFile(uint32_t file_access,
                                     StfsContainerEntry* entry)
    : File(file_access, entry), entry_(entry) {}

StfsContainerFile::~StfsContainerFile() = default;

void StfsContainerFile::Destroy() {
  // Flush the STFS device whenever a file is closed:
  if (entry_->is_device_closed()) {
    return;
  }
  auto device = reinterpret_cast<StfsContainerDevice*>(entry_->device());
  device->STFSFlush();
  delete this;
}

X_STATUS StfsContainerFile::ReadSync(void* buffer, size_t buffer_length,
                                     size_t byte_offset,
                                     size_t* out_bytes_read) {
  if (entry_->is_device_closed()) {
    return X_STATUS_INVALID_HANDLE;
  }
  if (byte_offset >= entry_->size()) {
    return X_STATUS_END_OF_FILE;
  }

  size_t src_offset = 0;
  uint8_t* p = reinterpret_cast<uint8_t*>(buffer);
  size_t remaining_length =
      std::min(buffer_length, entry_->size() - byte_offset);

  auto block_list = entry_->block_list();
  for (auto& record : block_list) {
    if (src_offset + record.length <= byte_offset) {
      // Doesn't begin in this region. Skip it.
      src_offset += record.length;
      continue;
    }

    size_t read_offset =
        (byte_offset > src_offset) ? byte_offset - src_offset : 0;
    size_t read_length =
        std::min(record.length - read_offset, remaining_length);

    auto& file = entry_->files()->at(record.file);
    xe::filesystem::Seek(file, record.offset + read_offset, SEEK_SET);
    auto num_read = fread(p, 1, read_length, file);

    *out_bytes_read += num_read;
    p += num_read;
    src_offset += record.length;
    remaining_length -= read_length;
    if (remaining_length == 0) {
      break;
    }
  }

  return X_STATUS_SUCCESS;
}

X_STATUS StfsContainerFile::WriteSync(const void* buffer, size_t buffer_length,
                                      size_t byte_offset,
                                      size_t* out_bytes_written) {
  if (entry_->is_device_closed()) {
    return X_STATUS_INVALID_HANDLE;
  }

  if (!(file_access_ &
        (FileAccess::kFileWriteData | FileAccess::kFileAppendData)) ||
      entry_->is_read_only()) {
    return X_STATUS_ACCESS_DENIED;
  }

  // Make sure we have enough STFS blocks allocated for this
  if (byte_offset + buffer_length > entry_->size()) {
    if (!entry_->set_length(uint32_t(byte_offset + buffer_length))) {
      return X_STATUS_ACCESS_DENIED;  // TODO: use a better error code here...
    }
  }

  size_t dst_offset = 0;
  auto p = reinterpret_cast<const uint8_t*>(buffer);
  size_t remaining_length =
      std::min(buffer_length, entry_->size() - byte_offset);
  *out_bytes_written = remaining_length;

  auto block_list = entry_->block_list();
  for (auto& record : block_list) {
    if (dst_offset + record.length <= byte_offset) {
      // Doesn't begin in this region. Skip it.
      dst_offset += record.length;
      continue;
    }

    size_t write_offset =
        (byte_offset > dst_offset) ? byte_offset - dst_offset : 0;
    size_t write_length =
        std::min(record.length - write_offset, remaining_length);

    auto file = entry_->files()->at(record.file);
    xe::filesystem::Seek(file, record.offset + write_offset, SEEK_SET);
    fwrite(p, 1, write_length, file);

    p += write_length;
    dst_offset += record.length;
    remaining_length -= write_length;
    if (remaining_length == 0) {
      break;
    }
  }

  // Mark entry as dirty so its hash chain will be rehashed
  // (seems like the most efficient way, instead of needing STFSBlockMarkDirty
  // to check STFSBlockIsMarkedDirty on every block written...)
  entry_->mark_dirty();

  return X_STATUS_SUCCESS;
}

X_STATUS StfsContainerFile::SetLength(size_t length) {
  if (entry_->is_device_closed()) {
    return X_STATUS_INVALID_HANDLE;
  }

  if (!(file_access_ & FileAccess::kFileWriteData) || entry_->is_read_only()) {
    return X_STATUS_ACCESS_DENIED;
  }

  if (entry_->set_length(uint32_t(length))) {
    return X_STATUS_SUCCESS;
  } else {
    return X_STATUS_END_OF_FILE;
  }
}

}  // namespace vfs
}  // namespace xe