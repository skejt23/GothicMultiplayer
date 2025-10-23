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

#include "choose_language_state.hpp"

#include <spdlog/spdlog.h>

#include "keyboard.h"
#include "language.h"
#include "menu/states/choose_nickname_state.hpp"

namespace menu {
namespace states {

ChooseLanguageState::ChooseLanguageState(MenuContext& context) : context_(context), selectedLanguage_(0), shouldTransitionToNickname_(false) {
}

void ChooseLanguageState::OnEnter() {
  SPDLOG_INFO("Entering language selection state");

  // Set initial language from config if available
  const auto& languages = LanguageManager::Instance().GetAvailableLanguages();
  if (context_.config.lang >= 0 && context_.config.lang < static_cast<int>(languages.size())) {
    selectedLanguage_ = context_.config.lang;
  }
}

void ChooseLanguageState::OnExit() {
  SPDLOG_INFO("Exiting language selection state, selected language: {}", selectedLanguage_);
}

StateResult ChooseLanguageState::Update() {
  RenderLanguageSelection();
  HandleInput();
  return StateResult::Continue;
}

MenuState* ChooseLanguageState::CheckTransition() {
  if (shouldTransitionToNickname_) {
    return new ChooseNicknameState(context_);
  }
  return nullptr;
}

void ChooseLanguageState::ApplySelectedLanguage() {
  const auto* langInfo = LanguageManager::Instance().GetLanguage(selectedLanguage_);
  if (!langInfo) {
    SPDLOG_ERROR("Invalid language selection: {}", selectedLanguage_);
    return;
  }

  zSTRING path = LanguageManager::Instance().GetLanguageDir().c_str();
  path += langInfo->filename.c_str();

  SPDLOG_INFO("Applying language: {}", langInfo->filename);
  context_.language.LoadFromJsonFile(path.ToChar());

  // Update config
  context_.config.lang = selectedLanguage_;
  context_.config.SaveConfigToFile();
}

void ChooseLanguageState::RenderLanguageSelection() {
  const auto& languages = LanguageManager::Instance().GetAvailableLanguages();

  if (languages.empty()) {
    context_.screen->Print(200, 200, "Error: No languages available");
    return;
  }

  // Title
  context_.screen->Print(200, 200, "Choose your language:");

  // Selected language (highlighted)
  if (selectedLanguage_ >= 0 && selectedLanguage_ < static_cast<int>(languages.size())) {
    context_.screen->Print(200, 350, languages[selectedLanguage_].displayName);
  }
}

void ChooseLanguageState::HandleInput() {
  const auto& languages = LanguageManager::Instance().GetAvailableLanguages();

  if (languages.empty()) {
    return;
  }

  // Navigate left
  if (context_.input->KeyToggled(KEY_LEFT)) {
    if (selectedLanguage_ == 0) {
      selectedLanguage_ = static_cast<int>(languages.size()) - 1;
    } else {
      selectedLanguage_--;
    }
  }

  // Navigate right
  if (context_.input->KeyToggled(KEY_RIGHT)) {
    selectedLanguage_++;
    if (selectedLanguage_ >= static_cast<int>(languages.size())) {
      selectedLanguage_ = 0;
    }
  }

  // Confirm selection
  if (context_.input->KeyPressed(KEY_RETURN)) {
    ApplySelectedLanguage();
    context_.input->ClearKeyBuffer();
    shouldTransitionToNickname_ = true;
  }
}

}  // namespace states
}  // namespace menu
