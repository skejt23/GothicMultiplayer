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

#include <algorithm>
#include <cstdint>
#include <string>

#include <spdlog/spdlog.h>

#include "gmp_core.h"
#include "character_appearance.h"
#include "content/content_transition_manager.h"
#include "Interface.h"
#include "language.h"
#include "world_utils.hpp"
#include "Patch.h"
#include "keyboard.h"
#include "main_menu.h"
#include "menu/states/exit_menu_state.hpp"
#include "menu/states/main_menu_loop_state.hpp"
#include "net_game.h"
#include "world_utils.hpp"

namespace menu {
namespace states {

namespace {

constexpr int kProgressFrameLeft = 1000;
constexpr int kProgressFrameTop = 6800;
constexpr int kProgressFrameRight = 7192;
constexpr int kProgressFrameBottom = 7300;
constexpr int kProgressFillInsetX = 900;
constexpr int kProgressFillInsetY = 75;
constexpr int kBannerTextLeft = 700;
constexpr int kBannerTextTop = 5600;
constexpr int kBannerTextRight = 7492;
constexpr int kBannerTextBottom = 6050;
constexpr int kProgressTextLeft = 700;
constexpr int kProgressTextTop = 6100;
constexpr int kProgressTextRight = 7492;
constexpr int kProgressTextBottom = 6700;
constexpr int kListStatusTop = 6650;
constexpr int kListNavigationHintTop = 7050;
constexpr int kListFavoriteHintTop = 7400;
constexpr int kListOtherHintTop = 7750;
constexpr int kEndpointTitleTop = 3150;
constexpr int kEndpointValueTop = 3850;
constexpr int kEndpointErrorTop = 4550;
constexpr int kEndpointHintTop = 7350;
constexpr const char* kProgressFrameTexture = "PROGRESS.TGA";
constexpr const char* kProgressFillTexture = "PROGRESS_BAR.TGA";
constexpr int kNewGameSlot = -2;

void PrintCentered(zCView* view, int y, const char* text) {
  if (!view || !text) {
    return;
  }
  const int x = std::max(0, (8192 - view->FontSize(text)) / 2);
  view->Print(x, y, text);
}

}  // namespace

ServerListState::ServerListState(MenuContext& context)
    : context_(context),
      shouldReturnToMainMenu_(false),
      shouldConnectToServer_(false),
      shouldExitMenuAfterConnection_(false),
      endpointEntryMode_(EndpointEntryMode::None),
      connectionAttemptInProgress_(false) {
}

ServerListState::~ServerListState() { CloseConnectionProgress(); }

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
    endpointEntryMode_ = EndpointEntryMode::None;
    endpointEntryError_.clear();
    serverListStatus_.clear();

    // Refresh server list
    if (context_.extendedServerList) {
      context_.extendedServerList->RefreshList();
    }
  } else {
    // Fast join mode - auto-connect
    SPDLOG_INFO("Fast join to: {}", context_.selectedServerIP.ToChar());
    endpointEntryMode_ = EndpointEntryMode::DirectConnect;
    shouldConnectToServer_ = true;  // Trigger immediate connection
  }
}

void ServerListState::OnExit() {
  SPDLOG_INFO("Exiting server list state");
  CloseConnectionProgress();

  // Restore logo (unless we're connecting to a server)
  if (context_.logoView && !shouldExitMenuAfterConnection_) {
    context_.screen->InsertItem(context_.logoView);
  }
}

