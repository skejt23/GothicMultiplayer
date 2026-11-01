
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

#include <bitsery/adapter/buffer.h>
#include <bitsery/bitsery.h>
#include <bitsery/ext/std_optional.h>
#include <bitsery/traits/string.h>
#include <bitsery/traits/vector.h>
#include <fmt/ostream.h>

#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <optional>
#include <string>

#include "common_structs.h"
#include "net_enums.h"

struct InventoryEntry {
  std::string item_instance;
  std::int32_t amount{0};
};

enum class ActorActionKind : std::uint8_t { AnimationStart, AnimationStop, Shot, ItemUse, MobState };

// Server-accepted damage presentation is independent of the attacker's native
// collision callbacks and the victim's client-generated animation sequence.
struct ActorDamagePacket {
  std::uint8_t packet_type{Net::PT_ACTOR_DAMAGE};
  std::uint32_t victim_id{0};
  std::uint64_t victim_generation{0};
  std::uint32_t state_sequence{0};
  std::uint32_t command_revision{0};
  std::uint64_t source_time_ms{0};
  std::optional<std::uint32_t> attacker_id;
  std::uint64_t attacker_generation{0};
  std::int32_t damage{0};
  std::uint32_t damage_type{0};
};

template <typename S>
void serialize(S& s, ActorDamagePacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.victim_id);
  s.value8b(packet.victim_generation);
  s.value4b(packet.state_sequence);
  s.value4b(packet.command_revision);
  s.value8b(packet.source_time_ms);
  s.ext4b(packet.attacker_id, bitsery::ext::StdOptional{});
  s.value8b(packet.attacker_generation);
  s.value4b(packet.damage);
  s.value4b(packet.damage_type);
}

struct ActorActionPacket {
  std::uint32_t command_revision{0};
  std::uint32_t control_epoch{0};
  std::uint8_t packet_type{Net::PT_ACTOR_ACTION};
  std::uint64_t actor_generation{0};
  std::uint64_t target_generation{0};
  std::uint64_t source_time_ms{0};
  std::uint32_t action_id{0};
  std::uint32_t actor_id{0};
  std::optional<std::uint32_t> target_id;
  ActorActionKind kind{ActorActionKind::AnimationStart};
  std::string name;
  std::string scheme;
  glm::vec3 position{0.0f};
  glm::vec3 direction{0.0f};
  std::uint8_t weapon_mode{0};
  std::string item_instance;
  std::int32_t quantity{0};
  std::int32_t health_delta{0};
  std::int32_t mana_delta{0};
  std::int32_t target_state{0};
};

struct ItemTransactionResultPacket {
  std::uint32_t actor_id{0};
  std::uint8_t packet_type{Net::PT_ITEM_TRANSACTION_RESULT};
  std::uint64_t actor_generation{0};
  std::uint32_t state_sequence{0};
  std::uint32_t request_id{0};
  std::string item_instance;
  std::int32_t resulting_amount{0};
  bool accepted{false};
  std::optional<std::uint32_t> item_ground_id;
};

struct EquipmentResultPacket {
  std::uint32_t actor_id{0};
  std::uint32_t control_epoch{0};
  std::uint8_t packet_type{Net::PT_EQUIPMENT_RESULT};
  std::uint64_t actor_generation{0};
  std::uint32_t command_revision{0};
  std::int16_t slot_id{Net::EQUIP_SLOT_GENERIC};
  // The native controller identifies the slot for a generic equip command.
  std::int16_t observed_slot{Net::EQUIP_SLOT_GENERIC};
  std::string item_instance;
  std::string observed_instance;
  bool success{false};
};

template <typename S>
void serialize(S& s, ActorActionPacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.command_revision);
  s.value4b(packet.control_epoch);
  s.value8b(packet.actor_generation);
  s.value8b(packet.target_generation);
  s.value8b(packet.source_time_ms);
  s.value4b(packet.action_id);
  s.value4b(packet.actor_id);
  s.ext4b(packet.target_id, bitsery::ext::StdOptional{});
  s.value1b(packet.kind);
  s.text1b(packet.name, 128);
  s.text1b(packet.scheme, 128);
  s.object(packet.position);
  s.object(packet.direction);
  s.value1b(packet.weapon_mode);
  s.text1b(packet.item_instance, gmp::kMaxScriptNameLength);
  s.value4b(packet.quantity);
  s.value4b(packet.health_delta);
  s.value4b(packet.mana_delta);
  s.value4b(packet.target_state);
}

template <typename S>
void serialize(S& s, ItemTransactionResultPacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.actor_id);
  s.value8b(packet.actor_generation);
  s.value4b(packet.state_sequence);
  s.value4b(packet.request_id);
  s.text1b(packet.item_instance, gmp::kMaxScriptNameLength);
  s.value4b(packet.resulting_amount);
  s.boolValue(packet.accepted);
  s.ext4b(packet.item_ground_id, bitsery::ext::StdOptional{});
}

template <typename S>
void serialize(S& s, EquipmentResultPacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.actor_id);
  s.value4b(packet.control_epoch);
  s.value8b(packet.actor_generation);
  s.value4b(packet.command_revision);
  s.value2b(packet.slot_id);
  s.text1b(packet.item_instance, gmp::kMaxScriptNameLength);
  s.text1b(packet.observed_instance, gmp::kMaxScriptNameLength);
  s.value2b(packet.observed_slot);
  s.boolValue(packet.success);
}
template <typename S>
void serialize(S& s, InventoryEntry& entry) {
  s.text1b(entry.item_instance, gmp::kMaxScriptNameLength);
  s.value4b(entry.amount);
}

namespace glm {
template <typename S>
void serialize(S& s, glm::vec3& vec) {
  s.value4b(vec.x);
  s.value4b(vec.y);
  s.value4b(vec.z);
}
}  // namespace glm

struct ExistingPlayerInfo {
  std::uint8_t packet_type{0};
  std::uint32_t player_id{0};
  std::uint32_t state_sequence{0};
  glm::vec3 position{0.0f};
  std::string left_hand_item_instance;
  std::string right_hand_item_instance;
  std::string equipped_armor_instance;
  std::string equipped_helmet_instance;
  std::string equipped_shield_instance;
  std::string equipped_amulet_instance;
  std::string equipped_belt_instance;
  std::string equipped_ring_left_instance;
  std::string equipped_ring_right_instance;
  std::string animation_name;
  std::string body_model;
  std::uint16_t body_texture{0};
  std::string head_model;
  std::uint16_t head_texture{0};
  std::uint16_t teeth_texture{0};
  std::uint16_t skin_color{0};
  std::uint8_t walk_style{0};
  std::string player_name;
  std::string instance;
  std::uint8_t name_color_r{255};
  std::uint8_t name_color_g{255};
  std::uint8_t name_color_b{255};

  std::int32_t strength{0};
  std::int32_t dexterity{0};
  std::int32_t level{0};
  std::int32_t exp{0};
  std::int32_t next_level_exp{0};
  std::int32_t learn_points{0};
  std::int32_t health{0};
  std::int32_t max_health{0};
  std::int32_t mana{0};
  std::int32_t max_mana{0};
  std::uint8_t life_state{PLAYER_LIFE_ALIVE};

  float fatness{1.0f};
  glm::vec3 scale{1.0f, 1.0f, 1.0f};

  struct SkillEntry {
    std::int32_t skill_id{0};
    std::int32_t percentage{0};
  };
  std::vector<SkillEntry> weapon_skills;

  struct TalentEntry {
    std::int32_t talent_id{0};
    std::int32_t value{0};
  };
  std::vector<TalentEntry> talents;

  std::vector<std::string> overlays;
};

template <typename S>
void serialize(S& s, ExistingPlayerInfo::SkillEntry& entry) {
  s.value4b(entry.skill_id);
  s.value4b(entry.percentage);
}

template <typename S>
void serialize(S& s, ExistingPlayerInfo::TalentEntry& entry) {
  s.value4b(entry.talent_id);
  s.value4b(entry.value);
}

template <typename S>
void serialize(S& s, ExistingPlayerInfo& info) {
  s.value4b(info.player_id);
  s.value4b(info.state_sequence);
  s.object(info.position);
  s.text1b(info.left_hand_item_instance, gmp::kMaxScriptNameLength);
  s.text1b(info.right_hand_item_instance, gmp::kMaxScriptNameLength);
  s.text1b(info.equipped_armor_instance, gmp::kMaxScriptNameLength);
  s.text1b(info.equipped_helmet_instance, gmp::kMaxScriptNameLength);
  s.text1b(info.equipped_shield_instance, gmp::kMaxScriptNameLength);
  s.text1b(info.equipped_amulet_instance, gmp::kMaxScriptNameLength);
  s.text1b(info.equipped_belt_instance, gmp::kMaxScriptNameLength);
  s.text1b(info.equipped_ring_left_instance, gmp::kMaxScriptNameLength);
  s.text1b(info.equipped_ring_right_instance, gmp::kMaxScriptNameLength);
  s.text1b(info.animation_name, kMaxPlayerAnimationNameLength);
  s.text1b(info.body_model, 255);
  s.value2b(info.body_texture);
  s.text1b(info.head_model, 255);
  s.value2b(info.head_texture);
  s.value2b(info.teeth_texture);
  s.value2b(info.skin_color);
  s.value1b(info.walk_style);
  s.text1b(info.player_name, 255);

  s.text1b(info.instance, 255);
  s.value1b(info.name_color_r);
  s.value1b(info.name_color_g);
  s.value1b(info.name_color_b);

  s.value4b(info.strength);
  s.value4b(info.dexterity);
  s.value4b(info.level);
  s.value4b(info.exp);
  s.value4b(info.next_level_exp);
  s.value4b(info.learn_points);
  s.value4b(info.health);
  s.value4b(info.max_health);
  s.value4b(info.mana);
  s.value4b(info.max_mana);
  s.value1b(info.life_state);

  s.value4b(info.fatness);
  s.object(info.scale);

  s.container(info.weapon_skills, 128);
  s.container(info.talents, 128);
  s.container(info.overlays, 64, [](S& s, std::string& overlay) { s.text1b(overlay, 255); });
}

