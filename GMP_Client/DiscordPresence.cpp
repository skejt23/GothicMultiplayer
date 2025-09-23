
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
#include "DiscordPresence.h"

#if DISCORD_RICH_PRESENCE_ENABLED

#include <spdlog/spdlog.h>

#include <memory>
#include <mutex>

#include "discord.h"

namespace {
std::unique_ptr<discord::Core> g_core;
std::mutex g_coreMutex;

void RunCallbacksLocked() {
  if (!g_core) {
    return;
  }

  auto result = g_core->RunCallbacks();
  if (result != discord::Result::Ok) {
    spdlog::warn("Discord Rich Presence callbacks returned {}", static_cast<int>(result));
  }
}
}  // namespace

namespace DiscordGMP {
bool Initialize(long long clientId) {
  std::lock_guard<std::mutex> lock(g_coreMutex);
  if (g_core) {
    return true;
  }

  discord::Core* coreInstance = nullptr;
  auto result = discord::Core::Create(clientId, DiscordCreateFlags_Default, &coreInstance);
  if (result != discord::Result::Ok || coreInstance == nullptr) {
    spdlog::error("Failed to initialize Discord Rich Presence: {}", static_cast<int>(result));
    return false;
  }

  g_core.reset(coreInstance);
  spdlog::info("Discord Rich Presence initialized");
  return true;
}

void Shutdown() {
  std::lock_guard<std::mutex> lock(g_coreMutex);
  g_core.reset();
  spdlog::info("Discord Rich Presence shut down");
}

void PumpCallbacks() {
  std::lock_guard<std::mutex> lock(g_coreMutex);
  RunCallbacksLocked();
}

void UpdateActivity(const std::string& state, const std::string& details, int64_t startTimestamp, int64_t endTimestamp,
                    const std::string& largeImageKey, const std::string& largeImageText, const std::string& smallImageKey,
                    const std::string& smallImageText) {
  std::lock_guard<std::mutex> lock(g_coreMutex);
  if (!g_core) {
    spdlog::warn("Discord Rich Presence update requested before initialization");
    return;
  }

  discord::Activity activity{};
  activity.SetState(state.c_str());
  activity.SetDetails(details.c_str());

  auto& timestamps = activity.GetTimestamps();
  if (startTimestamp != 0) {
    timestamps.SetStart(startTimestamp);
  }
  if (endTimestamp != 0) {
    timestamps.SetEnd(endTimestamp);
  }

  auto& assets = activity.GetAssets();
  assets.SetLargeImage(largeImageKey.c_str());
  assets.SetLargeText(largeImageText.c_str());
  assets.SetSmallImage(smallImageKey.c_str());
  assets.SetSmallText(smallImageText.c_str());

  g_core->ActivityManager().UpdateActivity(activity, [](discord::Result result) {
    if (result != discord::Result::Ok) {
      spdlog::error("Discord Rich Presence update failed: {}", static_cast<int>(result));
    }
  });

  RunCallbacksLocked();
}

void ClearActivity() {
  std::lock_guard<std::mutex> lock(g_coreMutex);
  if (!g_core) {
    return;
  }

  g_core->ActivityManager().ClearActivity([](discord::Result result) {
    if (result != discord::Result::Ok) {
      spdlog::error("Discord Rich Presence clear failed: {}", static_cast<int>(result));
    }
  });

  RunCallbacksLocked();
}
}  // namespace DiscordGMP

#else  // DISCORD_RICH_PRESENCE_ENABLED

namespace DiscordGMP {
bool Initialize(long long) {
  return false;
}

void Shutdown() {
}

void PumpCallbacks() {
}

void UpdateActivity(const std::string&, const std::string&, int64_t, int64_t,
                    const std::string&, const std::string&, const std::string&,
                    const std::string&){
}

void ClearActivity() {
}
}  // namespace DiscordGMP

#endif  // DISCORD_RICH_PRESENCE_ENABLED