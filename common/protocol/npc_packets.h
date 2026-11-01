/*
MIT License

Copyright (c) 2026 Gothic Multiplayer Team.

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

#include "net_enums.h"
#include "packets.h"

// NPC commands use reliable ordered channel 0, alongside spawn/despawn. IDs
// are never reused during a server lifetime. An epoch identifies an exact
// host/action assignment; transform updates have a separate state sequence and
// do not restart actions or invalidate their completion reports.
constexpr std::uint32_t kNpcActionPlayAnimation = 1;

struct NpcControlPacket {
  std::uint8_t packet_type{Net::PT_NPC_CONTROL};
  std::uint64_t actor_generation{0};
  std::uint64_t source_time_ms{0};
  std::uint32_t elapsed_ms{0};
  std::uint32_t npc_id{0};
  std::uint32_t control_epoch{0};
  std::uint32_t host_player_id{0};
  std::uint32_t action_id{0};  // zero means idle (or paused without a host)
  std::string animation;
  std::uint32_t timeout_ms{0};
  std::uint32_t state_sequence{0};
  glm::vec3 position{0.0f};
  glm::vec3 normal{0.0f, 0.0f, 1.0f};
};

template <typename S>
void serialize(S& s, NpcControlPacket& packet) {
  s.value1b(packet.packet_type);
  s.value8b(packet.actor_generation);
  s.value8b(packet.source_time_ms);
  s.value4b(packet.elapsed_ms);
  s.value4b(packet.npc_id);
  s.value4b(packet.control_epoch);
  s.value4b(packet.host_player_id);
  s.value4b(packet.action_id);
  s.text1b(packet.animation, kMaxPlayerAnimationNameLength);
  s.value4b(packet.timeout_ms);
  s.value4b(packet.state_sequence);
  s.object(packet.position);
  s.object(packet.normal);
}

// Persistent presentation state is independent of the finite action queue.
// Each actual change receives a new revision; snapshots include the current
// value so late viewers and recreated engine actors do not depend on old events.
struct NpcAnimationPacket {
  std::uint8_t packet_type{Net::PT_NPC_ANIMATION};
  std::uint64_t actor_generation{0};
  std::uint64_t source_time_ms{0};
  std::uint32_t npc_id{0};
  std::uint32_t revision{0};
  std::string animation;
};

template <typename S>
void serialize(S& s, NpcAnimationPacket& packet) {
  s.value1b(packet.packet_type);
  s.value8b(packet.actor_generation);
  s.value8b(packet.source_time_ms);
  s.value4b(packet.npc_id);
  s.value4b(packet.revision);
  s.text1b(packet.animation, kMaxPlayerAnimationNameLength);
}

struct NpcSpawnPacket {
  std::uint8_t packet_type{Net::PT_NPC_SPAWN};
  PlayerSpawnPacket actor{};
  NpcControlPacket control;
  NpcAnimationPacket animation;
};

template <typename S>
void serialize(S& s, NpcSpawnPacket& packet) {
  s.value1b(packet.packet_type);
  s.object(packet.actor);
  s.object(packet.control);
  s.object(packet.animation);
}

struct NpcActionResultPacket {
  std::uint8_t packet_type{Net::PT_NPC_ACTION_RESULT};
  std::uint64_t actor_generation{0};
  std::uint32_t npc_id{0};
  std::uint32_t control_epoch{0};
  std::uint32_t action_id{0};
  std::uint8_t success{0};
};

template <typename S>
void serialize(S& s, NpcActionResultPacket& packet) {
  s.value1b(packet.packet_type);
  s.value8b(packet.actor_generation);
  s.value4b(packet.npc_id);
  s.value4b(packet.control_epoch);
  s.value4b(packet.action_id);
  s.value1b(packet.success);
}
