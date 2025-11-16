
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

#include <cassert>
#include <cstdint>
#include <glm/glm.hpp>
#include <map>
#include <memory>
#include <string>

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

  // Character appearance
  std::uint8_t selected_class() const {
    return selected_class_;
  }
  void set_selected_class(std::uint8_t value) {
    selected_class_ = value;
  }

  std::uint8_t head_model() const {
    return head_model_;
  }
  void set_head_model(std::uint8_t value) {
    head_model_ = value;
  }

  std::uint8_t skin_texture() const {
    return skin_texture_;
  }
  void set_skin_texture(std::uint8_t value) {
    skin_texture_ = value;
  }

  std::uint8_t face_texture() const {
    return face_texture_;
  }
  void set_face_texture(std::uint8_t value) {
    face_texture_ = value;
  }

  std::uint8_t walk_style() const {
    return walk_style_;
  }
  void set_walk_style(std::uint8_t value) {
    walk_style_ = value;
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

  // Health and mana
  std::int16_t hp() const {
    return hp_;
  }
  void set_hp(std::int16_t value) {
    hp_ = value;
  }

  std::int16_t mana() const {
    return mana_;
  }
  void set_mana(std::int16_t value) {
    mana_ = value;
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

  std::int16_t update_hp_packet_counter() const {
    return update_hp_packet_;
  }
  void set_update_hp_packet_counter(std::int16_t counter) {
    update_hp_packet_ = counter;
  }

protected:
  // Core identity
  std::uint64_t id_{0};
  std::string name_;

  // Character appearance
  std::uint8_t selected_class_{0};
  std::uint8_t head_model_{0};
  std::uint8_t skin_texture_{0};
  std::uint8_t face_texture_{0};
  std::uint8_t walk_style_{0};

  // Position and rotation
  glm::vec3 position_{0.0f, 0.0f, 0.0f};
  glm::vec3 rotation_{0.0f, 0.0f, 0.0f};

  // Health and mana
  std::int16_t hp_{0};
  std::int16_t mana_{0};

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
  std::int16_t update_hp_packet_{0};
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