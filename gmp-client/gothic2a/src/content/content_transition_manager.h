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

#pragma once

#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace gmp::gothic {

inline constexpr const char* kGmpLoadingScreenTexture = "GMP_LOADING.TGA";

void PrepareDisconnectLoadingScreen();

class ContentTransitionManager {
public:
  enum class State {
    Base,
    Connecting,
    Downloading,
    ActivatingAddon,
    Server,
    DeactivatingAddon,
    ReturningToBase,
  };

  bool Initialize(std::string& error);
  bool BeginConnection(std::string& error);
  void MarkDownloading();
  void AbortConnection();
  bool ActivateServerContent(const std::vector<std::filesystem::path>& archives, bool contains_gothic_dat, std::string& error);
  bool StartServerGameSession(std::string& error);
  bool DeactivateServerContent(std::string& error);
  bool HasActiveAddonArchives() const;
  bool HasServerGameSessionStarted() const;
  State CurrentState() const;

private:
  void PurgeResourceCaches(const char* reason);
  bool RecreateGameSession(bool deactivate_overlay, std::string& error);

  mutable std::mutex mutex_;
  State state_{State::Base};
  bool active_has_addon_archives_{false};
  bool server_game_session_started_{false};
};

}  // namespace gmp::gothic
