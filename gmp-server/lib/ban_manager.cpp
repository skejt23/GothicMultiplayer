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

#include "ban_manager.h"

#include <algorithm>
#include <fstream>
#include <utility>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace {
constexpr const char* kDefaultBanListFileName = "bans.json";
}

BanManager::BanManager(std::filesystem::path storage_path) : storage_path_(std::move(storage_path)) {
  if (storage_path_.empty()) {
    storage_path_ = std::filesystem::path(kDefaultBanListFileName);
  }
}

bool BanManager::Load() {
  std::ifstream ifs(storage_path_, std::ios::binary);
  if (!ifs.is_open()) {
    SPDLOG_WARN("{} which contains active IP bans does not exist", storage_path_.string());
    return false;
  }

  nlohmann::json json_data;
  try {
    ifs >> json_data;
  } catch (const std::exception& ex) {
    SPDLOG_ERROR("Failed to parse {}: {}", storage_path_.string(), ex.what());
    return false;
  }

  if (!json_data.is_array()) {
    SPDLOG_ERROR("{} is expected to contain a JSON array of ban entries", storage_path_.string());
    return false;
  }

  std::vector<BanEntry> loaded_bans;
  for (const auto& node : json_data) {
    if (!node.is_object()) {
      SPDLOG_WARN("Ignoring malformed ban entry that is not a JSON object");
      continue;
    }

    auto ip_it = node.find("IP");
    if (ip_it == node.end() || !ip_it->is_string()) {
      SPDLOG_WARN("Ignoring ban entry without a valid IP string");
      continue;
    }

    BanEntry entry;
    entry.ip = ip_it->get<std::string>();
    if (entry.ip.empty()) {
      SPDLOG_WARN("Ignoring ban entry with an empty IP");
      continue;
    }

    if (auto nickname_it = node.find("Nickname"); nickname_it != node.end() && nickname_it->is_string()) {
      entry.nickname = nickname_it->get<std::string>();
    }
    if (auto date_it = node.find("Date"); date_it != node.end() && date_it->is_string()) {
      entry.date = date_it->get<std::string>();
    }
    if (auto reason_it = node.find("Reason"); reason_it != node.end() && reason_it->is_string()) {
      entry.reason = reason_it->get<std::string>();
    }

    loaded_bans.emplace_back(std::move(entry));
  }

  ban_list_ = std::move(loaded_bans);

  if (ban_list_.empty()) {
    SPDLOG_INFO("No active bans found in {}", storage_path_.string());
    return true;
  }

  SPDLOG_INFO("Loaded {} active ban{} from {}.", ban_list_.size(), ban_list_.size() == 1 ? "" : "s",
              storage_path_.string());

  return true;
}

bool BanManager::Save() const {
  nlohmann::json json_data = nlohmann::json::array();
  for (const auto& entry : ban_list_) {
    json_data.push_back({{"Nickname", entry.nickname}, {"IP", entry.ip}, {"Date", entry.date}, {"Reason", entry.reason}});
  }

  std::ofstream ofs(storage_path_, std::ios::binary);
  if (!ofs.is_open()) {
    SPDLOG_ERROR("Could not save active bans to file {}!", storage_path_.string());
    return false;
  }

  ofs << json_data.dump(2) << '\n';
  ofs.close();
  if (!ofs) {
    SPDLOG_ERROR("Failed to write active bans to {}", storage_path_.string());
    return false;
  }

  SPDLOG_INFO("Bans written to {}.", storage_path_.string());
  return true;
}

bool BanManager::IsBanned(std::string_view ip, Clock::time_point now) const {
  if (ip.empty()) {
    return false;
  }
  if (std::any_of(ban_list_.begin(), ban_list_.end(), [&](const BanEntry& entry) { return entry.ip == ip; })) {
    return true;
  }
  const auto temporary = temporary_bans_.find(std::string(ip));
  return temporary != temporary_bans_.end() && now < temporary->second;
}

void BanManager::AddBan(BanEntry entry) {
  if (entry.ip.empty()) {
    return;
  }
  auto existing = std::find_if(ban_list_.begin(), ban_list_.end(), [&](const BanEntry& ban) { return ban.ip == entry.ip; });
  if (existing == ban_list_.end()) {
    ban_list_.push_back(std::move(entry));
  } else {
    *existing = std::move(entry);
  }
}

void BanManager::AddTemporaryBan(std::string ip, Clock::duration duration, Clock::time_point now) {
  std::erase_if(temporary_bans_, [&](const auto& ban) { return ban.second <= now; });
  if (ip.empty() || duration <= Clock::duration::zero()) {
    return;
  }
  auto [ban, inserted] = temporary_bans_.try_emplace(std::move(ip), now + duration);
  if (!inserted) {
    ban->second = std::max(ban->second, now + duration);
  }
}

bool BanManager::RemoveBan(std::string_view ip) {
  const auto removed = std::erase_if(ban_list_, [&](const BanEntry& entry) { return entry.ip == ip; });
  const auto temporary_removed = temporary_bans_.erase(std::string(ip));
  return removed != 0 || temporary_removed != 0;
}
