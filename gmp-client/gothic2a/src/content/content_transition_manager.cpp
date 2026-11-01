/*
MIT License

Copyright (c) 2026 Gothic Multiplayer Team.

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

#include "content/content_transition_manager.h"

#include <spdlog/spdlog.h>

#include <utility>

#include "ZenGin/zGothicAPI.h"
#include "content/gothic_vfs_overlay.h"
#include "hooking/MemoryPatch.h"

namespace gmp::gothic {

namespace {

constexpr DWORD kInsertBackAddress = 0x007A6130;
using InsertBackFn = void(__thiscall*)(zCView*, const zSTRING&);

InsertBackFn g_insert_back = nullptr;
bool g_disconnect_loading_screen_pending = false;

void __fastcall HookInsertBack(zCView* view, void*, const zSTRING& texture_name) {
  if (std::exchange(g_disconnect_loading_screen_pending, false)) {
    g_insert_back(view, zSTRING(kGmpLoadingScreenTexture));
    return;
  }
  g_insert_back(view, texture_name);
}

const char* StateName(ContentTransitionManager::State state) {
  switch (state) {
    case ContentTransitionManager::State::Base:
      return "Base";
    case ContentTransitionManager::State::Connecting:
      return "Connecting";
    case ContentTransitionManager::State::Downloading:
      return "Downloading";
    case ContentTransitionManager::State::ActivatingAddon:
      return "ActivatingAddon";
    case ContentTransitionManager::State::Server:
      return "Server";
    case ContentTransitionManager::State::DeactivatingAddon:
      return "DeactivatingAddon";
    case ContentTransitionManager::State::ReturningToBase:
      return "ReturningToBase";
  }
  return "Unknown";
}

class ScopedResourceThreadingPause {
public:
  ScopedResourceThreadingPause() {
    if (zresMan) {
      previous_ = zresMan->GetThreadingEnabled();
      zresMan->SetThreadingEnabled(0);
    }
  }

  ~ScopedResourceThreadingPause() {
    if (zresMan) {
      zresMan->SetThreadingEnabled(previous_);
    }
  }

private:
  int previous_{0};
};

}  // namespace

void PrepareDisconnectLoadingScreen() { g_disconnect_loading_screen_pending = true; }

bool ContentTransitionManager::Initialize(std::string& error) {
  if (!GothicVfsOverlay::Instance().InstallHooks(error)) {
    return false;
  }
  if (!g_insert_back) {
    const auto trampoline = CreateHook(kInsertBackAddress, reinterpret_cast<DWORD>(&HookInsertBack));
    if (!trampoline) {
      error = "Failed to install the disconnect loading-screen hook";
      return false;
    }
    g_insert_back = reinterpret_cast<InsertBackFn>(*trampoline);
    SPDLOG_INFO("Addon content: installed disconnect loading-screen hook");
  }
  error.clear();
  return true;
}

bool ContentTransitionManager::BeginConnection(std::string& error) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (state_ != State::Base) {
    error = std::string("Cannot start a connection while content state is ") + StateName(state_);
    return false;
  }
  state_ = State::Connecting;
  SPDLOG_INFO("Content transition: Base -> Connecting");
  error.clear();
  return true;
}

void ContentTransitionManager::MarkDownloading() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (state_ == State::Connecting) {
    state_ = State::Downloading;
    SPDLOG_INFO("Content transition: Connecting -> Downloading");
  }
}

void ContentTransitionManager::AbortConnection() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (state_ == State::Connecting || state_ == State::Downloading) {
    SPDLOG_INFO("Content transition: {} -> Base", StateName(state_));
    state_ = State::Base;
  }
}

bool ContentTransitionManager::ActivateServerContent(const std::vector<std::filesystem::path>& archives, bool contains_gothic_dat,
                                                     std::string& error) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != State::Connecting && state_ != State::Downloading) {
      error = std::string("Cannot activate server content while state is ") + StateName(state_);
      return false;
    }
    state_ = State::ActivatingAddon;
  }
  SPDLOG_INFO("Content transition: ActivatingAddon ({} archive(s), GOTHIC.DAT={})", archives.size(), contains_gothic_dat);

  if (!archives.empty() && !GothicVfsOverlay::Instance().Activate(archives, error)) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = State::Base;
    active_has_addon_archives_ = false;
    server_game_session_started_ = false;
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    active_has_addon_archives_ = !archives.empty();
    server_game_session_started_ = false;
    state_ = State::Server;
  }
  SPDLOG_INFO("Content transition: ActivatingAddon -> Server");
  error.clear();
  return true;
}

bool ContentTransitionManager::StartServerGameSession(std::string& error) {
  bool needs_recreation = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != State::Server) {
      error = std::string("Cannot start the server game session while state is ") + StateName(state_);
      return false;
    }
    if (server_game_session_started_) {
      error.clear();
      return true;
    }
    needs_recreation = active_has_addon_archives_;
  }

  if (needs_recreation && !RecreateGameSession(false, error)) {
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    server_game_session_started_ = needs_recreation;
  }
  error.clear();
  return true;
}

bool ContentTransitionManager::DeactivateServerContent(std::string& error) {
  bool had_addon_archives = false;
  bool had_server_game_session = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == State::Base) {
      error.clear();
      return true;
    }
    if (state_ == State::Connecting || state_ == State::Downloading) {
      state_ = State::Base;
      active_has_addon_archives_ = false;
      server_game_session_started_ = false;
      error.clear();
      return true;
    }
    if (state_ != State::Server) {
      error = std::string("Cannot deactivate server content while state is ") + StateName(state_);
      return false;
    }
    state_ = State::DeactivatingAddon;
    had_addon_archives = active_has_addon_archives_;
    had_server_game_session = server_game_session_started_;
  }

  SPDLOG_INFO("Content transition: Server -> DeactivatingAddon");
  bool success = true;
  if (had_server_game_session) {
    success = RecreateGameSession(true, error);
  } else if (had_addon_archives) {
    GothicVfsOverlay::Instance().Deactivate();
    PurgeResourceCaches("returning from asset-only addon content");
    error.clear();
  } else {
    error.clear();
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = State::ReturningToBase;
    active_has_addon_archives_ = false;
    server_game_session_started_ = false;
    state_ = State::Base;
  }
  SPDLOG_INFO("Content transition: ReturningToBase -> Base");
  return success;
}

bool ContentTransitionManager::HasActiveAddonArchives() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return state_ == State::Server && active_has_addon_archives_;
}

ContentTransitionManager::State ContentTransitionManager::CurrentState() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return state_;
}

void ContentTransitionManager::PurgeResourceCaches(const char* reason) {
  ScopedResourceThreadingPause pause;
  if (zresMan) {
    zresMan->PurgeCaches(nullptr);
    SPDLOG_INFO("Purged Gothic resource caches while {}", reason);
  }
}

bool ContentTransitionManager::RecreateGameSession(bool deactivate_overlay, std::string& error) {
  if (!gameMan || !gameMan->gameSession) {
    error = "Gothic game session is unavailable during content transition";
    return false;
  }

  ScopedResourceThreadingPause pause;
  // World disposal reads the global sky controller. It must belong to the
  // session being destroyed, even if that session has not rendered yet.
  auto* old_world = gameMan->gameSession->GetGameWorld();
  zCSkyControler::s_activeSkyControler = old_world ? old_world->GetActiveSkyControler() : nullptr;
  gameMan->GameSessionDone();
  // Native world destruction releases the controller without clearing this pointer.
  zCSkyControler::s_activeSkyControler = nullptr;

  if (deactivate_overlay) {
    GothicVfsOverlay::Instance().Deactivate();
  }

  if (zresMan) {
    zresMan->PurgeCaches(nullptr);
  }
  zCCacheBase::S_ClearCaches();

  // Native parser Reset() frees the symbol table but leaves the shared instance
  // context intact. INITPERCEPTIONS must not restore a symbol from the old session.
  zCPar_Symbol::SetUseInstance(nullptr);
  gameMan->GameSessionInit();
  if (!gameMan->gameSession || !ogame || !ogame->GetGameWorld()) {
    error = "Gothic failed to initialize a fresh game session";
    return false;
  }

  zCSkyControler::s_activeSkyControler = ogame->GetGameWorld()->GetActiveSkyControler();
  SPDLOG_INFO("Recreated Gothic game session for {} content", deactivate_overlay ? "base" : "server");
  error.clear();
  return true;
}

bool ContentTransitionManager::HasServerGameSessionStarted() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return state_ == State::Server && server_game_session_started_;
}

}  // namespace gmp::gothic
