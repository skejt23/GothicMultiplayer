
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

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <glm/glm.hpp>
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>
#include <string>
#include <vector>
#include <utility>

#include "common_structs.h"

namespace gmp::client {

class Player {
public:
  Player() = default;
  virtual ~Player() = default;

  // Core identity
  std::uint64_t id() const {
    return id_;
  }
  void set_id(std::uint64_t id) {
    id_ = id;
  }

  std::uint64_t actor_generation() const { return actor_generation_; }
  // Generation changes invalidate every sequence, pending action and cached
  // presentation revision belonging to the previous native actor.
  void set_actor_generation(std::uint64_t generation) {
    if (actor_generation_ == generation) return;
    actor_generation_ = generation;
    instance_change_revision_ = 0;
    component_sequences_.Reset();
    has_state_sequence_ = false;
    has_lifecycle_sequence_ = false;
    last_action_sequence_ = 0;
    last_damage_sequence_.reset();
    summon_notified_ = false;
    transformation_spell_id_ = 0;
    transformation_cast_id_ = 0;
    transformation_sequence_.reset();
    command_revision_ = 0;
    has_command_revision_ = false;
    source_time_ms_ = 0;
    npc_control_epoch_ = npc_action_id_ = npc_animation_revision_ = 0;
    attribute_initialized_.fill(false);
    inventory_sequences_.clear();
    inventory_snapshot_sequence_.reset();
    latest_inventory_sequence_.reset();
    life_command_revision_.reset();
  }
  bool matches_generation(std::uint64_t generation) const {
    return generation != 0 && actor_generation_ == generation;
  }
  std::uint32_t instance_change_revision() const { return instance_change_revision_; }
  void set_instance_change_revision(std::uint32_t revision) { instance_change_revision_ = revision; }
  bool accept_summon_notification() {
    if (summon_notified_) return false;
    summon_notified_ = true;
    return true;
  }
  std::uint16_t transformation_spell_id() const { return transformation_spell_id_; }
  std::uint32_t transformation_cast_id() const { return transformation_cast_id_; }
  void set_transformation(std::uint16_t spell, std::uint32_t cast) {
    transformation_spell_id_ = spell;
    transformation_cast_id_ = cast;
  }
  bool accept_transformation_sequence(std::uint32_t sequence) {
    if (transformation_sequence_ && !IsNewActorSequence(sequence, *transformation_sequence_)) return false;
    transformation_sequence_ = sequence;
    return true;
  }
  const ActorComponentSequences& component_sequences() const { return component_sequences_; }
  bool accept_inventory_sequence(const std::string& instance, std::uint32_t sequence) {
    if (inventory_snapshot_sequence_ && !IsNewActorSequence(sequence, *inventory_snapshot_sequence_)) return false;
    auto found = inventory_sequences_.find(instance);
    if (found != inventory_sequences_.end() && !IsNewActorSequence(sequence, found->second)) return false;
    inventory_sequences_[instance] = sequence;
    if (!latest_inventory_sequence_ || IsNewActorSequence(sequence, *latest_inventory_sequence_)) latest_inventory_sequence_ = sequence;
    return true;
  }
  const std::unordered_map<std::string, std::int32_t>& inventory() const { return inventory_; }
  std::int32_t inventory_count(const std::string& instance) const {
    const auto found = inventory_.find(instance);
    return found == inventory_.end() ? 0 : found->second;
  }
  void set_inventory_count(const std::string& instance, std::int32_t amount) {
    if (!gmp::IsValidScriptName(instance)) return;
    if (amount > 0) inventory_[instance] = amount;
    else inventory_.erase(instance);
  }
  void apply_inventory_delta(const std::string& instance, std::int32_t delta) {
    set_inventory_count(instance, static_cast<std::int32_t>(std::clamp<std::int64_t>(
        static_cast<std::int64_t>(inventory_count(instance)) + delta, 0, std::numeric_limits<std::int32_t>::max())));
  }
  bool begin_inventory_snapshot(std::uint32_t sequence) {
    if (latest_inventory_sequence_ && IsNewActorSequence(*latest_inventory_sequence_, sequence)) return false;
    inventory_.clear();
    inventory_sequences_.clear();
    inventory_snapshot_sequence_ = latest_inventory_sequence_ = sequence;
    return true;
  }
  void reset_inventory_snapshot() {
    inventory_.clear();
    inventory_sequences_.clear();
    inventory_snapshot_sequence_.reset();
    latest_inventory_sequence_.reset();
  }
  std::uint32_t command_revision() const { return command_revision_; }
  void set_command_revision(std::uint32_t revision) {
    if (!has_command_revision_ || revision == command_revision_ || IsNewActorSequence(revision, command_revision_)) {
      command_revision_ = revision;
      has_command_revision_ = true;
    }
  }
  std::uint64_t source_time_ms() const { return source_time_ms_; }
  void set_source_time_ms(std::uint64_t time) { source_time_ms_ = time; }
  std::uint16_t accept_components(std::uint16_t components, std::uint32_t sequence, bool allow_equal = false) {
    const auto accepted = component_sequences_.Accept(components, sequence, allow_equal);
    if (accepted && (!has_state_sequence_ || IsNewActorSequence(sequence, state_sequence_))) set_state_sequence(sequence);
    return accepted;
  }
  bool accept_action_sequence(std::uint32_t sequence) {
    if (sequence == 0 || (last_action_sequence_ && !IsNewActorSequence(sequence, last_action_sequence_))) return false;
    last_action_sequence_ = sequence;
    return true;
  }
  bool accept_damage_sequence(std::uint32_t sequence) {
    if (last_damage_sequence_ && !IsNewActorSequence(sequence, *last_damage_sequence_))
      return false;
    last_damage_sequence_ = sequence;
    return true;
  }
  bool accept_attribute_sequence(PlayerAttributeId attribute, std::uint32_t sequence) {
    const auto index = static_cast<std::size_t>(attribute);
    if (index >= attribute_sequences_.size()) return false;
    if (attribute_initialized_[index] && !IsNewActorSequence(sequence, attribute_sequences_[index])) return false;
    const auto component = attribute == ATTR_HEALTH ? kSyncHealth : attribute == ATTR_MANA ? kSyncMana : 0;
    if (component && !accept_components(component, sequence, true)) return false;
    attribute_sequences_[index] = sequence;
    attribute_initialized_[index] = true;
    return true;
  }
  void clear_snapshot_collections() { overlays_.clear(); weapon_skills_.clear(); talents_.clear(); }

