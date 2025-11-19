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

#include <cstddef>
#include <cstdint>
#include <string>

#include "common_structs.h"

namespace gmp::client {

// Forward declaration
class Player;

class EventObserver {
public:
  EventObserver() = default;
  virtual ~EventObserver() = default;

  // Connection events
  virtual void OnConnectionStarted() {}
  virtual void OnConnected() {}
  virtual void OnConnectionFailed(const std::string& error) {}
  virtual void OnDisconnected() {}
  virtual void OnConnectionLost() {}
  virtual bool RequestResourceDownloadConsent(std::size_t resource_count, std::uint64_t total_bytes) { return true; }
  virtual void OnResourceDownloadProgress(const std::string& resource_name, std::uint64_t downloaded_bytes, std::uint64_t total_bytes) {}
  virtual void OnResourceDownloadFailed(const std::string& reason) {}
  virtual void OnResourcesReady() {}
  
  // World/map events
  virtual void OnMapChange(const std::string& map_name) {}
  virtual void OnGameInfoReceived(std::uint32_t raw_game_time, std::uint8_t flags) {}
  
  // Player events
  virtual void OnLocalPlayerJoined(gmp::client::Player& player) {}
  virtual void OnLocalPlayerSpawned(gmp::client::Player& player) {}
  virtual void OnPlayerJoined(gmp::client::Player& player) {}
  virtual void OnPlayerSpawned(gmp::client::Player& player) {}
  virtual void OnPlayerLeft(std::uint64_t player_id, const std::string& player_name) {}
  virtual void OnPlayerStateUpdate(std::uint64_t player_id, const PlayerState& state) {}
  virtual void OnPlayerPositionUpdate(std::uint64_t player_id, float x, float z) {}
  virtual void OnPlayerDied(std::uint64_t player_id) {}
  virtual void OnPlayerRespawned(std::uint64_t player_id) {}
  
  // Item events
  virtual void OnItemDropped(std::uint64_t player_id, std::uint16_t item_instance, std::uint16_t amount) {}
  virtual void OnItemTaken(std::uint64_t player_id, std::uint16_t item_instance) {}
  
  // Spell/combat events
  virtual void OnSpellCast(std::uint64_t caster_id, std::uint16_t spell_id) {}
  virtual void OnSpellCastOnTarget(std::uint64_t caster_id, std::uint64_t target_id, std::uint16_t spell_id) {}
  
  // Chat/messaging events
  virtual void OnChatMessage(std::uint64_t sender_id, const std::string& sender_name, const std::string& message) {}
  virtual void OnWhisperReceived(std::uint64_t sender_id, const std::string& sender_name, const std::string& message) {}
  virtual void OnServerMessage(const std::string& message) {}
  virtual void OnRconResponse(const std::string& response, bool is_admin) {}
  
  // Discord integration
  virtual void OnDiscordActivityUpdate(const std::string& state, const std::string& details, 
                                       const std::string& large_image_key, const std::string& large_image_text,
                                       const std::string& small_image_key, const std::string& small_image_text) {}
};

}  // namespace gmp::client