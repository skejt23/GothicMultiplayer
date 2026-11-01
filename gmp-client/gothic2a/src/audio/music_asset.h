#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gmp::audio {

// Names are relative to the two music roots. No input prefix is stripped.
inline std::optional<std::array<std::string, 2>> MusicAssetPaths(std::string_view filepath) {
  std::string path(filepath);
  std::replace(path.begin(), path.end(), '\\', '/');
  if (path.empty() || path.front() == '/' || path.back() == '/' || path.find(':') != std::string::npos || path.find('\0') != std::string::npos)
    return std::nullopt;
  std::vector<std::string> components;
  for (std::size_t start = 0; start < path.size();) {
    const auto end = path.find('/', start);
    const auto component = path.substr(start, end == std::string::npos ? end : end - start);
    if (component == "..") return std::nullopt;
    if (!component.empty() && component != ".") components.push_back(component);
    if (end == std::string::npos) break;
    start = end + 1;
  }
  if (components.empty()) return std::nullopt;
  std::string relative;
  for (std::size_t index = 0; index < components.size(); ++index) {
    if (!relative.empty()) relative += '/';
    relative += components[index];
  }
  return std::array<std::string, 2>{"Multiplayer/Music/" + relative, "_Work/Data/Music/" + relative};
}

// Invalid relative names and missing/unreadable assets throw.
std::vector<std::uint8_t> ReadMusicAsset(const std::string& filepath);

}  // namespace gmp::audio