  PlayerState state() const {
    PlayerState value;
    value.source_time_ms = source_time_ms_;
    value.position = position_;
    value.nrot = rotation_;
    value.left_hand_item_instance = left_hand_item_instance_;
    value.right_hand_item_instance = right_hand_item_instance_;
    value.equipped_armor_instance = equipped_armor_instance_;
    value.equipped_helmet_instance = equipped_helmet_instance_;
    value.equipped_shield_instance = equipped_shield_instance_;
    value.equipped_amulet_instance = equipped_amulet_instance_;
    value.equipped_belt_instance = equipped_belt_instance_;
    value.equipped_ring_left_instance = equipped_ring_left_instance_;
    value.equipped_ring_right_instance = equipped_ring_right_instance_;
    value.melee_weapon_instance = melee_weapon_instance_;
    value.ranged_weapon_instance = ranged_weapon_instance_;
    value.animation_name = animation_name_;
    value.health_points = health_;
    value.mana_points = mana_;
    value.life_state = life_state_;
    value.invulnerable = invulnerable_;
    value.weapon_mode = weapon_mode_;
    value.active_spell_nr = active_spell_nr_;
    value.active_spell_instance = active_spell_instance_;
    value.active_spell_level = active_spell_level_;
    value.spell_investing = spell_investing_;
    value.head_direction = head_direction_;
    value.body_state = body_state_;
    value.walk_mode = walk_mode_;
    value.look_yaw = look_yaw_;
    value.look_pitch = look_pitch_;
    value.focus_target_id = focus_target_id_;
    return value;
  }

  bool is_npc() const { return is_npc_; }
  void set_is_npc(bool value) { is_npc_ = value; }
  std::uint32_t npc_control_epoch() const { return npc_control_epoch_; }
  std::uint32_t npc_host_player_id() const { return npc_host_player_id_; }
  std::uint32_t npc_action_id() const { return npc_action_id_; }
  std::uint32_t npc_animation_revision() const { return npc_animation_revision_; }
  void set_npc_animation_revision(std::uint32_t revision) { npc_animation_revision_ = revision; }
  void set_npc_control(std::uint32_t epoch, std::uint32_t host_id, std::uint32_t action_id) {
    if (epoch != npc_control_epoch_) last_action_sequence_ = 0;
    npc_control_epoch_ = epoch;
    npc_host_player_id_ = host_id;
    npc_action_id_ = action_id;
  }

