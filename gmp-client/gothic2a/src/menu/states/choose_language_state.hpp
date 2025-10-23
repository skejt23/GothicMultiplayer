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
 * @brief State for choosing the game language on first launch
 *
 * This state allows the player to select their preferred language
 * when they run the game for the first time (or when no language is configured).
 * Uses LanguageManager to access available languages.
 */
class ChooseLanguageState : public MenuState {
private:
  MenuContext& context_;

  int selectedLanguage_;
  bool shouldTransitionToNickname_;

public:
  explicit ChooseLanguageState(MenuContext& context);
  ~ChooseLanguageState() override = default;

  // MenuState interface
  void OnEnter() override;
  void OnExit() override;
  StateResult Update() override;
  MenuState* CheckTransition() override;
  const char* GetStateName() const override {
    return "ChooseLanguage";
  }

private:
  void ApplySelectedLanguage();
  void RenderLanguageSelection();
  void HandleInput();
};

}  // namespace states
}  // namespace menu
