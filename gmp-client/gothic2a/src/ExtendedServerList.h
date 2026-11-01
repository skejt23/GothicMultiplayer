
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

#pragma once

#include <chrono>
#include <cstdint>
#include <string_view>
#include <vector>

#include "CServerList.h"
#include "G2W.h"
#include "ZenGin/zGothicAPI.h"
#include "language.h"

#define TAB_ALL 0
#define TAB_FAV 1

class ServerInfo {
public:
  std::string name, map, ip;
  int max_players, num_of_players, port, ping;
  ServerInfo();
};

struct FavoriteServerEndpoint {
  std::string ip;
  std::uint16_t port;
};

enum class FavoriteAddResult {
  Added,
  AlreadyExists,
  InvalidEndpoint,
  SaveFailed,
};

class ExtendedServerList {
public:
  ExtendedServerList(CServerList& server_list);
  ~ExtendedServerList(void);
  void SelectServer(int index);
  void Draw();
  void addServer(ServerInfo& si);
  void clearList();
  void selectTab(int index);
  void nextTab();
  void prevTab();
  void loadFav(const char* file);
  bool saveFav(const char* file);
  void HandleInput();
  void addSelectedToFav();
  FavoriteAddResult AddFavorite(std::string_view endpoint);
  bool RefreshList();
  bool getSelectedServer(void* buffer, int size);
  void fillTables();
  void updatePings();

private:
  void PollNetworkResults();
  std::vector<ServerInfo>& GetActiveServerList();
  const std::vector<ServerInfo>& GetActiveServerList() const;
  G2W::Table* GetActiveTable();
  void ClampSelectedServer();

  G2W::Button *tab_all, *tab_fav;
  G2W::Table *list_all, *list_fav;
  std::vector<FavoriteServerEndpoint> favorite_servers_;
  std::vector<ServerInfo> srvList;
  std::vector<ServerInfo> favorite_server_list_;
  int SelectedTab;
  int SelectedServer;
  CServerList& server_list_;
  std::chrono::steady_clock::time_point next_ping_update_{};
  bool tables_dirty_{true};
};
