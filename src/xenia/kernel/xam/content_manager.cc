/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2015 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/kernel/xam/content_manager.h"

#include <string>

#include "xenia/base/filesystem.h"
#include "xenia/base/string.h"
#include "xenia/emulator.h"
#include "xenia/kernel/kernel_state.h"
#include "xenia/kernel/user_module.h"
#include "xenia/kernel/xam/content_package.h"
#include "xenia/kernel/xobject.h"
#include "xenia/vfs/devices/host_path_device.h"
#include "xenia/vfs/devices/stfs_container_device.h"

namespace xe {
namespace kernel {
namespace xam {

constexpr const wchar_t* const ContentManager::kStfsHeadersExtension;

static const wchar_t* kGameUserContentDirName = L"profile";

ContentManager::ContentManager(KernelState* kernel_state,
                               std::wstring root_path)
    : kernel_state_(kernel_state), root_path_(std::move(root_path)) {}

ContentManager::~ContentManager() = default;

std::wstring ContentManager::ResolvePackageRoot(uint32_t content_type) {
  wchar_t title_id[9] = L"00000000";
  std::swprintf(title_id, 9, L"%.8X", kernel_state_->title_id());

  std::wstring type_name;
  switch (content_type) {
    case 1:
      // Save games.
      type_name = L"00000001";
      break;
    case 2:
      // DLC from the marketplace.
      type_name = L"00000002";
      break;
    case 3:
      // Publisher content?
      type_name = L"00000003";
      break;
    case 0x000D0000:
      // ???
      type_name = L"000D0000";
      break;
    default:
      assert_unhandled_case(data.content_type);
      return nullptr;
  }

  // Package root path:
  // content_root/title_id/type_name/
  auto package_root =
      xe::join_paths(root_path_, xe::join_paths(title_id, type_name));
  return package_root + xe::kWPathSeparator;
}

std::wstring ContentManager::ResolvePackagePath(const XCONTENT_DATA& data) {
  // Content path:
  // content_root/title_id/type_name/data_file_name/
  auto package_root = ResolvePackageRoot(data.content_type);
  auto package_path =
      xe::join_paths(package_root, xe::to_wstring(data.file_name));
  return package_path;
}

std::vector<XCONTENT_DATA> ContentManager::ListContent(uint32_t device_id,
                                                       uint32_t content_type) {
  std::vector<XCONTENT_DATA> result;

  // Search path:
  // content_root/title_id/type_name/*
  auto package_root = ResolvePackageRoot(content_type);
  auto file_infos = xe::filesystem::ListFiles(package_root);
  for (const auto& file_info : file_infos) {
    XCONTENT_DATA content_data;
    content_data.device_id = device_id;
    content_data.content_type = content_type;
    content_data.display_name = file_info.name;
    content_data.file_name = xe::to_string(file_info.name);

    auto headers_path = file_info.path + file_info.name;
    if (file_info.type == xe::filesystem::FileInfo::Type::kDirectory) {
      headers_path = headers_path + ContentManager::kStfsHeadersExtension;
    }

    if (xe::filesystem::PathExists(headers_path)) {
      // File is either package or directory that has .headers file

      if (file_info.type != xe::filesystem::FileInfo::Type::kDirectory) {
        // Not a directory so must be a package, verify size to make sure
        if (file_info.total_size <= sizeof(vfs::StfsHeader)) {
          continue;  // Invalid package (maybe .headers.bin)
        }
      }

      auto map = MappedMemory::Open(headers_path, MappedMemory::Mode::kRead, 0,
                                    sizeof(vfs::StfsHeader));
      if (map) {
        auto* header = (vfs::StfsHeader*)map->data();

        content_data.content_type =
            static_cast<uint32_t>(header->metadata.content_type);
        content_data.display_name = header->metadata.get_display_name(
            kernel_state_->emulator()->game_language());

        map->Close();
      }
    }

    result.emplace_back(std::move(content_data));
  }

  return result;
}

ContentPackage* ContentManager::ResolvePackage(const XCONTENT_DATA& data) {
  auto package_path = ResolvePackagePath(data);
  if (!xe::filesystem::PathExists(package_path)) {
    return nullptr;
  }

  auto global_lock = global_critical_region_.Acquire();

  for (auto package : open_packages_) {
    if (package->package_path() == package_path) {
      return package;
    }
  }

  std::unique_ptr<ContentPackage> package;

  // Open as FolderContentPackage if the package is a folder or doesn't exist
  if (xe::filesystem::IsFolder(package_path) ||
      !xe::filesystem::PathExists(package_path)) {
    package = std::make_unique<FolderContentPackage>(kernel_state_, data,
                                                     package_path);
  } else {
    package =
        std::make_unique<StfsContentPackage>(kernel_state_, data, package_path);
  }

  return package.release();
}

bool ContentManager::ContentExists(const XCONTENT_DATA& data) {
  auto path = ResolvePackagePath(data);
  return xe::filesystem::PathExists(path);
}

X_RESULT ContentManager::CreateContent(std::string root_name,
                                       const XCONTENT_DATA& data,
                                       uint32_t flags) {
  auto global_lock = global_critical_region_.Acquire();

  auto package_path = ResolvePackagePath(data);
  if (xe::filesystem::PathExists(package_path)) {
    // Exists, must not!
    return X_ERROR_ALREADY_EXISTS;
  }

  for (auto package : open_packages_) {
    if (package->package_path() == package_path) {
      return X_ERROR_ALREADY_EXISTS;
    }
  }

  if (!xe::filesystem::CreateFolder(package_path)) {
    return X_ERROR_ACCESS_DENIED;
  }

  auto package = ResolvePackage(data);
  if (!package) {
    return X_ERROR_FUNCTION_FAILED;  // Failed to create directory?
  }

  // Create .headers.bin file
  auto headers_path = package_path + ContentManager::kStfsHeadersExtension;
  auto* file = xe::filesystem::OpenFile(headers_path, "wb");
  if (!file) {
    return X_ERROR_FUNCTION_FAILED;  // failed to create headers file :(
  }

  // Setup package headers!
  {
    // TODO: set publisher from XDBF info (but most saves don't have this set?)

    // Set header defaults...
    vfs::StfsHeader* header = new vfs::StfsHeader();
    header->set_defaults();

    // Set metadata flags...
    if (flags & XCONTENTFLAG_NODEVICE_TRANSFER) {
      header->metadata.flags.bits.device_transfer = false;
    }
    if ((flags & XCONTENTFLAG_ALLOWPROFILE_TRANSFER) &&
        !(flags & XCONTENTFLAG_NOPROFILE_TRANSFER)) {
      header->metadata.flags.bits.profile_transfer = true;
    }
    if (flags & XCONTENTFLAG_MOVEONLY_TRANSFER) {
      header->metadata.flags.bits.move_only_transfer = true;
    }

    // Try copying execution info from XEX opt headers
    auto exe_module = kernel_state_->GetExecutableModule();
    if (exe_module) {
      xex2_opt_execution_info* exec_info = 0;
      exe_module->GetOptHeader(XEX_HEADER_EXECUTION_INFO, &exec_info);
      if (exec_info) {
        memcpy(&header->metadata.execution_info, exec_info,
               sizeof(xex2_opt_execution_info));
      }
    }

    // Copy game title in the games default language
    // TODO: should this get the title in the users chosen lang instead?
    header->metadata.set_title_name(kernel_state_->emulator()->game_title());

    // Set package profile id (should be the offline XUID, 0xE0....)
    header->metadata.profile_id = kernel_state_->user_profile()->xuid();

    // Now copy data from XCONTENT_DATA into package headers
    header->metadata.content_type = (xe::vfs::XContentType)data.content_type;
    header->metadata.set_display_name(
        data.display_name, kernel_state_->emulator()->game_language());

    fwrite(header, sizeof(vfs::StfsHeader), 1, file);
    fclose(file);
    delete header;
  }

  if (!package->Mount(root_name)) {
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }

  open_packages_.push_back(package);

  return X_ERROR_SUCCESS;
}

X_RESULT ContentManager::OpenContent(std::string root_name,
                                     const XCONTENT_DATA& data) {
  auto global_lock = global_critical_region_.Acquire();

  auto package = ResolvePackage(data);
  if (!package) {
    return X_ERROR_FILE_NOT_FOUND;
  }

  if (!package->Mount(root_name)) {
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }

  open_packages_.push_back(package);

  return X_ERROR_SUCCESS;
}

X_RESULT ContentManager::CloseContent(std::string root_name) {
  auto global_lock = global_critical_region_.Acquire();

  for (auto it = open_packages_.begin(); it != open_packages_.end(); ++it) {
    auto& root_names = (*it)->root_names();
    auto root = std::find(root_names.begin(), root_names.end(), root_name);
    if (root != root_names.end()) {
      if ((*it)->Unmount(root_name)) {
        delete *it;
        open_packages_.erase(it);
      }

      return X_ERROR_SUCCESS;
    }
  }

  return X_ERROR_FILE_NOT_FOUND;
}

X_RESULT ContentManager::GetContentThumbnail(const XCONTENT_DATA& data,
                                             std::vector<uint8_t>* buffer) {
  auto global_lock = global_critical_region_.Acquire();

  auto package = ResolvePackage(data);
  if (!package) {
    return X_ERROR_FILE_NOT_FOUND;
  }

  return package->GetThumbnail(buffer);
}

X_RESULT ContentManager::SetContentThumbnail(const XCONTENT_DATA& data,
                                             std::vector<uint8_t> buffer) {
  auto global_lock = global_critical_region_.Acquire();

  auto package = ResolvePackage(data);
  if (!package) {
    return X_ERROR_FILE_NOT_FOUND;
  }

  return package->SetThumbnail(buffer);
}

X_RESULT ContentManager::DeleteContent(const XCONTENT_DATA& data) {
  auto global_lock = global_critical_region_.Acquire();

  auto package = ResolvePackage(data);
  if (!package) {
    return X_ERROR_FILE_NOT_FOUND;
  }

  auto result = package->Delete();
  if (XSUCCEEDED(result)) {
    auto it = std::find(open_packages_.begin(), open_packages_.end(), package);
    if (it != open_packages_.end()) {
      open_packages_.erase(it);
    }

    delete package;
  }

  return result;
}

std::wstring ContentManager::ResolveGameUserContentPath() {
  wchar_t title_id[9] = L"00000000";
  std::swprintf(title_id, 9, L"%.8X", kernel_state_->title_id());
  auto user_name = xe::to_wstring(kernel_state_->user_profile()->name());

  // Per-game per-profile data location:
  // content_root/title_id/profile/user_name
  auto package_root = xe::join_paths(
      root_path_,
      xe::join_paths(title_id,
                     xe::join_paths(kGameUserContentDirName, user_name)));
  return package_root + xe::kWPathSeparator;
}

}  // namespace xam
}  // namespace kernel
}  // namespace xe
