
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

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "common_structs.h"
#include "packets.h"
#include "npc_packets.h"
#include "event_observer.hpp"
#include "players.hpp"
#include "task_scheduler.h"
#include "world.hpp"
#include "transport/transport.h"
#include "resource_downloader.h"

namespace gmp::client {

constexpr std::uint32_t kDefaultServerPort = 57005;

bool ParseServerEndpoint(std::string_view full_address, std::string& host, std::uint32_t& port);

class GameClient {
public:
  enum class ConnectionState { Disconnected, Connecting, Connected, Failed };

  using ResourcePayload = ResourceDownloader::ResourcePayload;
  using AddonPayload = ResourceDownloader::AddonPayload;

  GameClient(EventObserver& eventObserver, gmp::TaskScheduler& taskScheduler);
  ~GameClient();

  void ConnectAsync(std::string_view endpoint);
  void Disconnect();
  bool IsConnected() const;
  ConnectionState GetConnectionState() const;
  std::string GetConnectionError() const;
  Net::CloseReason GetConnectionCloseReason() const;
  int GetPing();
  std::int32_t GetPlayerPing(std::uint64_t player_id);
  Net::NetworkStats GetNetworkStats() const;
  const std::map<std::uint32_t, PlayerPresenceEntry>& GetOnlinePlayers() const {
    return online_players_;
  }
  const PlayerPresenceEntry* GetOnlinePlayer(std::uint64_t id) const {
    const auto it = online_players_.find(static_cast<std::uint32_t>(id));
    return it != online_players_.end() && it->second.player_id == id ? &it->second : nullptr;
  }

  // To be called after connecting to the server and receiving the initial info packet,
  // downloading all the required files, and loading the world.
  void JoinGame(const std::string& player_name, const std::string& character_name, 
                const std::string& body_model, int body_texture, const std::string& head_model, int head_texture, int walk_style,
                int teeth_texture = 0, int skin_color = 0, const std::string& walking_overlay = {});

  // Send methods
  void SendChatMessage(const std::string& msg);
  void SendNativeTeleport(PlayerTeleportPacket packet);
  std::uint32_t SendCastSpell(std::uint32_t target_id, std::uint16_t spell_id, const std::string& item_instance = std::string{}, std::uint32_t caster_id = 0,
                     std::int32_t mana_after = -1, std::uint8_t spell_level = 1, bool transformation_succeeded = false, bool scroll = false);
  void SendTransformationReturn(std::uint32_t cast_id);
  void ApplyNativeInstanceDefaults(const PlayerSpawnPacket& defaults);
  void SendInstanceReady(const PlayerSpawnPacket& defaults, bool success = true);
  std::uint32_t SendDropItem(const std::string& instance, std::int32_t amount, const glm::vec3& position,
                    const glm::vec3& rotation, bool physics_enabled);
  std::uint32_t SendTakeItem(const std::string& instance, std::int32_t amount, std::optional<std::uint32_t> item_ground_id);
  void SendPlayerWorldEnter(const std::string& world_name, const std::vector<InventoryEntry>& inventory = {});
  void SendActorAction(ActorActionPacket action);
  void SendEquipmentResult(std::int16_t slot, const std::string& requested, bool success, const std::string& observed,
                           std::uint32_t command_revision, std::uint32_t actor_id = 0, std::uint32_t control_epoch = 0, std::int16_t observed_slot = Net::EQUIP_SLOT_GENERIC);
  void SendPlayerHit(std::uint32_t victim_id, std::int32_t damage, std::uint32_t damage_type, bool dont_kill,
                     std::optional<std::uint32_t> attacker_id = {}, std::uint32_t control_epoch = 0, bool finishing_move = false);
  // Native casts can hit synchronously; their approval must precede the hit.
  void BeginHitBatch();
  void EndHitBatch(bool accepted);
  void SendPlayerUnconscious(std::optional<std::uint32_t> attacker_id);
  void SendPlayerStandUp();
  void SendPlayerDeath(std::optional<std::uint32_t> killer_id);
  void SendNpcActionResult(std::uint32_t npc_id, std::uint32_t epoch, std::uint32_t action_id, bool success);
  bool SendVoiceFrame(std::uint32_t talkspurt_id, std::uint32_t sequence,
                      const std::vector<std::uint8_t>& encoded_data);
  bool SendVoiceChannel(const std::string& channel);
  bool SendLuaEventToServer(const std::string& event_name, std::uint32_t source_element, const std::string& payload);
  void UpdatePlayerStats(const PlayerState& state);
  void SyncGameTime();

  // Network handling - MUST be called from main thread
  void HandleNetwork();

  PlayerManager& player_manager() {
    return player_manager_;
  }

  const std::string& GetServerName() const { return server_name_; }
  const std::string& GetServerIp() const {
    return server_ip_;
  }

  std::uint32_t GetServerPort() const {
    return server_port_;
  }

  std::uint32_t GetMaxSlots() const { return max_slots_; }

  std::vector<ResourcePayload> ConsumeDownloadedResources();
  std::vector<AddonPayload> ConsumeDownloadedAddons();

private:
  friend class GameClientTestPeer;

  struct Packet {
    unsigned char* data = nullptr;
    std::uint32_t length = 0;
  };

  void InitPacketHandlers();
  bool HandlePacket(unsigned char* data, std::uint32_t size);
  void OnTransportEvent(const Net::ConnectionEvent& event);

