
/*
MIT License

Copyright (c) 2022 Gothic Multiplayer Team (pampi, skejt23, mecio)

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

#include "ExtendedServerList.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>

#include "StandardFonts.h"
#include "game_client.hpp"
#include "language.h"

namespace {
constexpr char kDefaultFavoritesFile[] = "Multiplayer/Favorites.json";

std::filesystem::path ResolveFavoritesPath(const char* file) {
  if (file && *file) {
    return std::filesystem::path(file);
  }
  return std::filesystem::path("Multiplayer") / "Favorites.json";
}

bool HasSameEndpoint(const ServerInfo& server, const FavoriteServerEndpoint& favorite) {
  return server.ip == favorite.ip && server.port == static_cast<int>(favorite.port);
}

bool HasSameEndpoint(const ServerInfo& lhs, const ServerInfo& rhs) {
  return lhs.ip == rhs.ip && lhs.port == rhs.port;
}

ServerInfo MakeFavoritePlaceholder(const FavoriteServerEndpoint& favorite) {
  ServerInfo server{};
  server.ip = favorite.ip;
  server.port = favorite.port;
  server.name = favorite.ip + ":" + std::to_string(favorite.port);
  server.map = "Unknown";
  server.num_of_players = -1;
  server.max_players = -1;
  server.ping = -1;
  return server;
}
}  // namespace

using namespace std;
using namespace G2W;

ExtendedServerList::ExtendedServerList(CServerList& server_list) : server_list_(server_list) {
  tab_all = new Button(0, 0);
  tab_all->setHighlightTexture("LOG_PAPER.TGA");
  tab_all->setTexture("INV_BACK_PLUNDER.TGA");
  tab_all->setText(Language::Instance()[Language::SRVLIST_ALL].ToChar());
  tab_all->setFont(FNT_WHITE_10);
  tab_all->setHighlightFont(FNT_GREEN_10);
  tab_all->highlight = true;

  tab_fav = new Button(1000, 0);
  tab_fav->setHighlightTexture("LOG_PAPER.TGA");
  tab_fav->setTexture("INV_BACK_PLUNDER.TGA");
  tab_fav->setText(Language::Instance()[Language::SRVLIST_FAVOURITES].ToChar());
  tab_fav->setFont(FNT_WHITE_10);
  tab_fav->setHighlightFont(FNT_GREEN_10);
  tab_fav->highlight = false;

  list_all = new Table(0, 400, 8192, 7792);
  list_all->addColumn(Language::Instance()[Language::SRVLIST_NAME].ToChar(), 2500);
  list_all->addColumn(Language::Instance()[Language::SRVLIST_MAP].ToChar(), 3000);
  list_all->addColumn(Language::Instance()[Language::SRVLIST_PLAYERNUMBER].ToChar(), 1500);
  list_all->addColumn("Ping", 600);
  list_all->setBackground("INV_BACK_PLUNDER.TGA");
  list_all->setHighlightFont(FNT_GREEN_10);
  list_all->setFont(FNT_WHITE_10);

  list_fav = new Table(0, 400, 8192, 7792);
  list_fav->addColumn(Language::Instance()[Language::SRVLIST_NAME].ToChar(), 2500);
  list_fav->addColumn(Language::Instance()[Language::SRVLIST_MAP].ToChar(), 3000);
  list_fav->addColumn(Language::Instance()[Language::SRVLIST_PLAYERNUMBER].ToChar(), 1500);
  list_fav->addColumn("Ping", 600);
  list_fav->setBackground("INV_BACK_PLUNDER.TGA");
  list_fav->setHighlightFont(FNT_GREEN_10);
  list_fav->setFont(FNT_WHITE_10);

  SelectedTab = TAB_ALL;
  SelectedServer = 0;
  loadFav(kDefaultFavoritesFile);
  fillTables();
}

void ExtendedServerList::loadFav(const char* file) {
  favorite_servers_.clear();

  const std::filesystem::path favorites_path = ResolveFavoritesPath(file);

  std::ifstream favorites_file(favorites_path);
  if (!favorites_file.good()) {
    return;
  }

  try {
    nlohmann::json favorites_json;
    favorites_file >> favorites_json;

    if (!favorites_json.is_array()) {
      SPDLOG_WARN("Favorites file '{}' is not a JSON array. Ignoring its contents.", favorites_path.string());
      return;
    }

    for (const auto& entry : favorites_json) {
      const std::string ip = entry.value("ip", std::string{});
      const int port_value = entry.value("port", 0);

      if (ip.empty()) {
        SPDLOG_WARN("Encountered favorite entry without an IP address in '{}'.", favorites_path.string());
        continue;
      }

      if (port_value <= 0 || port_value > std::numeric_limits<std::uint16_t>::max()) {
        SPDLOG_WARN("Encountered favorite entry with invalid port '{}' for IP '{}' in '{}'.", port_value, ip, favorites_path.string());
        continue;
      }

      favorite_servers_.push_back({ip, static_cast<std::uint16_t>(port_value)});
    }
  } catch (const std::exception& ex) {
    SPDLOG_WARN("Failed to parse favorites file '{}': {}", favorites_path.string(), ex.what());
    favorite_servers_.clear();
  }
}

bool ExtendedServerList::getSelectedServer(void* buffer, int size) {
  if (buffer == nullptr || size <= 0) {
    return false;
  }

  const auto& active_servers = GetActiveServerList();
  if (SelectedServer < 0 || static_cast<size_t>(SelectedServer) >= active_servers.size()) {
    return false;
  }

  const ServerInfo& si = active_servers[SelectedServer];
  const int ret = std::snprintf(static_cast<char*>(buffer), static_cast<size_t>(size), "%s:%d", si.ip.c_str(), si.port);
  return ret > 0 && ret < size;
}

void ExtendedServerList::addSelectedToFav() {
  const auto& active_servers = GetActiveServerList();
  if (SelectedServer < 0 || static_cast<size_t>(SelectedServer) >= active_servers.size()) {
    return;
  }

  const ServerInfo selected_server = active_servers[SelectedServer];

  if (selected_server.port <= 0 || selected_server.port > static_cast<int>(std::numeric_limits<std::uint16_t>::max())) {
    SPDLOG_WARN("Selected server '{}' has an invalid port '{}'.", selected_server.ip, selected_server.port);
    return;
  }

  loadFav(kDefaultFavoritesFile);
  const auto previous_favorites = favorite_servers_;

  const auto already_favorited =
      std::any_of(favorite_servers_.begin(), favorite_servers_.end(), [&selected_server](const FavoriteServerEndpoint& favorite) {
        return favorite.ip == selected_server.ip && favorite.port == static_cast<std::uint16_t>(selected_server.port);
      });

  if (already_favorited) {
    favorite_servers_.erase(std::remove_if(favorite_servers_.begin(), favorite_servers_.end(),
                                           [&](const FavoriteServerEndpoint& f) {
                                             return f.ip == selected_server.ip && f.port == static_cast<std::uint16_t>(selected_server.port);
                                           }),
                            favorite_servers_.end());
  } else {
    favorite_servers_.push_back({selected_server.ip, static_cast<std::uint16_t>(selected_server.port)});
  }

  if (!saveFav(kDefaultFavoritesFile)) {
    favorite_servers_ = previous_favorites;
    return;
  }

  fillTables();
  next_ping_update_ = {};
}

FavoriteAddResult ExtendedServerList::AddFavorite(std::string_view endpoint) {
  std::string host;
  std::uint32_t port = 0;
  if (!gmp::client::ParseServerEndpoint(endpoint, host, port)) {
    return FavoriteAddResult::InvalidEndpoint;
  }

  loadFav(kDefaultFavoritesFile);
  const auto already_favorited = std::any_of(
      favorite_servers_.begin(), favorite_servers_.end(),
      [&host, port](const FavoriteServerEndpoint& favorite) { return favorite.ip == host && favorite.port == static_cast<std::uint16_t>(port); });
  if (already_favorited) {
    fillTables();
    return FavoriteAddResult::AlreadyExists;
  }

  favorite_servers_.push_back({std::move(host), static_cast<std::uint16_t>(port)});
  if (!saveFav(kDefaultFavoritesFile)) {
    favorite_servers_.pop_back();
    return FavoriteAddResult::SaveFailed;
  }

  fillTables();
  next_ping_update_ = {};

  return FavoriteAddResult::Added;
}

bool ExtendedServerList::saveFav(const char* file) {
  const std::filesystem::path favorites_path = ResolveFavoritesPath(file);
  const std::filesystem::path parent_path = favorites_path.parent_path();

  if (!parent_path.empty()) {
    std::error_code ec;
    std::filesystem::create_directories(parent_path, ec);
    if (ec) {
      SPDLOG_ERROR("Failed to create directory '{}' for favorites: {}", parent_path.string(), ec.message());
      return false;
    }
  }

  nlohmann::json favorites_json = nlohmann::json::array();
  for (const auto& favorite : favorite_servers_) {
    favorites_json.push_back({{"ip", favorite.ip}, {"port", favorite.port}});
  }

  std::ofstream favorites_file(favorites_path, std::ios::trunc);
  if (!favorites_file.is_open()) {
    SPDLOG_ERROR("Failed to open favorites file '{}' for writing.", favorites_path.string());
    return false;
  }

  favorites_file << favorites_json.dump(2);
  favorites_file.flush();
  if (!favorites_file.good()) {
    SPDLOG_ERROR("Failed to write favorites file '{}'.", favorites_path.string());
    return false;
  }
  return true;
}

void ExtendedServerList::HandleInput() {
  PollNetworkResults();
  if (zinput->KeyToggled(KEY_RIGHT)) {
    this->nextTab();
  }
  if (zinput->KeyToggled(KEY_LEFT)) {
    this->prevTab();
  }
  if (zinput->KeyToggled(KEY_R)) {
    this->RefreshList();
  }

  {
    auto& active_servers = GetActiveServerList();
    auto* active_table = GetActiveTable();
    if (zinput->KeyToggled(KEY_DOWN) && !active_servers.empty()) {
      if ((active_servers.size() - 1) > (size_t)SelectedServer) {
        SelectedServer++;
      }
    }
    if (zinput->KeyToggled(KEY_UP) && !active_servers.empty()) {
      if (SelectedServer > 0) {
        SelectedServer--;
      }
    }
    ClampSelectedServer();
    active_table->ensureRowVisible(static_cast<unsigned int>(SelectedServer));
  }

  if (zinput->KeyToggled(KEY_A)) {
    addSelectedToFav();
  }
}

bool ExtendedServerList::RefreshList() {
  loadFav(kDefaultFavoritesFile);
  tables_dirty_ = true;
  next_ping_update_ = {};

  if (!server_list_.BeginRefresh()) {
    if (const char* error_message = server_list_.GetLastError()) {
      SPDLOG_ERROR("Failed to refresh server list: {}", error_message);
    }
    return false;
  }
  return true;
}

void ExtendedServerList::PollNetworkResults() {
  if (const auto refreshed = server_list_.PollRefresh()) {
    if (*refreshed) {
      srvList.clear();
      const size_t server_count = server_list_.GetListSize();
      srvList.reserve(server_count);
      for (size_t index = 0; index < server_count; ++index) {
        if (auto server_info = server_list_.At(index)) {
          ServerInfo server{};
          server.name = server_info->name.ToChar();
          server.map = server_info->map.ToChar();
          server.ip = server_info->ip.ToChar();
          server.num_of_players = server_info->num_of_players;
          server.max_players = server_info->max_players;
          server.port = server_info->port;
          server.ping = server_info->ping;
          srvList.push_back(std::move(server));
        }
      }
    } else if (const char* error_message = server_list_.GetLastError()) {
      SPDLOG_ERROR("Failed to refresh server list: {}", error_message);
    }
    tables_dirty_ = true;
    next_ping_update_ = {};
  }

  if (const auto pings = server_list_.PollPings()) {
    for (const auto& [host, ping] : *pings) {
      for (auto& server : srvList) {
        if (server.ip == host) {
          server.ping = ping;
        }
      }
      for (auto& server : favorite_server_list_) {
        if (server.ip == host) {
          server.ping = ping;
        }
      }
    }
    tables_dirty_ = true;
  }

  if (tables_dirty_) {
    fillTables();
  }
}

ExtendedServerList::~ExtendedServerList(void) {
  delete tab_all;
  delete tab_fav;
  delete list_all;
  delete list_fav;
}

void ExtendedServerList::fillTables() {
  list_all->clear();
  list_fav->clear();
  const auto previous_favorites = favorite_server_list_;
  favorite_server_list_.clear();

  const auto build_row = [](const ServerInfo& server) {
    std::vector<std::string> row;
    row.push_back(server.name);
    row.push_back(server.map);
    char buff[128];
    if (server.num_of_players >= 0 && server.max_players >= 0) {
      sprintf(buff, "%d/%d", server.num_of_players, server.max_players);
      row.push_back(buff);
    } else {
      row.push_back("?");
    }

    if (server.ping >= 0) {
      sprintf(buff, "%d", server.ping);
      row.push_back(buff);
    } else {
      row.push_back("?");
    }

    return row;
  };

  for (const auto& server : srvList) {
    TableRow tr = {build_row(server), false};
    list_all->addRow(tr);
  }

  for (const auto& favorite : favorite_servers_) {
    auto public_server =
        std::find_if(srvList.begin(), srvList.end(), [&favorite](const ServerInfo& server) { return HasSameEndpoint(server, favorite); });

    ServerInfo favorite_server = public_server != srvList.end() ? *public_server : MakeFavoritePlaceholder(favorite);
    if (public_server == srvList.end()) {
      auto previous_server = std::find_if(previous_favorites.begin(), previous_favorites.end(),
                                          [&favorite_server](const ServerInfo& server) { return HasSameEndpoint(server, favorite_server); });
      if (previous_server != previous_favorites.end()) {
        favorite_server.ping = previous_server->ping;
      }
    }

    favorite_server_list_.push_back(favorite_server);
    TableRow tr = {build_row(favorite_server), false};
    list_fav->addRow(tr);
  }

  ClampSelectedServer();
  GetActiveTable()->ensureRowVisible(static_cast<unsigned int>(SelectedServer));
  tables_dirty_ = false;
}

void ExtendedServerList::SelectServer(int index) {
  this->SelectedServer = index;
  ClampSelectedServer();
  GetActiveTable()->ensureRowVisible(static_cast<unsigned int>(SelectedServer));
}

void ExtendedServerList::nextTab() {
  selectTab(TAB_FAV);
}
void ExtendedServerList::prevTab() {
  selectTab(TAB_ALL);
}
void ExtendedServerList::selectTab(int index) {
  switch (index) {
    case TAB_ALL:
      tab_all->highlight = true;
      tab_fav->highlight = false;
      SelectedTab = index;
      ClampSelectedServer();
      GetActiveTable()->ensureRowVisible(static_cast<unsigned int>(SelectedServer));
      break;
    case TAB_FAV:
      tab_all->highlight = false;
      tab_fav->highlight = true;
      SelectedTab = index;
      ClampSelectedServer();
      GetActiveTable()->ensureRowVisible(static_cast<unsigned int>(SelectedServer));
      break;
    default:
      // Nie ma takiej zakladki
      break;
  }
}
void ExtendedServerList::addServer(ServerInfo& si) {
  // si.updatePing();
  srvList.push_back(si);
  tables_dirty_ = true;
  next_ping_update_ = {};
}

void ExtendedServerList::clearList() {
  srvList.clear();
  tables_dirty_ = true;
}

void ExtendedServerList::Draw() {
  PollNetworkResults();
  const auto now = std::chrono::steady_clock::now();
  if (now >= next_ping_update_) {
    updatePings();
    next_ping_update_ = now + std::chrono::seconds(1);
  }

  tab_all->render();
  tab_fav->render();
  auto* active_table = GetActiveTable();
  active_table->unHighlightAll();
  if (!active_table->rows.empty() && SelectedServer >= 0 && static_cast<size_t>(SelectedServer) < active_table->rows.size()) {
    active_table->rows[SelectedServer].highlight = true;
  }
  active_table->render();
}
void ExtendedServerList::updatePings() {
  std::vector<std::string> hosts;
  hosts.reserve(srvList.size() + favorite_servers_.size());
  for (const auto& server : srvList) {
    hosts.push_back(server.ip);
  }
  for (const auto& favorite : favorite_servers_) {
    hosts.push_back(favorite.ip);
  }
  server_list_.BeginPings(std::move(hosts));
}

std::vector<ServerInfo>& ExtendedServerList::GetActiveServerList() {
  return SelectedTab == TAB_FAV ? favorite_server_list_ : srvList;
}

const std::vector<ServerInfo>& ExtendedServerList::GetActiveServerList() const {
  return SelectedTab == TAB_FAV ? favorite_server_list_ : srvList;
}

G2W::Table* ExtendedServerList::GetActiveTable() {
  return SelectedTab == TAB_FAV ? list_fav : list_all;
}

void ExtendedServerList::ClampSelectedServer() {
  const auto& active_servers = GetActiveServerList();
  if (active_servers.empty()) {
    SelectedServer = 0;
    return;
  }

  if (SelectedServer < 0) {
    SelectedServer = 0;
    return;
  }

  if (static_cast<size_t>(SelectedServer) >= active_servers.size()) {
    SelectedServer = static_cast<int>(active_servers.size()) - 1;
  }
}

ServerInfo::ServerInfo() {
  this->max_players = 0;
  this->num_of_players = 0;
  this->port = 0;
  this->ping = -1;
}