StateResult ServerListState::Update() {
  if (connectionAttemptInProgress_) {
    // Pump transport callbacks and gameplay messages while still in menus.
    NetGame::Instance().HandleNetwork();
  }

  // Check if we should initiate connection (for fast join or user selection)
  if (shouldConnectToServer_) {
    ConnectToServer();
    shouldConnectToServer_ = false;       // Only call ConnectToServer once
    connectionAttemptInProgress_ = true;  // Mark that we're attempting to connect
  }

  const auto progress = NetGame::Instance().GetConnectionProgressDisplay();
  if (progress.visible) {
    RenderConnectionProgress();
    if (progress.failure_expired) {
      NetGame::Instance().ClearConnectionProgressDisplay();
      NetGame::Instance().Disconnect();
      context_.sceneManager.Configure(context_.config.extended_menu_scenes);
      connectionAttemptInProgress_ = false;
      context_.selectedServerIP.Clear();
      context_.selectedServerIndex = 0;
      endpointEntryMode_ = EndpointEntryMode::None;
      shouldReturnToMainMenu_ = true;
    }
    return StateResult::Continue;
  }
  CloseConnectionProgress();

  // Check packet-driven connection status only if we have an active connection attempt
  if (connectionAttemptInProgress_ && NetGame::Instance().game_client) {
    auto connState = NetGame::Instance().game_client->GetConnectionState();

    if (connState == gmp::client::GameClient::ConnectionState::Connecting) {
      // OnConnectionStarted owns the visible progress state while connecting.
      return StateResult::Continue;
    } else if (connState == gmp::client::GameClient::ConnectionState::Connected) {
      // Connection successful - schedule game setup to run outside render loop
      if (NetGame::Instance().IsConnected() && NetGame::Instance().IsReadyToJoin) {
        ScheduleGameSetup();
        connectionAttemptInProgress_ = false;
        // Trigger transition to ExitMenuState
        shouldExitMenuAfterConnection_ = true;
      }
      return StateResult::Continue;
    } else if (connState == gmp::client::GameClient::ConnectionState::Failed) {
      // Connection failed - handle it once and reset
      HandleConnectionFailure();
      connectionAttemptInProgress_ = false;  // Clear the flag so we don't handle it again
      // Disconnect to reset the connection state back to Disconnected
      NetGame::Instance().Disconnect();
      context_.sceneManager.Configure(context_.config.extended_menu_scenes);
    }
  }
  // Handle common input first (Enter to connect, ESC to exit)
  HandleCommonInput();

  // Then handle mode-specific input and rendering
  if (endpointEntryMode_ != EndpointEntryMode::None) {
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

  if (shouldExitMenuAfterConnection_) {
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

  const auto font = Language::Instance().ApplyFontPrefix("FONT_OLD_10_WHITE.TGA");
  context_.screen->SetFont(font.c_str());
  if (!serverListStatus_.empty() && std::chrono::steady_clock::now() < serverListStatusExpiresAt_) {
    context_.screen->SetFontColor({80, 220, 120});
    PrintCentered(context_.screen, kListStatusTop, serverListStatus_.c_str());
  } else {
    serverListStatus_.clear();
  }
  context_.screen->SetFontColor({255, 255, 255});
  PrintCentered(context_.screen, kListNavigationHintTop,
                Language::Instance()[Language::SRVLIST_HINT_NAVIGATION].ToChar());
  PrintCentered(context_.screen, kListFavoriteHintTop,
                Language::Instance()[Language::SRVLIST_HINT_FAVOURITES].ToChar());
  PrintCentered(context_.screen, kListOtherHintTop,
                Language::Instance()[Language::SRVLIST_HINT_OTHER].ToChar());
}

void ServerListState::RenderCustomIPEntry() {
  const auto title = endpointEntryMode_ == EndpointEntryMode::AddFavorite
                         ? Language::Instance()[Language::SRVLIST_ADD_FAVOURITE_TITLE].ToChar()
                         : Language::Instance()[Language::SRVLIST_DIRECT_TITLE].ToChar();

  const auto large_font = Language::Instance().ApplyFontPrefix("FONT_OLD_20_WHITE.TGA");
  context_.screen->SetFont(large_font.c_str());
  context_.screen->SetFontColor({255, 255, 255});
  PrintCentered(context_.screen, kEndpointTitleTop, title);

  context_.screen->SetFontColor({80, 220, 120});
  PrintCentered(context_.screen, kEndpointValueTop,
                context_.selectedServerIP.IsEmpty() ? "IP[:port]" : context_.selectedServerIP.ToChar());

  if (!endpointEntryError_.empty()) {
    context_.screen->SetFontColor({255, 80, 80});
    PrintCentered(context_.screen, kEndpointErrorTop, endpointEntryError_.c_str());
  }

  const auto small_font = Language::Instance().ApplyFontPrefix("FONT_OLD_10_WHITE.TGA");
  context_.screen->SetFont(small_font.c_str());
  context_.screen->SetFontColor({255, 255, 255});
  PrintCentered(context_.screen, kEndpointHintTop,
                Language::Instance()[Language::SRVLIST_ENTRY_HINT].ToChar());
}

void ServerListState::HandleInput() {
  // This method is now split into HandleCommonInput, HandleServerListInput and HandleCustomIPInput
}

void ServerListState::HandleCommonInput() {
  if (context_.input->KeyPressed(KEY_ESCAPE)) {
    context_.input->ClearKeyBuffer();
    if (endpointEntryMode_ != EndpointEntryMode::None) {
      CancelEndpointEntry();
      return;
    }
    shouldReturnToMainMenu_ = true;
    return;
  }

  if (context_.input->KeyPressed(KEY_RETURN)) {
    context_.input->ClearKeyBuffer();
    SPDLOG_INFO("Enter pressed, endpoint entry mode: {}, IP: '{}'", static_cast<int>(endpointEntryMode_),
                context_.selectedServerIP.ToChar());

    if (endpointEntryMode_ == EndpointEntryMode::AddFavorite) {
      if (!context_.extendedServerList) {
        endpointEntryError_ = Language::Instance()[Language::SRVLIST_FAVOURITE_SAVE_FAILED].ToChar();
        return;
      }

      const auto result = context_.extendedServerList->AddFavorite(context_.selectedServerIP.ToChar());
      switch (result) {
        case FavoriteAddResult::Added:
          serverListStatus_ = Language::Instance()[Language::SRVLIST_FAVOURITE_ADDED].ToChar();
          serverListStatusExpiresAt_ = std::chrono::steady_clock::now() + std::chrono::seconds(4);
          context_.extendedServerList->selectTab(TAB_FAV);
          CancelEndpointEntry();
          break;
        case FavoriteAddResult::AlreadyExists:
          serverListStatus_ = Language::Instance()[Language::SRVLIST_FAVOURITE_EXISTS].ToChar();
          serverListStatusExpiresAt_ = std::chrono::steady_clock::now() + std::chrono::seconds(4);
          context_.extendedServerList->selectTab(TAB_FAV);
          CancelEndpointEntry();
          break;
        case FavoriteAddResult::InvalidEndpoint:
          endpointEntryError_ = Language::Instance()[Language::SRVLIST_INVALID_ENDPOINT].ToChar();
          break;
        case FavoriteAddResult::SaveFailed:
          endpointEntryError_ = Language::Instance()[Language::SRVLIST_FAVOURITE_SAVE_FAILED].ToChar();
          break;
      }
    } else if (endpointEntryMode_ == EndpointEntryMode::DirectConnect) {
      std::string host;
      std::uint32_t port = 0;
      if (!gmp::client::ParseServerEndpoint(context_.selectedServerIP.ToChar(), host, port)) {
        endpointEntryError_ = Language::Instance()[Language::SRVLIST_INVALID_ENDPOINT].ToChar();
        return;
      }
      SPDLOG_INFO("Connecting to custom IP: {}", context_.selectedServerIP.ToChar());
      shouldConnectToServer_ = true;
    } else {
      if (context_.selectedServerIndex != -1) {
        context_.selectedServerIP.Clear();
        char buffer[256];
        if (context_.extendedServerList && context_.extendedServerList->getSelectedServer(buffer, sizeof(buffer))) {
          context_.selectedServerIP += buffer;
          SPDLOG_INFO("Connecting to selected server: {}", context_.selectedServerIP.ToChar());
          shouldConnectToServer_ = true;
        } else {
          SPDLOG_WARN("Cannot connect: no server is selected");
        }
      }
    }
    return;
  }
}

void ServerListState::HandleServerListInput() {
  if (context_.input->KeyPressed(KEY_W)) {
    context_.input->ClearKeyBuffer();
    BeginEndpointEntry(EndpointEntryMode::DirectConnect);
    return;
  }

  if (context_.input->KeyToggled(KEY_SLASH)) {
    BeginEndpointEntry(EndpointEntryMode::DirectConnect);
    return;
  }

  if (context_.input->KeyPressed(KEY_F)) {
    context_.input->ClearKeyBuffer();
    BeginEndpointEntry(EndpointEntryMode::AddFavorite);
    return;
  }
}

void ServerListState::HandleCustomIPInput() {
  if (context_.input->KeyPressed(KEY_W)) {
    context_.input->ClearKeyBuffer();
    CancelEndpointEntry();
    return;
  }

  if (context_.input->KeyToggled(KEY_SLASH)) {
    CancelEndpointEntry();
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

  // Keep endpoint input bounded so it remains usable on the fixed-size menu.
  if (x[0] > 0x20 && x[0] != 0x0D && context_.selectedServerIP.Length() < 255) {
    context_.selectedServerIP += x;
    endpointEntryError_.clear();
    SPDLOG_DEBUG("Added character, IP now: '{}'", context_.selectedServerIP.ToChar());
  }

  // Backspace
  if ((x[0] == 0x08) && (context_.selectedServerIP.Length() > 0)) {
    context_.selectedServerIP.DeleteRight(1);
    endpointEntryError_.clear();
    SPDLOG_DEBUG("Backspace, IP now: '{}'", context_.selectedServerIP.ToChar());
  }
}

void ServerListState::BeginEndpointEntry(EndpointEntryMode mode) {
  endpointEntryMode_ = mode;
  endpointEntryError_.clear();
  serverListStatus_.clear();
  context_.selectedServerIndex = -1;
  context_.selectedServerIP.Clear();
}

void ServerListState::CancelEndpointEntry() {
  endpointEntryMode_ = EndpointEntryMode::None;
  endpointEntryError_.clear();
  context_.selectedServerIndex = 0;
  context_.selectedServerIP.Clear();
}

void ServerListState::ConnectToServer() {
  context_.input->ClearKeyBuffer();

  SPDLOG_INFO("Starting connection to: {}", context_.selectedServerIP.ToChar());

  // Start the connection attempt. Completion is reported by transport callbacks in Update().
  NetGame::Instance().Connect(context_.selectedServerIP.ToChar());
}

void ServerListState::RenderConnectionProgress() {
  if (!connectionBackground_) {
    connectionBackground_ = new zCView(0, 0, 8192, 8192, VIEW_ITEM);
    connectionBackground_->InsertBack(zSTRING(gmp::gothic::kGmpLoadingScreenTexture));
    context_.screen->InsertItem(connectionBackground_);
  }
  if (!connectionProgressFrame_) {
    connectionProgressFrame_ =
        new zCView(kProgressFrameLeft, kProgressFrameTop, kProgressFrameRight, kProgressFrameBottom, VIEW_ITEM);
    connectionProgressFrame_->InsertBack(zSTRING(kProgressFrameTexture));
    connectionProgressFrame_->SetAlphaBlendFunc(zRND_ALPHA_FUNC_BLEND);
    connectionProgressFrame_->SetTransparency(255);
    context_.screen->InsertItem(connectionProgressFrame_);
  }
  if (!connectionProgressFill_) {
    connectionProgressFill_ = new zCView(
        kProgressFrameLeft + kProgressFillInsetX, kProgressFrameTop + kProgressFillInsetY,
        kProgressFrameRight - kProgressFillInsetX, kProgressFrameBottom - kProgressFillInsetY, VIEW_ITEM);
    connectionProgressFill_->InsertBack(zSTRING(kProgressFillTexture));
    connectionProgressFill_->SetAlphaBlendFunc(zRND_ALPHA_FUNC_BLEND);
    connectionProgressFill_->SetTransparency(255);
    context_.screen->InsertItem(connectionProgressFill_);
  }
  if (!connectionProgressText_) {
    connectionProgressText_ =
        new zCView(kProgressTextLeft, kProgressTextTop, kProgressTextRight, kProgressTextBottom, VIEW_ITEM);
    const auto font = Language::Instance().ApplyFontPrefix("FONT_OLD_20_WHITE.TGA");
    connectionProgressText_->SetFont(zSTRING(font.c_str()));
    connectionProgressText_->SetFontColor(zCOLOR(255, 255, 255, 255));
    context_.screen->InsertItem(connectionProgressText_);
  }
  if (!connectionBannerText_) {
    connectionBannerText_ =
        new zCView(kBannerTextLeft, kBannerTextTop, kBannerTextRight, kBannerTextBottom, VIEW_ITEM);
    const auto font = Language::Instance().ApplyFontPrefix("FONT_OLD_20_WHITE.TGA");
    connectionBannerText_->SetFont(zSTRING(font.c_str()));
    connectionBannerText_->SetFontColor(zCOLOR(0, 200, 255, 255));
    context_.screen->InsertItem(connectionBannerText_);
  }

  const auto progress = NetGame::Instance().GetConnectionProgressDisplay();
  const int percent = std::clamp(progress.percent, 0, 100);
  const int maximum_fill_width =
      (kProgressFrameRight - kProgressFrameLeft) - (2 * kProgressFillInsetX);
  connectionProgressFill_->SetSize((maximum_fill_width * percent) / 100,
                                   (kProgressFrameBottom - kProgressFrameTop) - (2 * kProgressFillInsetY));

  if (percent != renderedConnectionProgressPercent_ || progress.message != renderedConnectionProgressMessage_) {
    std::string text = progress.message;
    if (!progress.failed) {
      text += " (" + std::to_string(percent) + "%)";
    }
    connectionProgressText_->ClrPrintwin();
    connectionProgressText_->PrintCXY(zSTRING(text.c_str()));
    renderedConnectionProgressPercent_ = percent;
    renderedConnectionProgressMessage_ = progress.message;
  }
  if (progress.banner != renderedConnectionBanner_) {
    connectionBannerText_->ClrPrintwin();
    if (!progress.banner.empty()) {
      connectionBannerText_->PrintCXY(zSTRING(progress.banner.c_str()));
    }
    renderedConnectionBanner_ = progress.banner;
  }
}

void ServerListState::CloseConnectionProgress() {
  if (connectionBannerText_) {
    context_.screen->RemoveItem(connectionBannerText_);
    delete connectionBannerText_;
    connectionBannerText_ = nullptr;
  }
  if (connectionProgressText_) {
    context_.screen->RemoveItem(connectionProgressText_);
    delete connectionProgressText_;
    connectionProgressText_ = nullptr;
  }
  if (connectionProgressFill_) {
    context_.screen->RemoveItem(connectionProgressFill_);
    delete connectionProgressFill_;
    connectionProgressFill_ = nullptr;
  }
  if (connectionProgressFrame_) {
    context_.screen->RemoveItem(connectionProgressFrame_);
    delete connectionProgressFrame_;
    connectionProgressFrame_ = nullptr;
  }
  if (connectionBackground_) {
    context_.screen->RemoveItem(connectionBackground_);
    delete connectionBackground_;
    connectionBackground_ = nullptr;
  }
  renderedConnectionProgressPercent_ = -1;
  renderedConnectionProgressMessage_.clear();
  renderedConnectionBanner_.clear();
}

void ServerListState::ScheduleGameSetup() {
  SPDLOG_INFO("Connection successful, scheduling deferred game setup...");
  context_.sceneManager.StopScene();

  if (!ogame || !ogame->GetGameWorld()) {
    SPDLOG_ERROR("Cannot set up the server world because the Gothic game world is unavailable");
    NetGame::Instance().Disconnect();
    return;
  }

  // Handle initial network sync (safe during render)
  NetGame::Instance().HandleNetwork();
  NetGame::Instance().SyncGameTime();

  // Enable player interface
  Patch::PlayerInterfaceEnabled(true);

  // Check if level change is needed
  if (!NetGame::Instance().map.IsEmpty()) {
    // Level change must be deferred to BEFORE the next render frame starts.
    // ChangeLevel destroys world/camera state, which would crash if done during rendering.
    // Capture player position now before level change.
    // MenuContext keeps the hero pointer from menu creation. Use Gothic's
    // current global hero first because content activation/level changes can
    // replace that object before this setup runs.
    oCNpc* current_player = ::player ? ::player : context_.player;
    if (!current_player || !ogame || !ogame->GetGameWorld()) {
      SPDLOG_ERROR("Cannot set up the server world because the Gothic game or hero is unavailable");
      NetGame::Instance().Disconnect();
      return;
    }
    zVEC3 spawnPosition = current_player->GetPositionWorld();
    zSTRING mapName = NetGame::Instance().map;
    GMPCore::Instance().DeferToNextFrame([spawnPosition, mapName]() {
      SPDLOG_INFO("Executing deferred game setup for {}...", mapName.ToChar());

      auto fail_setup = [](const std::string& reason) {
        SPDLOG_ERROR("Server game setup failed: {}", reason);
        NetGame::Instance().Disconnect();
        GMPCore::Instance().DeferToNextFrame([]() { ReturnToBigMainMenuAfterDisconnect(); });
      };

      PrepareWorldChange();
      auto* content = NetGame::Instance().content_transition_manager.get();
      const bool recreate_session = content && content->HasActiveAddonArchives();
      if (recreate_session) {
        std::string error;
        if (!content->StartServerGameSession(error)) {
          fail_setup(error);
          return;
        }
        if (!ogame) {
          fail_setup("Gothic did not provide the recreated server game session");
          return;
        }
        ogame->LoadGame(kNewGameSlot, mapName);
      } else {
        // Now safe to change level - we're at the start of the frame, before rendering.
        Patch::ChangeLevelEnabled(true);
        ogame->ChangeLevel(mapName, zSTRING("????"));
        Patch::ChangeLevelEnabled(false);
      }

      if (!ogame->GetGameWorld() || !::player) {
        fail_setup("Gothic failed to create the server world or hero");
        return;
      }

      ResetWorldRenderState();
      // Clean up NPCs and world objects
      DeleteAllNpcsAndDisableSpawning();
      // ChangeLevel replaces the Gothic hero object. Never retain or touch the
      // pre-transition pointer; use the newly-created global hero instead.
      ::player->trafoObjToWorld.SetTranslation(spawnPosition);
      CleanupWorldObjects(ogame->GetGameWorld());

      // Apply the local preference to the final hero, before server resources
      // start. A resource can replace this initial appearance during startup.
      gmp::gothic::ApplyCharacterAppearance(*::player, Config::Instance().GetCharacterAppearance());
      std::string resource_error;
      if (!NetGame::Instance().FinalizeDownloadedContent(resource_error)) {
        fail_setup(resource_error);
        return;
      }

      // Join game
      NetGame::Instance().JoinGame();
    });
    SPDLOG_INFO("Level change deferred to next frame");
  } else {
    // Client resources may execute arbitrary Lua. Initialize them at the same
    // frame boundary used for level changes, never from the menu render pass.
    GMPCore::Instance().DeferToNextFrame([]() {
      if (!ogame || !ogame->GetGameWorld() || !::player) {
        SPDLOG_ERROR("Server game setup failed: Gothic game world or hero is unavailable");
        NetGame::Instance().Disconnect();
        GMPCore::Instance().DeferToNextFrame([]() { ReturnToBigMainMenuAfterDisconnect(); });
        return;
      }

      DeleteAllNpcsAndDisableSpawning();
      CleanupWorldObjects(ogame->GetGameWorld());
      gmp::gothic::ApplyCharacterAppearance(*::player, Config::Instance().GetCharacterAppearance());
      std::string resource_error;
      if (!NetGame::Instance().FinalizeDownloadedContent(resource_error)) {
        SPDLOG_ERROR("Server game setup failed: {}", resource_error);
        NetGame::Instance().Disconnect();
        GMPCore::Instance().DeferToNextFrame([]() { ReturnToBigMainMenuAfterDisconnect(); });
        return;
      }
      NetGame::Instance().JoinGame();
    });
  }
}

void ServerListState::HandleConnectionFailure() {
  auto error = NetGame::Instance().game_client->GetConnectionError();
  SPDLOG_ERROR("Connection failed: {}", error);

  // Reset to normal server list mode
  context_.selectedServerIP.Clear();
  context_.selectedServerIndex = 0;
  endpointEntryMode_ = EndpointEntryMode::None;
  endpointEntryError_.clear();

  // Refresh server list
  if (context_.extendedServerList) {
    context_.extendedServerList->RefreshList();
  }
}

}  // namespace states
}  // namespace menu