inline std::ostream& operator<<(std::ostream& os, const ExistingPlayerInfo& packet) {
  os << "ExistingPlayerInfo {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", player_id: " << packet.player_id
     << ", state_sequence: " << packet.state_sequence << ", position: (" << packet.position.x
     << ", " << packet.position.y << ", " << packet.position.z << ")"
     << ", left_hand_item_instance: " << packet.left_hand_item_instance << ", right_hand_item_instance: " << packet.right_hand_item_instance
     << ", equipped_armor_instance: " << packet.equipped_armor_instance
     << ", equipped_helmet_instance: " << packet.equipped_helmet_instance
     << ", equipped_shield_instance: " << packet.equipped_shield_instance
     << ", equipped_amulet_instance: " << packet.equipped_amulet_instance
     << ", equipped_belt_instance: " << packet.equipped_belt_instance
     << ", equipped_ring_left_instance: " << packet.equipped_ring_left_instance
     << ", equipped_ring_right_instance: " << packet.equipped_ring_right_instance
     << ", animation_name: " << packet.animation_name
     << ", body_model: " << packet.body_model << ", body_texture: " << static_cast<int>(packet.body_texture)
     << ", head_model: " << packet.head_model << ", head_texture: " << static_cast<int>(packet.head_texture)
     << ", teeth_texture: " << static_cast<int>(packet.teeth_texture) << ", skin_color: " << static_cast<int>(packet.skin_color)
     << ", walk_style: " << static_cast<int>(packet.walk_style) << ", player_name: " << packet.player_name
     << ", instance: " << packet.instance
     << ", name_color: (" << static_cast<int>(packet.name_color_r) << ", " << static_cast<int>(packet.name_color_g) << ", "
     << static_cast<int>(packet.name_color_b) << ")"
     << ", strength: " << packet.strength << ", dexterity: " << packet.dexterity << ", level: " << packet.level
     << ", exp: " << packet.exp << ", next_level_exp: " << packet.next_level_exp << ", learn_points: " << packet.learn_points
     << ", health: " << packet.health << ", max_health: " << packet.max_health << ", mana: " << packet.mana
     << ", max_mana: " << packet.max_mana << ", life_state: " << static_cast<int>(packet.life_state)
     << ", fatness: " << packet.fatness
     << ", scale: (" << packet.scale.x << ", " << packet.scale.y << ", " << packet.scale.z << ")"
     << ", weapon_skills: " << packet.weapon_skills.size() << ", talents: " << packet.talents.size() << ", overlays: "
     << packet.overlays.size() << " }";
  return os;
}

template <>
struct fmt::formatter<ExistingPlayerInfo> : ostream_formatter {};

struct PlayerNameUpdatePacket {
  std::uint8_t packet_type{0};
  std::uint32_t player_id{0};
  std::string name;
};

template <typename S>
void serialize(S& s, PlayerNameUpdatePacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.player_id);
  s.text1b(packet.name, 255);
}

inline std::ostream& operator<<(std::ostream& os, const PlayerNameUpdatePacket& packet) {
  os << "PlayerNameUpdatePacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", player_id: " << packet.player_id << ", name: " << packet.name << " }";
  return os;
}

template <>
struct fmt::formatter<PlayerNameUpdatePacket> : ostream_formatter {};

struct PlayerInstanceUpdatePacket {
  bool initialize_defaults{false};
  std::uint64_t actor_generation{0};
  std::uint32_t command_revision{0};
  std::uint8_t packet_type{0};
  std::uint32_t player_id{0};
  std::string instance;
};

template <typename S>
void serialize(S& s, PlayerInstanceUpdatePacket& packet) {
  s.value1b(packet.packet_type);
  s.boolValue(packet.initialize_defaults);
  s.value8b(packet.actor_generation);
  s.value4b(packet.command_revision);
  s.value4b(packet.player_id);
  s.text1b(packet.instance, 255);
}

inline std::ostream& operator<<(std::ostream& os, const PlayerInstanceUpdatePacket& packet) {
  os << "PlayerInstanceUpdatePacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", player_id: " << packet.player_id
     << ", instance: " << packet.instance << " }";
  return os;
}

template <>
struct fmt::formatter<PlayerInstanceUpdatePacket> : ostream_formatter {};

struct PlayerColorUpdatePacket {
  std::uint8_t packet_type{0};
  std::uint32_t player_id{0};
  std::uint8_t r{255};
  std::uint8_t g{255};
  std::uint8_t b{255};
};

template <typename S>
void serialize(S& s, PlayerColorUpdatePacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.player_id);
  s.value1b(packet.r);
  s.value1b(packet.g);
  s.value1b(packet.b);
}

inline std::ostream& operator<<(std::ostream& os, const PlayerColorUpdatePacket& packet) {
  os << "PlayerColorUpdatePacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", player_id: " << packet.player_id
     << ", r: " << static_cast<int>(packet.r) << ", g: " << static_cast<int>(packet.g)
     << ", b: " << static_cast<int>(packet.b) << " }";
  return os;
}

template <>
struct fmt::formatter<PlayerColorUpdatePacket> : ostream_formatter {};

struct PlayerVisualUpdatePacket {
  std::uint8_t packet_type{0};
  std::uint32_t player_id{0};
  std::string body_model;
  std::uint16_t body_texture{0};
  std::string head_model;
  std::uint16_t head_texture{0};
  std::uint16_t teeth_texture{0};
  std::uint16_t skin_color{0};
};

template <typename S>
void serialize(S& s, PlayerVisualUpdatePacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.player_id);
  s.text1b(packet.body_model, 255);
  s.value2b(packet.body_texture);
  s.text1b(packet.head_model, 255);
  s.value2b(packet.head_texture);
  s.value2b(packet.teeth_texture);
  s.value2b(packet.skin_color);
}

inline std::ostream& operator<<(std::ostream& os, const PlayerVisualUpdatePacket& packet) {
  os << "PlayerVisualUpdatePacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", player_id: " << packet.player_id
     << ", body_model: " << packet.body_model << ", body_texture: " << packet.body_texture 
     << ", head_model: " << packet.head_model << ", head_texture: " << packet.head_texture
     << ", teeth_texture: " << packet.teeth_texture << ", skin_color: " << packet.skin_color << " }";
  return os;
}

template <>
struct fmt::formatter<PlayerVisualUpdatePacket> : ostream_formatter {};

struct PlayerFatnessUpdatePacket {
  std::uint8_t packet_type{0};
  std::uint32_t player_id{0};
  float fatness{1.0f};
};

template <typename S>
void serialize(S& s, PlayerFatnessUpdatePacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.player_id);
  s.value4b(packet.fatness);
}

inline std::ostream& operator<<(std::ostream& os, const PlayerFatnessUpdatePacket& packet) {
  os << "PlayerFatnessUpdatePacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", player_id: " << packet.player_id
     << ", fatness: " << packet.fatness << " }";
  return os;
}

template <>
struct fmt::formatter<PlayerFatnessUpdatePacket> : ostream_formatter {};

struct PlayerScaleUpdatePacket {
  std::uint8_t packet_type{0};
  std::uint32_t player_id{0};
  glm::vec3 scale{1.0f, 1.0f, 1.0f};
};

template <typename S>
void serialize(S& s, PlayerScaleUpdatePacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.player_id);
  s.object(packet.scale);
}

inline std::ostream& operator<<(std::ostream& os, const PlayerScaleUpdatePacket& packet) {
  os << "PlayerScaleUpdatePacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", player_id: " << packet.player_id
     << ", scale: (" << packet.scale.x << ", " << packet.scale.y << ", " << packet.scale.z << ") }";
  return os;
}

template <>
struct fmt::formatter<PlayerScaleUpdatePacket> : ostream_formatter {};

struct PlayerOverlayUpdatePacket {
  std::uint8_t packet_type{0};
  std::uint32_t player_id{0};
  std::string overlay;
  std::uint8_t apply{1};
};

template <typename S>
void serialize(S& s, PlayerOverlayUpdatePacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.player_id);
  s.text1b(packet.overlay, 255);
  s.value1b(packet.apply);
}