  const std::string& name() const {
    return name_;
  }
  void set_name(const std::string& name) {
    name_ = name;
  }

  const std::string& instance() const {
    return instance_;
  }
  void set_instance(const std::string& instance) {
    instance_ = instance;
  }

  std::uint8_t name_color_r() const {
    return name_color_r_;
  }
  std::uint8_t name_color_g() const {
    return name_color_g_;
  }
  std::uint8_t name_color_b() const {
    return name_color_b_;
  }

  void set_name_color(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    name_color_r_ = r;
    name_color_g_ = g;
    name_color_b_ = b;
  }

  // Character appearance
  std::uint8_t walk_style() const {
    return walk_style_;
  }
  void set_walk_style(std::uint8_t value) {
    walk_style_ = value;
  }

  const std::string& body_model() const {
    return body_model_;
  }
  void set_body_model(const std::string& value) {
    body_model_ = value;
  }

  float fatness() const {
    return fatness_;
  }
  void set_fatness(float fatness) {
    fatness_ = fatness;
  }

  const glm::vec3& scale() const {
    return scale_;
  }
  void set_scale(const glm::vec3& scale) {
    scale_ = scale;
  }

  const std::vector<std::string>& overlays() const {
    return overlays_;
  }

  void add_overlay(const std::string& overlay) {
    if (std::find(overlays_.begin(), overlays_.end(), overlay) == overlays_.end()) {
      overlays_.push_back(overlay);
    }
  }

  void remove_overlay(const std::string& overlay) {
    overlays_.erase(std::remove(overlays_.begin(), overlays_.end(), overlay), overlays_.end());
  }

  std::int16_t body_texture() const {
    return body_texture_;
  }
  void set_body_texture(std::int16_t value) {
    body_texture_ = value;
  }

  const std::string& head_model() const {
    return head_model_name_;
  }
  void set_head_model(const std::string& value) {
    head_model_name_ = value;
  }

  std::int16_t head_texture() const {
    return head_texture_;
  }
  void set_head_texture(std::int16_t value) {
    head_texture_ = value;
  }

  std::int16_t teeth_texture() const {
    return teeth_texture_;
  }
  void set_teeth_texture(std::int16_t value) {
    teeth_texture_ = value;
  }

  std::int16_t skin_color() const {
    return skin_color_;
  }
  void set_skin_color(std::int16_t value) {
    skin_color_ = value;
  }

  // Position and rotation
  const glm::vec3& position() const {
    return position_;
  }
  void set_position(const glm::vec3& pos) {
    position_ = pos;
  }
  void set_position(float x, float y, float z) {
    position_ = glm::vec3(x, y, z);
  }

  const glm::vec3& rotation() const {
    return rotation_;
  }
  void set_rotation(const glm::vec3& rot) {
    rotation_ = rot;
  }

  // Attributes
  std::int32_t health() const {
    return health_;
  }
  void set_health(std::int32_t value) {
    health_ = value;
  }

  std::int32_t mana() const {
    return mana_;
  }
  void set_mana(std::int32_t value) {
    mana_ = value;
  }

  std::uint8_t life_state() const {
    return life_state_;
  }
  void set_life_state(std::uint8_t value, std::optional<std::uint32_t> command_revision = {}) {
    if (command_revision && value != life_state_) set_life_command_revision(*command_revision);
    life_state_ = value;
  }
  void set_lifecycle_state(std::uint8_t value, std::uint32_t command_revision) {
    set_life_command_revision(command_revision);
    life_state_ = value;
  }
  bool accepts_life_action_revision(std::uint32_t revision) const {
    return !life_command_revision_ || revision == *life_command_revision_ || IsNewActorSequence(revision, *life_command_revision_);
  }
  void set_life_command_revision(std::uint32_t revision) {
    if (!life_command_revision_ || revision == *life_command_revision_ || IsNewActorSequence(revision, *life_command_revision_))
      life_command_revision_ = revision;
  }
  bool invulnerable() const { return invulnerable_; }
  void set_invulnerable(bool value) { invulnerable_ = value; }

