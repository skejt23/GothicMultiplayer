

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

#include <string.h>

#include <atomic>
#include <ctime>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace httplib {
class Server;
struct Request;
struct Response;
}  // namespace httplib

#include "Script.h"
#include "ban_manager.h"
#include "client_resource_packager.h"
#include "common_structs.h"
#include "config.h"
#include "packet.h"
#include "player_manager.h"
#include "resource_manager.h"
#include "resource_server.h"
#include "znet_server.h"

#define DEFAULT_ADMIN_PORT 0x404

class CLog;
class GothicClock;

enum CONFIG_FLAGS { HIDE_MAP = 0x04 };

class GameServer : public Net::PacketHandler {
public:
  using PlayerId = PlayerManager::PlayerId;
  using Player = PlayerManager::Player;

  using BanEntry = BanManager::BanEntry;

  GameServer();
  ~GameServer() override;

  void AddToPublicListHTTP();
  bool Receive();
  bool HandlePacket(Net::ConnectionHandle connectionHandle, unsigned char* data, std::uint32_t size);
  void Run();
  bool Init();
  bool IsPublic(void);
  void SendServerMessage(const std::string& message);
  bool SpawnPlayer(PlayerId player_id, std::optional<glm::vec3> position_override = std::nullopt);
  bool SetPlayerPosition(PlayerId player_id, const glm::vec3& position);
  std::optional<glm::vec3> GetPlayerPosition(PlayerId player_id) const;

  PlayerManager& GetPlayerManager() {
    return player_manager_;
  }
  const PlayerManager& GetPlayerManager() const {
    return player_manager_;
  }

  std::uint32_t GetPort() const;

private:
  void DeleteFromPlayerList(PlayerId player_id);
  void HandleCastSpell(Packet p, bool target);
  void HandleDropItem(Packet p);
  void HandleTakeItem(Packet p);
  void HandleVoice(Packet p);
  void SomeoneJoinGame(Packet p);
  void HandlePlayerUpdate(Packet p);
  void MakeHPDiff(Packet p);
  void HandlePlayerDisconnect(Net::ConnectionHandle connection);
  void HandlePlayerDeath(Player& victim, std::optional<PlayerId> killer_id);
  void HandleNormalMsg(Packet p);
  void HandleWhisp(Packet p);
  void HandleRMConsole(Packet p);
  void HandleGameInfo(Packet p);
  void HandleMapNameReq(Packet p);
  void SendDisconnectionInfo(PlayerId player_id);
  void SendDeathInfo(PlayerId player_id);
  void SendRespawnInfo(PlayerId player_id);
  void BroadcastPlayerJoined(const Player& joining_player);
  void SendGameInfo(Net::ConnectionHandle connection);
  void SendExistingPlayersPacket(const Player& target_player);

  std::unique_ptr<BanManager> ban_manager_;
  std::unique_ptr<LuaScript> lua_script_;
  std::unique_ptr<ResourceManager> resource_manager_;
  time_t last_stand_timer;
  time_t regen_time;

  void ProcessRespawns();

  unsigned char GetPacketIdentifier(const Packet& p);
  int serverPort;
  unsigned short maxConnections;
  PlayerManager player_manager_;
  bool allow_modification = false;
  Config config_;
  std::unique_ptr<GothicClock> clock_;
  std::future<void> public_list_http_thread_future_;
  std::chrono::time_point<std::chrono::steady_clock> last_update_time_{};
  std::thread main_thread;
  std::atomic<bool> main_thread_running = false;
  std::vector<ClientResourceDescriptor> client_resource_descriptors_;

  std::unique_ptr<ResourceServer> resource_server_;
};

inline GameServer* g_server = nullptr;
