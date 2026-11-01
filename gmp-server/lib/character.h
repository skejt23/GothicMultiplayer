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

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <chrono>
#include <ctime>
#include <deque>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "common_structs.h"

// State shared by connected players and server-owned NPCs. A character has
// viewers, but neither a network connection nor a set of entities it streams.
struct Character {
  using CharacterId = std::uint32_t;

  // Retain the existing name for compatibility with player packets and APIs.
  CharacterId player_id{0};
  // Allocated independently of reusable player slots; never reset on reconnect.
  std::uint64_t actor_generation{AllocateGeneration()};
  std::uint32_t command_revision{0};
  // Native jumps may follow unacknowledged mana updates, but must not replace
  // a newer scripted position or heading command.
  std::optional<std::uint32_t> native_teleport_barrier_revision;
  std::string name;
  std::string instance;
  std::uint8_t name_color_r{255};
  std::uint8_t name_color_g{255};
  std::uint8_t name_color_b{255};
  std::string world;
  std::int32_t virtual_world{0};
  std::unordered_set<CharacterId> streamed_by_players;

  std::string body_model;
  std::int16_t body_texture{0};
  std::string head_model;
  std::int16_t head_texture{0};
  std::int16_t teeth_texture{0};
  std::int16_t skin_color{0};
  float fatness{1.0f};
  glm::vec3 scale{1.0f, 1.0f, 1.0f};
  std::vector<std::string> overlays;

  std::uint8_t flags{0};
  std::uint8_t walkstyle{0};
  std::uint8_t fight_pos{0};
  std::uint8_t spellhand{0};
  std::uint8_t headstate{0};
  std::uint8_t is_ingame{0};

  std::int32_t health{0};
  std::int32_t max_health{100};
  std::int32_t mana{0};
  std::int32_t max_mana{100};
  std::int32_t level{0};
  std::int32_t exp{0};
  std::int32_t next_level_exp{0};
  std::int32_t learn_points{0};
  std::int32_t strength{0};
  std::int32_t dexterity{0};
  bool invulnerable{false};
  std::unordered_map<std::string, std::int32_t> inventory;

  std::int32_t InventoryCount(const std::string& instance) const {
    const auto it = inventory.find(instance);
    return it == inventory.end() ? 0 : std::max<std::int32_t>(0, it->second);
  }

  bool AddInventory(const std::string& instance, std::int32_t amount) {
    if (!gmp::IsValidScriptName(instance) || amount <= 0 || InventoryCount(instance) > std::numeric_limits<std::int32_t>::max() - amount) return false;
    if (!inventory.contains(instance)) {
      if (inventory.size() >= kMaxInventoryEntries) return false;
      std::size_t name_bytes = instance.size();
      for (const auto& [name, _] : inventory) name_bytes += name.size();
      if (name_bytes > kMaxInventoryNameBytes) return false;
    }
    inventory[instance] = InventoryCount(instance) + amount;
    return true;
  }

  // An inventory transaction either transfers the whole requested quantity or
  // leaves ownership unchanged. Callers can explicitly clamp administrative removals.
  bool RemoveInventory(const std::string& instance, std::int32_t amount) {
    const auto count = InventoryCount(instance);
    if (instance.empty() || amount <= 0 || amount > count) return false;
    if (amount == count) inventory.erase(instance);
    else inventory[instance] = count - amount;
    return true;
  }
  std::unordered_map<int, int> weapon_skills;
  std::unordered_map<int, int> talents;

