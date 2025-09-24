

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
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "Script.h"
#include "character_definition.h"
#include "common_structs.h"
#include "config.h"
#include "znet_server.h"

#define DEFAULT_ADMIN_PORT 0x404

class CLog;
class GothicClock;
class HTTPServer;

enum CONFIG_FLAGS { QUICK_POTS = 0x01, DROP_ITEMS = 0x02, HIDE_MAP = 0x04 };

struct Packet {
  // Not owning.
  unsigned char* data = nullptr;
  std::uint32_t length = 0;
  Net::PlayerId id;
};

class GameServer : public Net::PacketHandler {
public:
  enum PL_FLAGS {
    PL_UNCONCIOUS = 0x01,  // 00000001
    PL_BURN = 0x02,        // 00000010
  };
  enum FILE_REQ { CLASS_FILE = 1, SPAWN_FILE = 2, WB_FILE = 3, NULL_SIZE = 255 };
  struct sPlayer {
    Net::PlayerId id;
    std::string name;
    unsigned char char_class, flags, head, skin, body, walkstyle, figth_pos, spellhand, headstate, is_ingame, passed_crc_test, mute;
    short health, mana;
    // miejce na obrót głowy
    time_t tod;  // time of death
    PlayerState state;
  };

  struct DiscordActivityState {
    std::string state;
    std::string details;
    std::string large_image_key;
    std::string large_image_text;
    std::string small_image_key;
    std::string small_image_text;
  };

public:
  GameServer();
  ~GameServer() override;

  void AddToPublicListHTTP();
  bool Receive();
  bool HandlePacket(Net::PlayerId playerId, unsigned char* data, std::uint32_t size);
  void Run();
  bool Init();
  void SaveBanList(void);
  bool IsPublic(void);
  void SendServerMessage(const std::string& message);

  std::optional<std::reference_wrapper<sPlayer>> GetPlayerById(std::uint64_t id);

  void UpdateDiscordActivity(const DiscordActivityState& activity);
  const DiscordActivityState& GetDiscordActivity() const;

private:
  void DeleteFromPlayerList(Net::PlayerId guid);
  void LoadBanList(void);
  void HandleCastSpell(Packet p, bool target);
  void HandleDropItem(Packet p);
  void HandleTakeItem(Packet p);
  void HandleVoice(Packet p);
  void SomeoneJoinGame(Packet p);
  void HandlePlayerUpdate(Packet p);
  void MakeHPDiff(Packet p);
  void HandlePlayerDisconnect(Net::PlayerId id);
  void HandlePlayerDeath(sPlayer& victim, std::optional<std::uint64_t> killer_id);
  void HandleNormalMsg(Packet p);
  void HandleWhisp(Packet p);
  void HandleRMConsole(Packet p);
  void HandleGameInfo(Packet p);
  void HandleMapNameReq(Packet p);
  void SendDisconnectionInfo(uint64_t disconnected_id);
  void SendDeathInfo(uint64_t deadman);
  void SendRespawnInfo(uint64_t luckyguy);
  void SendGameInfo(Net::PlayerId guid);
  void SendDiscordActivity(Net::PlayerId guid);

  std::vector<std::string> ban_list;
  std::unique_ptr<CharacterDefinitionManager> character_definition_manager_;
  std::unique_ptr<Script> script;
  time_t last_stand_timer;
  time_t regen_time;

  void ProcessRespawns();

  unsigned char GetPacketIdentifier(const Packet& p);
  int serverPort;
  unsigned short maxConnections;
  std::unordered_map<std::uint64_t, sPlayer> players_;
  bool allow_modification = false;
  Config config_;
  std::unique_ptr<GothicClock> clock_;
  std::unique_ptr<HTTPServer> http_server_;
  std::future<void> public_list_http_thread_future_;
  std::chrono::time_point<std::chrono::steady_clock> last_update_time_{};
  std::thread main_thread;
  std::atomic<bool> main_thread_running = false;
  DiscordActivityState discord_activity_{};
  bool discord_activity_initialized_{false};
};

inline GameServer* g_server = nullptr;
