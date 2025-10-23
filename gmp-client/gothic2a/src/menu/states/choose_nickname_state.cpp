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

#include "choose_nickname_state.hpp"

#include <spdlog/spdlog.h>

#include "keyboard.h"
#include "language.h"
#include "menu/states/main_menu_loop_state.hpp"

// External declarations from main_menu.cpp (global namespace)
extern float fWRatio;

namespace menu {
namespace states {

ChooseNicknameState::ChooseNicknameState(MenuContext& context) : context_(context), shouldTransitionToMainMenu_(false) {
  // Initialize with existing nickname if any
  currentNickname_ = context_.config.Nickname;
}

void ChooseNicknameState::OnEnter() {
  SPDLOG_INFO("Entering nickname selection state");

  // Make sure language is loaded
  if (!context_.language.IsLoaded()) {
    SPDLOG_WARN("Language not loaded in nickname state");
  }
}

void ChooseNicknameState::OnExit() {
  SPDLOG_INFO("Exiting nickname selection state, nickname: {}", currentNickname_.ToChar());
}

StateResult ChooseNicknameState::Update() {
  RenderNicknameInput();
  HandleInput();
  return StateResult::Continue;
}

MenuState* ChooseNicknameState::CheckTransition() {
  if (shouldTransitionToMainMenu_) {
    return new MainMenuLoopState(context_);
  }
  return nullptr;
}

void ChooseNicknameState::RenderNicknameInput() {
  // Prompt text
  const char* promptText = Language::Instance()[Language::WRITE_NICKNAME].ToChar();
  context_.screen->Print(200, 200, promptText);

  // Calculate position for nickname (after prompt text)
  int nicknameX = 200 + static_cast<int>(static_cast<float>(Language::Instance()[Language::WRITE_NICKNAME].Length() * 70) * fWRatio);

  // Current nickname
  context_.screen->Print(nicknameX, 200, currentNickname_);
}

void ChooseNicknameState::HandleInput() {
  char inputChar[2] = {0, 0};
  inputChar[0] = GInput::GetCharacterFormKeyboard();

  // Backspace - delete last character
  if (inputChar[0] == 8 && currentNickname_.Length() > 0) {
    currentNickname_.DeleteRight(1);
  }

  // Regular character - add to nickname (max 24 chars)
  if (inputChar[0] >= 0x20 && currentNickname_.Length() < 24) {
    currentNickname_ += inputChar;
  }

  // Enter - confirm nickname (must not be empty)
  if (inputChar[0] == 0x0D && !currentNickname_.IsEmpty()) {
    SaveNickname();
    shouldTransitionToMainMenu_ = true;
  }
}

void ChooseNicknameState::SaveNickname() {
  SPDLOG_INFO("Saving nickname: {}", currentNickname_.ToChar());
  context_.config.Nickname = currentNickname_;
  context_.config.SaveConfigToFile();
}

}  // namespace states
}  // namespace menu
