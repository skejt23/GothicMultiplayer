

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

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
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
#include "gothic_clock.h"
#include "gothic_weather.h"
#include "item_ground_manager.h"
#include "npc_manager.h"
#include "player_manager.h"
#include "resource_manager.h"
#include "resource_server.h"
#include "server_addon_context.h"
#include "summon_registry.h"
#include "transport/transport.h"

#define DEFAULT_ADMIN_PORT 0x404

class CLog;
class GothicClock;
struct InventoryEntry;

enum CONFIG_FLAGS { HIDE_MAP = 0x04 };

struct Packet {
  // Not owning.
  unsigned char* data = nullptr;
  std::uint32_t length = 0;
  Net::ConnectionHandle id;
};

class GameServer {
public:
  using PlayerId = PlayerManager::PlayerId;
  using Player = PlayerManager::Player;

  using BanEntry = BanManager::BanEntry;

  GameServer();
  ~GameServer();

  void AddToPublicListHTTP();
  nlohmann::json BuildMasterServerPayload() const;
  bool Receive();
  bool HandlePacket(Net::ConnectionHandle connectionHandle, unsigned char* data, std::uint32_t size);
  void Run();
  bool Init();
  bool IsPublic(void);
  void SendMessageToAll(std::uint8_t r, std::uint8_t g, std::uint8_t b, const std::string& text);
  void SendMessageToPlayer(PlayerId player_id, std::uint8_t r, std::uint8_t g, std::uint8_t b,
                           const std::string& text);
  void SendPlayerMessageToAll(PlayerId sender_id, std::uint8_t r, std::uint8_t g, std::uint8_t b,
                              const std::string& text);
  void SendPlayerMessageToPlayer(PlayerId sender_id, PlayerId receiver_id, std::uint8_t r, std::uint8_t g,
                                 std::uint8_t b, const std::string& text);
  bool SpawnCharacter(PlayerId player_id, std::optional<glm::vec3> position_override = std::nullopt);
  bool UnspawnCharacter(PlayerId player_id);
  PlayerId CreateNpc(const std::string& name, const std::string& instance = "PC_HERO");
  bool DestroyNpc(PlayerId npc_id);
  void DestroyNpcsForResource(const std::string& owner);
  bool SetNpcHostPlayer(PlayerId npc_id, PlayerId host_id);
  bool SetNpcAnimation(PlayerId npc_id, const std::string& animation);
  std::uint32_t QueueNpcAnimation(PlayerId npc_id, const std::string& animation, std::uint32_t timeout_ms = 10000);
  bool ClearNpcActions(PlayerId npc_id);
  std::optional<std::reference_wrapper<Character>> GetCharacter(PlayerId id);
  std::optional<std::reference_wrapper<const Character>> GetCharacter(PlayerId id) const;
  NpcManager& GetNpcManager() { return npc_manager_; }
  const NpcManager& GetNpcManager() const { return npc_manager_; }
  bool SetCharacterName(PlayerId player_id, const std::string& name);
  bool SetCharacterInstance(PlayerId player_id, const std::string& instance);
  bool SetCharacterNameColor(PlayerId player_id, std::uint8_t r, std::uint8_t g, std::uint8_t b);
  bool SetCharacterStrength(PlayerId player_id, std::int32_t strength);
  bool SetCharacterDexterity(PlayerId player_id, std::int32_t dexterity);
  bool SetCharacterLevel(PlayerId player_id, std::int32_t level);
  bool SetCharacterExperience(PlayerId player_id, std::int32_t exp);
  bool SetCharacterNextLevelExperience(PlayerId player_id, std::int32_t next_level_exp);
  bool SetCharacterLearnPoints(PlayerId player_id, std::int32_t learn_points);
  bool SetCharacterMaxHealth(PlayerId player_id, std::int32_t max_health);
  bool SetCharacterHealth(PlayerId player_id, std::int32_t health);
  bool SetCharacterInvulnerable(PlayerId player_id, bool enabled);
  bool SetCharacterMaxMana(PlayerId player_id, std::int32_t max_mana);
  bool SetCharacterMana(PlayerId player_id, std::int32_t mana);
  bool SetCharacterSkillWeapon(PlayerId player_id, std::int32_t skill_id, std::int32_t percentage);
  bool SetCharacterTalent(PlayerId player_id, std::int32_t talent_id, std::int32_t talent_value);
  bool SetCharacterVisual(PlayerId player_id, const std::string& body_model, std::int16_t body_texture, const std::string& head_model,
                       std::int16_t head_texture, std::int16_t teeth_texture, std::int16_t skin_color);
  bool SetCharacterFatness(PlayerId player_id, float fatness);
  bool SetCharacterScale(PlayerId player_id, const glm::vec3& scale);
  bool SetCharacterWeaponMode(PlayerId player_id, std::int32_t weapon_mode);
  bool ApplyCharacterOverlay(PlayerId player_id, const std::string& overlay);
  bool RemoveCharacterOverlay(PlayerId player_id, const std::string& overlay);
  bool PlayAnimation(PlayerId player_id, const std::string& animation);
  bool StopAnimation(PlayerId player_id, const std::string& animation);
  bool PlayFaceAnimation(PlayerId player_id, const std::string& animation);
  bool StopFaceAnimation(PlayerId player_id, const std::string& animation);
  bool PlayGesticulation(PlayerId player_id);
  bool SetCharacterPosition(PlayerId player_id, const glm::vec3& position, std::optional<bool> teleport = std::nullopt);
  bool SetCharacterAngle(PlayerId player_id, float angle);
  bool GiveItem(PlayerId player_id, const std::string& instance, std::int32_t amount);
  bool EquipItem(PlayerId player_id, const std::string& instance, std::int32_t slot_id = -1);
  bool UnequipItem(PlayerId player_id, const std::string& instance, std::int32_t slot_id = Net::EQUIP_SLOT_GENERIC);
  bool EquipArmor(PlayerId player_id, const std::string& instance);
  bool UnequipArmor(PlayerId player_id);
  bool EquipMeleeWeapon(PlayerId player_id, const std::string& instance);
  bool UnequipMeleeWeapon(PlayerId player_id);
  bool EquipRangedWeapon(PlayerId player_id, const std::string& instance);
  bool UnequipRangedWeapon(PlayerId player_id);
  bool EquipHelmet(PlayerId player_id, const std::string& instance);
  bool UnequipHelmet(PlayerId player_id);
  bool EquipShield(PlayerId player_id, const std::string& instance);
  bool UnequipShield(PlayerId player_id);
  std::int32_t HasItem(PlayerId player_id, const std::string& instance) const;
  bool RemoveItem(PlayerId player_id, const std::string& instance, std::int32_t amount);
  bool SetCharacterWorld(PlayerId player_id, const std::string& world, std::optional<std::string> start_point = std::nullopt);
  bool SetCharacterVirtualWorld(PlayerId player_id, std::int32_t virtual_world);
  std::optional<glm::vec3> GetCharacterPosition(PlayerId player_id) const;
  std::string GetHostname() const;
  std::uint32_t GetMaxSlots() const;
  bool SetServerWorld(const std::string& world);
  std::string GetServerWorld() const;
  std::vector<PlayerId> FindNearbyPlayers(const glm::vec3& position, float radius, const std::string& world,
                                          std::int32_t virtual_world) const;
  std::vector<PlayerId> GetCharactersStreamedToPlayer(PlayerId player_id) const;
  std::vector<PlayerId> GetPlayersStreamingCharacter(PlayerId player_id) const;
  bool SetStreamerRadius(std::int32_t radius);
  std::int32_t GetStreamerRadius() const;
  bool SetStreamerHeight(std::int32_t height);
  std::int32_t GetStreamerHeight() const;
  bool SetVoiceEnabled(bool enabled);
  bool SetVoiceRange(std::int32_t range);
  bool SetVoiceEnabledForPlayer(PlayerId player_id, bool enabled);
  bool GetVoiceEnabled() const;
  std::int32_t GetVoiceRange() const;
  std::optional<bool> GetVoiceEnabledForPlayer(PlayerId player_id) const;
  bool SetTime(std::int32_t hour, std::int32_t min, std::int32_t day = 0);
  GothicClock::Time GetTime() const;
  bool SetDayLength(float day_length_ms);
  float GetDayLength() const;
  bool SetWeatherType(std::int32_t weather_type);
  std::int32_t GetWeatherType() const;
  bool SetRainStartTime(std::int32_t hour, std::int32_t min);
  std::pair<std::int32_t, std::int32_t> GetRainStartTime() const;
  bool SetRainStopTime(std::int32_t hour, std::int32_t min);
  std::pair<std::int32_t, std::int32_t> GetRainStopTime() const;
  bool SetWindScale(float wind_scale);
  float GetWindScale() const;
  bool SetDontRain(bool toggle);
  bool GetDontRain() const;
  bool SetWeatherDisabled(bool toggle);
  bool GetWeatherDisabled() const;
  bool TriggerClientEvent(const std::vector<PlayerId>& targets, const std::string& event_name, PlayerId source_element,
                          const std::string& payload);
  bool KickPlayer(PlayerId player_id, const std::string& reason);
  bool BanPlayer(PlayerId player_id, const std::string& reason);
  bool IsPlayerConnected(PlayerId player_id) const;
  bool IsPlayerAdmin(PlayerId player_id) const;
  bool IsPlayerDead(PlayerId player_id) const;
  bool IsPlayerSpawned(PlayerId player_id) const;
  bool IsPlayerUnconscious(PlayerId player_id) const;
  bool RespawnPlayer(PlayerId player_id);
  bool SetPlayerRespawnTime(PlayerId player_id, std::int32_t respawn_time_ms);
  std::optional<std::int32_t> GetPlayerRespawnTime(PlayerId player_id) const;
  std::string GetPlayerIp(PlayerId player_id) const;
  std::int32_t GetPlayerPing(PlayerId player_id) const;
  std::string GetPlayerMacAddress(PlayerId player_id) const;
  std::string GetPlayerIdentity(PlayerId player_id) const;
  std::uint32_t CreateItemGround(ItemGroundManager::CreateOptions options);
  bool DestroyItemGround(std::uint32_t item_ground_id);
  bool SetItemGroundPosition(std::uint32_t item_ground_id, const glm::vec3& position);
  bool SetItemGroundRotation(std::uint32_t item_ground_id, const glm::vec3& rotation);
  bool SetItemGroundVirtualWorld(std::uint32_t item_ground_id, std::int32_t virtual_world);
  bool SetItemGroundPhysicsEnabled(std::uint32_t item_ground_id, bool enabled);
  ItemGroundManager& GetItemGroundManager() {
    return item_ground_manager_;
  }
  const ItemGroundManager& GetItemGroundManager() const {
    return item_ground_manager_;
  }

