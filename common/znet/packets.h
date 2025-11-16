
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
  std::uint8_t selected_class{0};
  glm::vec3 position{0.0f};
  std::int16_t left_hand_item_instance{0};
  std::int16_t right_hand_item_instance{0};
  std::int16_t equipped_armor_instance{0};
  std::uint8_t head_model{0};
  std::uint8_t skin_texture{0};
  std::uint8_t face_texture{0};
  std::uint8_t walk_style{0};
  std::string player_name;
};

template <typename S>
void serialize(S& s, ExistingPlayerInfo& info) {
  s.value4b(info.player_id);
  s.value1b(info.selected_class);
  s.object(info.position);
  s.value2b(info.left_hand_item_instance);
  s.value2b(info.right_hand_item_instance);
  s.value2b(info.equipped_armor_instance);
  s.value1b(info.head_model);
  s.value1b(info.skin_texture);
  s.value1b(info.face_texture);
  s.value1b(info.walk_style);
  s.text1b(info.player_name, 255);
}

inline std::ostream& operator<<(std::ostream& os, const ExistingPlayerInfo& packet) {
  os << "ExistingPlayerInfo {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", player_id: " << packet.player_id
     << ", selected_class: " << static_cast<int>(packet.selected_class) << ", position: (" << packet.position.x << ", " << packet.position.y << ", "
     << packet.position.z << ")"
     << ", left_hand_item_instance: " << packet.left_hand_item_instance << ", right_hand_item_instance: " << packet.right_hand_item_instance
     << ", equipped_armor_instance: " << packet.equipped_armor_instance << ", head_model: " << static_cast<int>(packet.head_model)
     << ", skin_texture: " << static_cast<int>(packet.skin_texture) << ", face_texture: " << static_cast<int>(packet.face_texture)
     << ", walk_style: " << static_cast<int>(packet.walk_style) << ", player_name: " << packet.player_name << " }";
  return os;
}

template <>
struct fmt::formatter<ExistingPlayerInfo> : ostream_formatter {};

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
  std::uint8_t selected_class{0};
  glm::vec3 position{0.0f};
  glm::vec3 normal{0.0f};
  std::int16_t left_hand_item_instance{0};
  std::int16_t right_hand_item_instance{0};
  std::int16_t equipped_armor_instance{0};
  std::int16_t animation{0};
  std::uint8_t head_model{0};
  std::uint8_t skin_texture{0};
  std::uint8_t face_texture{0};
  std::uint8_t walk_style{0};
  std::string player_name;
  // May be used to identify the player (e.g. when relaying the information about the player to other players)
  std::optional<std::uint32_t> player_id;
};

template <typename S>
void serialize(S& s, JoinGamePacket& packet) {
  s.value1b(packet.packet_type);
  s.value1b(packet.selected_class);
  s.object(packet.position);
  s.object(packet.normal);
  s.value2b(packet.left_hand_item_instance);
  s.value2b(packet.right_hand_item_instance);
  s.value2b(packet.equipped_armor_instance);
  s.value2b(packet.animation);
  s.value1b(packet.head_model);
  s.value1b(packet.skin_texture);
  s.value1b(packet.face_texture);
  s.value1b(packet.walk_style);
  s.text1b(packet.player_name, 255);
  s.ext4b(packet.player_id, bitsery::ext::StdOptional{});
}

inline std::ostream& operator<<(std::ostream& os, const JoinGamePacket& packet) {
  os << "JoinGamePacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", selected_class: " << static_cast<int>(packet.selected_class) << ", position: ("
     << packet.position.x << ", " << packet.position.y << ", " << packet.position.z << ")"
     << ", normal: (" << packet.normal.x << ", " << packet.normal.y << ", " << packet.normal.z << ")"
     << ", left_hand_item_instance: " << packet.left_hand_item_instance << ", right_hand_item_instance: " << packet.right_hand_item_instance
     << ", equipped_armor_instance: " << packet.equipped_armor_instance << ", animation: " << packet.animation
     << ", head_model: " << static_cast<int>(packet.head_model) << ", skin_texture: " << static_cast<int>(packet.skin_texture)
     << ", face_texture: " << static_cast<int>(packet.face_texture) << ", walk_style: " << static_cast<int>(packet.walk_style)
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
  std::uint8_t selected_class{0};
  glm::vec3 position{0.0f};
  glm::vec3 normal{0.0f};
  std::int16_t left_hand_item_instance{0};
  std::int16_t right_hand_item_instance{0};
  std::int16_t equipped_armor_instance{0};
  std::int16_t animation{0};
  std::uint8_t head_model{0};
  std::uint8_t skin_texture{0};
  std::uint8_t face_texture{0};
  std::uint8_t walk_style{0};
  std::string player_name;
};

template <typename S>
void serialize(S& s, PlayerSpawnPacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.player_id);
  s.value1b(packet.selected_class);
  s.object(packet.position);
  s.object(packet.normal);
  s.value2b(packet.left_hand_item_instance);
  s.value2b(packet.right_hand_item_instance);
  s.value2b(packet.equipped_armor_instance);
  s.value2b(packet.animation);
  s.value1b(packet.head_model);
  s.value1b(packet.skin_texture);
  s.value1b(packet.face_texture);
  s.value1b(packet.walk_style);
  s.text1b(packet.player_name, 255);
}