  void DispatchPendingConnectionNotifications();

  // Helper to update player state from PlayerState struct
  void UpdatePlayerState(Player* player, const PlayerState& state, std::optional<std::uint32_t> command_revision = {});
  void ApplyAttribute(Player& player, PlayerAttributeId id, std::int32_t value);
  void ApplyEquipmentSlot(Player& player, std::int16_t slot, const std::string& instance);

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
  void OnItemGroundCreate(Packet packet);
  void OnItemGroundDestroy(Packet packet);
  void OnItemsGroundDestroy(Packet packet);
  void OnGiveItem(Packet packet);
  void OnEquipItem(Packet packet);
  void OnUnequipItem(Packet packet);
  void OnRemoveItem(Packet packet);
  void OnActorAction(Packet packet);
  void OnActorDamage(Packet packet);
  void OnPlayerSummon(Packet packet);
  void OnPlayerTeleport(Packet packet);
  void OnPlayerTransformation(Packet packet);
  void DrainMobStateReplay();
  void ResetMobStateReplay() {
    world_objects_ready_ = false;
    pending_mob_states_.clear();
  }
  void OnItemTransactionResult(Packet packet);
  Player* FindActor(std::uint64_t id);
  Player* FindActor(std::uint64_t id, std::uint64_t generation);
  void OnMessage(Packet packet);
  void OnCharacterSpawn(Packet packet);
  void ApplyPlayerSpawn(const PlayerSpawnPacket& packet, bool is_npc = false, bool native_defaults = false);
  void OnNpcSpawn(Packet packet);
  void OnNpcControl(Packet packet);
  void ApplyNpcControl(const NpcControlPacket& control);
  void OnNpcAnimation(Packet packet);
  void ApplyNpcAnimation(const NpcAnimationPacket& animation);
  void OnJoinGame(Packet packet);
  void OnPlayerNameUpdate(Packet packet);
  void OnPlayerInstanceUpdate(Packet packet);
  void OnPlayerColorUpdate(Packet packet);
  void OnPlayerSkillWeaponUpdate(Packet packet);
  void OnPlayerTalentUpdate(Packet packet);
  void OnPlayerVisualUpdate(Packet packet);
  void OnPlayerFatnessUpdate(Packet packet);
  void OnPlayerScaleUpdate(Packet packet);
  void OnPlayerOverlayUpdate(Packet packet);
  void OnPlayerAnimationPlay(Packet packet);
  void OnPlayerAnimationStop(Packet packet);
  void OnPlayerFaceAnimationPlay(Packet packet);
  void OnPlayerFaceAnimationStop(Packet packet);
  void OnPlayerGesticulation(Packet packet);
  void OnPlayerAttributeUpdate(Packet packet);
  void OnPlayerAttributeSnapshot(Packet packet);
  void OnPlayerWorldUpdate(Packet packet);
  void OnPlayerUnconscious(Packet packet);
  void OnPlayerStandUp(Packet packet);
  void OnGameInfo(Packet packet);
  void OnSkySettings(Packet packet);
  void OnPlayerPresence(Packet packet);
  void OnAdminAuth(Packet packet);
  void OnVoiceConfiguration(Packet packet);
  void OnVoice(Packet packet);
  void OnLeftGame(Packet packet);
  void OnLuaEvent(Packet packet);

  Net::ClientTransport transport_;
  EventObserver& event_observer_;
  ResourceDownloader resource_downloader_;
  PlayerManager player_manager_;
  // Global UI metadata, independent of the actors streamed into the world.
  std::map<std::uint32_t, PlayerPresenceEntry> online_players_;

  using PacketHandlerFunc = std::function<void(Packet)>;
  std::map<int, PacketHandlerFunc> packet_handlers_;
  std::vector<World> worlds_;

  std::string server_name_;
  std::string server_ip_;
  std::uint32_t server_port_{0};
  std::uint32_t max_slots_{0};
  std::uint32_t outgoing_state_sequence_{0};
  std::uint32_t outgoing_action_sequence_{0};
  std::uint32_t outgoing_item_request_{0};
  std::uint32_t last_item_result_{0};
  std::uint16_t pending_local_correction_mask_{0};
  struct PendingEquipmentVerification {
    bool equip;
    std::string instance;
    std::int16_t slot;
    std::uint32_t revision;
  };
  std::vector<PendingEquipmentVerification> pending_equipment_verifications_;
  struct PendingInventoryCommand {
    bool give;
    std::string instance;
    std::int32_t amount;
  };
  std::vector<PendingInventoryCommand> pending_inventory_commands_;
  // World-object state is scoped to the receiving hero's world incarnation,
  // independently of the actor who originally operated the object.
  bool world_objects_ready_{false};
  std::vector<ActorActionPacket> pending_mob_states_;
  std::vector<std::vector<PlayerHitReportPacket>> pending_hit_batches_;
  void SendCapturedHit(PlayerHitReportPacket packet);
  bool admin_authenticated_{false};
  bool connection_lost_{false};
  bool is_in_game_{false};
  bool notify_connected_pending_{false};
  bool notify_connection_failed_pending_{false};
  bool notify_connection_lost_pending_{false};
  bool notify_connection_closed_pending_{false};

  ConnectionState connection_state_{ConnectionState::Disconnected};
  Net::CloseReason connection_close_reason_{Net::CloseReason::None};
  mutable std::mutex connection_mutex_;
  std::string connection_error_;
};

}  // namespace gmp::client