  std::int32_t max_health() const {
    return max_health_;
  }
  void set_max_health(std::int32_t value) {
    max_health_ = value;
  }

  std::int32_t max_mana() const {
    return max_mana_;
  }
  void set_max_mana(std::int32_t value) {
    max_mana_ = value;
  }

  std::int32_t level() const {
    return level_;
  }
  void set_level(std::int32_t value) {
    level_ = value;
  }

  std::int32_t exp() const {
    return exp_;
  }
  void set_exp(std::int32_t value) {
    exp_ = value;
  }

  std::int32_t next_level_exp() const {
    return next_level_exp_;
  }
  void set_next_level_exp(std::int32_t value) {
    next_level_exp_ = value;
  }

  std::int32_t learn_points() const {
    return learn_points_;
  }
  void set_learn_points(std::int32_t value) {
    learn_points_ = value;
  }

  std::int32_t strength() const {
    return strength_;
  }
  void set_strength(std::int32_t value) {
    strength_ = value;
  }

  std::int32_t dexterity() const {
    return dexterity_;
  }
  void set_dexterity(std::int32_t value) {
    dexterity_ = value;
  }

  std::int32_t ping() const {
    return ping_;
  }
  void set_ping(std::int32_t value) {
    ping_ = value;
  }

  const std::unordered_map<std::int32_t, std::int32_t>& weapon_skills() const {
    return weapon_skills_;
  }
  void set_weapon_skill(std::int32_t skill_id, std::int32_t value) {
    weapon_skills_[skill_id] = value;
  }

  const std::unordered_map<std::int32_t, std::int32_t>& talents() const {
    return talents_;
  }
  void set_talent(std::int32_t talent_id, std::int32_t value) {
    talents_[talent_id] = value;
  }

  // Items
  const std::string& left_hand_item() const {
    return left_hand_item_instance_;
  }
  void set_left_hand_item(const std::string& instance) {
    left_hand_item_instance_ = instance;
  }

  const std::string& right_hand_item() const {
    return right_hand_item_instance_;
  }
  void set_right_hand_item(const std::string& instance) {
    right_hand_item_instance_ = instance;
  }

  const std::string& equipped_armor() const {
    return equipped_armor_instance_;
  }
  void set_equipped_armor(const std::string& instance) {
    equipped_armor_instance_ = instance;
  }

  const std::string& equipped_helmet() const {
    return equipped_helmet_instance_;
  }
  void set_equipped_helmet(const std::string& instance) {
    equipped_helmet_instance_ = instance;
  }

  const std::string& equipped_shield() const {
    return equipped_shield_instance_;
  }
  void set_equipped_shield(const std::string& instance) {
    equipped_shield_instance_ = instance;
  }

  const std::string& equipped_amulet() const {
    return equipped_amulet_instance_;
  }
  void set_equipped_amulet(const std::string& instance) {
    equipped_amulet_instance_ = instance;
  }

  const std::string& equipped_belt() const {
    return equipped_belt_instance_;
  }
  void set_equipped_belt(const std::string& instance) {
    equipped_belt_instance_ = instance;
  }

  const std::string& equipped_ring_left() const {
    return equipped_ring_left_instance_;
  }
  void set_equipped_ring_left(const std::string& instance) {
    equipped_ring_left_instance_ = instance;
  }

  const std::string& equipped_ring_right() const {
    return equipped_ring_right_instance_;
  }
  void set_equipped_ring_right(const std::string& instance) {
    equipped_ring_right_instance_ = instance;
  }

  const std::string& melee_weapon() const {
    return melee_weapon_instance_;
  }
  void set_melee_weapon(const std::string& instance) {
    melee_weapon_instance_ = instance;
  }

  const std::string& ranged_weapon() const {
    return ranged_weapon_instance_;
  }
  void set_ranged_weapon(const std::string& instance) {
    ranged_weapon_instance_ = instance;
  }

  // State

  const std::string& animation_name() const {
    return animation_name_;
  }
  void set_animation_name(const std::string& animation_name) {
    animation_name_ = animation_name;
  }

  std::uint8_t weapon_mode() const {
    return weapon_mode_;
  }
  void set_weapon_mode(std::uint8_t mode) {
    weapon_mode_ = mode;
  }