inline std::ostream& operator<<(std::ostream& os, const PlayerSpawnPacket& packet) {
  os << "PlayerSpawnPacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type)
     << ", player_id: " << packet.player_id
     << ", selected_class: " << static_cast<int>(packet.selected_class)
     << ", position: (" << packet.position.x << ", " << packet.position.y << ", " << packet.position.z << ")"
     << ", normal: (" << packet.normal.x << ", " << packet.normal.y << ", " << packet.normal.z << ")"
     << ", left_hand_item_instance: " << packet.left_hand_item_instance
     << ", right_hand_item_instance: " << packet.right_hand_item_instance
     << ", equipped_armor_instance: " << packet.equipped_armor_instance
     << ", animation: " << packet.animation
     << ", head_model: " << static_cast<int>(packet.head_model)
     << ", skin_texture: " << static_cast<int>(packet.skin_texture)
     << ", face_texture: " << static_cast<int>(packet.face_texture)
     << ", walk_style: " << static_cast<int>(packet.walk_style)
     << ", player_name: " << packet.player_name << " }";
  return os;
}

template <>
struct fmt::formatter<PlayerSpawnPacket> : ostream_formatter {};

struct MessagePacket {
  std::uint8_t packet_type;
  std::string message;
  std::optional<std::uint32_t> sender;
  std::optional<std::uint32_t> recipient;
};

template <typename S>
void serialize(S& s, MessagePacket& packet) {
  s.value1b(packet.packet_type);
  s.text1b(packet.message, 1024);
  s.ext4b(packet.sender, bitsery::ext::StdOptional{});
  s.ext4b(packet.recipient, bitsery::ext::StdOptional{});
}

inline std::ostream& operator<<(std::ostream& os, const MessagePacket& packet) {
  os << "MessagePacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", message: " << packet.message;
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

struct HPDiffPacket {
  std::uint8_t packet_type;
  std::uint32_t player_id;
  std::int16_t hp_difference;
};

template <typename S>
void serialize(S& s, HPDiffPacket& packet) {
  s.value1b(packet.packet_type);
  s.value4b(packet.player_id);
  s.value2b(packet.hp_difference);
}

inline std::ostream& operator<<(std::ostream& os, const HPDiffPacket& packet) {
  os << "HPDiffPacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", player_id: " << packet.player_id << ", hp_difference: " << packet.hp_difference
     << " }";
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

// Server informs the client about the map name and the ID assigned to the player
struct InitialInfoPacket {
  std::uint8_t packet_type;
  std::string map_name;
  std::uint32_t player_id;
};

template <typename S>
void serialize(S& s, InitialInfoPacket& packet) {
  s.value1b(packet.packet_type);
  s.text1b(packet.map_name, 64);
  s.value4b(packet.player_id);
}

inline std::ostream& operator<<(std::ostream& os, const InitialInfoPacket& packet) {
  os << "InitialInfoPacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", map_name: " << packet.map_name << ", player_id: " << packet.player_id << " }";
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

struct DiscordActivityPacket {
  std::uint8_t packet_type;
  std::string state;
  std::string details;
  std::string large_image_key;
  std::string large_image_text;
  std::string small_image_key;
  std::string small_image_text;
};

template <typename S>
void serialize(S& s, DiscordActivityPacket& packet) {
  s.value1b(packet.packet_type);
  s.text1b(packet.state, 128);
  s.text1b(packet.details, 128);
  s.text1b(packet.large_image_key, 32);
  s.text1b(packet.large_image_text, 128);
  s.text1b(packet.small_image_key, 32);
  s.text1b(packet.small_image_text, 128);
}

inline std::ostream& operator<<(std::ostream& os, const DiscordActivityPacket& packet) {
  os << "DiscordActivityPacket {"
     << " packet_type: " << static_cast<int>(packet.packet_type) << ", state: '" << packet.state << "', details: '" << packet.details
     << "', large_image_key: '" << packet.large_image_key << "', large_image_text: '" << packet.large_image_text << "', small_image_key: '"
     << packet.small_image_key << "', small_image_text: '" << packet.small_image_text << "' }";
  return os;
}

template <>
struct fmt::formatter<DiscordActivityPacket> : ostream_formatter {};