  PlayerManager& GetPlayerManager() {
    return player_manager_;
  }
  const PlayerManager& GetPlayerManager() const {
    return player_manager_;
  }

  std::uint32_t GetPort() const;

private:
  enum class EquipmentSlot { Armor, Helmet, Shield, MeleeWeapon, RangedWeapon, Amulet, Belt, RingLeft, RingRight };

  struct VoiceRateState {
    std::chrono::steady_clock::time_point window_started{};
    std::uint32_t packets_in_window{0};
  };

  void HandleConnectionEvent(const Net::ConnectionEvent& event);
  void HandlePlayerConnected(Net::ConnectionHandle connection);
  void DeleteFromPlayerList(PlayerId player_id);
  void HandleCastSpell(Packet p, bool target);
  void HandleNativeTeleport(Packet p);
  void HandleTransformationReturn(Packet p);
  std::vector<PlayerId> PrepareSpellSummons(const Character& caster, std::uint32_t cast_id, const SummonRegistry::Definition& definition);
  void HandleActorAction(Packet p);
  bool CanReportActorAction(const Player& sender, const Character& actor, std::uint32_t control_epoch,
                            std::uint32_t command_revision) const;
  bool CanControlActor(const Player& sender, const Character& actor, std::uint32_t control_epoch) const;
  void ReplayActorActions(Player& viewer);
  void ProcessActorEffects();
  void DispatchPendingActorEvents();
  void HandlePlayerHitReport(Packet p, std::uint8_t packet_identifier);
  void HandlePlayerUnconsciousReport(Packet p, std::uint8_t packet_identifier);
  void HandlePlayerStandUpReport(Packet p, std::uint8_t packet_identifier);
  void HandlePlayerDeathReport(Packet p, std::uint8_t packet_identifier);
  void HandleDropItem(Packet p);
  void HandleTakeItem(Packet p);
  void HandleEquipmentResult(Packet p);
  void ProcessEquipmentCommands();
  void ProcessGenericEquipment(PlayerId player_id);
  void CompleteEquipmentCommand(PlayerId player_id, EquipmentSlot slot, bool success);
  void SendEquipmentCorrection(Character& character, EquipmentSlot slot, const std::string& rejected_instance = std::string{});
  std::optional<EquipmentSlot> PacketEquipmentSlot(std::int16_t slot_id) const;
  bool InitializePlayerInventory(Player& player, const std::vector<InventoryEntry>& inventory);
  void HandlePlayerWorldEnter(Packet p);
  void HandleLuaEvent(Packet p);
  void HandleVoice(Packet p);
  void HandleVoiceChannel(Packet p);
  void SendVoiceConfiguration(const Player& player);
  bool IsVoiceChannelSpatial(const std::string& channel) const;
  void MarkPlayerVoiceActive(PlayerId player_id, std::chrono::steady_clock::time_point now);
  void EndPlayerVoiceActivity(PlayerId player_id);
  void ProcessVoiceActivity(std::chrono::steady_clock::time_point now);
  void SomeoneJoinGame(Packet p);
  void HandlePlayerUpdate(Packet p);
  void HandlePlayerInstanceReady(Packet p);
  void CompletePlayerInstanceChange(Character& actor, const PlayerSpawnPacket& defaults,
                                   std::unordered_map<std::string, std::int32_t> inventory, bool success);
  void ProcessInstanceChanges();
  void HandlePlayerDisconnect(Net::ConnectionHandle connection, std::int32_t reason);
  void HandleRconCommand(Player& player, const std::string& params);
  void HandleDiagnosticsCommand(Player& player);
  void HandleAdminLogin(Player& player, const std::string& password);
  bool ApplyPlayerDamage(Player& victim, std::optional<PlayerId> attacker_id, std::int32_t damage, std::uint32_t damage_type, bool dont_kill);
  bool ApplyCharacterDamage(Character& victim, std::optional<PlayerId> attacker_id, std::int32_t damage, std::uint32_t damage_type,
                            bool dont_kill, bool melee_attack = false, bool finishing_move = false);
  bool MakePlayerUnconscious(Player& victim, std::optional<PlayerId> attacker_id);
  void HandlePlayerDeath(Character& victim, std::optional<PlayerId> killer_id);
  void HandleNormalMsg(Packet p);
  void HandleGameInfo(Packet p);
  void HandleMapNameReq(Packet p);
  void SendDisconnectionInfo(PlayerId player_id);
  void SendDeathInfo(PlayerId player_id);
  void SendRespawnInfo(PlayerId player_id);
  void SendUnconsciousInfo(PlayerId player_id, std::optional<PlayerId> attacker_id);
  void SendStandUpInfo(PlayerId player_id);
  void BroadcastPlayerJoined(const Player& joining_player);
  void SendGameInfo(Net::ConnectionHandle connection);
  void SendGameInfo(Net::ConnectionHandle connection, GothicClock::Time time);
  void SendSkySettings(Net::ConnectionHandle connection);
  void BroadcastGameInfo();
  void BroadcastGameInfo(GothicClock::Time time);
  void BroadcastSkySettings();
  void BroadcastPlayerPresence();
  void SendAdminAuthStatus(const Player& player);
  void UpdateAuthoritativeWorldState(const std::vector<GothicClock::Time>& advanced_times);
  void RefreshPlayerStreaming(bool send_state_updates = false);
  bool RespawnPlayerInternal(Player& player);
  void SendItemGroundCreate(const ItemGroundManager::ItemGround& item_ground, Net::ConnectionHandle connection);
  void SendItemGroundDestroy(std::uint32_t item_ground_id, Net::ConnectionHandle connection);
  void SendItemGroundClear(Net::ConnectionHandle connection);
  void StreamItemGroundToPlayer(ItemGroundManager::ItemGround& item_ground, Player& player, bool force = false);
  void StreamRelevantGroundItemsToPlayer(Player& player, bool force = false);
  void UnstreamGroundItemsFromPlayer(Player& player, bool notify_client);
  void RefreshItemGroundStreaming(ItemGroundManager::ItemGround& item_ground);
  void SendItemTransactionResult(Character& player, std::uint32_t request_id, const std::string& item_instance,
                                 bool accepted, std::optional<std::uint32_t> item_ground_id = std::nullopt);
  void ClearRemovedEquipment(Character& character, const std::string& item_instance, std::int32_t remaining_amount);
  bool EquipItemInSlot(PlayerId player_id, const std::string& instance, EquipmentSlot slot);
  bool UnequipItemInSlot(PlayerId player_id, EquipmentSlot slot);
  std::int16_t EquipmentSlotPacketId(EquipmentSlot slot) const;
  std::string& EquipmentStateField(PlayerState& state, EquipmentSlot slot) const;
  std::optional<std::string>& PendingEquipmentStateField(Character& player, EquipmentSlot slot) const;
  bool TriggerEquipmentEvent(PlayerId player_id, EquipmentSlot slot, std::optional<std::string> item_instance,
                             std::optional<std::string> previous_instance = std::nullopt);
  std::optional<std::string> ResolveItemInstance(std::string instance) const;
  void NormalizePlayerStateInstances(PlayerState& state) const;
  void ValidatePlayerEquipment(Character& character, PlayerState& incoming) const;
  std::optional<std::reference_wrapper<Player>> GetIngamePlayerByConnection(Net::ConnectionHandle connection);

