
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

#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <string>
#include <thread>

#ifdef DISCORD_APPLICATION_ID

namespace discord {
class Core;
struct Activity;
}  // namespace discord

class DiscordRichPresence {
public:
  DiscordRichPresence();
  ~DiscordRichPresence();

  void Init();

  void UpdateActivity(const std::string& state, const std::string& details, int64_t startTimestamp = 0, int64_t endTimestamp = 0,
                      const std::string& largeImageKey = "", const std::string& largeImageText = "", const std::string& smallImageKey = "",
                      const std::string& smallImageText = "");

  void ClearActivity();

  static DiscordRichPresence& Instance() {
    static DiscordRichPresence instance;
    return instance;
  }

private:
  void ActivityThreadLoop();

  std::mutex core_mutex_;
  std::unique_ptr<discord::Core> core_;
  std::thread activity_thread_;
  std::atomic<bool> should_stop_{false};
  std::condition_variable cv_;
};

#else
// Empty implementations when Discord is not enabled
class DiscordRichPresence {
public:
  void Init() {
  }
  void UpdateActivity(const std::string& /*state*/, const std::string& /*details*/, int64_t /*startTimestamp*/ = 0, int64_t /*endTimestamp*/ = 0,
                      const std::string& /*largeImageKey*/ = "", const std::string& /*largeImageText*/ = "",
                      const std::string& /*smallImageKey*/ = "", const std::string& /*smallImageText*/ = {}) {
  }
  void ClearActivity() {
  }
  static DiscordRichPresence& Instance() {
    static DiscordRichPresence instance;
    return instance;
  }
};

#endif