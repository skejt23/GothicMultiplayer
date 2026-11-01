
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

#include <algorithm>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <string_view>
#include <utility>

#include "renderer/renderer_config.h"
#include "scripting/process_input.h"
#include "shared/toml_wrapper.h"
#include "windows_paths.h"

using namespace Gothic_II_Addon;

namespace {
constexpr std::string_view kConfigFileName = "GMP_Config.toml";

TomlWrapper ReadConfigFile(const std::filesystem::path& path) {
  // Opening the filesystem path directly preserves Unicode installation paths.
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("Cannot open GMP configuration");
  }
  return TomlWrapper::CreateFromStream(input, std::string(kConfigFileName));
}

}  // namespace

Config::Config() {
  const auto system_directory = gmp::paths::ModulePath().parent_path();
  config_file_path_ = system_directory.parent_path() / L"Multiplayer" / kConfigFileName;
  LoadConfigFromFile();
}

Config::~Config() {};

void Config::LoadConfigFromFile() {
  DefaultSettings();

  if (!std::filesystem::exists(config_file_path_)) {
    SPDLOG_INFO("GMP configuration not found in Multiplayer. Writing defaults.");
    SaveConfigToFile();
    return;
  }

  TomlWrapper toml;
  try {
    toml = ReadConfigFile(config_file_path_);
    is_default_ = false;
  } catch (const std::exception& ex) {
    SPDLOG_INFO("Using default GMP configuration: {}", ex.what());
    return;
  }

  if (auto nickname_opt = toml.GetValue<std::string>("gmp", "nickname"); nickname_opt) {
    Nickname = nickname_opt->c_str();
  }

  if (auto language_opt = toml.GetValue<std::string>("gmp", "language"); language_opt && !language_opt->empty()) {
    language = *language_opt;
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

  if (auto always_on_top = toml.GetValue<bool>("display", "window_always_on_top"); always_on_top) {
    window_always_on_top_ = *always_on_top;
  }

  if (auto vsync_opt = toml.GetValue<bool>("display", "vsync_enabled"); vsync_opt) {
    vsync_enabled = *vsync_opt;
  }
  // Propagate to renderer config (used by renderers during their init)
  RendererConfig::Instance().vsync_enabled = vsync_enabled;

  // Load renderer type (default: D3D9)
  if (auto renderer_str = toml.GetValue<std::string>("display", "renderer_type"); renderer_str) {
    if (*renderer_str == "D3D7") {
      renderer_type_ = RendererType::D3D7;
    } else if (*renderer_str == "D3D9") {
      renderer_type_ = RendererType::D3D9;
    } else if (*renderer_str == "D3D11") {
      renderer_type_ = RendererType::D3D11;
    }
  }

  // MCP pipe enable flag
  if (auto mcp_opt = toml.GetValue<bool>("debug", "mcp_pipe_enabled"); mcp_opt) {
    mcp_pipe_enabled_ = *mcp_opt;
  }

  // Debug console enable flag
  if (auto debug_console_opt = toml.GetValue<bool>("debug", "debug_console_enabled"); debug_console_opt) {
    debug_console_enabled_ = *debug_console_opt;
  }

  if (auto voice_enabled = toml.GetValue<bool>("gmp", "voice_enabled"); voice_enabled) {
    voice_chat_enabled_ = *voice_enabled;
  }
  if (auto voice_key = toml.GetValue<std::string>("gmp", "voice_push_to_talk_key"); voice_key) {
    if (const auto key_code = gmp::gothic::FindKeyboardKeyCode(*voice_key); key_code) {
      voice_push_to_talk_key_ = *key_code;
    } else {
      SPDLOG_WARN("Invalid voice_push_to_talk_key '{}'; using KEY_K", *voice_key);
    }
  }
  if (auto voice_volume = toml.GetValue<int>("gmp", "voice_output_volume"); voice_volume) {
    voice_output_volume_percent_ = *voice_volume;
  }
  if (auto extended_scenes = toml.GetValue<bool>("gmp", "extended_menu_scenes"); extended_scenes) {
    extended_menu_scenes = *extended_scenes;
  }
  voice_output_volume_percent_ = std::clamp(voice_output_volume_percent_, 0, 100);
  if (auto delay = toml.GetValue<int>("gmp", "interpolation_delay_ms"); delay) {
    interpolation_delay_ms_ = std::clamp(*delay, 0, 500);
  }

  if (auto body = toml.GetValue<std::string>("character", "body_model")) character_appearance_.body_model = *body;
  if (auto texture = toml.GetValue<int>("character", "body_texture")) character_appearance_.body_texture = *texture;
  if (auto head = toml.GetValue<std::string>("character", "head_model")) character_appearance_.head_model = *head;
  if (auto texture = toml.GetValue<int>("character", "head_texture")) character_appearance_.head_texture = *texture;
  if (auto overlay = toml.GetValue<std::string>("character", "walking_overlay")) character_appearance_.walking_overlay = *overlay;
  gmp::NormalizeCharacterAppearance(character_appearance_);

  // If nickname is empty, the user didn't set up the config yet.
  is_default_ = Nickname.IsEmpty();
}

void Config::DefaultSettings() {
  Nickname.Clear();
  language = "EN";
  window_position_.reset();
  console_position_.reset();
  window_always_on_top_ = false;
  renderer_type_ = RendererType::D3D9;
  mcp_pipe_enabled_ = false;
  debug_console_enabled_ = true;
  voice_chat_enabled_ = true;
  voice_push_to_talk_key_ = KEY_K;
  voice_output_volume_percent_ = 100;
  interpolation_delay_ms_ = 120;
  vsync_enabled = true;
  extended_menu_scenes = false;
  character_appearance_ = {};
  is_default_ = true;
};

void Config::SaveConfigToFile() {
  TomlWrapper toml;

  // TomlWrapper preserves insertion order. Keep these tables and their keys in
  // the same user-facing order as the configuration documentation/menu.
  toml["gmp"]["nickname"] = Nickname.string();
  toml["gmp"]["language"] = language;
  toml["gmp"]["voice_enabled"] = toml::value(voice_chat_enabled_);
  const auto voice_key_name = gmp::gothic::FindKeyboardKeyName(voice_push_to_talk_key_);
  toml["gmp"]["voice_push_to_talk_key"] =
      toml::value(std::string(voice_key_name.empty() ? std::string_view{"KEY_K"} : voice_key_name));
  toml["gmp"]["voice_output_volume"] = toml::value(voice_output_volume_percent_);
  toml["gmp"]["interpolation_delay_ms"] = toml::value(interpolation_delay_ms_);
  toml["gmp"]["extended_menu_scenes"] = toml::value(extended_menu_scenes);

  toml["character"]["body_model"] = character_appearance_.body_model;
  toml["character"]["body_texture"] = toml::value(character_appearance_.body_texture);
  toml["character"]["head_model"] = character_appearance_.head_model;
  toml["character"]["head_texture"] = toml::value(character_appearance_.head_texture);
  toml["character"]["walking_overlay"] = character_appearance_.walking_overlay;

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

  toml["display"]["window_always_on_top"] = toml::value(window_always_on_top_);
  toml["display"]["vsync_enabled"] = toml::value(vsync_enabled);
  toml["display"]["renderer_type"] = toml::value(renderer_str);

  toml["debug"]["mcp_pipe_enabled"] = toml::value(mcp_pipe_enabled_);
  toml["debug"]["debug_console_enabled"] = toml::value(debug_console_enabled_);

  if (window_position_) {
    toml["window_position"]["x"] = toml::value(window_position_->x);
    toml["window_position"]["y"] = toml::value(window_position_->y);
  }

  if (console_position_) {
    toml["console_position"]["x"] = toml::value(console_position_->x);
    toml["console_position"]["y"] = toml::value(console_position_->y);
  }

  try {
    std::filesystem::create_directories(config_file_path_.parent_path());
    std::ofstream output;
    output.exceptions(std::ios::failbit | std::ios::badbit);
    output.open(config_file_path_, std::ios::binary | std::ios::trunc);
    toml.Serialize(output);
    output.close();
  } catch (const std::exception& ex) {
    SPDLOG_ERROR("Failed to save GMP configuration: {}", ex.what());
    return;
  }

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

void Config::SetVoiceOutputVolume(float volume) {
  voice_output_volume_percent_ = static_cast<int>(std::lround(std::clamp(volume, 0.0f, 1.0f) * 100.0f));
}

bool Config::IsDefault() const {
  return is_default_;
}

void Config::SetCharacterAppearance(gmp::CharacterAppearance appearance) {
  gmp::NormalizeCharacterAppearance(appearance);
  character_appearance_ = std::move(appearance);
}