  std::time_t tod{0};
  std::optional<std::chrono::steady_clock::time_point> death_started_at;
  std::optional<std::int32_t> respawn_time_ms;
  PlayerState state{};
  struct InstanceChange {
    std::uint32_t request_revision{0};
    std::chrono::steady_clock::time_point deadline{std::chrono::steady_clock::now() + std::chrono::seconds(30)};
    std::string previous_instance;
    std::unordered_map<int, int> attributes;
    std::unordered_map<int, int> weapon_skills, talents;
    std::unordered_map<std::string, bool> overlays;
    std::unordered_map<std::string, std::int64_t> inventory_deltas;
    PlayerState previous_state;
    bool visual_changed{false}, fatness_changed{false}, scale_changed{false}, invulnerable_changed{false}, weapon_mode_changed{false};
  };
  std::optional<InstanceChange> instance_change;
  struct Transformation {
    std::uint16_t spell_id{0};
    std::uint32_t cast_id{0};
    std::string instance;
    std::int32_t max_health{0}, max_mana{0}, mana{0}, strength{0}, dexterity{0};
    std::int32_t health{0}, form_health{0}, form_max_health{0};
    PlayerState equipment;
  };
  std::optional<Transformation> transformation;
  std::uint32_t state_sequence{0};
  std::optional<std::string> pending_equipped_armor_instance;
  std::optional<std::string> pending_equipped_helmet_instance;
  std::optional<std::string> pending_equipped_shield_instance;
  std::optional<std::string> pending_melee_weapon_instance;
  std::optional<std::string> pending_ranged_weapon_instance;
  std::optional<std::string> pending_equipped_amulet_instance;
  std::optional<std::string> pending_equipped_belt_instance;
  std::optional<std::string> pending_equipped_ring_left_instance;
  std::optional<std::string> pending_equipped_ring_right_instance;
  std::vector<std::string> generic_equipment_requests;
  struct EquipmentCommand {
    std::uint32_t command_revision{0};
    std::string expected_instance;
    std::string previous_instance;
    std::chrono::steady_clock::time_point deadline;
  };
  std::unordered_map<std::int16_t, EquipmentCommand> pending_equipment_commands;
  std::unordered_map<std::int16_t, std::uint32_t> equipment_command_revisions;
  std::optional<std::uint32_t> last_actor_action_id;
  std::optional<std::uint32_t> last_cast_action_id;
  std::optional<std::uint32_t> last_teleport_action_id;
  std::optional<std::uint32_t> last_hit_action_id;
  std::unordered_set<CharacterId> hit_victims;
  std::chrono::steady_clock::time_point last_attack_at{};
  std::chrono::steady_clock::time_point last_cast_at{};
  std::chrono::steady_clock::time_point last_item_use_at{};
  std::chrono::steady_clock::time_point action_window_started{};
  std::uint32_t actions_in_window{0};
  std::unordered_map<std::string, std::chrono::steady_clock::time_point> timed_overlays;
  std::optional<CharacterId> last_validated_attacker;
  std::chrono::steady_clock::time_point last_damage_at{};
  struct ApprovedAttack {
    std::uint32_t action_id{0};
    std::int32_t damage_cap{0};
    std::uint32_t damage_types{0};
    std::uint32_t range{0};
    std::chrono::steady_clock::time_point expires_at{};
    std::unordered_set<CharacterId> victims;
    std::uint32_t max_victims{1};
    glm::vec3 origin{0.0f};
  };
  std::deque<ApprovedAttack> approved_projectiles;
  std::uint32_t combat_barrier_revision{0};
  bool CombatRevisionIsCurrent(std::uint32_t revision) const {
    return (revision == command_revision || IsNewActorSequence(command_revision, revision)) &&
           (!combat_barrier_revision || revision == combat_barrier_revision || IsNewActorSequence(revision, combat_barrier_revision));
  }

  static std::uint64_t AllocateGeneration() {
    static std::atomic<std::uint64_t> next{1};
    return next.fetch_add(1, std::memory_order_relaxed);
  }

  void RenewGeneration() {
    actor_generation = AllocateGeneration();
    instance_change.reset();
    last_teleport_action_id.reset();
    native_teleport_barrier_revision.reset();
    approved_projectiles.clear();
    combat_barrier_revision = 0;
    last_validated_attacker.reset();
    pending_equipment_commands.clear();
    pending_equipped_armor_instance.reset();
    pending_equipped_helmet_instance.reset();
    pending_equipped_shield_instance.reset();
    pending_melee_weapon_instance.reset();
    pending_ranged_weapon_instance.reset();
    pending_equipped_amulet_instance.reset();
    pending_equipped_belt_instance.reset();
    pending_equipped_ring_left_instance.reset();
    pending_equipped_ring_right_instance.reset();
  }
};
