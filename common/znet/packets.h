
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

#include <cstdint>
#include <glm/glm.hpp>
#include <optional>
#include <string>

#include "common_structs.h"

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
  glm::vec3 position{0.0f};
  std::int16_t left_hand_item_instance{0};
  std::int16_t right_hand_item_instance{0};
  std::int16_t equipped_armor_instance{0};
  std::string body_model;
  std::uint16_t body_texture{0};
  std::string head_model;
  std::uint16_t head_texture{0};
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
  s.object(info.position);
  s.value2b(info.left_hand_item_instance);
  s.value2b(info.right_hand_item_instance);
  s.value2b(info.equipped_armor_instance);
  s.text1b(info.body_model, 255);
  s.value2b(info.body_texture);
  s.text1b(info.head_model, 255);
  s.value2b(info.head_texture);
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

  s.value4b(info.fatness);
  s.object(info.scale);

  s.container(info.weapon_skills, 128);
  s.container(info.talents, 128);
  s.container(info.overlays, 64, [](S& s, std::string& overlay) { s.text1b(overlay, 255); });
}

inline std::ostream& operator<<(std::ostream& os, const ExistingPlayerInfo& packet) {
  os << "ExistingPlayerInfo {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", player_id: " << packet.player_id << ", position: (" << packet.position.x
     << ", " << packet.position.y << ", " << packet.position.z << ")"
     << ", left_hand_item_instance: " << packet.left_hand_item_instance << ", right_hand_item_instance: " << packet.right_hand_item_instance
     << ", equipped_armor_instance: " << packet.equipped_armor_instance 
     << ", body_model: " << packet.body_model << ", body_texture: " << static_cast<int>(packet.body_texture)
     << ", head_model: " << packet.head_model << ", head_texture: " << static_cast<int>(packet.head_texture)
     << ", walk_style: " << static_cast<int>(packet.walk_style) << ", player_name: " << packet.player_name
     << ", instance: " << packet.instance
     << ", name_color: (" << static_cast<int>(packet.name_color_r) << ", " << static_cast<int>(packet.name_color_g) << ", "
     << static_cast<int>(packet.name_color_b) << ")"
     << ", strength: " << packet.strength << ", dexterity: " << packet.dexterity << ", level: " << packet.level
     << ", exp: " << packet.exp << ", next_level_exp: " << packet.next_level_exp << ", learn_points: " << packet.learn_points
     << ", health: " << packet.health << ", max_health: " << packet.max_health << ", mana: " << packet.mana
     << ", max_mana: " << packet.max_mana << ", fatness: " << packet.fatness
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
  std::uint8_t packet_type{0};
  std::uint32_t player_id{0};
  std::string instance;
};

template <typename S>
void serialize(S& s, PlayerInstanceUpdatePacket& packet) {
  s.value1b(packet.packet_type);
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
};

template <typename S>
void serialize(S& s, PlayerVisualUpdatePacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.player_id);
  s.text1b(packet.body_model, 255);
  s.value2b(packet.body_texture);
  s.text1b(packet.head_model, 255);
  s.value2b(packet.head_texture);
}

inline std::ostream& operator<<(std::ostream& os, const PlayerVisualUpdatePacket& packet) {
  os << "PlayerVisualUpdatePacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", player_id: " << packet.player_id
     << ", body_model: " << packet.body_model << ", body_texture: " << packet.body_texture 
     << ", head_model: " << packet.head_model << ", head_texture: " << packet.head_texture << " }";
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
  std::uint8_t packet_type{0};
  std::uint32_t player_id{0};
  PlayerAttributeId attribute_id{0};
  std::int32_t value{0};
};

template <typename S>
void serialize(S& s, PlayerAttributeUpdatePacket& packet) {
  s.value1b(packet.packet_type);
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
  std::uint8_t packet_type{0};
  std::uint32_t player_id{0};
  std::string world_name;
  std::string start_point;
};

template <typename S>
void serialize(S& s, PlayerWorldUpdatePacket& packet) {
  s.value1b(packet.packet_type);
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
  std::uint8_t packet_type{0};
  glm::vec3 position{0.0f};
  glm::vec3 normal{0.0f};
  std::int16_t left_hand_item_instance{0};
  std::int16_t right_hand_item_instance{0};
  std::int16_t equipped_armor_instance{0};
  std::int16_t animation{0};
  std::string body_model;
  std::uint16_t body_texture{0};
  std::string head_model;
  std::uint16_t head_texture{0};
  std::uint8_t walk_style{0};
  std::string player_name;
  // May be used to identify the player (e.g. when relaying the information about the player to other players)
  std::optional<std::uint32_t> player_id;
};

