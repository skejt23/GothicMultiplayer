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

#include "Button.h"
#include "Table.h"
#include "menu/menu_context.hpp"
#include "menu/states/menu_state.hpp"

namespace menu {
namespace states {

/**
 * @brief State for first-launch setup (language + nickname)
 *
 * This state allows the player to configure language and nickname
 * in a single screen, then confirm via the Accept button.
 */
class ChooseLanguageState : public MenuState {
private:
  MenuContext& context_;

  enum class SetupItem {
    NICKNAME = 0,
    LANGUAGE = 1,
    ACCEPT = 2,
  };

  int selectedLanguage_;
  SetupItem selectedItem_;
  bool isEditingNickname_;
  bool shouldTransitionToMainMenu_;
  zSTRING currentNickname_;

public:
  explicit ChooseLanguageState(MenuContext& context);
  ~ChooseLanguageState() override;

  // MenuState interface
  void OnEnter() override;
  void OnExit() override;
  StateResult Update() override;
  MenuState* CheckTransition() override;
  const char* GetStateName() const override {
    return "ChooseLanguage";
  }

private:
  void InitializeControls();
  void ApplySelectedLanguage();
  void RenderSetup();
  void HandleInput();

  G2W::Table* setupTable_;
  G2W::Button* acceptButton_;
};

}  // namespace states
}  // namespace menu