inline std::ostream& operator<<(std::ostream& os, const PlayerOverlayUpdatePacket& packet) {
  os << "PlayerOverlayUpdatePacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", player_id: " << packet.player_id
     << ", overlay: " << packet.overlay << ", apply: " << static_cast<int>(packet.apply) << " }";
  return os;
}

template <>
struct fmt::formatter<PlayerOverlayUpdatePacket> : ostream_formatter {};

struct PlayerSkillWeaponUpdatePacket {
  std::uint8_t packet_type{0};
  std::uint32_t player_id{0};
  std::int32_t skill_id{0};
  std::int32_t percentage{0};
};

struct PlayerAnimationPacket {
  std::uint8_t packet_type{0};
  std::uint32_t player_id{0};
  std::string animation;
};

template <typename S>
void serialize(S& s, PlayerAnimationPacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.player_id);
  s.text1b(packet.animation, kMaxPlayerAnimationNameLength);
}

inline std::ostream& operator<<(std::ostream& os, const PlayerAnimationPacket& packet) {
  os << "PlayerAnimationPacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", player_id: " << packet.player_id
     << ", animation: " << packet.animation << " }";
  return os;
}

template <>
struct fmt::formatter<PlayerAnimationPacket> : ostream_formatter {};

struct PlayerAnimationStopPacket {
  std::uint8_t packet_type{0};
  std::uint32_t player_id{0};
  std::string animation;
};

template <typename S>
void serialize(S& s, PlayerAnimationStopPacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.player_id);
  s.text1b(packet.animation, kMaxPlayerAnimationNameLength);
}

inline std::ostream& operator<<(std::ostream& os, const PlayerAnimationStopPacket& packet) {
  os << "PlayerAnimationStopPacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", player_id: " << packet.player_id
     << ", animation: " << packet.animation << " }";
  return os;
}

template <>
struct fmt::formatter<PlayerAnimationStopPacket> : ostream_formatter {};

struct PlayerFaceAnimationPacket {
  std::uint8_t packet_type{0};
  std::uint32_t player_id{0};
  std::string animation;
};

template <typename S>
void serialize(S& s, PlayerFaceAnimationPacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.player_id);
  s.text1b(packet.animation, 255);
}

inline std::ostream& operator<<(std::ostream& os, const PlayerFaceAnimationPacket& packet) {
  os << "PlayerFaceAnimationPacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", player_id: " << packet.player_id
     << ", animation: " << packet.animation << " }";
  return os;
}

template <>
struct fmt::formatter<PlayerFaceAnimationPacket> : ostream_formatter {};

struct PlayerFaceAnimationStopPacket {
  std::uint8_t packet_type{0};
  std::uint32_t player_id{0};
  std::string animation;
};

template <typename S>
void serialize(S& s, PlayerFaceAnimationStopPacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.player_id);
  s.text1b(packet.animation, 255);
}

inline std::ostream& operator<<(std::ostream& os, const PlayerFaceAnimationStopPacket& packet) {
  os << "PlayerFaceAnimationStopPacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", player_id: " << packet.player_id
     << ", animation: " << packet.animation << " }";
  return os;
}

template <>
struct fmt::formatter<PlayerFaceAnimationStopPacket> : ostream_formatter {};

struct PlayerGesticulationPacket {
  std::uint8_t packet_type{0};
  std::uint32_t player_id{0};
};

template <typename S>
void serialize(S& s, PlayerGesticulationPacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.player_id);
}

inline std::ostream& operator<<(std::ostream& os, const PlayerGesticulationPacket& packet) {
  os << "PlayerGesticulationPacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", player_id: " << packet.player_id << " }";
  return os;
}

template <>
struct fmt::formatter<PlayerGesticulationPacket> : ostream_formatter {};

template <typename S>
void serialize(S& s, PlayerSkillWeaponUpdatePacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.player_id);
  s.value4b(packet.skill_id);
  s.value4b(packet.percentage);
}

inline std::ostream& operator<<(std::ostream& os, const PlayerSkillWeaponUpdatePacket& packet) {
  os << "PlayerSkillWeaponUpdatePacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", player_id: " << packet.player_id
     << ", skill_id: " << packet.skill_id << ", percentage: " << packet.percentage << " }";
  return os;
}

template <>
struct fmt::formatter<PlayerSkillWeaponUpdatePacket> : ostream_formatter {};

struct PlayerTalentUpdatePacket {
  std::uint8_t packet_type{0};
  std::uint32_t player_id{0};
  std::int32_t talent_id{0};
  std::int32_t talent_value{0};
};

template <typename S>
void serialize(S& s, PlayerTalentUpdatePacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.player_id);
  s.value4b(packet.talent_id);
  s.value4b(packet.talent_value);
}

inline std::ostream& operator<<(std::ostream& os, const PlayerTalentUpdatePacket& packet) {
  os << "PlayerTalentUpdatePacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", player_id: " << packet.player_id
     << ", talent_id: " << packet.talent_id << ", talent_value: " << packet.talent_value << " }";
  return os;
}

template <>
struct fmt::formatter<PlayerTalentUpdatePacket> : ostream_formatter {};

struct PlayerAttributeUpdatePacket {
  std::uint64_t actor_generation{0};
  std::uint32_t command_revision{0};
  std::uint32_t state_sequence{0};
  std::uint8_t packet_type{0};
  std::uint32_t player_id{0};
  PlayerAttributeId attribute_id{0};
  std::int32_t value{0};
};

template <typename S>
void serialize(S& s, PlayerAttributeUpdatePacket& packet) {
  s.value1b(packet.packet_type);
  s.value8b(packet.actor_generation);
  s.value4b(packet.command_revision);
  s.value4b(packet.state_sequence);
  s.value4b(packet.player_id);
  s.value1b(packet.attribute_id);
  s.value4b(packet.value);
}

inline std::ostream& operator<<(std::ostream& os, const PlayerAttributeUpdatePacket& packet) {
  os << "PlayerAttributeUpdatePacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", player_id: " << packet.player_id
     << ", attribute_id: " << static_cast<int>(packet.attribute_id) << ", value: " << packet.value << " }";
  return os;
}

template <>
struct fmt::formatter<PlayerAttributeUpdatePacket> : ostream_formatter {};

struct PlayerAttributeSnapshotPacket {
  std::uint64_t actor_generation{0};
  std::uint32_t command_revision{0};
  std::uint32_t state_sequence{0};
  std::uint8_t packet_type{0};
  std::uint32_t player_id{0};
  std::int32_t strength{0};
  std::int32_t dexterity{0};
  std::int32_t level{0};
  std::int32_t exp{0};
  std::int32_t next_level_exp{0};
  std::int32_t learn_points{0};
  std::int32_t health{0};
  std::int32_t max_health{0};
  std::int32_t mana{0};
  std::int32_t max_mana{0};
};

template <typename S>
void serialize(S& s, PlayerAttributeSnapshotPacket& packet) {
  s.value1b(packet.packet_type);
  s.value8b(packet.actor_generation);
  s.value4b(packet.command_revision);
  s.value4b(packet.state_sequence);
  s.value4b(packet.player_id);
  s.value4b(packet.strength);
  s.value4b(packet.dexterity);
  s.value4b(packet.level);
  s.value4b(packet.exp);
  s.value4b(packet.next_level_exp);
  s.value4b(packet.learn_points);
  s.value4b(packet.health);
  s.value4b(packet.max_health);
  s.value4b(packet.mana);
  s.value4b(packet.max_mana);
}

inline std::ostream& operator<<(std::ostream& os, const PlayerAttributeSnapshotPacket& packet) {
  os << "PlayerAttributeSnapshotPacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", player_id: " << packet.player_id
     << ", strength: " << packet.strength << ", dexterity: " << packet.dexterity << ", level: " << packet.level << ", exp: " << packet.exp
     << ", next_level_exp: " << packet.next_level_exp << ", learn_points: " << packet.learn_points << ", health: " << packet.health
     << ", max_health: " << packet.max_health << ", mana: " << packet.mana << ", max_mana: " << packet.max_mana << " }";
  return os;
}

template <>
struct fmt::formatter<PlayerAttributeSnapshotPacket> : ostream_formatter {};

struct PlayerWorldUpdatePacket {
  std::uint64_t actor_generation{0};
  std::uint32_t command_revision{0};
  std::uint8_t packet_type{0};
  std::uint32_t player_id{0};
  std::string world_name;
  std::string start_point;
};

template <typename S>
void serialize(S& s, PlayerWorldUpdatePacket& packet) {
  s.value1b(packet.packet_type);
  s.value8b(packet.actor_generation);
  s.value4b(packet.command_revision);
  s.value4b(packet.player_id);
  s.text1b(packet.world_name, 64);
  s.text1b(packet.start_point, 64);
}

inline std::ostream& operator<<(std::ostream& os, const PlayerWorldUpdatePacket& packet) {
  os << "PlayerWorldUpdatePacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", player_id: " << packet.player_id
     << ", world_name: " << packet.world_name << ", start_point: " << packet.start_point << " }";
  return os;
}

template <>
struct fmt::formatter<PlayerWorldUpdatePacket> : ostream_formatter {};

struct ExistingPlayersPacket {
  std::uint8_t packet_type{0};
  std::vector<ExistingPlayerInfo> existing_players;
};