  std::uint8_t active_spell() const {
    return active_spell_nr_;
  }
  void set_active_spell(std::uint8_t spell) {
    active_spell_nr_ = spell;
  }

  const std::string& active_spell_instance() const {
    return active_spell_instance_;
  }
  void set_active_spell_instance(const std::string& instance) {
    active_spell_instance_ = instance;
  }

  void set_spell_investment(std::uint8_t level, bool investing) {
    active_spell_level_ = level;
    spell_investing_ = investing;
  }

  std::uint8_t head_direction() const {
    return head_direction_;
  }
  void set_head_direction(std::uint8_t dir) {
    head_direction_ = dir;
  }
  std::uint32_t body_state() const { return body_state_; }
  void set_body_state(std::uint32_t value) { body_state_ = value; }
  std::uint8_t walk_mode() const { return walk_mode_; }
  void set_walk_mode(std::uint8_t value) { walk_mode_ = value; }
  float look_yaw() const { return look_yaw_; }
  float look_pitch() const { return look_pitch_; }
  void set_look_angles(float yaw, float pitch) { look_yaw_ = yaw; look_pitch_ = pitch; }
  std::optional<std::uint32_t> focus_target_id() const { return focus_target_id_; }
  void set_focus_target_id(std::optional<std::uint32_t> value) { focus_target_id_ = value; }

  std::uint32_t state_sequence() const { return state_sequence_; }

  void set_state_sequence(std::uint32_t sequence) {
    state_sequence_ = sequence;
    has_state_sequence_ = true;
  }

  bool accept_state_sequence(std::uint32_t sequence) {
    if (!has_state_sequence_ || IsStateSequenceNewer(sequence, state_sequence_)) {
      set_state_sequence(sequence);
      return true;
    }
    return false;
  }

  bool accept_lifecycle_sequence(std::uint32_t sequence) {
    if (has_lifecycle_sequence_ && !IsNewActorSequence(sequence, lifecycle_sequence_)) return false;
    lifecycle_sequence_ = sequence;
    has_lifecycle_sequence_ = true;
    return true;
  }

  bool is_enabled() const {
    return enabled_;
  }
  void set_enabled(bool enabled) {
    enabled_ = enabled;
  }

  bool has_spawned() const {
    return has_spawned_;
  }
  void set_has_spawned(bool value) {
    has_spawned_ = value;
  }

  bool has_joined() const {
    return has_joined_;
  }
  void set_has_joined(bool value) {
    has_joined_ = value;
  }

  std::int16_t update_health_packet_counter() const {
    return update_health_packet_;
  }
  void set_update_health_packet_counter(std::int16_t counter) {
    update_health_packet_ = counter;
  }

protected:
  // Core identity
  std::uint64_t id_{0};
  std::uint64_t actor_generation_{0};
  std::uint32_t instance_change_revision_{0};
  std::uint64_t source_time_ms_{0};
  std::uint32_t command_revision_{0};
  bool has_command_revision_{false};
  ActorComponentSequences component_sequences_;
  std::uint32_t lifecycle_sequence_{0};
  bool has_lifecycle_sequence_{false};
  std::uint32_t last_action_sequence_{0};
  bool summon_notified_{false};
  std::uint16_t transformation_spell_id_{0};
  std::uint32_t transformation_cast_id_{0};
  std::optional<std::uint32_t> transformation_sequence_;
  std::optional<std::uint32_t> last_damage_sequence_;
  std::array<std::uint32_t, 10> attribute_sequences_{};
  std::array<bool, 10> attribute_initialized_{};
  std::unordered_map<std::string, std::uint32_t> inventory_sequences_;
  std::unordered_map<std::string, std::int32_t> inventory_;
  std::optional<std::uint32_t> inventory_snapshot_sequence_;
  std::optional<std::uint32_t> latest_inventory_sequence_;
  std::optional<std::uint32_t> life_command_revision_;
  bool is_npc_{false};
  std::uint32_t npc_control_epoch_{0};
  std::uint32_t npc_host_player_id_{0};
  std::uint32_t npc_action_id_{0};
  std::uint32_t npc_animation_revision_{0};
  std::string name_;
  std::string instance_;
  std::uint8_t name_color_r_{255};
  std::uint8_t name_color_g_{255};
  std::uint8_t name_color_b_{255};

