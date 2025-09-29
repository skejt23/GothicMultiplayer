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

#include <fstream>
#include <utility>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "znet_server.h"

namespace {
constexpr const char* kDefaultBanListFileName = "bans.json";
}

BanManager::BanManager(Net::NetServer& net_server, std::filesystem::path storage_path)
    : net_server_(net_server), storage_path_(std::move(storage_path)) {
  if (storage_path_.empty()) {
    storage_path_ = std::filesystem::path(kDefaultBanListFileName);
  }
}

bool BanManager::Load() {
  ban_list_.clear();

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

    if (auto nickname_it = node.find("Nickname"); nickname_it != node.end() && nickname_it->is_string()) {
      entry.nickname = nickname_it->get<std::string>();
    }
    if (auto date_it = node.find("Date"); date_it != node.end() && date_it->is_string()) {
      entry.date = date_it->get<std::string>();
    }
    if (auto reason_it = node.find("Reason"); reason_it != node.end() && reason_it->is_string()) {
      entry.reason = reason_it->get<std::string>();
    }

    ban_list_.emplace_back(std::move(entry));
  }

  if (ban_list_.empty()) {
    SPDLOG_INFO("No active bans found in {}", storage_path_.string());
    return true;
  }

  SyncWithNetwork();

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

  SPDLOG_INFO("Bans written to {}.", storage_path_.string());
  return true;
}

void BanManager::SyncWithNetwork() {
  for (const auto& entry : ban_list_) {
    net_server_.AddToBanList(entry.ip.c_str(), 0);
  }
}
