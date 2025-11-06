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

#include "options_menu_state.hpp"

#include <spdlog/spdlog.h>

#include <cstring>

#include "keyboard.h"
#include "menu/states/main_menu_loop_state.hpp"

namespace menu {
namespace states {

OptionsMenuState::OptionsMenuState(MenuContext& context) : context_(context), optionsMenu_(nullptr), shouldReturnToMainMenu_(false) {
}

void OptionsMenuState::OnEnter() {
  SPDLOG_INFO("Entering options menu state");

  // Lock player movement
  if (context_.player && !context_.player->IsMovLock()) {
    context_.player->SetMovLock(1);
  }

  // Create and run Gothic's native options menu
  if (!optionsMenu_) {
    optionsMenu_ = zCMenu::Create(zSTRING("MENU_OPTIONS"));
  }

  if (optionsMenu_) {
    optionsMenu_->Run();
  }
}

void OptionsMenuState::OnExit() {
  SPDLOG_INFO("Exiting options menu state");
  ApplySettings();

  // Leave the menu properly
  if (optionsMenu_) {
    context_.input->ClearKeyBuffer();
    optionsMenu_->Leave();
  }

  // Ensure player is locked
  if (context_.player) {
    context_.player->SetMovLock(1);
  }
}

StateResult OptionsMenuState::Update() {
  context_.scene.Update();
  CheckForMenuExit();
  return StateResult::Continue;
}

MenuState* OptionsMenuState::CheckTransition() {
  if (shouldReturnToMainMenu_) {
    return new MainMenuLoopState(context_);
  }
  return nullptr;
}

void OptionsMenuState::CheckForMenuExit() {
  // Check if user pressed ESC while in the options menu
  if (zCMenu::GetActive()) {
    const char* menuName = zCMenu::GetActive()->GetName().ToChar();
    if (std::memcmp("MENU_OPTIONS", menuName, 12) == 0 && context_.input->KeyPressed(KEY_ESCAPE)) {
      shouldReturnToMainMenu_ = true;
      return;
    }

    // Check if user selected "Back" item and pressed Enter
    zCMenuItem* activeItem = optionsMenu_ ? optionsMenu_->GetActiveItem() : nullptr;
    if (activeItem) {
      const char* itemName = activeItem->GetName().ToChar();
      if (std::memcmp("MENUITEM_OPT_BACK", itemName, 17) == 0 && context_.input->KeyPressed(KEY_RETURN)) {
        shouldReturnToMainMenu_ = true;
      }
    }
  }
}

void OptionsMenuState::ApplySettings() {
  // Apply any changed settings from the Gothic menu
  if (context_.game) {
    gameMan->ApplySomeSettings();
  }

  // Update screen resolution in case it changed
  int newResX = zoptions->ReadInt(zOPT_SEC_VIDEO, "zVidResFullscreenX", 320);
  SPDLOG_INFO("Screen resolution after options: {}x", newResX);
}

}  // namespace states
}  // namespace menu