  // Character appearance
  std::uint8_t walk_style_{0};
  std::string body_model_;
  std::int16_t body_texture_{0};
  std::string head_model_name_;
  std::int16_t head_texture_{0};
  std::int16_t teeth_texture_{0};
  std::int16_t skin_color_{0};
  float fatness_{1.0f};
  glm::vec3 scale_{1.0f, 1.0f, 1.0f};
  std::vector<std::string> overlays_;

  // Position and rotation
  glm::vec3 position_{0.0f, 0.0f, 0.0f};
  glm::vec3 rotation_{0.0f, 0.0f, 0.0f};

  // Attributes
  std::int32_t health_{0};
  std::int32_t mana_{0};
  std::uint8_t life_state_{PLAYER_LIFE_ALIVE};
  bool invulnerable_{false};
  std::int32_t max_health_{0};
  std::int32_t max_mana_{0};
  std::int32_t level_{0};
  std::int32_t exp_{0};
  std::int32_t next_level_exp_{0};
  std::int32_t learn_points_{0};
  std::int32_t strength_{0};
  std::int32_t dexterity_{0};
  std::int32_t ping_{-1};
  std::unordered_map<std::int32_t, std::int32_t> weapon_skills_;
  std::unordered_map<std::int32_t, std::int32_t> talents_;

  // Items
  std::string left_hand_item_instance_;
  std::string right_hand_item_instance_;
  std::string equipped_armor_instance_;
  std::string equipped_helmet_instance_;
  std::string equipped_shield_instance_;
  std::string equipped_amulet_instance_;
  std::string equipped_belt_instance_;
  std::string equipped_ring_left_instance_;
  std::string equipped_ring_right_instance_;
  std::string melee_weapon_instance_;
  std::string ranged_weapon_instance_;

  // State
  std::string animation_name_;
  std::uint8_t weapon_mode_{0};
  std::uint8_t active_spell_nr_{0};
  std::string active_spell_instance_;
  std::uint8_t active_spell_level_{0};
  bool spell_investing_{false};
  std::uint8_t head_direction_{0};
  std::uint32_t body_state_{0};
  std::uint8_t walk_mode_{0};
  float look_yaw_{0.5f};
  float look_pitch_{0.5f};
  std::optional<std::uint32_t> focus_target_id_;
  std::uint32_t state_sequence_{0};
  bool has_state_sequence_{false};
  bool enabled_{false};
  std::int16_t update_health_packet_{0};
  bool has_spawned_{false};
  bool has_joined_{false};

private:
  static bool IsStateSequenceNewer(std::uint32_t sequence, std::uint32_t previous_sequence) {
    const auto delta = sequence - previous_sequence;
    return delta != 0 && delta < 0x80000000u;
  }
};

class LocalPlayer : public Player {
public:
  LocalPlayer(std::uint64_t player_id) {
    id_ = player_id;
  }
  ~LocalPlayer() override = default;
};

class PlayerManager {
public:
  PlayerManager() = default;
  ~PlayerManager() = default;

  LocalPlayer& GetLocalPlayer() {
    assert(local_player_ != nullptr);
    return *local_player_;
  }

  bool HasLocalPlayer() const {
    return static_cast<bool>(local_player_);
  }

  LocalPlayer* CreateLocalPlayer(std::uint64_t id) {
    local_player_ = std::make_unique<LocalPlayer>(id);
    // Note: Local player is stored separately, not in the players_ map
    return local_player_.get();
  }

  Player* CreatePlayer(std::uint64_t id) {
    if (auto* existing = GetPlayer(id)) return existing;
    auto player = std::make_unique<Player>();
    player->set_id(id);
    Player* ptr = player.get();
    players_[id] = std::move(player);
    return ptr;
  }

  Player* GetPlayer(std::uint64_t id) {
    auto it = players_.find(id);
    return it != players_.end() ? it->second.get() : nullptr;
  }

  void RemovePlayer(std::uint64_t id) {
    // Don't remove local player
    if (local_player_ && local_player_->id() == id) {
      return;
    }
    players_.erase(id);
  }

  const std::map<std::uint64_t, std::unique_ptr<Player>>& GetAllPlayers() const {
    return players_;
  }

  void Clear() {
    players_.clear();
    local_player_.reset();
  }

private:
  std::unique_ptr<LocalPlayer> local_player_;
  std::map<std::uint64_t, std::unique_ptr<Player>> players_;
};

}  // namespace gmp::client
