/*
MIT License

Copyright (c) 2025 Gothic Multiplayer Team.

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

#include "menu/menu_context.hpp"
#include "menu/states/menu_state.hpp"

namespace menu {
namespace states {

/**
 * @brief State for GMP-specific online options (nickname, chat, language, etc.)
 *
 * This state allows configuration of:
 * - Nickname
 * - Chat logging
 * - Watch/clock settings
 * - Anti-aliasing
 * - Joystick
 * - Chat lines
 * - Keyboard layout
 * - Language
 * - Intro videos
 */
class OnlineOptionsState : public MenuState {
private:
  MenuContext& context_;

  enum class OptionItem {
    NICKNAME = 0,
    LOG_CHAT = 1,
    WATCH_TOGGLE = 2,
    WATCH_POSITION = 3,
    ANTIALIASING = 4,
    JOYSTICK = 5,
    CHAT_LINES = 6,
    KEYBOARD_LAYOUT = 7,
    LANGUAGE = 8,
    INTRO_VIDEOS = 9,
    BACK = 10,
    OPTION_COUNT = 11
  };

  OptionItem selectedOption_;
  bool shouldReturnToMainMenu_;
  bool shouldEnterWatchPositioning_;

public:
  explicit OnlineOptionsState(MenuContext& context);
  ~OnlineOptionsState() override = default;

  // MenuState interface
  void OnEnter() override;
  void OnExit() override;
  StateResult Update() override;
  MenuState* CheckTransition() override;
  const char* GetStateName() const override {
    return "OnlineOptions";
  }

private:
  void RenderOptionsMenu();
  void HandleInput();
  void ExecuteOption(OptionItem option);
  void AdjustOption(OptionItem option, int direction);
};

}  // namespace states
}  // namespace menu
