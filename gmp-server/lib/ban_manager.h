

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

#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class BanManager {
public:
  struct BanEntry {
    std::string nickname;
    std::string ip;
    std::string date;
    std::string reason;
  };

  using Clock = std::chrono::steady_clock;

  explicit BanManager(std::filesystem::path storage_path = std::filesystem::path("bans.json"));

  bool Load();
  bool Save() const;

  const std::vector<BanEntry>& GetBanList() const noexcept {
    return ban_list_;
  }

  bool IsBanned(std::string_view ip, Clock::time_point now = Clock::now()) const;
  void AddBan(BanEntry entry);
  void AddTemporaryBan(std::string ip, Clock::duration duration, Clock::time_point now = Clock::now());
  bool RemoveBan(std::string_view ip);

private:
  std::filesystem::path storage_path_;
  std::vector<BanEntry> ban_list_;
  // Temporary integrity bans last for this server run and are not persisted.
  std::unordered_map<std::string, Clock::time_point> temporary_bans_;
};
