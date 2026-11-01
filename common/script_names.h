#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

namespace gmp {

constexpr std::size_t kMaxScriptNameLength = 255;
constexpr std::size_t kMaxAnimationNameLength = 128;

// Daedalus symbols are case insensitive. Empty strings represent empty slots,
// but are not valid instance names. Never truncate an identifier into another.
inline bool IsValidScriptName(std::string_view name) {
  return !name.empty() && name.size() <= kMaxScriptNameLength &&
         std::all_of(name.begin(), name.end(), [](unsigned char ch) {
           return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
                  (ch >= '0' && ch <= '9') || ch == '_';
         });
}

inline std::string NormalizeScriptName(std::string_view name) {
  std::string result(name);
  for (auto& ch : result) {
    if (ch >= 'a' && ch <= 'z') ch -= 'a' - 'A';
  }
  return result;
}

inline bool IsValidAnimationName(std::string_view name) {
  return !name.empty() && name.size() <= kMaxAnimationNameLength &&
         std::all_of(name.begin(), name.end(), [](unsigned char ch) { return ch >= 0x20 && ch != 0x7f; });
}

}  // namespace gmp
