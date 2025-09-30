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

#include "player_manager.h"

PlayerManager::PlayerId PlayerManager::AddPlayer(Net::ConnectionHandle connection, const std::string& name) {
  PlayerId player_id = next_player_id_++;
  
  Player player{};
  player.player_id = player_id;
  player.connection = connection;
  player.name = name;
  player.char_class = 0;
  player.head = 0;
  player.skin = 0;
  player.body = 0;
  player.flags = 0;
  player.walkstyle = 0;
  player.figth_pos = 0;
  player.spellhand = 0;
  player.headstate = 0;
  player.is_ingame = 0;
  player.passed_crc_test = 0;
  player.mute = 0;
  player.health = 0;
  player.mana = 0;
  player.tod = 0;
  
  players_[player_id] = std::move(player);
  player_to_connection_[player_id] = connection;
  connection_to_player_[connection] = player_id;
  
  return player_id;
}

bool PlayerManager::RemovePlayer(PlayerId player_id) {
  auto it = players_.find(player_id);
  if (it == players_.end()) {
    return false;
  }
  
  Net::ConnectionHandle connection = it->second.connection;
  
  players_.erase(it);
  player_to_connection_.erase(player_id);
  connection_to_player_.erase(connection);
  
  return true;
}

bool PlayerManager::RemovePlayerByConnection(Net::ConnectionHandle connection) {
  auto it = connection_to_player_.find(connection);
  if (it == connection_to_player_.end()) {
    return false;
  }
  
  PlayerId player_id = it->second;
  return RemovePlayer(player_id);
}

std::optional<std::reference_wrapper<PlayerManager::Player>> PlayerManager::GetPlayer(PlayerId player_id) {
  auto it = players_.find(player_id);
  if (it == players_.end()) {
    return std::nullopt;
  }
  return std::ref(it->second);
}

std::optional<std::reference_wrapper<const PlayerManager::Player>> PlayerManager::GetPlayer(PlayerId player_id) const {
  auto it = players_.find(player_id);
  if (it == players_.end()) {
    return std::nullopt;
  }
  return std::cref(it->second);
}

std::optional<std::reference_wrapper<PlayerManager::Player>> PlayerManager::GetPlayerByConnection(Net::ConnectionHandle connection) {
  auto it = connection_to_player_.find(connection);
  if (it == connection_to_player_.end()) {
    return std::nullopt;
  }
  return GetPlayer(it->second);
}

std::optional<std::reference_wrapper<const PlayerManager::Player>> PlayerManager::GetPlayerByConnection(Net::ConnectionHandle connection) const {
  auto it = connection_to_player_.find(connection);
  if (it == connection_to_player_.end()) {
    return std::nullopt;
  }
  return GetPlayer(it->second);
}

std::optional<Net::ConnectionHandle> PlayerManager::GetConnectionHandle(PlayerId player_id) const {
  auto it = player_to_connection_.find(player_id);
  if (it == player_to_connection_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<PlayerManager::PlayerId> PlayerManager::GetPlayerId(Net::ConnectionHandle connection) const {
  auto it = connection_to_player_.find(connection);
  if (it == connection_to_player_.end()) {
    return std::nullopt;
  }
  return it->second;
}
