#include "machine_identity.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <fstream>
#endif

#include <sodium.h>

#include <algorithm>
#include <string>

namespace Net {
namespace {

bool IsWhitespace(char value) {
  return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

int HexDigit(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  return -1;
}

}  // namespace

std::optional<MachineIdentity> DeriveMachineIdentity(std::string_view os_identifier) {
  while (!os_identifier.empty() && IsWhitespace(os_identifier.front())) os_identifier.remove_prefix(1);
  while (!os_identifier.empty() && IsWhitespace(os_identifier.back())) os_identifier.remove_suffix(1);
  if (os_identifier.size() == 38 && os_identifier.front() == '{' && os_identifier.back() == '}') {
    os_identifier.remove_prefix(1);
    os_identifier.remove_suffix(1);
  }
  const bool uuid_format = os_identifier.size() == 36;
  if (!uuid_format && os_identifier.size() != 32) return std::nullopt;

  std::array<unsigned char, 16> machine_id{};
  std::size_t digit = 0;
  for (std::size_t i = 0; i < os_identifier.size(); ++i) {
    if (uuid_format && (i == 8 || i == 13 || i == 18 || i == 23)) {
      if (os_identifier[i] != '-') return std::nullopt;
      continue;
    }
    const int value = HexDigit(os_identifier[i]);
    if (value < 0) return std::nullopt;
    machine_id[digit / 2] |= static_cast<unsigned char>(value << (digit % 2 == 0 ? 4 : 0));
    ++digit;
  }
  if (std::all_of(machine_id.begin(), machine_id.end(), [](unsigned char value) { return value == 0; })) return std::nullopt;

  // The domain is public and must remain fixed across releases. Hashing keeps
  // the raw OS identifier off the network; it does not prevent client spoofing.
  constexpr char domain[] = "GothicMultiplayer/machine-identity/v1";
  static_assert(crypto_hash_sha256_BYTES == kMachineIdentityBytes);
  if (sodium_init() < 0) return std::nullopt;
  crypto_hash_sha256_state state;
  crypto_hash_sha256_init(&state);
  crypto_hash_sha256_update(&state, reinterpret_cast<const unsigned char*>(domain), sizeof(domain) - 1);
  crypto_hash_sha256_update(&state, machine_id.data(), machine_id.size());
  MachineIdentity identity{};
  crypto_hash_sha256_final(&state, identity.data());
  sodium_memzero(machine_id.data(), machine_id.size());
  return identity;
}

std::optional<MachineIdentity> ReadMachineIdentity() {
#ifdef _WIN32
  // Read the same registry view from both the 32-bit client and 64-bit tools.
  wchar_t value[64]{};
  DWORD bytes = sizeof(value);
  const auto result = RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", L"MachineGuid",
                                  RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY, nullptr, value, &bytes);
  if (result != ERROR_SUCCESS) return std::nullopt;
  std::string text;
  for (const auto character : value) {
    if (character == L'\0') break;
    if (character > 127) return std::nullopt;
    text.push_back(static_cast<char>(character));
  }
  return DeriveMachineIdentity(text);
#elif defined(__linux__)
  for (const char* path : {"/etc/machine-id", "/var/lib/dbus/machine-id"}) {
    std::ifstream input(path);
    std::string text;
    if (std::getline(input, text)) {
      if (auto identity = DeriveMachineIdentity(text)) return identity;
    }
  }
  return std::nullopt;
#else
  return std::nullopt;
#endif
}

}  // namespace Net
