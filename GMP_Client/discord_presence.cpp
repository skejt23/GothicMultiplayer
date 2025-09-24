
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

#include "discord_presence.h"

#if DISCORD_APPLICATION_ID

#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>

#include "discord.h"

DiscordRichPresence::DiscordRichPresence() {
}

DiscordRichPresence::~DiscordRichPresence() {
  should_stop_ = true;
  cv_.notify_all();

  if (activity_thread_.joinable()) {
    activity_thread_.join();
  }

  std::lock_guard<std::mutex> lock(core_mutex_);
  core_.reset();
}

void DiscordRichPresence::Init() {
  std::lock_guard<std::mutex> lock(core_mutex_);
  if (core_) {
    return;
  }

  discord::Core* coreInstance = nullptr;
  auto result = discord::Core::Create(DISCORD_APPLICATION_ID, DiscordCreateFlags_Default, &coreInstance);
  if (result != discord::Result::Ok || coreInstance == nullptr) {
    spdlog::error("Failed to initialize Discord Rich Presence: {}", static_cast<int>(result));
    return;
  }

  core_.reset(coreInstance);
  should_stop_ = false;
  activity_thread_ = std::thread(&DiscordRichPresence::ActivityThreadLoop, this);
  spdlog::info("Discord Rich Presence initialized");
}

void DiscordRichPresence::ActivityThreadLoop() {
  std::unique_lock<std::mutex> lock(core_mutex_);

  while (!should_stop_) {
    if (core_) {
      auto result = core_->RunCallbacks();
      if (result != discord::Result::Ok) {
        spdlog::warn("Discord Rich Presence callbacks returned {}", static_cast<int>(result));
      }
    }

    // Wait for 16ms (roughly 60 FPS) or until shutdown
    cv_.wait_for(lock, std::chrono::milliseconds(16), [this] { return should_stop_.load(); });
  }
}

void DiscordRichPresence::UpdateActivity(const std::string& state, const std::string& details, int64_t startTimestamp, int64_t endTimestamp,
                                         const std::string& largeImageKey, const std::string& largeImageText, const std::string& smallImageKey,
                                         const std::string& smallImageText) {
  std::lock_guard<std::mutex> lock(core_mutex_);
  if (!core_) {
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

  core_->ActivityManager().UpdateActivity(activity, [](discord::Result result) {
    if (result != discord::Result::Ok) {
      spdlog::error("Discord Rich Presence update failed: {}", static_cast<int>(result));
    }
  });
}

void DiscordRichPresence::ClearActivity() {
  std::lock_guard<std::mutex> lock(core_mutex_);
  if (!core_) {
    return;
  }

  core_->ActivityManager().ClearActivity([](discord::Result result) {
    if (result != discord::Result::Ok) {
      spdlog::error("Discord Rich Presence clear failed: {}", static_cast<int>(result));
    }
  });
}

#endif  // DISCORD_APPLICATION_ID