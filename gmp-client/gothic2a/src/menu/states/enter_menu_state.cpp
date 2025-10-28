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

#include "enter_menu_state.hpp"

#include <spdlog/spdlog.h>

#include "main_menu.h"
#include "menu/states/choose_language_state.hpp"
#include "menu/states/main_menu_loop_state.hpp"

namespace menu {
namespace states {

EnterMenuState::EnterMenuState(MenuContext& context, InitialFlow flow) : context_(context), flow_(flow) {
}

void EnterMenuState::OnEnter() {
  SPDLOG_INFO("Entering menu bootstrap state");

  PrepareMenuEnvironment();

  switch (flow_) {
    case InitialFlow::ChooseLanguage:
      SPDLOG_INFO("Bootstrap complete, transitioning to language selection");
      nextState_ = new ChooseLanguageState(context_);
      break;
    case InitialFlow::MainMenu:
      SPDLOG_INFO("Bootstrap complete, transitioning to main menu");
      nextState_ = new MainMenuLoopState(context_);
      break;
  }
}

void EnterMenuState::OnExit() {
  SPDLOG_INFO("Leaving menu bootstrap state");
}

StateResult EnterMenuState::Update() {
  // Nothing interactive happens here; we immediately hand off to the next state.
  return StateResult::Continue;
}

MenuState* EnterMenuState::CheckTransition() {
  // Return the next state once, then clear it to prevent re-transitioning
  MenuState* result = nextState_;
  nextState_ = nullptr;
  return result;
}

void EnterMenuState::CreateTitleLogo() {
  // Clean up any existing logo first
  if (context_.logoView) {
    if (context_.screen) {
      context_.screen->RemoveItem(context_.logoView);
    }
    delete context_.logoView;
    context_.logoView = nullptr;
  }

  // Create new logo view
  context_.logoView = new zCView(0, 0, 8192, 8192, VIEW_ITEM);
  context_.logoView->SetPos(4000, 200);
  context_.logoView->InsertBack(zSTRING("GMP_LOGO_MENU.TGA"));
  context_.logoView->SetSize(5500, 2000);

  SPDLOG_INFO("Created menu logo view");
}

void EnterMenuState::SetupMenuScene() {
  // Increase spawn manager range for menu world
  oCSpawnManager::SetRemoveRange(2097152.0f);

  // Create camera weapon placeholder
  context_.cameraWeapon = zfactory->CreateItem(zCParser::GetParser()->GetIndex("ItMw_1h_Mil_Sword"));
  context_.cameraWeapon->name.Clear();
  context_.cameraWeapon->SetPositionWorld(zVEC3(13354.502930f, 2040.0f, -1141.678467f));
  context_.cameraWeapon->RotateWorldY(-150);
  context_.game->CamInit(context_.cameraWeapon, zCCamera::activeCam);

  // Create title weapon
  context_.titleWeapon = zfactory->CreateItem(zCParser::GetParser()->GetIndex("ItMw_1H_Blessed_03"));
  context_.titleWeapon->SetPositionWorld(zVEC3(13346.502930f, 2006.0f, -1240.678467f));

  SPDLOG_INFO("Created menu scene with camera and title weapon");
}

void EnterMenuState::PrepareMenuEnvironment() {
  // Set up the 3D scene first
  SetupMenuScene();

  // Build the logo view so states can show/hide it
  CreateTitleLogo();

  // Note: Player and HUD preparation is done by CMainMenu::InitializeStateMachine()
  // before the state machine is created, to avoid singleton re-entry issues
}

}  // namespace states
}  // namespace menu
