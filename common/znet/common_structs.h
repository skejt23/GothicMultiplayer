
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

#include <fmt/ostream.h>

#include <cstdint>
#include <glm/glm.hpp>
#include <ostream>
#include <string>

struct PlayerState {
  glm::vec3 position{0.0f};
  glm::vec3 nrot{0.0f};
  std::int16_t left_hand_item_instance{0};
  std::int16_t right_hand_item_instance{0};
  std::int16_t equipped_armor_instance{0};
  std::int16_t animation{0};
  std::int16_t health_points{0};
  std::int16_t mana_points{0};
  std::uint8_t weapon_mode{0};
  std::uint8_t active_spell_nr{0};
  std::uint8_t head_direction{0};
  std::int16_t melee_weapon_instance{0};
  std::int16_t ranged_weapon_instance{0};
};

template <typename S>
void serialize(S& s, PlayerState& packet) {
  s.object(packet.position);
  s.object(packet.nrot);
  s.value2b(packet.left_hand_item_instance);
  s.value2b(packet.right_hand_item_instance);
  s.value2b(packet.equipped_armor_instance);
  s.value2b(packet.animation);
  s.value2b(packet.health_points);
  s.value2b(packet.mana_points);
  s.value1b(packet.weapon_mode);
  s.value1b(packet.active_spell_nr);
  s.value1b(packet.head_direction);
  s.value2b(packet.melee_weapon_instance);
  s.value2b(packet.ranged_weapon_instance);
}

inline std::ostream& operator<<(std::ostream& os, const PlayerState& player_state) {
  os << "PlayerState {"
     << " position: (" << player_state.position.x << ", " << player_state.position.y << ", " << player_state.position.z << "),"
     << " normal_rotation: (" << player_state.nrot.x << ", " << player_state.nrot.y << ", " << player_state.nrot.z << "),"
     << " left_hand_item_instance: " << player_state.left_hand_item_instance << ","
     << " right_hand_item_instance: " << player_state.right_hand_item_instance << ","
     << " equipped_armor_instance: " << player_state.equipped_armor_instance << ","
     << " animation: " << player_state.animation << ","
     << " health_points: " << player_state.health_points << ","
     << " mana_points: " << player_state.mana_points << ","
     << " weapon_mode: " << static_cast<int>(player_state.weapon_mode) << ","
     << " active_spell_nr: " << static_cast<int>(player_state.active_spell_nr) << ","
     << " head_direction: " << static_cast<int>(player_state.head_direction) << ","
     << " melee_weapon_instance: " << player_state.melee_weapon_instance << ","
     << " ranged_weapon_instance: " << player_state.ranged_weapon_instance << " }";
  return os;
}

template <>
struct fmt::formatter<PlayerState> : ostream_formatter {};
