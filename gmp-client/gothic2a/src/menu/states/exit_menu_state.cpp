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

#include "exit_menu_state.hpp"

#include <spdlog/spdlog.h>

namespace menu {
namespace states {

ExitMenuState::ExitMenuState(MenuContext& context) : context_(context) {
}

void ExitMenuState::OnEnter() {
  SPDLOG_INFO("Entering exit menu state - beginning cleanup");
  CleanUpMenuResources();
  cleanupComplete_ = true;
}

void ExitMenuState::OnExit() {
  SPDLOG_INFO("Exiting exit menu state");
  // Nothing to do here - cleanup already done in OnEnter
}

StateResult ExitMenuState::Update() {
  // After cleanup is done, signal exit
  if (cleanupComplete_) {
    return StateResult::Exit;
  }
  return StateResult::Continue;
}

MenuState* ExitMenuState::CheckTransition() {
  // No transitions - we just exit via Update()
  return nullptr;
}

void ExitMenuState::CleanUpMenuResources() {
  SPDLOG_INFO("Cleaning up all menu resources");

  // Stop all sounds
  if (context_.soundSystem) {
    context_.soundSystem->StopAllSounds();
  }

  // Remove weapons from world
  context_.HideTitleWeapon();
  if (context_.titleWeapon) {
    context_.titleWeapon = nullptr;
  }
  if (context_.cameraWeapon) {
    context_.cameraWeapon->RemoveVobFromWorld();
    context_.cameraWeapon = nullptr;
  }
  if (context_.appearanceWeapon) {
    context_.appearanceWeapon->RemoveVobFromWorld();
    context_.appearanceWeapon = nullptr;
  }
  context_.appearanceCameraCreated = false;

  // Unlock player movement
  if (context_.player) {
    context_.player->SetMovLock(0);
  }

  // Enable health bar
  context_.EnableHealthBar();

  // Remove logo
  if (context_.logoView) {
    context_.screen->RemoveItem(context_.logoView);
    delete context_.logoView;
    context_.logoView = nullptr;
  }

  // Reset game time
  if (context_.game) {
    context_.game->GetWorldTimer()->SetDay(1);
    context_.game->GetWorldTimer()->SetTime(12, 0);
  }

  // Reset font
  if (context_.screen) {
    context_.screen->SetFont("FONT_DEFAULT.TGA");
  }

  context_.game->CamInit();

  SPDLOG_INFO("Menu cleanup complete");
}

}  // namespace states
}  // namespace menu
