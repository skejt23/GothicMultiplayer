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

#include <cstdint>
#include <ctime>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

#include "common_structs.h"
#include "znet_server.h"

/**
 * @brief Manages all player-related data and operations.
 *
 * PlayerManager maintains the mapping between network connections and game-level
 * player IDs, and handles player lifecycle (join, leave, state management).
 */
class PlayerManager {
public:
  using PlayerId = std::uint32_t;

  /**
   * @brief Player flags for various states
   */
  enum PlayerFlags : std::uint8_t { PL_UNCONCIOUS = 0x01 };

  /**
   * @brief Represents a player in the game
   */
  struct Player {
    PlayerId player_id;
    Net::ConnectionHandle connection;
    std::string name;

    // Character appearance
    std::uint8_t char_class;
    std::uint8_t head;
    std::uint8_t skin;
    std::uint8_t body;

    // Character state
    std::uint8_t flags;
    std::uint8_t walkstyle;
    std::uint8_t fight_pos;
    std::uint8_t spellhand;
    std::uint8_t headstate;

    // Game state
    std::uint8_t is_ingame;
    std::uint8_t passed_crc_test;
    std::uint8_t mute;

    std::int16_t health;
    std::int16_t mana;

    std::time_t tod;  // time of death
    PlayerState state;
  };

  PlayerManager() = default;
  ~PlayerManager() = default;

  // Prevent copying
  PlayerManager(const PlayerManager&) = delete;
  PlayerManager& operator=(const PlayerManager&) = delete;

  /**
   * @brief Adds a new player with the given connection handle
   * @param connection The network connection handle
   * @param name The player's name
   * @return The assigned player ID
   */
  PlayerId AddPlayer(Net::ConnectionHandle connection, const std::string& name);

  /**
   * @brief Removes a player by their player ID
   * @param player_id The player ID to remove
   * @return true if the player was found and removed, false otherwise
   */
  bool RemovePlayer(PlayerId player_id);

  /**
   * @brief Removes a player by their connection handle
   * @param connection The connection handle
   * @return true if the player was found and removed, false otherwise
   */
  bool RemovePlayerByConnection(Net::ConnectionHandle connection);

  /**
   * @brief Gets a player by their player ID
   * @param player_id The player ID
   * @return Optional reference to the player if found
   */
  std::optional<std::reference_wrapper<Player>> GetPlayer(PlayerId player_id);

  /**
   * @brief Gets a const player by their player ID
   * @param player_id The player ID
   * @return Optional const reference to the player if found
   */
  std::optional<std::reference_wrapper<const Player>> GetPlayer(PlayerId player_id) const;

  /**
   * @brief Gets a player by their connection handle
   * @param connection The connection handle
   * @return Optional reference to the player if found
   */
  std::optional<std::reference_wrapper<Player>> GetPlayerByConnection(Net::ConnectionHandle connection);

  /**
   * @brief Gets a const player by their connection handle
   * @param connection The connection handle
   * @return Optional const reference to the player if found
   */
  std::optional<std::reference_wrapper<const Player>> GetPlayerByConnection(Net::ConnectionHandle connection) const;

  /**
   * @brief Gets the connection handle for a player ID
   * @param player_id The player ID
   * @return Optional connection handle if found
   */
  std::optional<Net::ConnectionHandle> GetConnectionHandle(PlayerId player_id) const;

  /**
   * @brief Gets the player ID for a connection handle
   * @param connection The connection handle
   * @return Optional player ID if found
   */
  std::optional<PlayerId> GetPlayerId(Net::ConnectionHandle connection) const;

  /**
   * @brief Gets all players
   * @return Const reference to the players map
   */
  const std::unordered_map<PlayerId, Player>& GetAllPlayers() const {
    return players_;
  }

  /**
   * @brief Gets the number of players
   * @return The player count
   */
  std::size_t GetPlayerCount() const {
    return players_.size();
  }

  /**
   * @brief Checks if a player ID exists
   * @param player_id The player ID to check
   * @return true if the player exists, false otherwise
   */
  bool HasPlayer(PlayerId player_id) const {
    return players_.find(player_id) != players_.end();
  }

  /**
   * @brief Checks if a connection is associated with a player
   * @param connection The connection handle to check
   * @return true if a player with this connection exists, false otherwise
   */
  bool HasConnection(Net::ConnectionHandle connection) const {
    return connection_to_player_.find(connection) != connection_to_player_.end();
  }

  /**
   * @brief Iterates over all players
   * @param func Function to call for each player (receives Player&)
   */
  template <typename Func>
  void ForEachPlayer(Func&& func) {
    for (auto& [id, player] : players_) {
      func(player);
    }
  }

  /**
   * @brief Iterates over all players (const version)
   * @param func Function to call for each player (receives const Player&)
   */
  template <typename Func>
  void ForEachPlayer(Func&& func) const {
    for (const auto& [id, player] : players_) {
      func(player);
    }
  }

  /**
   * @brief Iterates over all in-game players
   * @param func Function to call for each in-game player (receives Player&)
   */
  template <typename Func>
  void ForEachIngamePlayer(Func&& func) {
    for (auto& [id, player] : players_) {
      if (player.is_ingame) {
        func(player);
      }
    }
  }

  /**
   * @brief Iterates over all in-game players (const version)
   * @param func Function to call for each in-game player (receives const Player&)
   */
  template <typename Func>
  void ForEachIngamePlayer(Func&& func) const {
    for (const auto& [id, player] : players_) {
      if (player.is_ingame) {
        func(player);
      }
    }
  }

  /**
   * @brief Clears all players
   */
  void Clear() {
    players_.clear();
    player_to_connection_.clear();
    connection_to_player_.clear();
    next_player_id_ = 1;
  }

private:
  PlayerId next_player_id_ = 1;
  std::unordered_map<PlayerId, Player> players_;
  std::unordered_map<PlayerId, Net::ConnectionHandle> player_to_connection_;
  std::unordered_map<Net::ConnectionHandle, PlayerId> connection_to_player_;
};