template <typename S>
void serialize(S& s, ExistingPlayersPacket& packet) {
  s.value1b(packet.packet_type);
  s.container(packet.existing_players, 400);
}

struct JoinGamePacket {
  std::uint64_t actor_generation{0};
  std::uint8_t packet_type{0};
  glm::vec3 position{0.0f};
  glm::vec3 normal{0.0f};
  std::string left_hand_item_instance;
  std::string right_hand_item_instance;
  std::string equipped_armor_instance;
  std::string equipped_helmet_instance;
  std::string equipped_shield_instance;
  std::string equipped_amulet_instance;
  std::string equipped_belt_instance;
  std::string equipped_ring_left_instance;
  std::string equipped_ring_right_instance;
  std::string animation_name;
  std::string body_model;
  std::uint16_t body_texture{0};
  std::string head_model;
  std::uint16_t head_texture{0};
  std::uint16_t teeth_texture{0};
  std::uint16_t skin_color{0};
  std::uint8_t walk_style{0};
  // Stock appearance overlay, independent of run/walk mode. Seeded before onPlayerConnect.
  std::string walking_overlay;
  std::string player_name;
  // May be used to identify the player (e.g. when relaying the information about the player to other players)
  std::optional<std::uint32_t> player_id;
};

template <typename S>
void serialize(S& s, JoinGamePacket& packet) {
  s.value1b(packet.packet_type);
  s.value8b(packet.actor_generation);
  s.object(packet.position);
  s.object(packet.normal);
  s.text1b(packet.left_hand_item_instance, gmp::kMaxScriptNameLength);
  s.text1b(packet.right_hand_item_instance, gmp::kMaxScriptNameLength);
  s.text1b(packet.equipped_armor_instance, gmp::kMaxScriptNameLength);
  s.text1b(packet.equipped_helmet_instance, gmp::kMaxScriptNameLength);
  s.text1b(packet.equipped_shield_instance, gmp::kMaxScriptNameLength);
  s.text1b(packet.equipped_amulet_instance, gmp::kMaxScriptNameLength);
  s.text1b(packet.equipped_belt_instance, gmp::kMaxScriptNameLength);
  s.text1b(packet.equipped_ring_left_instance, gmp::kMaxScriptNameLength);
  s.text1b(packet.equipped_ring_right_instance, gmp::kMaxScriptNameLength);
  s.text1b(packet.animation_name, kMaxPlayerAnimationNameLength);
  s.text1b(packet.body_model, 255);
  s.value2b(packet.body_texture);
  s.text1b(packet.head_model, 255);
  s.value2b(packet.head_texture);
  s.value2b(packet.teeth_texture);
  s.value2b(packet.skin_color);
  s.value1b(packet.walk_style);
  s.text1b(packet.walking_overlay, 32);
  s.text1b(packet.player_name, 255);
  s.ext4b(packet.player_id, bitsery::ext::StdOptional{});
}

inline std::ostream& operator<<(std::ostream& os, const JoinGamePacket& packet) {
  os << "JoinGamePacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", position: (" << packet.position.x << ", " << packet.position.y << ", "
     << packet.position.z << ")"
     << ", normal: (" << packet.normal.x << ", " << packet.normal.y << ", " << packet.normal.z << ")"
     << ", left_hand_item_instance: " << packet.left_hand_item_instance << ", right_hand_item_instance: " << packet.right_hand_item_instance
     << ", equipped_armor_instance: " << packet.equipped_armor_instance
     << ", equipped_helmet_instance: " << packet.equipped_helmet_instance
     << ", equipped_shield_instance: " << packet.equipped_shield_instance
     << ", equipped_amulet_instance: " << packet.equipped_amulet_instance
     << ", equipped_belt_instance: " << packet.equipped_belt_instance
     << ", equipped_ring_left_instance: " << packet.equipped_ring_left_instance
     << ", equipped_ring_right_instance: " << packet.equipped_ring_right_instance
     << ", animation_name: " << packet.animation_name
     << ", body_model: " << packet.body_model << ", body_texture: " << static_cast<int>(packet.body_texture)
     << ", head_model: " << packet.head_model << ", head_texture: " << static_cast<int>(packet.head_texture)
     << ", teeth_texture: " << static_cast<int>(packet.teeth_texture) << ", skin_color: " << static_cast<int>(packet.skin_color)
     << ", walk_style: " << static_cast<int>(packet.walk_style)
     << ", walking_overlay: " << packet.walking_overlay << ", player_name: " << packet.player_name;

  if (packet.player_id.has_value()) {
    os << ", player_id: " << packet.player_id.value();
  }

  os << " }";
  return os;
}

template <>
struct fmt::formatter<JoinGamePacket> : ostream_formatter {};

struct PlayerSpawnPacket {
  std::uint32_t instance_change_revision{0};
  std::uint16_t transformation_spell_id{0};
  std::uint32_t transformation_cast_id{0};
  // Owner and NPC snapshots include the complete server-owned inventory.
  std::vector<InventoryEntry> inventory;
  bool invulnerable{false};
  std::uint32_t body_state{0};
  std::uint8_t walk_mode{0};
  float look_yaw{0.5f};
  float look_pitch{0.5f};
  std::optional<std::uint32_t> focus_target_id;
  std::uint64_t actor_generation{0};
  std::uint32_t command_revision{0};
  std::uint64_t source_time_ms{0};
  std::uint8_t weapon_mode{0};
  std::uint8_t active_spell_nr{0};
  std::string active_spell_instance;
  std::uint8_t active_spell_level{0};
  bool spell_investing{false};
  std::uint8_t head_direction{0};
  std::string melee_weapon_instance;
  std::string ranged_weapon_instance;
  std::uint8_t packet_type{0};
  std::uint32_t player_id{0};
  std::uint32_t state_sequence{0};
  glm::vec3 position{0.0f};
  glm::vec3 normal{0.0f};
  std::string left_hand_item_instance;
  std::string right_hand_item_instance;
  std::string equipped_armor_instance;
  std::string equipped_helmet_instance;
  std::string equipped_shield_instance;
  std::string equipped_amulet_instance;
  std::string equipped_belt_instance;
  std::string equipped_ring_left_instance;
  std::string equipped_ring_right_instance;
  std::string animation_name;
  std::string body_model;
  std::uint16_t body_texture{0};
  std::string head_model;
  std::uint16_t head_texture{0};
  std::uint16_t teeth_texture{0};
  std::uint16_t skin_color{0};
  std::uint8_t walk_style{0};
  std::string player_name;

  // Additional player state snapshot (previously sent as multiple packets)
  std::string instance;
  std::uint8_t name_color_r{255};
  std::uint8_t name_color_g{255};
  std::uint8_t name_color_b{255};

  std::int32_t strength{0};
  std::int32_t dexterity{0};
  std::int32_t level{0};
  std::int32_t exp{0};
  std::int32_t next_level_exp{0};
  std::int32_t learn_points{0};
  std::int32_t health{0};
  std::int32_t max_health{0};
  std::int32_t mana{0};
  std::int32_t max_mana{0};
  std::uint8_t life_state{PLAYER_LIFE_ALIVE};

  float fatness{1.0f};
  glm::vec3 scale{1.0f, 1.0f, 1.0f};

  struct SkillEntry {
    std::int32_t skill_id{0};
    std::int32_t percentage{0};
  };
  std::vector<SkillEntry> weapon_skills;

  struct TalentEntry {
    std::int32_t talent_id{0};
    std::int32_t value{0};
  };
  std::vector<TalentEntry> talents;

  std::vector<std::string> overlays;
};

template <typename S>
void serialize(S& s, PlayerSpawnPacket::SkillEntry& entry) {
  s.value4b(entry.skill_id);
  s.value4b(entry.percentage);
}

template <typename S>
void serialize(S& s, PlayerSpawnPacket::TalentEntry& entry) {
  s.value4b(entry.talent_id);
  s.value4b(entry.value);
}

template <typename S>
void serialize(S& s, PlayerSpawnPacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.instance_change_revision);
  s.value2b(packet.transformation_spell_id);
  s.value4b(packet.transformation_cast_id);
  s.container(packet.inventory, kMaxInventoryEntries);
  s.boolValue(packet.invulnerable);
  s.value4b(packet.body_state);
  s.value1b(packet.walk_mode);
  s.value4b(packet.look_yaw);
  s.value4b(packet.look_pitch);
  s.ext(packet.focus_target_id, bitsery::ext::StdOptional{}, [](S& s, std::uint32_t& id) { s.value4b(id); });
  s.value8b(packet.actor_generation);
  s.value4b(packet.command_revision);
  s.value8b(packet.source_time_ms);
  s.value1b(packet.weapon_mode);
  s.value1b(packet.active_spell_nr);
  s.text1b(packet.active_spell_instance, gmp::kMaxScriptNameLength);
  s.value1b(packet.active_spell_level);
  s.boolValue(packet.spell_investing);
  s.value1b(packet.head_direction);
  s.text1b(packet.melee_weapon_instance, gmp::kMaxScriptNameLength);
  s.text1b(packet.ranged_weapon_instance, gmp::kMaxScriptNameLength);
  s.value4b(packet.player_id);
  s.value4b(packet.state_sequence);
  s.object(packet.position);
  s.object(packet.normal);
  s.text1b(packet.left_hand_item_instance, gmp::kMaxScriptNameLength);
  s.text1b(packet.right_hand_item_instance, gmp::kMaxScriptNameLength);
  s.text1b(packet.equipped_armor_instance, gmp::kMaxScriptNameLength);
  s.text1b(packet.equipped_helmet_instance, gmp::kMaxScriptNameLength);
  s.text1b(packet.equipped_shield_instance, gmp::kMaxScriptNameLength);
  s.text1b(packet.equipped_amulet_instance, gmp::kMaxScriptNameLength);
  s.text1b(packet.equipped_belt_instance, gmp::kMaxScriptNameLength);
  s.text1b(packet.equipped_ring_left_instance, gmp::kMaxScriptNameLength);
  s.text1b(packet.equipped_ring_right_instance, gmp::kMaxScriptNameLength);
  s.text1b(packet.animation_name, kMaxPlayerAnimationNameLength);
  s.text1b(packet.body_model, 255);
  s.value2b(packet.body_texture);
  s.text1b(packet.head_model, 255);
  s.value2b(packet.head_texture);
  s.value2b(packet.teeth_texture);
  s.value2b(packet.skin_color);
  s.value1b(packet.walk_style);
  s.text1b(packet.player_name, 255);

  s.text1b(packet.instance, 255);
  s.value1b(packet.name_color_r);
  s.value1b(packet.name_color_g);
  s.value1b(packet.name_color_b);

  s.value4b(packet.strength);
  s.value4b(packet.dexterity);
  s.value4b(packet.level);
  s.value4b(packet.exp);
  s.value4b(packet.next_level_exp);
  s.value4b(packet.learn_points);
  s.value4b(packet.health);
  s.value4b(packet.max_health);
  s.value4b(packet.mana);
  s.value4b(packet.max_mana);
  s.value1b(packet.life_state);

  s.value4b(packet.fatness);
  s.object(packet.scale);

  s.container(packet.weapon_skills, 128);
  s.container(packet.talents, 128);
  s.container(packet.overlays, 64, [](S& s, std::string& overlay) { s.text1b(overlay, 255); });
}

