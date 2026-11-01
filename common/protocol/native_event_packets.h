#pragma once

#include "packets.h"

// One-time creation notification, sent after the summoned NPC's spawn snapshot.
// It is not replayed when an existing summon streams in for a new viewer.
struct PlayerSummonPacket {
  std::uint8_t packet_type{Net::PT_PLAYER_SUMMON};
  std::uint32_t caster_id{0};
  std::uint64_t caster_generation{0};
  std::uint16_t spell_id{0};
  std::uint32_t npc_id{0};
  std::uint64_t npc_generation{0};
  std::string summon_instance;
};

template <typename S>
void serialize(S& s, PlayerSummonPacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.caster_id);
  s.value8b(packet.caster_generation);
  s.value2b(packet.spell_id);
  s.value4b(packet.npc_id);
  s.value8b(packet.npc_generation);
  s.text1b(packet.summon_instance, 128);
}

// Native completion report from the controlling client; the server fills its
// state sequence/revision/time and broadcasts the accepted relocation once.
struct PlayerTeleportPacket {
  std::uint8_t packet_type{Net::PT_PLAYER_TELEPORT};
  std::uint32_t player_id{0};
  std::uint64_t actor_generation{0};
  std::uint32_t control_epoch{0};
  std::uint32_t command_revision{0};
  std::uint32_t action_id{0};
  std::uint32_t state_sequence{0};
  std::uint64_t source_time_ms{0};
  glm::vec3 position{0.0f};
  glm::vec3 normal{0.0f, 0.0f, 1.0f};
  std::string destination;
};

template <typename S>
void serialize(S& s, PlayerTeleportPacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.player_id);
  s.value8b(packet.actor_generation);
  s.value4b(packet.control_epoch);
  s.value4b(packet.command_revision);
  s.value4b(packet.action_id);
  s.value4b(packet.state_sequence);
  s.value8b(packet.source_time_ms);
  s.object(packet.position);
  s.object(packet.normal);
  s.text1b(packet.destination, 255);
}

// A client sends only actor identity and cast_id to report native return. All
// instance, stats and state fields in the server response are server-owned.
struct PlayerTransformationPacket {
  std::uint8_t packet_type{Net::PT_PLAYER_TRANSFORMATION};
  std::uint32_t player_id{0};
  std::uint64_t actor_generation{0};
  std::uint32_t command_revision{0};
  std::uint32_t state_sequence{0};
  std::uint32_t cast_id{0};
  std::uint16_t spell_id{0};
  // Empty on return means the player's original default instance.
  std::string instance;
  std::int32_t max_health{0};
  std::int32_t max_mana{0};
  std::int32_t strength{0};
  std::int32_t dexterity{0};
  PlayerState state;
};

template <typename S>
void serialize(S& s, PlayerTransformationPacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.player_id);
  s.value8b(packet.actor_generation);
  s.value4b(packet.command_revision);
  s.value4b(packet.state_sequence);
  s.value4b(packet.cast_id);
  s.value2b(packet.spell_id);
  s.text1b(packet.instance, 255);
  s.value4b(packet.max_health);
  s.value4b(packet.max_mana);
  s.value4b(packet.strength);
  s.value4b(packet.dexterity);
  s.object(packet.state);
}
