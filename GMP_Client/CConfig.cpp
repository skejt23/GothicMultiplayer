
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
#include "CConfig.h"
#include <spdlog/spdlog.h>

#include <exception>
#include <filesystem>
#include <map>
#include <unordered_map>
#include <string>
#include <string_view>

#include "shared/toml_wrapper.h"


using namespace Gothic_II_Addon;

namespace {
constexpr std::string_view kConfigFileName = "GMP_Config.toml";

std::string ToStdString(const zSTRING& value) {
  return value.IsEmpty() ? std::string{} : std::string(value.ToChar());
}

}  // namespace

CConfig::CConfig() : d(true) {
  config_file_path_ = std::filesystem::current_path() / kConfigFileName;
  LoadConfigFromFile();
}

CConfig::~CConfig() {
  SaveConfigToFile();
};

void CConfig::LoadConfigFromFile() {
  DefaultSettings();

  if (!std::filesystem::exists(config_file_path_)) {
    SPDLOG_INFO("GMP configuration not found at {}. Writing defaults.", config_file_path_.string());
    SaveConfigToFile();
    return;
  }

  TomlWrapper toml;
  try {
    toml = TomlWrapper::CreateFromFile(config_file_path_.string());
    d = false;
  } catch (const std::exception& ex) {
    SPDLOG_INFO("Using default GMP configuration: {}", ex.what());
    ApplyEngineSettings();
    return;
  }

  if (auto nickname_opt = toml.GetValue<std::string>("nickname"); nickname_opt) {
    Nickname = nickname_opt->c_str();
  }

  if (auto skin_opt = toml.GetValue<int>("skin_texture"); skin_opt) {
    skintexture = *skin_opt;
  }

  if (auto face_opt = toml.GetValue<int>("face_texture"); face_opt) {
    facetexture = *face_opt;
  }

  if (auto head_opt = toml.GetValue<int>("head_model"); head_opt) {
    headmodel = *head_opt;
  }

  if (auto walk_opt = toml.GetValue<int>("walk_style"); walk_opt) {
    walkstyle = *walk_opt;
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

  if (auto aa_opt = toml.GetValue<bool>("antialiasing"); aa_opt) {
    antialiasing = *aa_opt;
  }

  if (auto joystick_opt = toml.GetValue<bool>("joystick"); joystick_opt) {
    joystick = *joystick_opt;
  }

  if (auto potion_opt = toml.GetValue<bool>("potion_keys"); potion_opt) {
    potionkeys = *potion_opt;
  }

  if (auto logo_opt = toml.GetValue<bool>("logo_videos"); logo_opt) {
    logovideos = *logo_opt;
  }

  if (auto window_position = toml.GetValue<std::map<std::string, int>>("window_position"); window_position) {
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

  if (auto console_position = toml.GetValue<std::map<std::string, int>>("console_position"); console_position) {
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

  d = Nickname.IsEmpty();

  ApplyEngineSettings();
}

void CConfig::DefaultSettings() {
  Nickname.Clear();
  skintexture = 9;
  facetexture = 18;
  headmodel = 3;
  walkstyle = 0;
  // 0 - polski, 1 - angielski
  lang = 0;
  logchat = false;
  watch = false;
  logovideos = true;
  antialiasing = false;
  joystick = false;
  potionkeys = false;
  keyboardlayout = 0;
  WatchPosX = 7000;
  WatchPosY = 2500;
  ChatLines = 6;
  window_position_.reset();
  console_position_.reset();
  d = true;
};

void CConfig::SaveConfigToFile(bool sync_engine_settings) {
  TomlWrapper toml;

  toml["nickname"] = ToStdString(Nickname);
  toml["skin_texture"] = skintexture;
  toml["face_texture"] = facetexture;
  toml["head_model"] = headmodel;
  toml["walk_style"] = walkstyle;
  toml["language"] = lang;
  toml["log_chat"] = logchat;
  toml["watch_enabled"] = watch;

  std::unordered_map<std::string, toml::value> watch_position_map;
  watch_position_map["x"] = toml::value(WatchPosX);
  watch_position_map["y"] = toml::value(WatchPosY);
  toml["watch_position"] = watch_position_map;

  toml["chat_lines"] = ChatLines;
  toml["keyboard_layout"] = keyboardlayout;
  toml["antialiasing"] = antialiasing;
  toml["joystick"] = joystick;
  toml["potion_keys"] = potionkeys;
  toml["logo_videos"] = logovideos;

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

  toml.Serialize(config_file_path_.string());
  d = Nickname.IsEmpty();
  ApplyEngineSettings();
}

const std::optional<CConfig::WindowPosition>& CConfig::GetWindowPosition() const {
  return window_position_;
}

void CConfig::SetWindowPosition(WindowPosition window_position) {
  window_position_ = window_position;
}

const std::optional<CConfig::ConsolePosition>& CConfig::GetConsolePosition() const {
  return console_position_;
}

void CConfig::SetConsolePosition(ConsolePosition console_position) {
  console_position_ = console_position;
}

bool CConfig::IsDefault() {
  return d;
}

void CConfig::ApplyEngineSettings() const {
  if (!zoptions) {
    SPDLOG_DEBUG("Skipping engine config sync because zoptions is not available");
    return;
  }

  zSTRING Multiplayer = "MULTIPLAYER";
  zSTRING Engine = "ENGINE";
  zSTRING Game = "GAME";

  // [MULTIPLAYER] Ini Section
  zoptions->WriteString(Multiplayer, "Nickname", Nickname);
  zoptions->WriteInt(Multiplayer, "Skintexture", skintexture);
  zoptions->WriteInt(Multiplayer, "Facetexture", facetexture);
  zoptions->WriteInt(Multiplayer, "Headmodel", headmodel);
  zoptions->WriteInt(Multiplayer, "Walkstyle", walkstyle);
  zoptions->WriteInt(Multiplayer, "Lang", lang);
  zoptions->WriteBool(Multiplayer, "Logchat", logchat);
  zoptions->WriteBool(Multiplayer, "Watch", watch);
  zoptions->WriteInt(Multiplayer, "WatchPosX", WatchPosX);
  zoptions->WriteInt(Multiplayer, "WatchPosY", WatchPosY);
  zoptions->WriteInt(Multiplayer, "ChatLines", ChatLines);
  zoptions->WriteInt(Multiplayer, "KeyboardLayout", keyboardlayout);
  // Other Sections
  zoptions->WriteBool(Engine, "zVidEnableAntiAliasing", antialiasing);
  zoptions->WriteBool(Game, "enableJoystick", joystick);
  zoptions->WriteBool(Game, "usePotionKeys", potionkeys);
  zoptions->WriteBool(Game, "playLogoVideos", logovideos);
  // Apply changes
  gameMan->ApplySomeSettings();
}