inline std::ostream& operator<<(std::ostream& os, const PlayerSpawnPacket& packet) {
  os << "PlayerSpawnPacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", player_id: " << packet.player_id
     << ", state_sequence: " << packet.state_sequence << ", position: (" << packet.position.x
     << ", " << packet.position.y << ", " << packet.position.z << ")"
     << ", normal: (" << packet.normal.x << ", " << packet.normal.y << ", " << packet.normal.z << ")"
     << ", left_hand_item_instance: " << packet.left_hand_item_instance << ", right_hand_item_instance: " << packet.right_hand_item_instance
     << ", equipped_armor_instance: " << packet.equipped_armor_instance
     << ", equipped_helmet_instance: " << packet.equipped_helmet_instance
     << ", equipped_shield_instance: " << packet.equipped_shield_instance
     << ", equipped_amulet_instance: " << packet.equipped_amulet_instance
     << ", equipped_belt_instance: " << packet.equipped_belt_instance
     << ", equipped_ring_left_instance: " << packet.equipped_ring_left_instance
     << ", equipped_ring_right_instance: " << packet.equipped_ring_right_instance
     << ", animation_name: " << packet.animation_name
     << ", body_model: " << packet.body_model << ", body_texture: " << static_cast<int>(packet.body_texture)
     << ", head_model: " << packet.head_model << ", head_texture: " << static_cast<int>(packet.head_texture)
     << ", teeth_texture: " << static_cast<int>(packet.teeth_texture) << ", skin_color: " << static_cast<int>(packet.skin_color)
     << ", walk_style: " << static_cast<int>(packet.walk_style)
     << ", player_name: " << packet.player_name
     << ", instance: " << packet.instance
     << ", name_color: (" << static_cast<int>(packet.name_color_r) << ", " << static_cast<int>(packet.name_color_g) << ", "
     << static_cast<int>(packet.name_color_b) << ")"
     << ", strength: " << packet.strength << ", dexterity: " << packet.dexterity << ", level: " << packet.level
     << ", exp: " << packet.exp << ", next_level_exp: " << packet.next_level_exp << ", learn_points: " << packet.learn_points
     << ", health: " << packet.health << ", max_health: " << packet.max_health << ", mana: " << packet.mana
     << ", max_mana: " << packet.max_mana << ", life_state: " << static_cast<int>(packet.life_state)
     << ", fatness: " << packet.fatness
     << ", scale: (" << packet.scale.x << ", " << packet.scale.y << ", " << packet.scale.z << ")"
     << ", weapon_skills: " << packet.weapon_skills.size() << ", talents: " << packet.talents.size() << ", overlays: "
     << packet.overlays.size() << " }";
  return os;
}

template <>
struct fmt::formatter<PlayerSpawnPacket> : ostream_formatter {};

// The controlling runtime completes only a server-issued constructor request.
// The accepted defaults are subsequently distributed as an ordinary snapshot.
struct PlayerInstanceReadyPacket {
  std::uint8_t packet_type{Net::PT_PLAYER_INSTANCE_READY};
  std::uint32_t request_revision{0};
  std::uint32_t control_epoch{0};
  bool success{true};
  PlayerSpawnPacket defaults;
};

template <typename S>
void serialize(S& s, PlayerInstanceReadyPacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.request_revision);
  s.value4b(packet.control_epoch);
  s.boolValue(packet.success);
  s.object(packet.defaults);
}

struct MessagePacket {
  std::uint8_t packet_type;
  std::string message;
  std::uint8_t r{255};
  std::uint8_t g{255};
  std::uint8_t b{255};
  std::optional<std::uint32_t> sender;
  std::optional<std::uint32_t> recipient;
};

template <typename S>
void serialize(S& s, MessagePacket& packet) {
  s.value1b(packet.packet_type);
  s.text1b(packet.message, kMaxChatMessageLength);
  s.value1b(packet.r);
  s.value1b(packet.g);
  s.value1b(packet.b);
  s.ext4b(packet.sender, bitsery::ext::StdOptional{});
  s.ext4b(packet.recipient, bitsery::ext::StdOptional{});
}

inline std::ostream& operator<<(std::ostream& os, const MessagePacket& packet) {
  os << "MessagePacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", message: " << packet.message
     << ", color: (" << static_cast<int>(packet.r) << ", " << static_cast<int>(packet.g) << ", " << static_cast<int>(packet.b) << ")";
  if (packet.sender) {
    os << ", sender: " << *packet.sender;
  }
  if (packet.recipient) {
    os << ", recipient: " << *packet.recipient;
  }
  os << " }";
  return os;
}

template <>
struct fmt::formatter<MessagePacket> : ostream_formatter {};

struct CastSpellPacket {
  // Captured before the native cast can consume its spell item.
  bool scroll{false};
  bool transformation_succeeded{false};
  std::uint8_t spell_level{1};
  std::int32_t mana_delta{0};
  std::uint64_t source_time_ms{0};
  std::uint32_t command_revision{0};
  std::string item_instance;
  std::uint32_t control_epoch{0};
  std::uint64_t actor_generation{0};
  std::uint64_t target_generation{0};
  std::uint32_t action_id{0};
  std::uint8_t packet_type;
  std::uint16_t spell_id;
  std::optional<std::uint32_t> target_id;
  std::optional<std::uint32_t> caster_id;
};

template <typename S>
void serialize(S& s, CastSpellPacket& packet) {
  s.value1b(packet.packet_type);
  s.boolValue(packet.transformation_succeeded);
  s.boolValue(packet.scroll);
  s.value1b(packet.spell_level);
  s.value4b(packet.mana_delta);
  s.value8b(packet.source_time_ms);
  s.value4b(packet.command_revision);
  s.text1b(packet.item_instance, gmp::kMaxScriptNameLength);
  s.value4b(packet.control_epoch);
  s.value8b(packet.actor_generation);
  s.value8b(packet.target_generation);
  s.value4b(packet.action_id);
  s.value2b(packet.spell_id);
  s.ext4b(packet.target_id, bitsery::ext::StdOptional{});
  s.ext4b(packet.caster_id, bitsery::ext::StdOptional{});
}

inline std::ostream& operator<<(std::ostream& os, const CastSpellPacket& packet) {
  os << "CastSpellPacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", spell_id: " << packet.spell_id;
  if (packet.target_id) {
    os << ", target_player_id: " << *packet.target_id;
  }
  os << " }";
  return os;
}

struct DropItemPacket {
  std::uint64_t actor_generation{0};
  std::uint32_t request_id{0};
  std::uint8_t packet_type;
  std::string item_instance;
  std::int32_t item_amount{0};
  glm::vec3 position{0.0f};
  glm::vec3 rotation{0.0f};
  bool physics_enabled{true};
  std::optional<std::uint32_t> player_id;
};

template <typename S>
void serialize(S& s, DropItemPacket& packet) {
  s.value1b(packet.packet_type);
  s.value8b(packet.actor_generation);
  s.value4b(packet.request_id);
  s.text1b(packet.item_instance, gmp::kMaxScriptNameLength);
  s.value4b(packet.item_amount);
  s.object(packet.position);
  s.object(packet.rotation);
  s.value1b(packet.physics_enabled);
  s.ext4b(packet.player_id, bitsery::ext::StdOptional{});
}