  Net::ServerTransport transport_;
  std::unique_ptr<BanManager> ban_manager_;
  std::unique_ptr<LuaScript> lua_script_;
  std::unique_ptr<ResourceManager> resource_manager_;

  void ProcessRespawns();
  void ProcessNpcs();
  void RefreshNpcStreaming(PlayerId npc_id);
  void BroadcastNpcControl(PlayerId npc_id);
  void ReleaseNpcsForPlayer(PlayerId player_id);
  void UnstreamNpc(NpcManager::Npc& npc);
  void HandleNpcActionResult(Packet packet);
  bool IsEligibleNpcHost(const NpcManager::Npc& npc, PlayerId host_id) const;
  bool SpawnNpc(PlayerId npc_id, std::optional<glm::vec3> position);
  bool UnspawnNpc(PlayerId npc_id);

  unsigned char GetPacketIdentifier(const Packet& p);
  PlayerManager player_manager_;
  NpcManager npc_manager_;
  // Deliver script callbacks after mutation/streaming loops, never while they
  // hold references that Lua may invalidate by destroying an NPC.
  std::vector<std::function<void()>> pending_npc_events_;
  std::unordered_set<std::string> unloading_npc_resources_;
  std::unordered_set<PlayerId> equipment_event_actors_;
  SummonRegistry summon_registry_;
  ItemGroundManager item_ground_manager_;
  bool allow_modification = false;
  Config config_;
  std::unique_ptr<GothicClock> clock_;
  GothicWeather weather_;
  std::string server_world_;
  std::int64_t last_weather_update_minute_{-1};
  std::chrono::time_point<std::chrono::steady_clock> last_player_presence_update_time_{};
  std::future<void> public_list_http_thread_future_;
  std::chrono::time_point<std::chrono::steady_clock> last_update_time_{};
  std::thread main_thread;
  std::atomic<bool> main_thread_running = false;
  std::vector<ClientResourceDescriptor> client_resource_descriptors_;
  std::unique_ptr<ServerAddonContext> addon_context_;
  std::unordered_map<PlayerId, VoiceRateState> voice_rate_states_;
  std::unordered_map<PlayerId, std::chrono::steady_clock::time_point> active_voice_players_;
  std::unordered_map<std::string, bool> voice_channel_spatial_;
  struct MobStateRecord {
    std::string world;
    std::int32_t virtual_world{0};
    std::string name;
    glm::vec3 position{0.0f};
    std::int32_t state{0};
  };
  std::vector<MobStateRecord> mob_states_;

  std::unique_ptr<ResourceServer> resource_server_;
};

inline GameServer* g_server = nullptr;