template <typename S>
void serialize(S& s, JoinGamePacket& packet) {
  s.value1b(packet.packet_type);
  s.object(packet.position);
  s.object(packet.normal);
  s.value2b(packet.left_hand_item_instance);
  s.value2b(packet.right_hand_item_instance);
  s.value2b(packet.equipped_armor_instance);
  s.value2b(packet.animation);
  s.text1b(packet.body_model, 255);
  s.value2b(packet.body_texture);
  s.text1b(packet.head_model, 255);
  s.value2b(packet.head_texture);
  s.value1b(packet.walk_style);
  s.text1b(packet.player_name, 255);
  s.ext4b(packet.player_id, bitsery::ext::StdOptional{});
}

inline std::ostream& operator<<(std::ostream& os, const JoinGamePacket& packet) {
  os << "JoinGamePacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", position: (" << packet.position.x << ", " << packet.position.y << ", "
     << packet.position.z << ")"
     << ", normal: (" << packet.normal.x << ", " << packet.normal.y << ", " << packet.normal.z << ")"
     << ", left_hand_item_instance: " << packet.left_hand_item_instance << ", right_hand_item_instance: " << packet.right_hand_item_instance
     << ", equipped_armor_instance: " << packet.equipped_armor_instance << ", animation: " << packet.animation
     << ", body_model: " << packet.body_model << ", body_texture: " << static_cast<int>(packet.body_texture)
     << ", head_model: " << packet.head_model << ", head_texture: " << static_cast<int>(packet.head_texture) 
     << ", walk_style: " << static_cast<int>(packet.walk_style)
     << ", player_name: " << packet.player_name;

  if (packet.player_id.has_value()) {
    os << ", player_id: " << packet.player_id.value();
  }

  os << " }";
  return os;
}

template <>
struct fmt::formatter<JoinGamePacket> : ostream_formatter {};

struct PlayerSpawnPacket {
  std::uint8_t packet_type{0};
  std::uint32_t player_id{0};
  glm::vec3 position{0.0f};
  glm::vec3 normal{0.0f};
  std::int16_t left_hand_item_instance{0};
  std::int16_t right_hand_item_instance{0};
  std::int16_t equipped_armor_instance{0};
  std::int16_t animation{0};
  std::string body_model;
  std::uint16_t body_texture{0};
  std::string head_model;
  std::uint16_t head_texture{0};
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
  s.value4b(packet.player_id);
  s.object(packet.position);
  s.object(packet.normal);
  s.value2b(packet.left_hand_item_instance);
  s.value2b(packet.right_hand_item_instance);
  s.value2b(packet.equipped_armor_instance);
  s.value2b(packet.animation);
  s.text1b(packet.body_model, 255);
  s.value2b(packet.body_texture);
  s.text1b(packet.head_model, 255);
  s.value2b(packet.head_texture);
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

  s.value4b(packet.fatness);
  s.object(packet.scale);

  s.container(packet.weapon_skills, 128);
  s.container(packet.talents, 128);
  s.container(packet.overlays, 64, [](S& s, std::string& overlay) { s.text1b(overlay, 255); });
}

inline std::ostream& operator<<(std::ostream& os, const PlayerSpawnPacket& packet) {
  os << "PlayerSpawnPacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", player_id: " << packet.player_id << ", position: (" << packet.position.x
     << ", " << packet.position.y << ", " << packet.position.z << ")"
     << ", normal: (" << packet.normal.x << ", " << packet.normal.y << ", " << packet.normal.z << ")"
     << ", left_hand_item_instance: " << packet.left_hand_item_instance << ", right_hand_item_instance: " << packet.right_hand_item_instance
     << ", equipped_armor_instance: " << packet.equipped_armor_instance << ", animation: " << packet.animation
     << ", body_model: " << packet.body_model << ", body_texture: " << static_cast<int>(packet.body_texture)
     << ", head_model: " << packet.head_model << ", head_texture: " << static_cast<int>(packet.head_texture) 
     << ", walk_style: " << static_cast<int>(packet.walk_style)
     << ", player_name: " << packet.player_name
     << ", instance: " << packet.instance
     << ", name_color: (" << static_cast<int>(packet.name_color_r) << ", " << static_cast<int>(packet.name_color_g) << ", "
     << static_cast<int>(packet.name_color_b) << ")"
     << ", strength: " << packet.strength << ", dexterity: " << packet.dexterity << ", level: " << packet.level
     << ", exp: " << packet.exp << ", next_level_exp: " << packet.next_level_exp << ", learn_points: " << packet.learn_points
     << ", health: " << packet.health << ", max_health: " << packet.max_health << ", mana: " << packet.mana
     << ", max_mana: " << packet.max_mana << ", fatness: " << packet.fatness
     << ", scale: (" << packet.scale.x << ", " << packet.scale.y << ", " << packet.scale.z << ")"
     << ", weapon_skills: " << packet.weapon_skills.size() << ", talents: " << packet.talents.size() << ", overlays: "
     << packet.overlays.size() << " }";
  return os;
}

template <>
struct fmt::formatter<PlayerSpawnPacket> : ostream_formatter {};

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
  s.text1b(packet.message, 1024);
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
  std::uint8_t packet_type;
  std::uint16_t spell_id;
  std::optional<std::uint32_t> target_id;
  std::optional<std::uint32_t> caster_id;
};

template <typename S>
void serialize(S& s, CastSpellPacket& packet) {
  s.value1b(packet.packet_type);
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
  std::uint8_t packet_type;
  std::int16_t item_instance;
  std::int16_t item_amount;
  std::optional<std::uint32_t> player_id;
};

template <typename S>
void serialize(S& s, DropItemPacket& packet) {
  s.value1b(packet.packet_type);
  s.value2b(packet.item_instance);
  s.value2b(packet.item_amount);
  s.ext4b(packet.player_id, bitsery::ext::StdOptional{});
}

inline std::ostream& operator<<(std::ostream& os, const DropItemPacket& packet) {
  os << "DropItemPacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", item_instance: " << packet.item_instance
     << ", item_amount: " << packet.item_amount << " }";

  if (packet.player_id) {
    os << ", player_id: " << *packet.player_id;
  }
  return os;
}

struct TakeItemPacket {
  std::uint8_t packet_type;
  std::int16_t item_instance;
  std::optional<std::uint32_t> player_id;
};

template <typename S>
void serialize(S& s, TakeItemPacket& packet) {
  s.value1b(packet.packet_type);
  s.value2b(packet.item_instance);
  s.ext4b(packet.player_id, bitsery::ext::StdOptional{});
}

inline std::ostream& operator<<(std::ostream& os, const TakeItemPacket& packet) {
  os << "TakeItemPacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", item_instance: " << packet.item_instance << " }";

  if (packet.player_id) {
    os << ", player_id: " << *packet.player_id;
  }
  return os;
}

