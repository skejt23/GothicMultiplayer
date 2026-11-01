
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

#include <cstdint>
#include <glm/glm.hpp>
#include <optional>
#include <string>

#include "item_ground_manager.h"

inline const std::string kEventOnTickName = "onTick";
inline const std::string kEventOnClockUpdateName = "onClockUpdate";
inline const std::string kEventOnPlayerConnectName = "onPlayerConnect";
inline const std::string kEventOnPlayerDisconnectName = "onPlayerDisconnect";
inline const std::string kEventOnPlayerMessageName = "onPlayerMessage";
inline const std::string kEventOnPlayerCommandName = "onPlayerCommand";
inline const std::string kEventOnPlayerUnconsciousName = "onPlayerUnconscious";
inline const std::string kEventOnPlayerStandUpName = "onPlayerStandUp";
inline const std::string kEventOnPlayerDeathName = "onPlayerDeath";
inline const std::string kEventOnPlayerDropItemName = "onPlayerDropItem";
inline const std::string kEventOnPlayerTakeItemName = "onPlayerTakeItem";
inline const std::string kEventOnPlayerUseItemName = "onPlayerUseItem";
inline const std::string kEventOnPlayerCastSpellName = "onPlayerCastSpell";
inline const std::string kEventOnPlayerSummonName = "onPlayerSummon";
inline const std::string kEventOnPlayerTeleportName = "onPlayerTeleport";
inline const std::string kEventOnPlayerChangeHealthName = "onPlayerChangeHealth";
inline const std::string kEventOnPlayerChangeManaName = "onPlayerChangeMana";
inline const std::string kEventOnPlayerWorldChangeName = "onPlayerWorldChange";
inline const std::string kEventOnPlayerWorldEnterName = "onPlayerWorldEnter";
inline const std::string kEventOnPlayerWeaponModeChangeName = "onPlayerWeaponModeChange";
inline const std::string kEventOnPlayerEquipAmuletName = "onPlayerEquipAmulet";
inline const std::string kEventOnPlayerEquipArmorName = "onPlayerEquipArmor";
inline const std::string kEventOnPlayerEquipBeltName = "onPlayerEquipBelt";
inline const std::string kEventOnPlayerEquipHandItemName = "onPlayerEquipHandItem";
inline const std::string kEventOnPlayerEquipHelmetName = "onPlayerEquipHelmet";
inline const std::string kEventOnPlayerEquipMeleeWeaponName = "onPlayerEquipMeleeWeapon";
inline const std::string kEventOnPlayerEquipRangedWeaponName = "onPlayerEquipRangedWeapon";
inline const std::string kEventOnPlayerEquipRingName = "onPlayerEquipRing";
inline const std::string kEventOnPlayerEquipShieldName = "onPlayerEquipShield";
inline const std::string kEventOnPlayerEquipSpellSlotName = "onPlayerEquipSpellSlot";
inline const std::string kEventOnPlayerSpawnName = "onPlayerSpawn";
inline const std::string kEventOnPlayerRespawnName = "onPlayerRespawn";
inline const std::string kEventOnPlayerSpawnForName = "onPlayerSpawnFor";
inline const std::string kEventOnPlayerUnspawnForName = "onPlayerUnspawnFor";
inline const std::string kEventOnPlayerHitName = "onPlayerHit";
inline const std::string kEventOnPlayerVoiceStartName = "onPlayerVoiceStart";
inline const std::string kEventOnPlayerVoiceStopName = "onPlayerVoiceStop";
inline const std::string kEventOnPlayerVoiceChannelChangeName = "onPlayerVoiceChannelChange";
inline const std::string kEventOnNpcCreatedName = "onNpcCreated";
inline const std::string kEventOnNpcDestroyedName = "onNpcDestroyed";
inline const std::string kEventOnNpcChangeHostPlayerName = "onNpcChangeHostPlayer";
inline const std::string kEventOnNpcActionSentName = "onNpcActionSent";
inline const std::string kEventOnNpcActionFinishedName = "onNpcActionFinished";

struct OnNpcCreatedEvent {
  std::uint32_t npc_id;
};

struct OnNpcDestroyedEvent {
  std::uint32_t npc_id;
};

struct OnNpcChangeHostPlayerEvent {
  std::uint32_t npc_id;
  std::uint32_t current_id;  // Zero means no host; Lua receives -1.
  std::uint32_t previous_id;
};

struct OnNpcActionSentEvent {
  std::uint32_t npc_id;
  std::uint32_t action_type;
  std::uint32_t action_id;
};

struct OnNpcActionFinishedEvent {
  std::uint32_t npc_id;
  std::uint32_t action_type;
  std::uint32_t action_id;
  bool result;
};

struct OnTickEvent {};

struct OnClockUpdateEvent {
  std::uint16_t day;
  std::uint8_t hour;
  std::uint8_t min;
};

struct OnPlayerDisconnectEvent {
  std::uint64_t player_id;
  std::int32_t reason;
};

