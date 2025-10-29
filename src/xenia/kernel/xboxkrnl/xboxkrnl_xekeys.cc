/**
******************************************************************************
* Xenia : Xbox 360 Emulator Research Project                                 *
******************************************************************************
* Copyright 2015 Ben Vanik. All rights reserved.                             *
* Released under the BSD license - see LICENSE in the root for more details. *
******************************************************************************
*/

#include <algorithm>

#include "xenia/config.h"
#include "xenia/kernel/kernel_state.h"
#include "xenia/kernel/util/shim_utils.h"
#include "xenia/kernel/xboxkrnl/xboxkrnl_private.h"
#include "xenia/xbox.h"

namespace xe {
namespace kernel {
namespace xboxkrnl {

// xboxkrnl_crypt.cc

void XeCryptBnQw_SwapDwQwLeBe_entry(pointer_t<uint64_t> qw_inp,
                              pointer_t<uint64_t> qw_out, dword_t size);
dword_result_t XeCryptBnQwNeRsaPrvCrypt_entry(pointer_t<uint64_t> qw_a,
                                        pointer_t<uint64_t> qw_b,
                                        pointer_t<XECRYPT_RSA> rsa);
dword_result_t XeCryptBnQwNeRsaPubCrypt_entry(pointer_t<uint64_t> qw_a,
                                        pointer_t<uint64_t> qw_b,
                                        pointer_t<XECRYPT_RSA> rsa);

void XeCryptBnDwLePkcs1Format_entry(lpvoid_t hash, dword_t format,
                              lpvoid_t output_sig, dword_t output_sig_size);
dword_result_t XeCryptBnDwLePkcs1Verify_entry(lpvoid_t hash, lpvoid_t input_sig,
                                        dword_t input_sig_size);

void XeCryptHmacSha_entry(lpvoid_t key, dword_t key_size_in, lpvoid_t inp_1,
                    dword_t inp_1_size, lpvoid_t inp_2, dword_t inp_2_size,
                    lpvoid_t inp_3, dword_t inp_3_size, lpvoid_t out,
                    dword_t out_size);
void XeCryptAesKey_entry(pointer_t<XECRYPT_AES_STATE> state_ptr, lpvoid_t key);
void XeCryptAesEcb_entry(pointer_t<XECRYPT_AES_STATE> state_ptr, lpvoid_t inp_ptr,
                   lpvoid_t out_ptr, dword_t encrypt);
void XeCryptAesCbc_entry(pointer_t<XECRYPT_AES_STATE> state_ptr, lpvoid_t inp_ptr,
                   dword_t inp_size, lpvoid_t out_ptr, lpvoid_t feed_ptr,
                   dword_t encrypt);
void XeCryptRc4_entry(lpvoid_t key, dword_t key_size, lpvoid_t data, dword_t size);
void XeCryptRc4Key_entry(pointer_t<XECRYPT_RC4_STATE> rc4_ctx, lpvoid_t key,
                   dword_t key_size);
void XeCryptRc4Ecb_entry(pointer_t<XECRYPT_RC4_STATE> rc4_ctx, lpvoid_t data,
                   dword_t size);

// Offsets/sizes of each xekey
std::map<XeKey, std::tuple<uint32_t, uint32_t>> kXeKeyProperties = {
    {XeKey::MANUFACTURING_MODE, {0x8, 0x1}},
    {XeKey::ALTERNATE_KEY_VAULT, {0x9, 0x1}},
    {XeKey::RESTRICTED_PRIVILEGES_FLAGS, {0xA, 0x1}},
    {XeKey::RESERVED_BYTE3, {0xB, 0x1}},
    {XeKey::ODD_FEATURES, {0xC, 0x2}},
    {XeKey::ODD_AUTHTYPE, {0xE, 0x2}},
    {XeKey::RESTRICTED_HVEXT_LOADER, {0x10, 0x4}},
    {XeKey::POLICY_FLASH_SIZE, {0x14, 0x4}},
    {XeKey::POLICY_BUILTIN_USBMU_SIZE, {0x18, 0x4}},
    {XeKey::RESERVED_DWORD4, {0x1C, 0x4}},
    {XeKey::RESTRICTED_PRIVILEGES, {0x20, 0x8}},
    {XeKey::RESERVED_QWORD2, {0x28, 0x8}},
    {XeKey::RESERVED_QWORD3, {0x30, 0x8}},
    {XeKey::RESERVED_QWORD4, {0x38, 0x8}},
    {XeKey::RESERVED_KEY1, {0x40, 0x10}},
    {XeKey::RESERVED_KEY2, {0x50, 0x10}},
    {XeKey::RESERVED_KEY3, {0x60, 0x10}},
    {XeKey::RESERVED_KEY4, {0x70, 0x10}},
    {XeKey::RESERVED_RANDOM_KEY1, {0x80, 0x10}},
    {XeKey::RESERVED_RANDOM_KEY2, {0x90, 0x10}},
    {XeKey::CONSOLE_SERIAL_NUMBER, {0xA0, 0xC}},
    {XeKey::MOBO_SERIAL_NUMBER, {0xAC, 0xC}},
    {XeKey::GAME_REGION, {0xB8, 0x2}},
    // 6 bytes padding
    {XeKey::CONSOLE_OBFUSCATION_KEY, {0xC0, 0x10}},
    {XeKey::KEY_OBFUSCATION_KEY, {0xD0, 0x10}},
    {XeKey::ROAMABLE_OBFUSCATION_KEY, {0xE0, 0x10}},
    {XeKey::DVD_KEY, {0xF0, 0x10}},
    {XeKey::PRIMARY_ACTIVATION_KEY, {0x100, 0x18}},
    {XeKey::SECONDARY_ACTIVATION_KEY, {0x118, 0x10}},
    {XeKey::GLOBAL_DEVICE_2DES_KEY1, {0x128, 0x10}},
    {XeKey::GLOBAL_DEVICE_2DES_KEY2, {0x138, 0x10}},
    {XeKey::WIRELESS_CONTROLLER_MS_2DES_KEY1, {0x148, 0x10}},
    {XeKey::WIRELESS_CONTROLLER_MS_2DES_KEY2, {0x158, 0x10}},
    {XeKey::WIRED_WEBCAM_MS_2DES_KEY1, {0x168, 0x10}},
    {XeKey::WIRED_WEBCAM_MS_2DES_KEY2, {0x178, 0x10}},
    {XeKey::WIRED_CONTROLLER_MS_2DES_KEY1, {0x188, 0x10}},
    {XeKey::WIRED_CONTROLLER_MS_2DES_KEY2, {0x198, 0x10}},
    {XeKey::MEMORY_UNIT_MS_2DES_KEY1, {0x1A8, 0x10}},
    {XeKey::MEMORY_UNIT_MS_2DES_KEY2, {0x1B8, 0x10}},
    {XeKey::OTHER_XSM3_DEVICE_MS_2DES_KEY1, {0x1C8, 0x10}},
    {XeKey::OTHER_XSM3_DEVICE_MS_2DES_KEY2, {0x1D8, 0x10}},
    {XeKey::WIRELESS_CONTROLLER_3P_2DES_KEY1, {0x1E8, 0x10}},
    {XeKey::WIRELESS_CONTROLLER_3P_2DES_KEY2, {0x1F8, 0x10}},
    {XeKey::WIRED_WEBCAM_3P_2DES_KEY1, {0x208, 0x10}},
    {XeKey::WIRED_WEBCAM_3P_2DES_KEY2, {0x218, 0x10}},
    {XeKey::WIRED_CONTROLLER_3P_2DES_KEY1, {0x228, 0x10}},
    {XeKey::WIRED_CONTROLLER_3P_2DES_KEY2, {0x238, 0x10}},
    {XeKey::MEMORY_UNIT_3P_2DES_KEY1, {0x248, 0x10}},
    {XeKey::MEMORY_UNIT_3P_2DES_KEY2, {0x258, 0x10}},
    {XeKey::OTHER_XSM3_DEVICE_3P_2DES_KEY1, {0x268, 0x10}},
    {XeKey::OTHER_XSM3_DEVICE_3P_2DES_KEY2, {0x278, 0x10}},
    {XeKey::CONSOLE_PRIVATE_KEY, {0x288, 0x1D0}},
    {XeKey::XEIKA_PRIVATE_KEY, {0x458, 0x390}},
    {XeKey::CARDEA_PRIVATE_KEY, {0x7E8, 0x1D0}},
    {XeKey::CONSOLE_CERTIFICATE, {0x9B8, 0x1A8}},
    {XeKey::XEIKA_CERTIFICATE, {0xB60, 0x1288}},
    {XeKey::SPECIAL_KEY_VAULT_SIGNATURE, {0x1DE8, 0x100}},
    {XeKey::CARDEA_CERTIFICATE, {0x1EE8, 0x2108}},
};

// Key 0x19 doesn't seem stored in the KV, must be generated by HV at runtime?
// We'll just copy in the known values for 0x19 into the KV after it's loaded
uint8_t kRoamableObfuscationKey_Retail[0x10] = {
    0xE1, 0xBC, 0x15, 0x9C, 0x73, 0xB1, 0xEA, 0xE9,
    0xAB, 0x31, 0x70, 0xF3, 0xAD, 0x47, 0xEB, 0xF3};
uint8_t kRoamableObfuscationKey_Devkit[0x10] = {
    0xDA, 0xB6, 0x9A, 0xD9, 0x8E, 0x28, 0x76, 0x4F,
    0x97, 0x7E, 0xE2, 0x48, 0x7E, 0x4F, 0x3F, 0x68};

// TODO: kXeKeyVault could probably be a struct instead, so the funcs here could
// access data directly (but we'd still need the offset list above though for
// XeKeysGetKey etc)
std::vector<uint8_t> kXeKeyVault(0x3FF0);
bool kXeKeyVaultLoaded = false;

bool xeKeysIsKeySupported(XeKey key_idx) {
  return kXeKeyProperties.count(key_idx) > 0;
}

dword_result_t XeKeysGetConsoleType_entry();

uint8_t* xeKeysGetKeyPtrRaw(XeKey key_idx) {
  auto& key_info = kXeKeyProperties.at(key_idx);
  auto& key_offset = std::get<0>(key_info);

  return kXeKeyVault.data() + key_offset;
}

void xeKeysFixupKeyVault() {
  // Perform any setup on loaded KV

  auto* roamable_key = xeKeysGetKeyPtrRaw(XeKey::ROAMABLE_OBFUSCATION_KEY);
  if (XeKeysGetConsoleType_entry() == (uint32_t)XConsoleType::Retail)
    std::copy_n(kRoamableObfuscationKey_Retail, 0x10, roamable_key);
  else
    std::copy_n(kRoamableObfuscationKey_Devkit, 0x10, roamable_key);
}

template <class T>
T* xeKeysGetKeyPtr(XeKey key_idx) {
  if (key_idx == XeKey::ROAMABLE_OBFUSCATION_KEY) {
    // Make sure we've copied in correct ROAMABLE_OBFUSCATION_KEY
    xeKeysFixupKeyVault();
  }

  if (!xeKeysIsKeySupported(key_idx)) {
    return nullptr;
  }

  auto& key_info = kXeKeyProperties.at(key_idx);
  auto& key_offset = std::get<0>(key_info);

  return reinterpret_cast<T*>(kXeKeyVault.data() + key_offset);
}

bool xeKeysLoadKeyVault() {
  // TODO: allow user to specify KV path inside config?
  // & allow user to store KV inside xenia config folder?

  auto kv_path = config::GetConfigFolder() / "KV.bin";
  FILE* file = xe::filesystem::OpenFile(kv_path, "rb");
  if (!file) {
    // Try kv.bin next to exe
    file = xe::filesystem::OpenFile("KV.bin", "rb");
    if (!file) {
      XELOGW(
          "Failed to load keyvault from kv.bin file (path: {}) - most XeKeys "
          "functions will fail!",
          kv_path.string());
      return false;
    }
  }

  xe::filesystem::Seek(file, 0, SEEK_END);
  auto filesize = xe::filesystem::Tell(file);

  if (filesize >= 0x4000) {
    // Skip digest at start of 0x4000-sized KV
    xe::filesystem::Seek(file, 0x10, SEEK_SET);
    filesize -= 0x10;
  } else {
    xe::filesystem::Seek(file, 0, SEEK_SET);
  }

  kXeKeyVault.resize(filesize);
  fread(kXeKeyVault.data(), 1, filesize, file);
  fclose(file);

  kXeKeyVaultLoaded = true;
  xeKeysFixupKeyVault();

  return true;
}

dword_result_t XeKeysLoadKeyVault_entry(lpvoid_t r3) { return xeKeysLoadKeyVault(); }
DECLARE_XBOXKRNL_EXPORT1(XeKeysLoadKeyVault, kNone, kImplemented);

bool xeKeysGetKey(XeKey key_idx, uint8_t* output, uint32_t* output_size) {
  if (output_size) {
    *output_size = 0;
  }

  if (!xeKeysIsKeySupported(key_idx)) {
    return false;
  }

  if (kXeKeyVault.empty()) {
    return false;
  }

  auto& key_info = kXeKeyProperties.at(key_idx);
  auto& key_offset = std::get<0>(key_info);
  auto& key_size = std::get<1>(key_info);

  if (output_size) {
    *output_size = key_size;
  }

  if (!output) {
    return true;
  }

  std::copy_n(kXeKeyVault.data() + key_offset, key_size, output);

  return true;
}

dword_result_t XeKeysGetKey_entry(dword_t key_idx, lpvoid_t output,
                            pointer_t<uint32_t> output_size) {
  return xeKeysGetKey((XeKey)(uint32_t)key_idx, output, output_size);
}
DECLARE_XBOXKRNL_EXPORT1(XeKeysGetKey, kNone, kImplemented);

dword_result_t XeKeysGetConsoleCertificate_entry(
    pointer_t<X_XE_CONSOLE_CERTIFICATE> output) {
  if (!kXeKeyVaultLoaded) {
    XELOGE(
        "XeKeysGetConsoleCertificate called without keyvault loaded - will "
        "likely cause failures!");
  }
  xeKeysGetKey(XeKey::CONSOLE_CERTIFICATE, (uint8_t*)output.host_address(),
               nullptr);
  return 0;
}
DECLARE_XBOXKRNL_EXPORT1(XeKeysGetConsoleCertificate, kNone, kImplemented);

dword_result_t XeKeysGetConsoleType_entry() {
  if (!kXeKeyVaultLoaded) {
    XELOGE(
        "XeKeysGetConsoleType called without keyvault loaded - returning 2 "
        "(retail)");
    return 2;
  }

  auto* console_cert =
      xeKeysGetKeyPtr<X_XE_CONSOLE_CERTIFICATE>(XeKey::CONSOLE_CERTIFICATE);
  if (!console_cert) {
    return 0;
  }

  return (uint32_t)(XConsoleType)console_cert->console_type;
}
DECLARE_XBOXKRNL_EXPORT1(XeKeysGetConsoleType, kNone, kImplemented);

dword_result_t XeKeysGetConsoleID_entry(lpvoid_t raw_bytes, lpvoid_t hex_string) {
  if (!kXeKeyVaultLoaded) {
    XELOGE(
        "XeKeysGetConsoleID called without keyvault loaded - returning empty "
        "ID");
  }

  auto* console_cert =
      xeKeysGetKeyPtr<X_XE_CONSOLE_CERTIFICATE>(XeKey::CONSOLE_CERTIFICATE);
  if (!console_cert) {
    return 0;
  }

  if (raw_bytes) {
    std::copy_n(console_cert->console_id, 5,
                reinterpret_cast<uint8_t*>(raw_bytes.host_address()));
  }
  if (hex_string) {
    // TODO: check if this is correct!
    for (int i = 0; i < 5; i++) {
      auto res = fmt::format("{:02X}", console_cert->console_id[i]);
      strcpy(reinterpret_cast<char*>(&hex_string[i * 2]), res.c_str());
    }
    // hex_string seems to be 0xC bytes, so null-term the end of it
    hex_string[10] = '\0';
    hex_string[11] = '\0';
  }
  return 0;
}
DECLARE_XBOXKRNL_EXPORT1(XeKeysGetConsoleID, kNone, kImplemented);

dword_result_t XeKeysQwNeRsaPrvCrypt_entry(dword_t key_idx, pointer_t<uint64_t> input,
                                     pointer_t<uint64_t> output) {
  // returns BOOL
  if (!kXeKeyVaultLoaded) {
    XELOGE(
        "XeKeysQwNeRsaPrvCrypt({:X}) called without keyvault loaded - will "
        "likely cause failures!",
        (uint32_t)key_idx);
  }

  auto key = (XeKey)(uint32_t)key_idx;
  if (key != XeKey::CONSOLE_PRIVATE_KEY && key != XeKey::XEIKA_PRIVATE_KEY &&
      key != XeKey::CARDEA_PRIVATE_KEY)
    return 0;

  // Xeika key is larger than the others, and likely needs a different D/PrivExp
  // value for it (see kStaticPrivateExponent1024), so disallow it for now
  if (key == XeKey::XEIKA_PRIVATE_KEY) {
    return 0;
  }

  auto* key_ptr = xeKeysGetKeyPtr<XECRYPT_RSA>(key);
  return XeCryptBnQwNeRsaPrvCrypt_entry(input, output, key_ptr);
}
DECLARE_XBOXKRNL_EXPORT1(XeKeysQwNeRsaPrvCrypt, kNone, kImplemented);

// Signs the given hash with the loaded keyvaults private-key + console cert
dword_result_t XeKeysConsolePrivateKeySign_entry(
    lpvoid_t hash, pointer_t<X_XE_CONSOLE_SIGNATURE> output_cert_sig) {
  // returns BOOL
  if (!kXeKeyVaultLoaded) {
    XELOGE(
        "xeKeysConsolePrivateKeySign called without keyvault loaded - "
        "returning false");
    return false;
  }

  uint64_t sig_buf[0x10];

  XeCryptBnDwLePkcs1Format_entry((uint8_t*)hash, 0,
                           reinterpret_cast<uint8_t*>(sig_buf), 0x10 * 8);
  XeCryptBnQw_SwapDwQwLeBe_entry(sig_buf, sig_buf, 0x10);

  if (!XeKeysQwNeRsaPrvCrypt_entry((uint32_t)XeKey::CONSOLE_PRIVATE_KEY, sig_buf,
                             sig_buf)) {
    return false;
  }

  XeCryptBnQw_SwapDwQwLeBe_entry(
      sig_buf, reinterpret_cast<uint64_t*>(output_cert_sig->signature), 0x10);

  // Copy in console cert
  XeKeysGetConsoleCertificate_entry(&output_cert_sig->console_certificate);
  return true;
}
DECLARE_XBOXKRNL_EXPORT1(XeKeysConsolePrivateKeySign, kNone, kImplemented);

bool xeKeysPkcs1Verify(const uint8_t* hash, const uint64_t* input_sig,
                       const XECRYPT_RSA* key) {
  uint64_t temp_sig[0x10];

  uint32_t key_digits = key->size;
  uint32_t modulus_size = key_digits * 8;
  if (modulus_size > 0x200) {
    return false;
  }

  xe::copy_and_swap<uint64_t>(temp_sig, input_sig, 0x10);
  if (!XeCryptBnQwNeRsaPubCrypt_entry(temp_sig, temp_sig, (XECRYPT_RSA*)key)) {
    return false;
  }

  xe::copy_and_swap<uint64_t>(temp_sig, temp_sig, 0x10);
  return XeCryptBnDwLePkcs1Verify_entry(
      (void*)hash, reinterpret_cast<uint8_t*>(temp_sig), 0x10 * 8);
}

dword_result_t XeKeysVerifyRSASignature_entry(dword_t use_live_key, lpvoid_t hash,
                                        pointer_t<uint64_t> signature) {
  // returns BOOL
  if (!kXeKeyVaultLoaded) {
    XELOGE(
        "XeKeysVerifyRSASignature called without keyvault loaded - will likely "
        "cause failure!");
  }

  XeKey main_key = XeKey::CONSTANT_PIRS_KEY;
  XeKey alt_key = (XeKey)0;

  if (use_live_key) {
    main_key = XeKey::CONSTANT_LIVE_KEY;
    alt_key = XeKey::CONSTANT_ALT_LIVE_KEY;
  }

  XeKey key_num = main_key;
  while (true) {
    auto* pub_key = xeKeysGetKeyPtr<XECRYPT_RSA>(key_num);
    if (pub_key && pub_key->size == 0x20 &&
        xeKeysPkcs1Verify(hash, signature, pub_key)) {
      return true;
    }
    if (key_num == alt_key) {
      break;
    }
    key_num = alt_key;
  }

  return false;
}
DECLARE_XBOXKRNL_EXPORT1(XeKeysVerifyRSASignature, kNone, kImplemented);

dword_result_t XeKeysHmacSha_entry(dword_t key_num, lpvoid_t inp_1,
                             dword_t inp_1_size, lpvoid_t inp_2,
                             dword_t inp_2_size, lpvoid_t inp_3,
                             dword_t inp_3_size, lpvoid_t out,
                             dword_t out_size) {
  const uint8_t* key = xeKeysGetKeyPtr<uint8_t>((XeKey)(uint32_t)key_num);

  if (key) {
    XeCryptHmacSha_entry((void*)key, 0x10, inp_1, inp_1_size, inp_2, inp_2_size,
                   inp_3, inp_3_size, out, out_size);

    return X_STATUS_SUCCESS;
  }

  return X_STATUS_UNSUCCESSFUL;
}
DECLARE_XBOXKRNL_EXPORT1(XeKeysHmacSha, kNone, kImplemented);

dword_result_t XeKeysAesCbcUsingKey_entry(lpvoid_t obscured_key, lpvoid_t inp_ptr,
                                    dword_t inp_size, lpvoid_t out_ptr,
                                    lpvoid_t feed_ptr, dword_t encrypt) {
  uint8_t key[16];

  // Deobscure key
  XECRYPT_AES_STATE aes;
  XeCryptAesKey_entry(&aes, xeKeysGetKeyPtr<uint8_t>(XeKey::KEY_OBFUSCATION_KEY));
  XeCryptAesEcb_entry(&aes, obscured_key, key, 0);

  // Run CBC using deobscured key
  XeCryptAesKey_entry(&aes, key);
  XeCryptAesCbc_entry(&aes, inp_ptr, inp_size, out_ptr, feed_ptr, encrypt);

  return X_STATUS_SUCCESS;
}
DECLARE_XBOXKRNL_EXPORT1(XeKeysAesCbcUsingKey, kNone, kImplemented);

dword_result_t XeKeysObscureKey_entry(lpvoid_t input, lpvoid_t output) {
  XECRYPT_AES_STATE aes;
  XeCryptAesKey_entry(&aes, xeKeysGetKeyPtr<uint8_t>(XeKey::KEY_OBFUSCATION_KEY));
  XeCryptAesEcb_entry(&aes, input, output, 1);

  return X_STATUS_SUCCESS;
}
DECLARE_XBOXKRNL_EXPORT1(XeKeysObscureKey, kNone, kImplemented);

dword_result_t XeKeysHmacShaUsingKey_entry(lpvoid_t obscured_key, lpvoid_t inp_1,
                                     dword_t inp_1_size, lpvoid_t inp_2,
                                     dword_t inp_2_size, lpvoid_t inp_3,
                                     dword_t inp_3_size, lpvoid_t out,
                                     dword_t out_size) {
  if (!obscured_key) {
    return X_STATUS_INVALID_PARAMETER;
  }

  uint8_t key[16];

  // Deobscure key
  XECRYPT_AES_STATE aes;
  XeCryptAesKey_entry(&aes, xeKeysGetKeyPtr<uint8_t>(XeKey::KEY_OBFUSCATION_KEY));
  XeCryptAesEcb_entry(&aes, obscured_key, key, 0);

  XeCryptHmacSha_entry(key, 0x10, inp_1, inp_1_size, inp_2, inp_2_size, inp_3,
                 inp_3_size, out, out_size);
  return X_STATUS_SUCCESS;
}
DECLARE_XBOXKRNL_EXPORT1(XeKeysHmacShaUsingKey, kNone, kImplemented);

dword_result_t XeKeysObfuscate_entry(dword_t roaming, lpvoid_t input,
                               dword_t input_size, lpvoid_t output,
                               pointer_t<uint32_t> output_size) {
  // Don't need to worry about keyvault being loaded as ROAMABLE key is setup by
  // xeKeysFixupKeyVault!

  const uint8_t* input_ptr = input;
  uint8_t* output_ptr = output;

  std::copy_n(input_ptr, input_size, output_ptr + 0x18);
  *output_size = input_size + 0x18;

  // TODO: set random nonce/confounder
  // ExCryptRandom(output + 0x10, 8);
  std::memset(output_ptr + 0x10, 0xBB, 8);

  auto key_idx = (uint32_t)(roaming ? XeKey::ROAMABLE_OBFUSCATION_KEY
                                    : XeKey::CONSOLE_OBFUSCATION_KEY);

  auto result =
      XeKeysHmacSha_entry((uint32_t)key_idx, output_ptr + 0x10, *output_size - 0x10,
                    nullptr, 0, nullptr, 0, output_ptr, 0x10);
  if (XFAILED(result)) {
    return result;
  }

  uint8_t key[0x10];
  auto result2 =
      XeKeysHmacSha_entry((uint32_t)key_idx, output_ptr, 0x10, 0, 0, 0, 0, key, 0x10);

  if (XFAILED(result2)) {
    return result2;
  }

  XeCryptRc4_entry(key, 0x10, output_ptr + 0x10, *output_size - 0x10);

  return result2;
}
DECLARE_XBOXKRNL_EXPORT1(XeKeysObfuscate, kNone, kImplemented);

dword_result_t XeKeysUnObfuscate_entry(dword_t roaming, lpvoid_t input,
                                 dword_t input_size, lpvoid_t output,
                                 pointer_t<uint32_t> output_size) {
  // returns BOOL

  // Don't need to worry about keyvault being loaded as ROAMABLE key is setup by
  // xeKeysFixupKeyVault!

  if (input_size < 0x18) {
    return false;
  }

  const uint8_t* input_ptr = input;
  uint8_t* output_ptr = output;

  uint8_t hmac_header[0x18];
  std::copy_n(input_ptr, 0x18, hmac_header);

  *output_size = input_size - 0x18;
  std::copy_n(input_ptr + 0x18, *output_size, output_ptr);

  auto key_idx = (uint32_t)(roaming ? XeKey::ROAMABLE_OBFUSCATION_KEY
                                    : XeKey::CONSOLE_OBFUSCATION_KEY);

  uint8_t key[0x10];
  auto result = XeKeysHmacSha_entry(key_idx, hmac_header, 0x10, nullptr, 0, nullptr,
                              0, key, 0x10);
  if (XFAILED(result)) {
    return false;
  }

  XECRYPT_RC4_STATE rc4;
  XeCryptRc4Key_entry(&rc4, key, 0x10);
  XeCryptRc4Ecb_entry(&rc4, hmac_header + 0x10, 8);
  XeCryptRc4Ecb_entry(&rc4, output_ptr, *output_size);

  uint8_t hash[0x10];
  XeKeysHmacSha_entry(key_idx, hmac_header + 0x10, 8, output_ptr, *output_size,
                nullptr, 0, hash, 0x10);

  return std::memcmp(hash, hmac_header, 0x10) == 0;
}
DECLARE_XBOXKRNL_EXPORT1(XeKeysUnObfuscate, kNone, kImplemented);

void RegisterXeKeysExports(xe::cpu::ExportResolver* export_resolver,
                           KernelState* kernel_state) {}

}  // namespace xboxkrnl
}  // namespace kernel
}  // namespace xe