struct GiveItemPacket {
  std::uint8_t packet_type;
  std::string item_instance;
  std::int32_t item_amount;
  std::uint32_t player_id;
};

template <typename S>
void serialize(S& s, GiveItemPacket& packet) {
  s.value1b(packet.packet_type);
  s.text1b(packet.item_instance, 255);
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
  std::uint8_t packet_type;
  std::string item_instance;
  std::int16_t slot_id;
  std::uint32_t player_id;
};

template <typename S>
void serialize(S& s, EquipItemPacket& packet) {
  s.value1b(packet.packet_type);
  s.text1b(packet.item_instance, 255);
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
  std::uint8_t packet_type;
  std::string item_instance;
  std::uint32_t player_id;
};

template <typename S>
void serialize(S& s, UnequipItemPacket& packet) {
  s.value1b(packet.packet_type);
  s.text1b(packet.item_instance, 255);
  s.value4b(packet.player_id);
}

inline std::ostream& operator<<(std::ostream& os, const UnequipItemPacket& packet) {
  os << "UnequipItemPacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", item_instance: " << packet.item_instance
     << ", player_id: " << packet.player_id << " }";
  return os;
}

struct PlayerStateUpdatePacket {
  std::uint8_t packet_type;
  PlayerState state;
  // May be used to identify the player (e.g. when relaying the information about the player to other players)
  std::optional<std::uint32_t> player_id;
};

template <typename S>
void serialize(S& s, PlayerStateUpdatePacket& packet) {
  s.value1b(packet.packet_type);
  s.object(packet.state);
  s.ext4b(packet.player_id, bitsery::ext::StdOptional{});
}

inline std::ostream& operator<<(std::ostream& os, const PlayerStateUpdatePacket& packet) {
  os << "PlayerStateUpdatePacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ","
     << " state: " << packet.state << " }";
  if (packet.player_id.has_value()) {
    os << ", player_id: " << packet.player_id.value();
  }
  return os;
}

template <>
struct fmt::formatter<PlayerStateUpdatePacket> : ostream_formatter {};

struct PlayerPositionUpdatePacket {
  std::uint8_t packet_type;
  glm::vec3 position;
  // May be used to identify the player (e.g. when relaying the information about the player to other players)
  std::optional<std::uint32_t> player_id;
};

