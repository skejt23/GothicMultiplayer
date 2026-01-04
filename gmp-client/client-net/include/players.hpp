
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
#include <glm/glm.hpp>
#include <map>
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>

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
  std::int16_t health() const {
    return health_;
  }
  void set_health(std::int16_t value) {
    health_ = value;
  }

  std::int16_t mana() const {
    return mana_;
  }
  void set_mana(std::int16_t value) {
    mana_ = value;
  }

  std::int16_t max_health() const {
    return max_health_;
  }
  void set_max_health(std::int16_t value) {
    max_health_ = value;
  }

  std::int16_t max_mana() const {
    return max_mana_;
  }
  void set_max_mana(std::int16_t value) {
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
  std::uint16_t left_hand_item() const {
    return left_hand_item_instance_;
  }
  void set_left_hand_item(std::uint16_t instance) {
    left_hand_item_instance_ = instance;
  }

  std::uint16_t right_hand_item() const {
    return right_hand_item_instance_;
  }
  void set_right_hand_item(std::uint16_t instance) {
    right_hand_item_instance_ = instance;
  }

  std::uint16_t equipped_armor() const {
    return equipped_armor_instance_;
  }
  void set_equipped_armor(std::uint16_t instance) {
    equipped_armor_instance_ = instance;
  }

  std::uint16_t melee_weapon() const {
    return melee_weapon_instance_;
  }
  void set_melee_weapon(std::uint16_t instance) {
    melee_weapon_instance_ = instance;
  }

  std::uint16_t ranged_weapon() const {
    return ranged_weapon_instance_;
  }
  void set_ranged_weapon(std::uint16_t instance) {
    ranged_weapon_instance_ = instance;
  }

  // State
  std::uint16_t animation() const {
    return animation_;
  }
  void set_animation(std::uint16_t anim) {
    animation_ = anim;
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

  std::uint8_t head_direction() const {
    return head_direction_;
  }
  void set_head_direction(std::uint8_t dir) {
    head_direction_ = dir;
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
  float fatness_{1.0f};
  glm::vec3 scale_{1.0f, 1.0f, 1.0f};
  std::vector<std::string> overlays_;

  // Position and rotation
  glm::vec3 position_{0.0f, 0.0f, 0.0f};
  glm::vec3 rotation_{0.0f, 0.0f, 0.0f};

  // Attributes
  std::int16_t health_{0};
  std::int16_t mana_{0};
  std::int16_t max_health_{0};
  std::int16_t max_mana_{0};
  std::int32_t level_{0};
  std::int32_t exp_{0};
  std::int32_t next_level_exp_{0};
  std::int32_t learn_points_{0};
  std::int32_t strength_{0};
  std::int32_t dexterity_{0};
  std::unordered_map<std::int32_t, std::int32_t> weapon_skills_;
  std::unordered_map<std::int32_t, std::int32_t> talents_;

  // Items
  std::uint16_t left_hand_item_instance_{0};
  std::uint16_t right_hand_item_instance_{0};
  std::uint16_t equipped_armor_instance_{0};
  std::uint16_t melee_weapon_instance_{0};
  std::uint16_t ranged_weapon_instance_{0};

  // State
  std::uint16_t animation_{0};
  std::uint8_t weapon_mode_{0};
  std::uint8_t active_spell_nr_{0};
  std::uint8_t head_direction_{0};
  bool enabled_{false};
  std::int16_t update_health_packet_{0};
  bool has_spawned_{false};
  bool has_joined_{false};
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