inline std::ostream& operator<<(std::ostream& os, const DropItemPacket& packet) {
  os << "DropItemPacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", item_instance: " << packet.item_instance
     << ", item_amount: " << packet.item_amount
     << ", position: (" << packet.position.x << ", " << packet.position.y << ", " << packet.position.z << ")"
     << ", rotation: (" << packet.rotation.x << ", " << packet.rotation.y << ", " << packet.rotation.z << ")"
     << ", physics_enabled: " << packet.physics_enabled << " }";

  if (packet.player_id) {
    os << ", player_id: " << *packet.player_id;
  }
  return os;
}

struct TakeItemPacket {
  std::uint64_t actor_generation{0};
  std::uint32_t request_id{0};
  std::uint8_t packet_type;
  std::string item_instance;
  std::int32_t item_amount{1};
  std::optional<std::uint32_t> item_ground_id;
  std::optional<std::uint32_t> player_id;
};

template <typename S>
void serialize(S& s, TakeItemPacket& packet) {
  s.value1b(packet.packet_type);
  s.value8b(packet.actor_generation);
  s.value4b(packet.request_id);
  s.text1b(packet.item_instance, gmp::kMaxScriptNameLength);
  s.value4b(packet.item_amount);
  s.ext4b(packet.item_ground_id, bitsery::ext::StdOptional{});
  s.ext4b(packet.player_id, bitsery::ext::StdOptional{});
}

inline std::ostream& operator<<(std::ostream& os, const TakeItemPacket& packet) {
  os << "TakeItemPacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", item_instance: " << packet.item_instance
     << ", item_amount: " << packet.item_amount;

  if (packet.item_ground_id) {
    os << ", item_ground_id: " << *packet.item_ground_id;
  }

  os << " }";

  if (packet.player_id) {
    os << ", player_id: " << *packet.player_id;
  }
  return os;
}

struct ItemGroundCreatePacket {
  std::uint8_t packet_type;
  std::uint32_t item_ground_id;
  std::string item_instance;
  std::int32_t amount{1};
  bool physics_enabled{false};
  glm::vec3 position{0.0f};
  glm::vec3 rotation{0.0f};
};

template <typename S>
void serialize(S& s, ItemGroundCreatePacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.item_ground_id);
  s.text1b(packet.item_instance, gmp::kMaxScriptNameLength);
  s.value4b(packet.amount);
  s.value1b(packet.physics_enabled);
  s.object(packet.position);
  s.object(packet.rotation);
}

inline std::ostream& operator<<(std::ostream& os, const ItemGroundCreatePacket& packet) {
  os << "ItemGroundCreatePacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", item_ground_id: " << packet.item_ground_id
     << ", item_instance: " << packet.item_instance << ", amount: " << packet.amount
     << ", physics_enabled: " << packet.physics_enabled << ", position: (" << packet.position.x << ", "
     << packet.position.y << ", " << packet.position.z << "), rotation: (" << packet.rotation.x << ", "
     << packet.rotation.y << ", " << packet.rotation.z << ") }";
  return os;
}

struct ItemGroundDestroyPacket {
  std::uint8_t packet_type;
  std::uint32_t item_ground_id;
};

template <typename S>
void serialize(S& s, ItemGroundDestroyPacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.item_ground_id);
}

inline std::ostream& operator<<(std::ostream& os, const ItemGroundDestroyPacket& packet) {
  os << "ItemGroundDestroyPacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", item_ground_id: " << packet.item_ground_id << " }";
  return os;
}

struct ItemGroundClearPacket {
  std::uint8_t packet_type;
};

template <typename S>
void serialize(S& s, ItemGroundClearPacket& packet) {
  s.value1b(packet.packet_type);
}

struct PlayerWorldEnterPacket {
  std::uint64_t actor_generation{0};
  std::vector<InventoryEntry> inventory;
  std::uint8_t packet_type;
  std::string world_name;
};

template <typename S>
void serialize(S& s, PlayerWorldEnterPacket& packet) {
  s.value1b(packet.packet_type);
  s.value8b(packet.actor_generation);
  s.container(packet.inventory, kMaxInventoryEntries);
  s.text1b(packet.world_name, 255);
}

inline std::ostream& operator<<(std::ostream& os, const PlayerWorldEnterPacket& packet) {
  os << "PlayerWorldEnterPacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", world_name: " << packet.world_name << " }";
  return os;
}

struct GiveItemPacket {
  std::uint64_t actor_generation{0};
  std::uint32_t command_revision{0};
  std::uint32_t state_sequence{0};
  std::uint8_t packet_type;
  std::string item_instance;
  std::int32_t item_amount;
  std::uint32_t player_id;
};

template <typename S>
void serialize(S& s, GiveItemPacket& packet) {
  s.value1b(packet.packet_type);
  s.value8b(packet.actor_generation);
  s.value4b(packet.command_revision);
  s.value4b(packet.state_sequence);
  s.text1b(packet.item_instance, gmp::kMaxScriptNameLength);
  s.value4b(packet.item_amount);
  s.value4b(packet.player_id);
}

inline std::ostream& operator<<(std::ostream& os, const GiveItemPacket& packet) {
  os << "GiveItemPacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", item_instance: " << packet.item_instance
     << ", item_amount: " << packet.item_amount << ", player_id: " << packet.player_id << " }";
  return os;
}

struct EquipItemPacket {
  std::uint64_t actor_generation{0};
  std::uint32_t command_revision{0};
  std::uint32_t state_sequence{0};
  std::uint8_t packet_type;
  std::string item_instance;
  std::int16_t slot_id;
  std::uint32_t player_id;
};

template <typename S>
void serialize(S& s, EquipItemPacket& packet) {
  s.value1b(packet.packet_type);
  s.value8b(packet.actor_generation);
  s.value4b(packet.command_revision);
  s.value4b(packet.state_sequence);
  s.text1b(packet.item_instance, gmp::kMaxScriptNameLength);
  s.value2b(packet.slot_id);
  s.value4b(packet.player_id);
}

inline std::ostream& operator<<(std::ostream& os, const EquipItemPacket& packet) {
  os << "EquipItemPacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", item_instance: " << packet.item_instance
     << ", slot_id: " << packet.slot_id << ", player_id: " << packet.player_id << " }";
  return os;
}

struct UnequipItemPacket {
  std::int16_t slot_id{Net::EQUIP_SLOT_GENERIC};
  std::uint64_t actor_generation{0};
  std::uint32_t command_revision{0};
  std::uint32_t state_sequence{0};
  std::uint8_t packet_type;
  std::string item_instance;
  std::uint32_t player_id;
};

template <typename S>
void serialize(S& s, UnequipItemPacket& packet) {
  s.value1b(packet.packet_type);
  s.value2b(packet.slot_id);
  s.value8b(packet.actor_generation);
  s.value4b(packet.command_revision);
  s.value4b(packet.state_sequence);
  s.text1b(packet.item_instance, gmp::kMaxScriptNameLength);
  s.value4b(packet.player_id);
}

inline std::ostream& operator<<(std::ostream& os, const UnequipItemPacket& packet) {
  os << "UnequipItemPacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", item_instance: " << packet.item_instance
     << ", player_id: " << packet.player_id << " }";
  return os;
}

struct RemoveItemPacket {
  std::uint64_t actor_generation{0};
  std::uint32_t command_revision{0};
  std::uint32_t state_sequence{0};
  std::uint8_t packet_type;
  std::string item_instance;
  std::int32_t item_amount;
  std::uint32_t player_id;
};

template <typename S>
void serialize(S& s, RemoveItemPacket& packet) {
  s.value1b(packet.packet_type);
  s.value8b(packet.actor_generation);
  s.value4b(packet.command_revision);
  s.value4b(packet.state_sequence);
  s.text1b(packet.item_instance, gmp::kMaxScriptNameLength);
  s.value4b(packet.item_amount);
  s.value4b(packet.player_id);
}

inline std::ostream& operator<<(std::ostream& os, const RemoveItemPacket& packet) {
  os << "RemoveItemPacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", item_instance: " << packet.item_instance
     << ", item_amount: " << packet.item_amount << ", player_id: " << packet.player_id << " }";
  return os;
}

struct PlayerStateUpdatePacket {
  std::uint64_t actor_generation{0};
  std::uint32_t command_revision{0};
  std::uint8_t packet_type;
  std::uint32_t state_sequence{0};
  PlayerState state;
  // May be used to identify the player (e.g. when relaying the information about the player to other players)
  std::optional<std::uint32_t> player_id;
};

template <typename S>
void serialize(S& s, PlayerStateUpdatePacket& packet) {
  s.value1b(packet.packet_type);
  s.value8b(packet.actor_generation);
  s.value4b(packet.command_revision);
  s.value4b(packet.state_sequence);
  s.object(packet.state);
  s.ext4b(packet.player_id, bitsery::ext::StdOptional{});
}