template <typename S>
void serialize(S& s, PlayerPositionUpdatePacket& packet) {
  s.value1b(packet.packet_type);
  s.object(packet.position);
  s.ext4b(packet.player_id, bitsery::ext::StdOptional{});
}

inline std::ostream& operator<<(std::ostream& os, const PlayerPositionUpdatePacket& packet) {
  os << "PlayerPositionUpdatePacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ","
     << " position: (" << packet.position.x << ", " << packet.position.y << ", " << packet.position.z << ")";

  if (packet.player_id.has_value()) {
    os << ", player_id: " << packet.player_id.value();
  }
  return os;
}

struct VoicePacket {
  std::uint8_t packet_type;
  std::uint32_t voice_data_size;
  std::vector<std::uint8_t> voice_data;
};

template <typename S>
void serialize(S& s, VoicePacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.voice_data_size);
  s.container1b(packet.voice_data, packet.voice_data_size);
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
  std::uint8_t packet_type;
  std::uint32_t player_id;
};

template <typename S>
void serialize(S& s, PlayerDeathInfoPacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.player_id);
}

inline std::ostream& operator<<(std::ostream& os, const PlayerDeathInfoPacket& info) {
  os << "PlayerDeathInfo {"
     << " packet_type: " << static_cast<int>(info.packet_type) << ", player_id: " << info.player_id << " }";
  return os;
}

struct PlayerRespawnInfoPacket {
  std::uint8_t packet_type;
  std::uint32_t player_id;
};

template <typename S>
void serialize(S& s, PlayerRespawnInfoPacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.player_id);
}

inline std::ostream& operator<<(std::ostream& os, const PlayerRespawnInfoPacket& info) {
  os << "PlayerRespawnInfo {"
     << " packet_type: " << static_cast<int>(info.packet_type) << ", player_id: " << info.player_id << " }";
  return os;
}

struct ClientResourceInfoEntry {
  std::string name;
  std::string version;
  std::string manifest_path;
  std::string manifest_sha256;
  std::string archive_path;
  std::string archive_sha256;
  std::uint64_t archive_size{0};
};

template <typename S>
void serialize(S& s, ClientResourceInfoEntry& entry) {
  s.text1b(entry.name, 64);
  s.text1b(entry.version, 64);
  s.text1b(entry.manifest_path, 255);
  s.text1b(entry.manifest_sha256, 64);
  s.text1b(entry.archive_path, 255);
  s.text1b(entry.archive_sha256, 64);
  s.value8b(entry.archive_size);
}

inline std::ostream& operator<<(std::ostream& os, const ClientResourceInfoEntry& entry) {
  os << "ClientResourceInfoEntry { name: " << entry.name << ", version: " << entry.version << ", manifest_path: " << entry.manifest_path
     << ", archive_path: " << entry.archive_path << ", archive_size: " << entry.archive_size << " }";
  return os;
}

// Server informs the client about the map name and the ID assigned to the player
struct InitialInfoPacket {
  std::uint8_t packet_type;
  std::string map_name;
  std::uint32_t player_id;
  std::string server_name;
  std::uint16_t max_slots{0};
  std::string resource_token;
  std::string resource_base_path;
  std::vector<ClientResourceInfoEntry> client_resources;
};

template <typename S>
void serialize(S& s, InitialInfoPacket& packet) {
  s.value1b(packet.packet_type);
  s.text1b(packet.map_name, 64);
  s.value4b(packet.player_id);
  s.text1b(packet.server_name, 64);
  s.value2b(packet.max_slots);
  s.text1b(packet.resource_token, 64);
  s.text1b(packet.resource_base_path, 64);
  s.container(packet.client_resources, 128);
}

inline std::ostream& operator<<(std::ostream& os, const InitialInfoPacket& packet) {
  os << "InitialInfoPacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", map_name: " << packet.map_name << ", player_id: " << packet.player_id
     << ", server_name: " << packet.server_name << ", max_slots: " << packet.max_slots
     << ", resources: " << packet.client_resources.size() << " }";
  return os;
}

template <>
struct fmt::formatter<InitialInfoPacket> : ostream_formatter {};

struct GameInfoPacket {
  std::uint8_t packet_type;
  std::uint32_t raw_game_time{0};
  std::uint8_t flags{0};
};

template <typename S>
void serialize(S& s, GameInfoPacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.raw_game_time);
  s.value1b(packet.flags);
}

inline std::ostream& operator<<(std::ostream& os, const GameInfoPacket& packet) {
  os << "GameInfoPacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", game_time: " << packet.raw_game_time
     << ", flags: " << static_cast<int>(packet.flags) << " }";
  return os;
}