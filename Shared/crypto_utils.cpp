#include "shared/crypto_utils.h"

#include <sodium.h>

#include <cctype>
#include <stdexcept>
#include <vector>

namespace gmp::crypto {

void EnsureSodiumInitialized() {
  static bool initialized = []() {
    if (sodium_init() < 0) {
      throw std::runtime_error("Failed to initialize libsodium");
    }
    return true;
  }();
  (void)initialized;
}

std::string BytesToHex(const unsigned char* data, std::size_t size) {
  static constexpr char hex_digits[] = "0123456789abcdef";
  std::string output;
  output.reserve(size * 2);
  for (std::size_t i = 0; i < size; ++i) {
    const unsigned char byte = data[i];
    output.push_back(hex_digits[(byte >> 4) & 0x0F]);
    output.push_back(hex_digits[byte & 0x0F]);
  }
  return output;
}

std::string ComputeSHA256(const std::uint8_t* data, std::size_t size) {
  EnsureSodiumInitialized();
  unsigned char hash[crypto_hash_sha256_BYTES];
  crypto_hash_sha256(hash, data, size);
  return BytesToHex(hash, crypto_hash_sha256_BYTES);
}

std::string NormalizeHex(std::string_view hex) {
  std::string normalized;
  normalized.reserve(hex.size());
  for (char ch : hex) {
    normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
  }
  return normalized;
}

}  // namespace gmp::crypto
