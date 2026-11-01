
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

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#include "default_character_appearance.h"

#include "ZenGin/zGothicAPI.h"

// KEYBOARD LAYOUTS
#define LAYOUT_GERMAN 0x00000407
#define LAYOUT_ENGLISH 0x00000409
#define LAYOUT_POLISH 0x00000415
#define LAYOUT_RUSSIAN 0x00000419

class Config {
public:
  struct WindowPosition {
    std::int32_t x;
    std::int32_t y;
  };

  using ConsolePosition = WindowPosition;

  bool IsDefault() const;

  zSTRING Nickname;
  std::string language = "EN";
  enum class RendererType { D3D7, D3D9, D3D11 };
  bool vsync_enabled = true;
  bool extended_menu_scenes = false;

  Config();
  ~Config();

  void DefaultSettings();
  void SaveConfigToFile();

  const gmp::CharacterAppearance& GetCharacterAppearance() const { return character_appearance_; }
  void SetCharacterAppearance(gmp::CharacterAppearance appearance);

  const std::optional<WindowPosition>& GetWindowPosition() const;
  void SetWindowPosition(WindowPosition window_position);

  const std::optional<ConsolePosition>& GetConsolePosition() const;
  void SetConsolePosition(ConsolePosition console_position);

  bool IsWindowAlwaysOnTop() const {
    return window_always_on_top_;
  }

  void SetWindowAlwaysOnTop(bool enabled) {
    window_always_on_top_ = enabled;
  }

  RendererType GetRendererType() const {
    return renderer_type_;
  }

  void SetRendererType(RendererType renderer_type) {
    renderer_type_ = renderer_type;
  }

  /**
   * @brief Check if MCP Named Pipe server should be started.
   * Defaults to false (disabled).
   */
  bool IsMCPPipeEnabled() const {
    return mcp_pipe_enabled_;
  }

  /**
   * @brief Check if Debug Console should be enabled.
   * Defaults to true (enabled).
   */
  bool IsDebugConsoleEnabled() const {
    return debug_console_enabled_;
  }

  bool IsVoiceChatEnabled() const {
    return voice_chat_enabled_;
  }

  void SetVoiceChatEnabled(bool enabled) {
    voice_chat_enabled_ = enabled;
  }

  int GetVoicePushToTalkKey() const {
    return voice_push_to_talk_key_;
  }

  void SetVoicePushToTalkKey(int key) {
    voice_push_to_talk_key_ = key;
  }

  float GetVoiceOutputVolume() const {
    return static_cast<float>(voice_output_volume_percent_) / 100.0f;
  }

  int GetInterpolationDelayMs() const { return interpolation_delay_ms_; }

  void SetVoiceOutputVolume(float volume);

  static Config& Instance() {
    static Config instance;
    return instance;
  }

private:
  void LoadConfigFromFile();

  bool is_default_{true};
  gmp::CharacterAppearance character_appearance_;
  std::filesystem::path config_file_path_;
  std::optional<WindowPosition> window_position_;
  std::optional<ConsolePosition> console_position_;
  bool window_always_on_top_ = false;
  RendererType renderer_type_ = RendererType::D3D9;  // D3D9 renderer by default
  // Whether to enable the MCP Named Pipe server (disabled by default)
  bool mcp_pipe_enabled_ = false;
  // Whether to spawn external debug console window (enabled by default)
  bool debug_console_enabled_ = true;
  bool voice_chat_enabled_ = true;
  int voice_push_to_talk_key_ = KEY_K;
  int voice_output_volume_percent_ = 100;
  int interpolation_delay_ms_ = 120;
};
