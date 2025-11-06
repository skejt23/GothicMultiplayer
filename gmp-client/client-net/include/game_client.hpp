
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
#include <functional>
#include <map>
#include <mutex>
#include <queue>
#include <string_view>
#include <thread>
#include <vector>

#include "common_structs.h"
#include "event_observer.hpp"
#include "players.hpp"
#include "task_scheduler.h"
#include "world.hpp"
#include "znet_client.h"

namespace gmp::client {

class GameClient : public Net::NetClient::PacketHandler {
public:
  enum class ConnectionState { Disconnected, Connecting, Connected, Failed };

  GameClient(EventObserver& eventObserver, gmp::TaskScheduler& taskScheduler);
  ~GameClient();

  void ConnectAsync(std::string_view endpoint);
  void Disconnect();
  bool IsConnected() const;
  ConnectionState GetConnectionState() const;
  std::string GetConnectionError() const;
  int GetPing();

  // To be called after connecting to the server and receiving the initial info packet,
  // downloading all the required files, and loading the world.
  void JoinGame(const std::string& player_name, const std::string& character_name, int head_model, int skin_texture, int face_texture,
                int walk_style);

  // Send methods
  void SendChatMessage(const std::string& msg);
  void SendWhisper(std::uint64_t recipient_id, const std::string& msg);
  void SendCommand(const std::string& msg);
  void SendCastSpell(std::uint64_t target_id, std::uint16_t spell_id);
  void SendDropItem(std::uint16_t instance, std::uint16_t amount);
  void SendTakeItem(std::uint16_t instance);
  void UpdatePlayerStats(const PlayerState& state);
  void SendHPDiff(std::uint64_t player_id, std::int16_t diff);
  void SyncGameTime();

  // Network handling - MUST be called from main thread
  void HandleNetwork();

  PlayerManager& player_manager() {
    return player_manager_;
  }

  const std::string& GetServerIp() const {
    return server_ip_;
  }

  std::uint32_t GetServerPort() const {
    return server_port_;
  }

private:
  struct Packet {
    unsigned char* data = nullptr;
    std::uint32_t length = 0;
  };

  void InitPacketHandlers();
  bool HandlePacket(unsigned char* data, std::uint32_t size) override;

  // Internal blocking connect called from connection thread
  bool ConnectInternal(std::string_view endpoint);

  // Helper to update player state from PlayerState struct
  void UpdatePlayerState(Player* player, const PlayerState& state);

  // Packet handlers
  void OnInitialInfo(Packet packet);
  void OnActualStatistics(Packet packet);
  void OnMapOnly(Packet packet);
  void OnDoDie(Packet packet);
  void OnRespawn(Packet packet);
  void OnCastSpell(Packet packet);
  void OnCastSpellOnTarget(Packet packet);
  void OnDropItem(Packet packet);
  void OnTakeItem(Packet packet);
  void OnWhisper(Packet packet);
  void OnMessage(Packet packet);
  void OnServerMessage(Packet packet);
  void OnRcon(Packet packet);
  void OnExistingPlayers(Packet packet);
  void OnJoinGame(Packet packet);
  void OnGameInfo(Packet packet);
  void OnLeftGame(Packet packet);
  void OnDiscordActivity(Packet packet);
  void OnDisconnectOrLostConnection(Packet packet);

  EventObserver& event_observer_;
  gmp::TaskScheduler& task_scheduler_;
  PlayerManager player_manager_;

  using PacketHandlerFunc = std::function<void(Packet)>;
  std::map<int, PacketHandlerFunc> packet_handlers_;
  std::vector<World> worlds_;

  std::string server_ip_;
  std::uint32_t server_port_{0};
  bool connection_lost_{false};
  bool is_in_game_{false};

  // Async connection support
  ConnectionState connection_state_{ConnectionState::Disconnected};
  std::thread connection_thread_;
  mutable std::mutex connection_mutex_;
  std::string connection_error_;
};

// Function to load the network library dynamically. Must be called
// before using any other functions in this library.
void LoadNetworkLibrary();

}  // namespace gmp::client