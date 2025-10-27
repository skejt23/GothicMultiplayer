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

#include "server_list_state.hpp"

#include <spdlog/spdlog.h>

#include "HooksManager.h"
#include "Patch.h"
#include "keyboard.h"
#include "main_menu.h"
#include "menu/states/exit_menu_state.hpp"
#include "menu/states/main_menu_loop_state.hpp"
#include "net_game.h"
#include "world_utils.hpp"

namespace menu {
namespace states {

ServerListState::ServerListState(MenuContext& context)
    : context_(context), shouldReturnToMainMenu_(false), shouldConnectToServer_(false), enteringCustomIP_(false) {
}

void ServerListState::OnEnter() {
  SPDLOG_INFO("Entering server list state, extended server list at {:p}", (void*)context_.extendedServerList);

  // Hide logo
  if (context_.logoView) {
    context_.screen->RemoveItem(context_.logoView);
  }

  // Enable title weapon rendering
  context_.ShowTitleWeapon();

  // Check if we're doing a fast join (F5 or pre-set IP)
  bool fastJoin = !context_.selectedServerIP.IsEmpty() && context_.selectedServerIndex == -1;

  if (!fastJoin) {
    // Normal entry - start in server list mode
    context_.selectedServerIndex = 0;
    context_.selectedServerIP.Clear();
    enteringCustomIP_ = false;

    // Refresh server list
    if (context_.extendedServerList) {
      context_.extendedServerList->RefreshList();
    }
  } else {
    // Fast join mode - auto-connect
    SPDLOG_INFO("Fast join to: {}", context_.selectedServerIP.ToChar());
    enteringCustomIP_ = true;       // We're in custom IP mode
    shouldConnectToServer_ = true;  // Trigger immediate connection
  }
}

void ServerListState::OnExit() {
  SPDLOG_INFO("Exiting server list state");

  // Restore logo (unless we're connecting to a server)
  if (context_.logoView && !shouldConnectToServer_) {
    context_.screen->InsertItem(context_.logoView);
  }
}

StateResult ServerListState::Update() {
  // Check if we should connect first (for fast join)
  if (shouldConnectToServer_) {
    ConnectToServer();
    return StateResult::Continue;  // Don't do anything else if we're connecting
  }

  UpdateTitleWeapon();

  // Handle common input first (Enter to connect, ESC to exit)
  HandleCommonInput();

  // Then handle mode-specific input and rendering
  if (enteringCustomIP_) {
    RenderCustomIPEntry();
    HandleCustomIPInput();
  } else {
    HandleServerListInput();
    RenderServerList();
  }

  return StateResult::Continue;
}

MenuState* ServerListState::CheckTransition() {
  if (shouldReturnToMainMenu_) {
    return new MainMenuLoopState(context_);
  }

  // If we successfully connected, transition to exit state for cleanup
  if (shouldConnectToServer_) {
    return new ExitMenuState(context_);
  }

  return nullptr;
}

void ServerListState::RenderServerList() {
  // Delegate input and rendering to ExtendedServerList
  // (matching original PrintMenu() logic for SERVER_LIST case)
  if (context_.extendedServerList) {
    context_.extendedServerList->HandleInput();
    context_.extendedServerList->Draw();
  }
}

void ServerListState::RenderCustomIPEntry() {
  // Display custom IP entry
  context_.screen->SetFont("FONT_OLD_20_WHITE.TGA");
  context_.screen->SetFontColor({255, 255, 255});
  context_.screen->PrintCXY(context_.selectedServerIP);
}

void ServerListState::HandleInput() {
  // This method is now split into HandleCommonInput, HandleServerListInput and HandleCustomIPInput
}

void ServerListState::HandleCommonInput() {
  // ESC to return to main menu (works in both modes)
  if (context_.input->KeyPressed(KEY_ESCAPE)) {
    context_.input->ClearKeyBuffer();
    shouldReturnToMainMenu_ = true;
    return;
  }

  // Enter to connect (works in both modes)
  if (context_.input->KeyPressed(KEY_RETURN)) {
    SPDLOG_INFO("Enter pressed, mode: {}, IP: '{}'", enteringCustomIP_ ? "custom IP" : "server list", context_.selectedServerIP.ToChar());

    if (enteringCustomIP_) {
      // Custom IP mode - connect to typed IP
      if (!context_.selectedServerIP.IsEmpty()) {
        SPDLOG_INFO("Connecting to custom IP: {}", context_.selectedServerIP.ToChar());
        context_.input->ClearKeyBuffer();
        shouldConnectToServer_ = true;
      } else {
        SPDLOG_WARN("Cannot connect: IP address is empty");
      }
    } else {
      // Server list mode - connect to selected server
      if (context_.selectedServerIndex != -1) {
        // Get selected server IP
        context_.selectedServerIP.Clear();
        char buffer[256];
        if (context_.extendedServerList && context_.extendedServerList->getSelectedServer(buffer, sizeof(buffer))) {
          context_.selectedServerIP += buffer;
          SPDLOG_INFO("Connecting to selected server: {}", context_.selectedServerIP.ToChar());
        }
      }
      shouldConnectToServer_ = true;
    }
    return;
  }
}

void ServerListState::HandleServerListInput() {
  // W to enter custom IP mode
  if (context_.input->KeyPressed(KEY_W)) {
    context_.input->ClearKeyBuffer();
    enteringCustomIP_ = true;
    context_.selectedServerIndex = -1;
    context_.selectedServerIP.Clear();
    return;
  }

  // Slash key to enter custom IP mode
  if (context_.input->KeyToggled(KEY_SLASH)) {
    enteringCustomIP_ = true;
    context_.selectedServerIndex = -1;
    context_.selectedServerIP.Clear();
    return;
  }
}

void ServerListState::HandleCustomIPInput() {
  // W to cancel custom IP entry and return to server list
  if (context_.input->KeyPressed(KEY_W)) {
    context_.input->ClearKeyBuffer();
    enteringCustomIP_ = false;
    context_.selectedServerIndex = 0;
    context_.selectedServerIP.Clear();
    return;
  }

  // Slash key to return to server list
  if (context_.input->KeyToggled(KEY_SLASH)) {
    enteringCustomIP_ = false;
    context_.selectedServerIndex = 0;
    context_.selectedServerIP.Clear();
    return;
  }

  // F1 for quick localhost
  if (context_.input->KeyToggled(KEY_F1)) {
    context_.selectedServerIP = "127.0.0.1";
    SPDLOG_INFO("F1 pressed, IP set to: {}", context_.selectedServerIP.ToChar());
  }

  // Character input
  char x[2] = {0, 0};
  x[0] = GInput::GetCharacterFormKeyboard(true);

  // Add character (printable ASCII, but exclude Enter)
  if (x[0] > 0x20 && x[0] != 0x0D) {
    context_.selectedServerIP += x;
    SPDLOG_DEBUG("Added character, IP now: '{}'", context_.selectedServerIP.ToChar());
  }

  // Backspace
  if ((x[0] == 0x08) && (context_.selectedServerIP.Length() > 0)) {
    context_.selectedServerIP.DeleteRight(1);
    SPDLOG_DEBUG("Backspace, IP now: '{}'", context_.selectedServerIP.ToChar());
  }
}

void ServerListState::ConnectToServer() {
  context_.input->ClearKeyBuffer();

  // Attempt connection
  if (!NetGame::Instance().Connect(context_.selectedServerIP.ToChar())) {
    SPDLOG_ERROR("Unable to connect to the server: {}", context_.selectedServerIP.ToChar());
    shouldConnectToServer_ = false;

    // Reset to normal server list mode on connection failure
    context_.selectedServerIP.Clear();
    context_.selectedServerIndex = 0;
    enteringCustomIP_ = false;

    // Refresh server list
    if (context_.extendedServerList) {
      context_.extendedServerList->RefreshList();
    }
    return;
  }

  // If connected, proceed with game setup
  if (NetGame::Instance().IsConnected()) {
    SPDLOG_INFO("Connected to server: {}", context_.selectedServerIP.ToChar());

    // Handle initial network sync
    NetGame::Instance().HandleNetwork();
    NetGame::Instance().SyncGameTime();

    // Save spawn position
    zVEC3 spawnPosition = context_.player->GetPositionWorld();

    // Enable player interface
    Patch::PlayerInterfaceEnabled(true);

    // Change level if needed
    if (!NetGame::Instance().map.IsEmpty()) {
      Patch::ChangeLevelEnabled(true);
      ogame->ChangeLevel(NetGame::Instance().map, zSTRING("????"));
      Patch::ChangeLevelEnabled(false);
    }

    // Clean up NPCs and world objects
    DeleteAllNpcsAndDisableSpawning();
    context_.player->trafoObjToWorld.SetTranslation(spawnPosition);
    CleanupWorldObjects(ogame->GetGameWorld());

    // Join game
    NetGame::Instance().JoinGame();

    // shouldConnectToServer_ stays true, which will trigger transition to ExitMenuState
    // The ExitMenuState will handle all menu cleanup
  } else {
    SPDLOG_ERROR("Connection failed");
    shouldConnectToServer_ = false;

    // Reset to normal server list mode on connection failure
    context_.selectedServerIP.Clear();
    context_.selectedServerIndex = 0;
    enteringCustomIP_ = false;

    // Refresh server list
    if (context_.extendedServerList) {
      context_.extendedServerList->RefreshList();
    }
  }
}

void ServerListState::UpdateTitleWeapon() {
  if (context_.titleWeapon) {
    context_.titleWeapon->RotateWorldX(0.6f);
  }
}

}  // namespace states
}  // namespace menu