inline std::ostream& operator<<(std::ostream& os, const PlayerStateUpdatePacket& packet) {
  os << "PlayerStateUpdatePacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ","
     << " state_sequence: " << packet.state_sequence << ","
     << " state: " << packet.state << " }";
  if (packet.player_id.has_value()) {
    os << ", player_id: " << packet.player_id.value();
  }
  return os;
}

template <>
struct fmt::formatter<PlayerStateUpdatePacket> : ostream_formatter {};

struct PlayerPositionUpdatePacket {
  std::uint64_t actor_generation{0};
  std::uint32_t command_revision{0};
  std::uint64_t source_time_ms{0};
  bool teleport{true};
  std::uint8_t packet_type;
  std::uint32_t state_sequence{0};
  glm::vec3 position;
  // May be used to identify the player (e.g. when relaying the information about the player to other players)
  std::optional<std::uint32_t> player_id;
};

template <typename S>
void serialize(S& s, PlayerPositionUpdatePacket& packet) {
  s.value1b(packet.packet_type);
  s.value8b(packet.actor_generation);
  s.value4b(packet.command_revision);
  s.value8b(packet.source_time_ms);
  s.boolValue(packet.teleport);
  s.value4b(packet.state_sequence);
  s.object(packet.position);
  s.ext4b(packet.player_id, bitsery::ext::StdOptional{});
}

inline std::ostream& operator<<(std::ostream& os, const PlayerPositionUpdatePacket& packet) {
  os << "PlayerPositionUpdatePacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ","
     << " state_sequence: " << packet.state_sequence << ","
     << " position: (" << packet.position.x << ", " << packet.position.y << ", " << packet.position.z << ")";

  if (packet.player_id.has_value()) {
    os << ", player_id: " << packet.player_id.value();
  }
  return os;
}

// The client encodes 20 ms mono frames at 24 kbit/s. This cap leaves ample
// VBR headroom while keeping one voice packet comfortably below the MTU.
constexpr std::size_t kMaxVoiceFrameSize = 512;
constexpr std::size_t kMaxVoiceChannelNameLength = 32;

struct VoicePacket {
  std::uint8_t packet_type{0};
  // Zero in client-to-server packets. The server derives and overwrites it
  // from the authenticated connection before relaying the frame.
  std::uint32_t player_id{0};
  std::uint32_t talkspurt_id{0};
  std::uint32_t sequence{0};
  std::uint8_t spatial{1};
  std::uint32_t range{0};
  std::vector<std::uint8_t> voice_data;
};

template <typename S>
void serialize(S& s, VoicePacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.player_id);
  s.value4b(packet.talkspurt_id);
  s.value4b(packet.sequence);
  s.value1b(packet.spatial);
  s.value4b(packet.range);
  s.container1b(packet.voice_data, kMaxVoiceFrameSize);
}

struct VoiceConfigurationPacket {
  std::uint8_t packet_type{0};
  std::uint8_t enabled{0};
  std::uint8_t transmit_enabled{0};
  std::uint32_t range{0};
};

template <typename S>
void serialize(S& s, VoiceConfigurationPacket& packet) {
  s.value1b(packet.packet_type);
  s.value1b(packet.enabled);
  s.value1b(packet.transmit_enabled);
  s.value4b(packet.range);
}

struct VoiceChannelPacket {
  std::uint8_t packet_type{0};
  std::string channel;
};

template <typename S>
void serialize(S& s, VoiceChannelPacket& packet) {
  s.value1b(packet.packet_type);
  s.text1b(packet.channel, kMaxVoiceChannelNameLength);
}

struct DisconnectionInfoPacket {
  std::uint8_t packet_type;
  std::uint32_t disconnected_id;
};

template <typename S>
void serialize(S& s, DisconnectionInfoPacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.disconnected_id);
}

inline std::ostream& operator<<(std::ostream& os, const DisconnectionInfoPacket& info) {
  os << "DisconnectionInfo {"
     << " packet_type: " << static_cast<int>(info.packet_type) << ", disconnected_id: " << info.disconnected_id << " }";
  return os;
}

struct PlayerDeathInfoPacket {
  std::uint64_t actor_generation{0};
  std::uint32_t command_revision{0};
  std::uint8_t packet_type;
  std::uint32_t player_id;
  std::uint32_t state_sequence{0};
};

template <typename S>
void serialize(S& s, PlayerDeathInfoPacket& packet) {
  s.value1b(packet.packet_type);
  s.value8b(packet.actor_generation);
  s.value4b(packet.command_revision);
  s.value4b(packet.player_id);
  s.value4b(packet.state_sequence);
}

inline std::ostream& operator<<(std::ostream& os, const PlayerDeathInfoPacket& info) {
  os << "PlayerDeathInfo {"
     << " packet_type: " << static_cast<int>(info.packet_type) << ", player_id: " << info.player_id << " }";
  return os;
}

struct PlayerRespawnInfoPacket {
  std::int32_t health{0};
  std::int32_t mana{0};
  std::uint64_t actor_generation{0};
  std::uint32_t command_revision{0};
  std::uint8_t packet_type;
  std::uint32_t player_id;
  std::uint32_t state_sequence{0};
};

template <typename S>
void serialize(S& s, PlayerRespawnInfoPacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.health);
  s.value4b(packet.mana);
  s.value8b(packet.actor_generation);
  s.value4b(packet.command_revision);
  s.value4b(packet.player_id);
  s.value4b(packet.state_sequence);
}

inline std::ostream& operator<<(std::ostream& os, const PlayerRespawnInfoPacket& info) {
  os << "PlayerRespawnInfo {"
     << " packet_type: " << static_cast<int>(info.packet_type) << ", player_id: " << info.player_id << " }";
  return os;
}

struct PlayerHitReportPacket {
  std::uint32_t command_revision{0};
  std::uint64_t actor_generation{0};
  std::uint64_t victim_generation{0};
  std::uint32_t attack_id{0};
  std::optional<std::uint32_t> attacker_id;
  std::uint32_t control_epoch{0};
  std::uint8_t packet_type;
  std::uint32_t victim_id;
  std::int32_t damage;
  std::uint32_t damage_type{0};
  bool dont_kill{false};
  bool finishing_move{false};
};

template <typename S>
void serialize(S& s, PlayerHitReportPacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.command_revision);
  s.value8b(packet.actor_generation);
  s.value8b(packet.victim_generation);
  s.value4b(packet.attack_id);
  s.ext4b(packet.attacker_id, bitsery::ext::StdOptional{});
  s.value4b(packet.control_epoch);
  s.value4b(packet.victim_id);
  s.value4b(packet.damage);
  s.value4b(packet.damage_type);
  s.value1b(packet.dont_kill);
  s.value1b(packet.finishing_move);
}

struct PlayerUnconsciousPacket {
  std::uint64_t actor_generation{0};
  std::uint32_t command_revision{0};
  std::uint8_t packet_type;
  std::uint32_t player_id{0};
  std::uint32_t state_sequence{0};
  std::optional<std::uint32_t> attacker_id;
};

template <typename S>
void serialize(S& s, PlayerUnconsciousPacket& packet) {
  s.value1b(packet.packet_type);
  s.value8b(packet.actor_generation);
  s.value4b(packet.command_revision);
  s.value4b(packet.player_id);
  s.value4b(packet.state_sequence);
  s.ext4b(packet.attacker_id, bitsery::ext::StdOptional{});
}

struct PlayerStandUpPacket {
  std::uint64_t actor_generation{0};
  std::uint32_t command_revision{0};
  std::uint8_t packet_type;
  std::uint32_t player_id{0};
  std::uint32_t state_sequence{0};
};

template <typename S>
void serialize(S& s, PlayerStandUpPacket& packet) {
  s.value1b(packet.packet_type);
  s.value8b(packet.actor_generation);
  s.value4b(packet.command_revision);
  s.value4b(packet.player_id);
  s.value4b(packet.state_sequence);
}

struct PlayerDeathReportPacket {
  std::uint64_t actor_generation{0};
  std::uint8_t packet_type;
  std::optional<std::uint32_t> killer_id;
};

template <typename S>
void serialize(S& s, PlayerDeathReportPacket& packet) {
  s.value1b(packet.packet_type);
  s.value8b(packet.actor_generation);
  s.ext4b(packet.killer_id, bitsery::ext::StdOptional{});
}

struct ClientResourceInfoEntry {
  std::string name;
  std::string version;
  std::string manifest_path;
  std::string manifest_sha256;
  std::string archive_path;
  std::string archive_sha256;
  std::uint64_t archive_size{0};
  std::uint64_t manifest_size{0};
};

struct AddonVdfInfoEntry {
  std::string logical_name;
  std::string sha256;
  std::uint64_t size{0};
  bool contains_gothic_dat{false};
};

template <typename S>
void serialize(S& s, AddonVdfInfoEntry& entry) {
  s.text1b(entry.logical_name, 255);
  s.text1b(entry.sha256, 64);
  s.value8b(entry.size);
  s.value1b(entry.contains_gothic_dat);
}

inline std::ostream& operator<<(std::ostream& os, const AddonVdfInfoEntry& entry) {
  os << "AddonVdfInfoEntry { logical_name: " << entry.logical_name << ", size: " << entry.size
     << ", contains_gothic_dat: " << entry.contains_gothic_dat << " }";
  return os;
}

