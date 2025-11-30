
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
  int lang;
  bool logchat;
  bool watch;
  enum KeyboardLayout { KEYBOARD_POLISH, KEYBOARD_GERMAN, KEYBOARD_CYRYLLIC };
  int keyboardlayout;
  int WatchPosX;
  int WatchPosY;
  int ChatLines;

  Config();
  ~Config();

  void DefaultSettings();
  void SaveConfigToFile();

  const std::optional<WindowPosition>& GetWindowPosition() const;
  void SetWindowPosition(WindowPosition window_position);

  const std::optional<ConsolePosition>& GetConsolePosition() const;
  void SetConsolePosition(ConsolePosition console_position);

  bool IsWindowAlwaysOnTop() const {
    return window_always_on_top_;
  }

  static Config& Instance() {
    static Config instance;
    return instance;
  }

private:
  void LoadConfigFromFile();

  bool is_default_{true};
  std::filesystem::path config_file_path_;
  std::optional<WindowPosition> window_position_;
  std::optional<ConsolePosition> console_position_;
  bool window_always_on_top_ = false;
};