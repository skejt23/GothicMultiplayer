/*
MIT License

Copyright (c) 2025 Gothic Multiplayer Team (pampi, skejt23, mecio)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "config.h"

#include <spdlog/common.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/fmt/ranges.h>
#include <spdlog/spdlog.h>

#include <cstdint>
#include <map>
#include <string_view>

#include "shared/toml_wrapper.h"

namespace {
constexpr std::string_view kClientConfigFileName = "gmp_client_config.toml";
}

Config::Config() {
  Load();
}

void Config::Load() {
  TomlWrapper toml;
  config_file_path_ = std::filesystem::current_path() / kClientConfigFileName;
  try {
    toml = TomlWrapper::CreateFromFile(config_file_path_.string());
  } catch (const std::exception& ex) {
    SPDLOG_ERROR("Failed to load config file: {}", ex.what());
    return;
  }

  std::optional<std::map<std::string, std::int32_t>> window_position =
      toml.GetValue<std::map<std::string, int>>("window_position");
  if (window_position) {
    std::int32_t x = 0;
    std::int32_t y = 0;
    auto it = window_position->find("x");
    if (it != window_position->end()) {
      x = it->second;
    }
    it = window_position->find("y");
    if (it != window_position->end()) {
      y = it->second;
    }
    if (x > 0 && y > 0) {
      window_position_ = WindowPosition{static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y)};
    }
  }
}

const std::optional<Config::WindowPosition>& Config::GetWindowPosition() const {
  return window_position_;
}

void Config::SetWindowPosition(WindowPosition window_position) {
  window_position_ = window_position;
}

void Config::Save() const {
  TomlWrapper toml;
  if (window_position_) {
    std::unordered_map<std::string, toml::value> window_position_map;
    window_position_map["x"] = toml::value(window_position_->x);
    window_position_map["y"] = toml::value(window_position_->y);

    toml["window_position"] = window_position_map;
  }
  toml.Serialize(config_file_path_.string());
}