struct AddonBundleInfoEntry {
  std::string archive_path;
  std::string archive_sha256;
  std::uint64_t archive_size{0};
  std::string manifest_path;
  std::string manifest_sha256;
  std::uint64_t manifest_size{0};
};

template <typename S>
void serialize(S& s, AddonBundleInfoEntry& entry) {
  s.text1b(entry.archive_path, 255);
  s.text1b(entry.archive_sha256, 64);
  s.value8b(entry.archive_size);
  s.text1b(entry.manifest_path, 255);
  s.text1b(entry.manifest_sha256, 64);
  s.value8b(entry.manifest_size);
}

template <typename S>
void serialize(S& s, ClientResourceInfoEntry& entry) {
  s.text1b(entry.name, 64);
  s.text1b(entry.version, 64);
  s.text1b(entry.manifest_path, 255);
  s.text1b(entry.manifest_sha256, 64);
  s.text1b(entry.archive_path, 255);
  s.text1b(entry.archive_sha256, 64);
  s.value8b(entry.archive_size);
  s.value8b(entry.manifest_size);
}

inline std::ostream& operator<<(std::ostream& os, const ClientResourceInfoEntry& entry) {
  os << "ClientResourceInfoEntry { name: " << entry.name << ", version: " << entry.version << ", manifest_path: " << entry.manifest_path
     << ", archive_path: " << entry.archive_path << ", archive_size: " << entry.archive_size << ", manifest_size: " << entry.manifest_size
     << " }";
  return os;
}

// Server informs the client about the map name and the ID assigned to the player
struct InitialInfoPacket {
  std::uint64_t actor_generation{0};
  std::uint8_t packet_type;
  std::string map_name;
  std::uint32_t player_id;
  std::string server_name;
  std::uint16_t max_slots{0};
  std::uint8_t voice_enabled{0};
  std::uint8_t voice_transmit_enabled{0};
  std::uint32_t voice_range{0};
  std::string resource_token;
  std::string resource_base_path;
  std::string downloader_group;
  std::vector<ClientResourceInfoEntry> client_resources;
  std::vector<AddonVdfInfoEntry> addon_vdfs;
  AddonBundleInfoEntry addon_bundle;
};

template <typename S>
void serialize(S& s, InitialInfoPacket& packet) {
  s.value1b(packet.packet_type);
  s.value8b(packet.actor_generation);
  s.text1b(packet.map_name, 64);
  s.value4b(packet.player_id);
  s.text1b(packet.server_name, 64);
  s.value2b(packet.max_slots);
  s.value1b(packet.voice_enabled);
  s.value1b(packet.voice_transmit_enabled);
  s.value4b(packet.voice_range);
  s.text1b(packet.resource_token, 64);
  s.text1b(packet.resource_base_path, 64);
  s.text1b(packet.downloader_group, 64);
  s.container(packet.client_resources, 128);
  s.container(packet.addon_vdfs, 32);
  s.object(packet.addon_bundle);
}

inline std::ostream& operator<<(std::ostream& os, const InitialInfoPacket& packet) {
  os << "InitialInfoPacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", map_name: " << packet.map_name << ", player_id: " << packet.player_id
     << ", server_name: " << packet.server_name << ", max_slots: " << packet.max_slots
     << ", downloader_group: " << packet.downloader_group << ", resources: " << packet.client_resources.size()
     << ", addon_vdfs: " << packet.addon_vdfs.size()
     << ", voice_enabled: " << static_cast<int>(packet.voice_enabled)
     << ", voice_transmit_enabled: " << static_cast<int>(packet.voice_transmit_enabled)
     << ", voice_range: " << packet.voice_range
     << ", resources: " << packet.client_resources.size() << " }";
  return os;
}

template <>
struct fmt::formatter<InitialInfoPacket> : ostream_formatter {};

struct GameInfoPacket {
  std::uint8_t packet_type;
  std::uint32_t raw_game_time{0};
  float day_length_ms{0.0f};
  std::uint8_t flags{0};
};

template <typename S>
void serialize(S& s, GameInfoPacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.raw_game_time);
  s.value4b(packet.day_length_ms);
  s.value1b(packet.flags);
}

inline std::ostream& operator<<(std::ostream& os, const GameInfoPacket& packet) {
  os << "GameInfoPacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", game_time: " << packet.raw_game_time
     << ", day_length_ms: " << packet.day_length_ms << ", flags: " << static_cast<int>(packet.flags) << " }";
  return os;
}

enum SkySettingsFlags : std::uint8_t {
  SKY_SETTING_WEATHER = 1 << 0,
  SKY_SETTING_RAIN_START = 1 << 1,
  SKY_SETTING_WIND_SCALE = 1 << 2,
  SKY_SETTING_DONT_RAIN = 1 << 3,
  SKY_SETTING_RAIN_WEIGHT = 1 << 4,
  SKY_SETTING_LIGHTNING = 1 << 5,
  SKY_SETTING_RAIN_STOP = 1 << 6,
};

struct SkySettingsPacket {
  std::uint8_t packet_type{0};
  std::uint8_t flags{0};
  std::int32_t weather_type{0};
  std::int16_t rain_start_hour{0};
  std::int16_t rain_start_min{0};
  std::int16_t rain_stop_hour{0};
  std::int16_t rain_stop_min{0};
  float wind_scale{0.0f};
  std::uint8_t dont_rain{0};
  float rain_weight{0.0f};
  std::uint8_t render_lightning{0};
};

template <typename S>
void serialize(S& s, SkySettingsPacket& packet) {
  s.value1b(packet.packet_type);
  s.value1b(packet.flags);
  s.value4b(packet.weather_type);
  s.value2b(packet.rain_start_hour);
  s.value2b(packet.rain_start_min);
  s.value2b(packet.rain_stop_hour);
  s.value2b(packet.rain_stop_min);
  s.value4b(packet.wind_scale);
  s.value1b(packet.dont_rain);
  s.value4b(packet.rain_weight);
  s.value1b(packet.render_lightning);
}

inline std::ostream& operator<<(std::ostream& os, const SkySettingsPacket& packet) {
  os << "SkySettingsPacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", flags: " << static_cast<int>(packet.flags)
     << ", weather_type: " << packet.weather_type
     << ", rain_start_hour: " << packet.rain_start_hour << ", rain_start_min: " << packet.rain_start_min
     << ", rain_stop_hour: " << packet.rain_stop_hour << ", rain_stop_min: " << packet.rain_stop_min
     << ", wind_scale: " << packet.wind_scale << ", dont_rain: " << static_cast<int>(packet.dont_rain)
     << ", rain_weight: " << packet.rain_weight << ", render_lightning: " << static_cast<int>(packet.render_lightning) << " }";
  return os;
}

template <>
struct fmt::formatter<SkySettingsPacket> : ostream_formatter {};

// UI metadata is replicated independently of native actor visibility.
struct PlayerPresenceEntry {
  std::uint32_t player_id{0};
  std::int32_t ping{-1};
  std::string name;
  std::string world;
  glm::vec3 position{0.0f};
  std::uint8_t r{255}, g{255}, b{255};
};

template <typename S>
void serialize(S& s, PlayerPresenceEntry& entry) {
  s.value4b(entry.player_id);
  s.value4b(entry.ping);
  s.text1b(entry.name, 255);
  s.text1b(entry.world, 64);
  s.value4b(entry.position.x);
  s.value4b(entry.position.y);
  s.value4b(entry.position.z);
  s.value1b(entry.r);
  s.value1b(entry.g);
  s.value1b(entry.b);
}

struct PlayerPresencePacket {
  std::uint8_t packet_type{0};
  std::vector<PlayerPresenceEntry> players;
};

template <typename S>
void serialize(S& s, PlayerPresencePacket& packet) {
  s.value1b(packet.packet_type);
  s.container(packet.players, 400);
}

inline std::ostream& operator<<(std::ostream& os, const PlayerPresencePacket& packet) {
  os << "PlayerPresencePacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", players: " << packet.players.size() << " }";
  return os;
}

template <>
struct fmt::formatter<PlayerPresencePacket> : ostream_formatter {};

struct AdminAuthPacket {
  std::uint8_t packet_type{0};
  std::uint8_t authenticated{0};
};

template <typename S>
void serialize(S& s, AdminAuthPacket& packet) {
  s.value1b(packet.packet_type);
  s.value1b(packet.authenticated);
}

constexpr std::size_t kMaxLuaEventPayloadSize = 8192;

struct LuaEventPacket {
  std::uint8_t packet_type{0};
  std::string event_name;
  std::uint32_t source_element{0};
  std::vector<std::uint8_t> payload;
};

template <typename S>
void serialize(S& s, LuaEventPacket& packet) {
  s.value1b(packet.packet_type);
  s.text1b(packet.event_name, 255);
  s.value4b(packet.source_element);
  s.container1b(packet.payload, kMaxLuaEventPayloadSize);
}

inline std::ostream& operator<<(std::ostream& os, const LuaEventPacket& packet) {
  os << "LuaEventPacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", event_name: " << packet.event_name
     << ", source_element: " << packet.source_element << ", payload_size: " << packet.payload.size() << " }";
  return os;
}

template <>
struct fmt::formatter<LuaEventPacket> : ostream_formatter {};
