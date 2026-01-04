
/*
MIT License

Copyright (c) 2022 Gothic Multiplayer Team (pampi, skejt23, mecio)

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

#pragma warning(disable : 4996 4800)
#include "config.h"

#include <spdlog/spdlog.h>

#include <exception>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>

#include "renderer/renderer_config.h"
#include "shared/toml_wrapper.h"

using namespace Gothic_II_Addon;

namespace {
constexpr std::string_view kConfigFileName = "GMP_Config.toml";

}  // namespace

Config::Config() {
  config_file_path_ = std::filesystem::current_path() / kConfigFileName;
  LoadConfigFromFile();
}

Config::~Config() {};

void Config::LoadConfigFromFile() {
  DefaultSettings();

  if (!std::filesystem::exists(config_file_path_)) {
    SPDLOG_INFO("GMP configuration not found at {}. Writing defaults.", config_file_path_.string());
    SaveConfigToFile();
    return;
  }

  TomlWrapper toml;
  try {
    toml = TomlWrapper::CreateFromFile(config_file_path_.string());
    is_default_ = false;
  } catch (const std::exception& ex) {
    SPDLOG_INFO("Using default GMP configuration: {}", ex.what());
    return;
  }

  if (auto nickname_opt = toml.GetValue<std::string>("nickname"); nickname_opt) {
    Nickname = nickname_opt->c_str();
  }

  if (auto lang_opt = toml.GetValue<int>("language"); lang_opt) {
    lang = *lang_opt;
  }

  if (auto logchat_opt = toml.GetValue<bool>("log_chat"); logchat_opt) {
    logchat = *logchat_opt;
  }

  if (auto watch_opt = toml.GetValue<bool>("watch_enabled"); watch_opt) {
    watch = *watch_opt;
  }

  if (auto watch_pos = toml.GetValue<std::map<std::string, int>>("watch_position"); watch_pos) {
    auto it_x = watch_pos->find("x");
    auto it_y = watch_pos->find("y");
    if (it_x != watch_pos->end()) {
      WatchPosX = it_x->second;
    }
    if (it_y != watch_pos->end()) {
      WatchPosY = it_y->second;
    }
  }

  if (auto chat_lines_opt = toml.GetValue<int>("chat_lines"); chat_lines_opt) {
    ChatLines = *chat_lines_opt;
  }

  if (auto keyboard_opt = toml.GetValue<int>("keyboard_layout"); keyboard_opt) {
    keyboardlayout = *keyboard_opt;
  }

  if (std::optional<std::map<std::string, std::int32_t>> window_position = toml.GetValue<std::map<std::string, int>>("window_position")) {
    std::int32_t x = 0;
    std::int32_t y = 0;
    if (auto it = window_position->find("x"); it != window_position->end()) {
      x = it->second;
    }
    if (auto it = window_position->find("y"); it != window_position->end()) {
      y = it->second;
    }
    if (x > 0 && y > 0) {
      window_position_ = WindowPosition{x, y};
    }
  }

  if (std::optional<std::map<std::string, std::int32_t>> console_position = toml.GetValue<std::map<std::string, int>>("console_position")) {
    std::int32_t x = 0;
    std::int32_t y = 0;
    if (auto it = console_position->find("x"); it != console_position->end()) {
      x = it->second;
    }
    if (auto it = console_position->find("y"); it != console_position->end()) {
      y = it->second;
    }
    if (x >= 0 && y >= 0) {
      console_position_ = ConsolePosition{x, y};
    }
  }

  window_always_on_top_ = toml.GetValue<bool>("window_always_on_top", window_always_on_top_);

  if (auto vsync_opt = toml.GetValue<bool>("vsync_enabled"); vsync_opt) {
    vsync_enabled = *vsync_opt;
  }
  // Propagate to renderer config (used by renderers during their init)
  RendererConfig::Instance().vsync_enabled = vsync_enabled;

  // Load renderer type (default: D3D9)
  if (auto renderer_str = toml.GetValue<std::string>("renderer_type"); renderer_str) {
    if (*renderer_str == "D3D7") {
      renderer_type_ = RendererType::D3D7;
    } else if (*renderer_str == "D3D9") {
      renderer_type_ = RendererType::D3D9;
    } else if (*renderer_str == "D3D11") {
      renderer_type_ = RendererType::D3D11;
    }
  }

  // Load test mode configuration (from [test_mode] section)
  // Using nested key access: GetValue<T>("table", defaultValue, "key")
  test_mode_config_.enabled = toml.GetValue<bool>("test_mode", false, "enabled");
  test_mode_config_.level = toml.GetValue<std::string>("test_mode", std::string{}, "level");
  test_mode_config_.spawn_x = static_cast<float>(toml.GetValue<double>("test_mode", 0.0, "spawn_x"));
  test_mode_config_.spawn_y = static_cast<float>(toml.GetValue<double>("test_mode", 0.0, "spawn_y"));
  test_mode_config_.spawn_z = static_cast<float>(toml.GetValue<double>("test_mode", 0.0, "spawn_z"));

  if (test_mode_config_.enabled) {
    SPDLOG_INFO("Test mode enabled: level='{}', spawn=({}, {}, {})", test_mode_config_.level, test_mode_config_.spawn_x, test_mode_config_.spawn_y,
                test_mode_config_.spawn_z);
  }

  // MCP pipe enable flag
  if (auto mcp_opt = toml.GetValue<bool>("mcp_pipe_enabled"); mcp_opt) {
    mcp_pipe_enabled_ = *mcp_opt;
  }

  // If nickname is empty, the user didn't set up the config yet.
  is_default_ = Nickname.IsEmpty();
}

void Config::DefaultSettings() {
  Nickname.Clear();
  // 0 - polski, 1 - angielski
  lang = 0;
  logchat = false;
  watch = false;
  keyboardlayout = 0;
  WatchPosX = 7000;
  WatchPosY = 2500;
  ChatLines = 6;
  window_position_.reset();
  console_position_.reset();
  renderer_type_ = RendererType::D3D9;
  test_mode_config_ = TestModeConfig{};  // Reset test mode to defaults
  mcp_pipe_enabled_ = false;
  vsync_enabled = true;
  is_default_ = true;
};

void Config::SaveConfigToFile() {
  TomlWrapper toml;

  toml["nickname"] = Nickname.string();
  toml["language"] = lang;
  toml["log_chat"] = logchat;
  toml["watch_enabled"] = watch;

  std::unordered_map<std::string, toml::value> watch_position_map;
  watch_position_map["x"] = toml::value(WatchPosX);
  watch_position_map["y"] = toml::value(WatchPosY);
  toml["watch_position"] = watch_position_map;

  toml["chat_lines"] = ChatLines;
  toml["keyboard_layout"] = keyboardlayout;

  if (window_position_) {
    std::unordered_map<std::string, toml::value> window_position_map;
    window_position_map["x"] = toml::value(window_position_->x);
    window_position_map["y"] = toml::value(window_position_->y);
    toml["window_position"] = window_position_map;
  }

  if (console_position_) {
    std::unordered_map<std::string, toml::value> console_position_map;
    console_position_map["x"] = toml::value(console_position_->x);
    console_position_map["y"] = toml::value(console_position_->y);
    toml["console_position"] = console_position_map;
  }

  toml["window_always_on_top"] = toml::value(window_always_on_top_);
  toml["vsync_enabled"] = toml::value(vsync_enabled);
  toml["mcp_pipe_enabled"] = toml::value(mcp_pipe_enabled_);

  // Save renderer type as string
  std::string renderer_str;
  switch (renderer_type_) {
    case RendererType::D3D7:
      renderer_str = "D3D7";
      break;
    case RendererType::D3D9:
      renderer_str = "D3D9";
      break;
    case RendererType::D3D11:
      renderer_str = "D3D11";
      break;
  }
  toml["renderer_type"] = toml::value(renderer_str);

  // Save test mode configuration
  std::unordered_map<std::string, toml::value> test_mode_map;
  test_mode_map["enabled"] = toml::value(test_mode_config_.enabled);
  test_mode_map["level"] = toml::value(test_mode_config_.level);
  test_mode_map["spawn_x"] = toml::value(static_cast<double>(test_mode_config_.spawn_x));
  test_mode_map["spawn_y"] = toml::value(static_cast<double>(test_mode_config_.spawn_y));
  test_mode_map["spawn_z"] = toml::value(static_cast<double>(test_mode_config_.spawn_z));
  toml["test_mode"] = test_mode_map;

  toml.Serialize(config_file_path_.string());
  is_default_ = Nickname.IsEmpty();
}

const std::optional<Config::WindowPosition>& Config::GetWindowPosition() const {
  return window_position_;
}

void Config::SetWindowPosition(WindowPosition window_position) {
  window_position_ = window_position;
}

const std::optional<Config::ConsolePosition>& Config::GetConsolePosition() const {
  return console_position_;
}

void Config::SetConsolePosition(ConsolePosition console_position) {
  console_position_ = console_position;
}

bool Config::IsDefault() const {
  return is_default_;
}