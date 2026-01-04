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

#include "main_menu_loop_state.hpp"

#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>
#include <windows.h>

#include <string>

#include "config.h"
#include "gmp_core.h"
#include "keyboard.h"
#include "language.h"
#include "menu/states/online_options_state.hpp"
#include "menu/states/options_menu_state.hpp"
#include "menu/states/server_list_state.hpp"
#include "version.h"

// External declarations from main_menu.cpp (global namespace)
extern zCOLOR Normal;
extern zCOLOR Highlighted;

// FDefault is constexpr in main_menu.cpp, so we can't extern it - just define it here
constexpr const char* FDefault = "FONT_DEFAULT.TGA";

namespace menu {
namespace states {

namespace {
// Version string stored as std::string to avoid zSTRING static destruction issues.
// zSTRING uses Gothic's allocator which may be invalid during CRT shutdown.
std::string g_version_string_storage;

const char* GetVersionString() {
  if (g_version_string_storage.empty()) {
    constexpr std::string_view version = GIT_TAG_LONG;
    g_version_string_storage = version.empty() ? "Unknown build" : std::string(version);
  }
  return g_version_string_storage.c_str();
}
}  // namespace

MainMenuLoopState::MainMenuLoopState(MenuContext& context) : context_(context), selectedMenuItem_(CHOOSE_SERVER), nextState_(nullptr) {
}

MainMenuLoopState::~MainMenuLoopState() {
  SPDLOG_INFO("~MainMenuLoopState: destructor called");
}

void MainMenuLoopState::OnEnter() {
  SPDLOG_INFO("Entering main menu state");

  // Show GMP logo
  if (context_.logoView) {
    context_.screen->InsertItem(context_.logoView);
  }

  // Enable title weapon animation
  context_.ShowTitleWeapon();
}

void MainMenuLoopState::OnExit() {
  SPDLOG_INFO("Exiting main menu state");
}

StateResult MainMenuLoopState::Update() {
  context_.scene.Update();
  RenderMenu();
  RenderVersionInfo();
  HandleInput();
  return StateResult::Continue;
}

MenuState* MainMenuLoopState::CheckTransition() {
  return nextState_;
}

void MainMenuLoopState::RenderMenu() {
  context_.screen->SetFont("FONT_OLD_20_WHITE.TGA");

  // Render menu items
  zCOLOR fcolor;

  // Choose Server
  fcolor = (selectedMenuItem_ == CHOOSE_SERVER) ? Highlighted : Normal;
  context_.screen->SetFontColor(fcolor);
  context_.screen->Print(200, 3200, Language::Instance()[Language::MMENU_CHSERVER]);

  // Options
  fcolor = (selectedMenuItem_ == OPTIONS) ? Highlighted : Normal;
  context_.screen->SetFontColor(fcolor);
  context_.screen->Print(200, 3600, Language::Instance()[Language::MMENU_OPTIONS]);

  // Online Options
  fcolor = (selectedMenuItem_ == ONLINE_OPTIONS) ? Highlighted : Normal;
  context_.screen->SetFontColor(fcolor);
  context_.screen->Print(200, 4000, Language::Instance()[Language::MMENU_ONLINEOPTIONS]);

  // Leave Game
  fcolor = (selectedMenuItem_ == LEAVE_GAME) ? Highlighted : Normal;
  context_.screen->SetFontColor(fcolor);
  context_.screen->Print(200, 4400, Language::Instance()[Language::MMENU_LEAVEGAME]);
}

void MainMenuLoopState::RenderVersionInfo() {
  context_.screen->SetFont(FDefault);
  context_.screen->SetFontColor(Normal);

  // D3D9/D3D11 experimental warning in top left
  auto renderer_type = Config::Instance().GetRendererType();
  if (renderer_type == Config::RendererType::D3D9) {
    context_.screen->Print(100, 100, "D3D9 experimental (may display visual glitches)");
  } else if (renderer_type == Config::RendererType::D3D11) {
    context_.screen->Print(100, 100, "D3D11 (not yet fully implemented)");
  }

  // Version in bottom right
  zSTRING version_text = GetVersionString();
#ifndef NDEBUG
  version_text += " (Debug)";
#endif
  context_.screen->Print(8192 - context_.screen->FontSize(version_text), 8192 - context_.screen->FontY(), version_text);

  // Shortcut in bottom left
  context_.screen->Print(100, 8192 - context_.screen->FontY(), "F5 - Fast join localhost server");
}

void MainMenuLoopState::HandleInput() {
  // Navigate up
  if (context_.input->KeyToggled(KEY_UP)) {
    if (selectedMenuItem_ == 0) {
      selectedMenuItem_ = MENU_ITEM_COUNT - 1;
    } else {
      selectedMenuItem_--;
    }
  }

  // Navigate down
  if (context_.input->KeyToggled(KEY_DOWN)) {
    selectedMenuItem_++;
    if (selectedMenuItem_ >= MENU_ITEM_COUNT) {
      selectedMenuItem_ = 0;
    }
  }

  // Select menu item
  if (context_.input->KeyPressed(KEY_RETURN)) {
    context_.input->ClearKeyBuffer();
    ExecuteMenuItem(static_cast<MenuItem>(selectedMenuItem_));
  }

  // Shortcut keys
  if (context_.input->KeyToggled(KEY_F5)) {
    // Quick localhost join
    context_.selectedServerIndex = -1;
    context_.selectedServerIP = "127.0.0.1";
    nextState_ = new ServerListState(context_);
  }
}

void MainMenuLoopState::ExecuteMenuItem(MenuItem item) {
  switch (item) {
    case CHOOSE_SERVER:
      SPDLOG_INFO("Selected: Choose Server");
      nextState_ = new ServerListState(context_);
      break;

    case OPTIONS:
      SPDLOG_INFO("Selected: Options");
      nextState_ = new OptionsMenuState(context_);
      break;

    case ONLINE_OPTIONS:
      SPDLOG_INFO("Selected: Online Options");
      nextState_ = new OnlineOptionsState(context_);
      break;

    case LEAVE_GAME:
      SPDLOG_INFO("Selected: Leave Game");
      GMPCore::ExitGame(0);
      break;
  }
}

}  // namespace states
}  // namespace menu
