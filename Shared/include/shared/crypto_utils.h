#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace gmp::crypto {

void EnsureSodiumInitialized();
std::string BytesToHex(const unsigned char* data, std::size_t size);
std::string ComputeSHA256(const std::uint8_t* data, std::size_t size);
std::string NormalizeHex(std::string_view hex);

}  // namespace gmp::crypto
