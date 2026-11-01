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
#include <optional>
#include <string>
#include <vector>

#include "common_structs.h"
#include "native_event_packets.h"
#include "packets.h"

struct NpcControlPacket;
struct NpcAnimationPacket;
struct ActorActionPacket;
struct ActorDamagePacket;
struct InventoryEntry;

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
  virtual void OnConnectionClosed(const std::string& reason) {}
  virtual void OnAdminAuthChanged(bool authenticated) {}
  virtual void OnResourceDownloadPhase(const std::string&) {}
  virtual void OnResourceDownloadProgress(const std::string& resource_name, std::uint64_t downloaded_bytes, std::uint64_t total_bytes) {}
  virtual void OnResourceDownloadFailed(const std::string& reason) {}
  virtual void OnResourcesReady() {}
  
  // World/map events
  virtual void OnMapChange(const std::string& map_name) {}
  virtual void OnGameInfoReceived(std::uint32_t raw_game_time, float day_length_ms, std::uint8_t flags) {}
  virtual void OnSkySettingsReceived(std::uint8_t flags, std::int32_t weather_type,
                                     std::int16_t rain_start_hour, std::int16_t rain_start_min,
                                     std::int16_t rain_stop_hour, std::int16_t rain_stop_min,
                                     float wind_scale, bool dont_rain, float rain_weight, bool render_lightning) {}

  // Native voice chat events
  virtual void OnVoiceConfiguration(bool enabled, bool transmit_enabled, std::uint32_t range) {}
  virtual void OnVoiceFrame(std::uint32_t player_id, std::uint32_t talkspurt_id, std::uint32_t sequence,
                            bool spatial, std::uint32_t range, const std::vector<std::uint8_t>& encoded_data) {}

  // Player events
  // Release engine-side references before the Player objects are destroyed.
  virtual void OnPlayersClearing() {}
  virtual void OnLocalPlayerJoined(gmp::client::Player& player) {}
  virtual void OnLocalPlayerSpawned(gmp::client::Player& player) {}
  virtual void OnPlayerJoined(gmp::client::Player& player) {}
  virtual void OnPlayerSpawned(gmp::client::Player& player) {}
  // Called after the engine actor and every attribute/visual in a spawn snapshot exist.
  virtual void OnPlayerSpawnSnapshotApplied(gmp::client::Player& player, bool new_spawn) {}
  virtual void OnNpcControl(const NpcControlPacket& control) {}
  virtual void OnNpcAnimation(const NpcAnimationPacket& animation) {}
  virtual void OnPlayerLeft(std::uint64_t player_id, const std::string& player_name) {}
  virtual void OnPlayerStateUpdate(std::uint64_t player_id, const PlayerState& state) {}
  virtual void OnPlayerStateUpdate(std::uint64_t player_id, const PlayerState& state, bool authoritative_correction) {
    OnPlayerStateUpdate(player_id, state);
  }
  virtual void OnPlayerPositionUpdate(std::uint64_t player_id, float x, float y, float z) {}
  virtual void OnPlayerNameUpdate(std::uint64_t player_id, const std::string& name) {}
  virtual void OnPlayerInstanceUpdate(std::uint64_t player_id, const std::string& instance) {}
  virtual void OnPlayerColorUpdate(std::uint64_t player_id, std::uint8_t r, std::uint8_t g, std::uint8_t b) {}
  virtual void OnPlayerSkillWeaponUpdate(std::uint64_t player_id, std::int32_t skill_id, std::int32_t percentage) {}
  virtual void OnPlayerTalentUpdate(std::uint64_t player_id, std::int32_t talent_id, std::int32_t talent_value) {}
  virtual void OnPlayerVisualUpdate(std::uint64_t player_id, const std::string& body_model, std::int16_t body_texture,
                                    const std::string& head_model, std::int16_t head_texture,
                                    std::int16_t teeth_texture, std::int16_t skin_color) {}
  virtual void OnPlayerFatnessUpdate(std::uint64_t player_id, float fatness) {}
  virtual void OnPlayerScaleUpdate(std::uint64_t player_id, const glm::vec3& scale) {}
  virtual void OnPlayerOverlayUpdate(std::uint64_t player_id, const std::string& overlay, bool apply) {}
  virtual void OnPlayerAnimationPlay(std::uint64_t player_id, const std::string& animation) {}
  virtual void OnPlayerAnimationStop(std::uint64_t player_id, const std::string& animation) {}
  virtual void OnPlayerFaceAnimationPlay(std::uint64_t player_id, const std::string& animation) {}
  virtual void OnPlayerFaceAnimationStop(std::uint64_t player_id, const std::string& animation) {}
  virtual void OnPlayerGesticulation(std::uint64_t player_id) {}
  virtual void OnPlayerAttributeUpdate(std::uint64_t player_id, PlayerAttributeId attribute_id, std::int32_t value) {}
  virtual void OnPlayerWorldUpdate(std::uint64_t player_id, const std::string& world_name, const std::string& start_point) {}
  virtual void OnPlayerDied(std::uint64_t player_id) {}
  virtual void OnPlayerDied(std::uint64_t player_id, bool apply_state) { if (apply_state) OnPlayerDied(player_id); }
  virtual void OnPlayerRespawned(std::uint64_t player_id) {}
  virtual void OnPlayerRespawned(std::uint64_t player_id, bool apply_state) { if (apply_state) OnPlayerRespawned(player_id); }
  virtual void OnPlayerUnconscious(std::uint64_t player_id, std::optional<std::uint64_t> attacker_id) {}
  virtual void OnPlayerUnconscious(std::uint64_t player_id, std::optional<std::uint64_t> attacker_id, bool apply_state) {
    if (apply_state) OnPlayerUnconscious(player_id, attacker_id);
  }
  virtual void OnPlayerStandUp(std::uint64_t player_id) {}
  virtual void OnPlayerStandUp(std::uint64_t player_id, bool apply_state) { if (apply_state) OnPlayerStandUp(player_id); }
  virtual void OnPlayerPingUpdate(std::uint64_t player_id, std::int32_t ping) {}
  
  // Item events
  virtual void OnItemDropped(std::uint64_t player_id, const std::string& item_instance, std::int32_t amount) {}
  virtual void OnItemTaken(std::uint64_t player_id, const std::string& item_instance) {}
  virtual void OnItemGroundCreate(std::uint32_t item_ground_id, const std::string& item_instance, std::int32_t amount,
                                  bool physics_enabled, const glm::vec3& position, const glm::vec3& rotation) {}
  virtual void OnItemGroundDestroy(std::uint32_t item_ground_id) {}
  virtual void OnItemsGroundDestroy() {}
  virtual void OnItemGiven(std::uint64_t player_id, const std::string& item_instance, std::int32_t amount) {}
  virtual void OnItemEquipped(std::uint64_t player_id, const std::string& item_instance, std::int16_t slot_id) {}
  virtual void OnItemEquipped(std::uint64_t player_id, const std::string& item_instance, std::int16_t slot_id,
                              std::uint32_t revision, bool apply_state) {
    if (apply_state) OnItemEquipped(player_id, item_instance, slot_id);
  }
  virtual void OnItemUnequipped(std::uint64_t player_id, const std::string& item_instance) {}
  virtual void OnItemUnequipped(std::uint64_t player_id, const std::string& item_instance, std::int16_t slot_id) {
    OnItemUnequipped(player_id, item_instance);
  }
  virtual void OnItemUnequipped(std::uint64_t player_id, const std::string& item_instance, std::int16_t slot_id,
                                std::uint32_t revision, bool apply_state) {
    if (apply_state) OnItemUnequipped(player_id, item_instance, slot_id);
  }
  virtual void OnItemRemoved(std::uint64_t player_id, const std::string& item_instance, std::int32_t amount) {}
  virtual void OnActorInventorySnapshot(std::uint64_t actor_id, const std::vector<InventoryEntry>& inventory) {}
  virtual void OnItemTransactionResult(std::uint32_t request_id, const std::string& instance, std::int32_t resulting_amount,
                                       bool accepted, std::optional<std::uint32_t> ground_id) {}
  virtual void OnActorItemTransactionResult(std::uint32_t actor_id, std::uint32_t request_id, const std::string& instance,
                                            std::int32_t resulting_amount, bool accepted,
                                            std::optional<std::uint32_t> ground_id) {
    OnItemTransactionResult(request_id, instance, resulting_amount, accepted, ground_id);
  }
  virtual void OnActorAction(const ActorActionPacket& action) {}
  virtual void OnActorDamage(const ActorDamagePacket& damage) {
  }
  virtual void OnPlayerSummon(const PlayerSummonPacket& summon) {}
  virtual void OnPlayerTeleport(const PlayerTeleportPacket& teleport, std::uint16_t correction_mask) {}
  virtual void OnPlayerTransformation(const PlayerTransformationPacket& transformation, std::uint16_t correction_mask) {}

  // Spell/combat events
  virtual void OnSpellCast(const CastSpellPacket& packet) {
    if (!packet.caster_id)
      return;
    if (packet.target_id) {
      OnSpellCastOnTarget(*packet.caster_id, *packet.target_id, packet.spell_id, packet.source_time_ms, packet.actor_generation,
                          packet.target_generation, packet.control_epoch);
    } else {
      OnSpellCast(*packet.caster_id, packet.spell_id, packet.source_time_ms, packet.actor_generation, packet.control_epoch);
    }
  }
  virtual void OnSpellCast(std::uint64_t caster_id, std::uint16_t spell_id) {}
  virtual void OnSpellCast(std::uint64_t caster_id, std::uint16_t spell_id, std::uint64_t source_time_ms,
                           std::uint64_t actor_generation, std::uint32_t control_epoch) {
    OnSpellCast(caster_id, spell_id);
  }
  virtual void OnSpellCastOnTarget(std::uint64_t caster_id, std::uint64_t target_id, std::uint16_t spell_id) {}
  virtual void OnSpellCastOnTarget(std::uint64_t caster_id, std::uint64_t target_id, std::uint16_t spell_id,
                                   std::uint64_t source_time_ms, std::uint64_t actor_generation,
                                   std::uint64_t target_generation, std::uint32_t control_epoch) {
    OnSpellCastOnTarget(caster_id, target_id, spell_id);
  }
  
  // Chat/messaging events
  virtual void OnPlayerMessage(std::optional<std::uint64_t> sender_id, std::uint8_t r, std::uint8_t g, std::uint8_t b,
                               const std::string& message) {}

  // Lua event relay
  virtual void OnLuaEvent(const std::string& event_name, std::uint32_t source_element, const std::string& payload) {}
};

}  // namespace gmp::client