struct OnPlayerMessageEvent {
  std::uint64_t pid;
  std::string text;
};

struct OnPlayerCommandEvent {
  std::uint64_t pid;
  std::string command;
  std::string params;
};

struct OnPlayerUnconsciousEvent {
  std::optional<std::uint64_t> attacker_id;
  std::uint64_t victim_id;
};

struct OnPlayerStandUpEvent {
  std::uint64_t player_id;
};

struct OnPlayerDeathEvent {
  std::uint64_t player_id;
  std::optional<std::uint64_t> killer_id;
};

struct OnPlayerDropItemEvent {
  std::uint64_t pid;
  ItemGroundManager::ItemGroundId item_ground_id;
};

struct OnPlayerTakeItemEvent {
  std::uint64_t pid;
  ItemGroundManager::ItemGroundId item_ground_id;
};

struct OnPlayerUseItemEvent {
  std::uint64_t player_id;
  std::string item_instance;
  std::int32_t quantity;
  std::string scheme;
};

struct OnPlayerCastSpellEvent {
  std::uint64_t caster_id;
  std::uint16_t spell_id;
  std::optional<std::uint64_t> target_id;
};

struct OnPlayerSummonEvent {
  std::uint32_t player_id;
  std::uint16_t spell_id;
  std::string summon_instance;
  std::uint32_t npc_id;
};

struct OnPlayerTeleportEvent {
  std::uint32_t player_id;
  std::string destination;
};

struct OnPlayerChangeHealthEvent {
  std::uint64_t player_id;
  std::int32_t old_hp;
  std::int32_t new_hp;
};

struct OnPlayerChangeManaEvent {
  std::uint64_t player_id;
  std::int32_t previous;
  std::int32_t current;
};

struct OnPlayerChangeWorldEvent {
  std::uint64_t player_id;
  std::string world;
  std::string waypoint;
};

struct OnPlayerWorldEnterEvent {
  std::uint64_t player_id;
  std::string world;
};

struct OnPlayerWeaponModeChangeEvent {
  std::uint64_t player_id;
  std::uint8_t old_mode;
  std::uint8_t new_mode;
};

struct OnPlayerEquipAmuletEvent {
  std::uint64_t player_id;
  std::optional<std::string> item_instance;
  std::optional<std::string> previous_item_instance;
};

struct OnPlayerEquipArmorEvent {
  std::uint64_t player_id;
  std::optional<std::string> item_instance;
  std::optional<std::string> previous_item_instance;
};

struct OnPlayerEquipBeltEvent {
  std::uint64_t player_id;
  std::optional<std::string> item_instance;
  std::optional<std::string> previous_item_instance;
};

struct OnPlayerEquipHandItemEvent {
  std::uint64_t player_id;
  std::uint8_t hand;
  std::optional<std::string> item_instance;
  std::optional<std::string> previous_item_instance;
};

struct OnPlayerEquipHelmetEvent {
  std::uint64_t player_id;
  std::optional<std::string> item_instance;
  std::optional<std::string> previous_item_instance;
};

struct OnPlayerEquipMeleeWeaponEvent {
  std::uint64_t player_id;
  std::optional<std::string> item_instance;
  std::optional<std::string> previous_item_instance;
};

struct OnPlayerEquipRangedWeaponEvent {
  std::uint64_t player_id;
  std::optional<std::string> item_instance;
  std::optional<std::string> previous_item_instance;
};

struct OnPlayerEquipRingEvent {
  std::uint64_t player_id;
  std::uint8_t ring_slot;
  std::optional<std::string> item_instance;
  std::optional<std::string> previous_item_instance;
};

struct OnPlayerEquipShieldEvent {
  std::uint64_t player_id;
  std::optional<std::string> item_instance;
  std::optional<std::string> previous_item_instance;
};

struct OnPlayerEquipSpellSlotEvent {
  std::uint64_t player_id;
  std::uint8_t slot_id;
  std::optional<std::string> item_instance;
  std::optional<std::string> previous_item_instance;
};

struct OnPlayerSpawnEvent {
  std::uint64_t player_id;
  glm::vec3 position;
};

struct OnPlayerRespawnEvent {
  std::uint64_t player_id;
  glm::vec3 position;
};

struct OnPlayerSpawnForEvent {
  std::uint64_t player_id;
  std::uint64_t spawn_id;
};

struct OnPlayerUnspawnForEvent {
  std::uint64_t player_id;
  std::uint64_t spawn_id;
};

struct OnPlayerHitEvent {
  std::optional<std::uint64_t> attacker_id;
  std::uint64_t victim_id;
  std::int32_t damage;
  std::uint32_t damage_type;
};

struct OnPlayerVoiceChannelChangeEvent {
  std::uint64_t player_id;
  std::string old_channel;
  std::string new_channel;
};

struct OnPlayerVoiceEvent {
  std::uint64_t player_id;
};
