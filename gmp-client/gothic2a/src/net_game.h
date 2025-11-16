/*
MIT License

Copyright (c) 2023 Gothic Multiplayer Team.

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

#include <memory>
#include <string>
#include <vector>

#include "CSyncFuncs.h"
#include "HooksManager.h"
#include "ZenGin/zGothicAPI.h"
#include "event_observer.hpp"
#include "game_client.hpp"
#include "gothic2a_player.hpp"
#include "gothic_task_scheduler.h"

struct MD5Sum {
  BYTE data[16];
};

union STime {
  int time;
  struct {
    unsigned short day;
    unsigned char hour, min;
  };
};

class NetGame : public CSyncFuncs, public gmp::client::EventObserver {
public:
  void HandleNetwork();
  bool IsConnected();
  bool Connect(std::string_view full_address);
  void JoinGame();
  void SendDropItem(short Instance, short amount);
  void SendTakeItem(short Instance);
  void SendCastSpell(oCNpc* Target, short SpellId);
  void SendMessage(const char* msg);
  void SendWhisper(const char* player_name, const char* msg);
  void SendCommand(const char* msg);
  void UpdatePlayerStats(short anim);
  void SendHPDiff(size_t who, short diff);
  void SyncGameTime();
  void Disconnect();
  void RestoreHealth();

  // Task scheduler hook - called from render hook
  static void __stdcall ProcessTaskScheduler();

  static NetGame& Instance() {
    static NetGame instance;
    return instance;
  }

  std::vector<Gothic2APlayer*> players;
  int HeroLastHp;
  zSTRING map;
  bool IsAdminOrModerator{false};
  bool IsInGame{false};
  short mp_restore{0};
  int DropItemsAllowed{0};
  int ForceHideMap{0};
  bool IsReadyToJoin{false};
  std::unique_ptr<gmp::GothicTaskScheduler> task_scheduler;
  std::unique_ptr<gmp::client::GameClient> game_client;

  // EventObserver interface implementation
  void OnConnectionStarted() override;
  void OnConnected() override;
  void OnConnectionFailed(const std::string& error) override;
  void OnDisconnected() override;
  void OnConnectionLost() override;
  void OnMapChange(const std::string& map_name) override;
  void OnGameInfoReceived(std::uint32_t raw_game_time, std::uint8_t flags) override;
  void OnLocalPlayerJoined(gmp::client::Player& player) override;
  void OnLocalPlayerSpawned(gmp::client::Player& player) override;
  void OnPlayerJoined(gmp::client::Player& player) override;
  void OnPlayerSpawned(gmp::client::Player& player) override;
  void OnPlayerLeft(std::uint64_t player_id, const std::string& player_name) override;
  void OnPlayerStateUpdate(std::uint64_t player_id, const PlayerState& state) override;
  void OnPlayerPositionUpdate(std::uint64_t player_id, float x, float z) override;
  void OnPlayerDied(std::uint64_t player_id) override;
  void OnPlayerRespawned(std::uint64_t player_id) override;
  void OnItemDropped(std::uint64_t player_id, std::uint16_t item_instance, std::uint16_t amount) override;
  void OnItemTaken(std::uint64_t player_id, std::uint16_t item_instance) override;
  void OnSpellCast(std::uint64_t caster_id, std::uint16_t spell_id) override;
  void OnSpellCastOnTarget(std::uint64_t caster_id, std::uint64_t target_id, std::uint16_t spell_id) override;
  void OnChatMessage(std::uint64_t sender_id, const std::string& sender_name, const std::string& message) override;
  void OnWhisperReceived(std::uint64_t sender_id, const std::string& sender_name, const std::string& message) override;
  void OnServerMessage(const std::string& message) override;
  void OnRconResponse(const std::string& response, bool is_admin) override;
  void OnDiscordActivityUpdate(const std::string& state, const std::string& details, const std::string& large_image_key,
                               const std::string& large_image_text, const std::string& small_image_key, const std::string& small_image_text) override;

private:
  NetGame();
  time_t last_mp_regen;

  Gothic2APlayer* GetPlayerById(std::uint64_t player_id);
  void SpawnRemotePlayer(gmp::client::Player& new_player);

};
