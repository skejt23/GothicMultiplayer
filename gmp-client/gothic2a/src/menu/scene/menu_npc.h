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

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ZenGin/zGothicAPI.h"

namespace menu {

struct MenuNpcVisual {
  std::string body = "HUM_BODY_NAKED0";
  int body_texture = 0;
  std::string head = "HUM_HEAD_PONY";
  int head_texture = 0;
};

// Shared authoring shape; each scene declares its own definitions in its .cpp.
struct MenuNpcDefinition {
  static constexpr const char* kDefaultInstance = "PC_HERO";
  static constexpr const char* kDefaultFont = "CP1250_FONT_DEFAULT.TGA";

  std::string name;
  std::array<std::uint8_t, 3> name_color{255, 255, 255};  // RGB
  std::string name_font = kDefaultFont;
  bool name_show = false;
  float name_fade_start = 1000.0f;  // Camera distance in world units; fully opaque up to here.
  float name_fade_end = 2000.0f;    // Fully hidden here; must be greater than name_fade_start.
  std::string instance = kDefaultInstance;
  MenuNpcVisual visual;  // Only applied to PC_HERO; other native arguments are 0.
  std::string overlay;   // Only applied to PC_HERO; empty adds no overlay.
  std::string equip_melee;
  std::string equip_ranged;
  std::string equip_armor;
  std::string spawn_wp;
  std::string end_wp;          // Used only when the scene explicitly starts walking.
  bool freeze_on_wp = true;    // Ignore standalone animation movement; explicit walking still moves the NPC.
  std::string animation;       // Optional initial animation; empty leaves playback to the scene.
  bool animation_loop = true;  // Restart the initial clip when it finishes; native loops/follow-ups still apply.
  bool draw_melee = false;     // Draw an equipped melee weapon before starting the initial animation.
};

// A local stage actor. Never registered with players, client NPCs, Lua or the network.
class MenuNpc {
public:
  enum class Movement { Idle, Walking, Reached, Failed };

  static bool CanCreate();
  static std::unique_ptr<MenuNpc> Create(oCWorld& world, const MenuNpcDefinition& definition);
  // Finds static ground below an authored location; does not alter any NPC.
  static bool FindGround(oCWorld& world, const zVEC3& hint, zVEC3& ground);
  ~MenuNpc();
  MenuNpc(const MenuNpc&) = delete;
  MenuNpc& operator=(const MenuNpc&) = delete;

  bool SetAdditionalVisuals(const MenuNpcVisual& visual);
  bool ApplyOverlay(const std::string& overlay);
  bool EquipArmor(const char* instance);
  bool EquipMelee(const char* instance);
  bool EquipRanged(const char* instance);
  bool DrawMeleeWeapon();
  bool SetPosition(const zVEC3& position);
  bool PlaceAtWaypoint(const char* waypoint);
  bool ResetToSpawn();
  bool WalkToEndWaypoint(float speed = 140.0f);
  void SetRotation(float yaw);
  zVEC3 GetPosition() const;
  bool PlayAnimation(const char* name, bool loop = false);
  void StopAnimation();
  bool HasAnimationFinished() const;
  bool WalkTo(const zVEC3& destination, float speed = 140.0f);
  // Resolves a native waynet route from the last placed/reached waypoint.
  // No direct-line fallback when names are missing or disconnected.
  bool WalkToWaypoint(const char* destination, float speed = 140.0f);
  void StopMovement();
  bool HasReachedDestination() const {
    return movement_ == Movement::Reached;
  }
  Movement GetMovement() const {
    return movement_;
  }
  void Update(float delta_time);
  bool IsValid() const;
  // Hides the model (including equipped visuals) and its nameplate together.
  // Keeps the actor and its movement state available for a later scene reset.
  void SetVisible(bool visible);

  void SetNameplate(std::string text) {
    definition_.name = std::move(text);
  }
  void SetNameplateVisible(bool visible) {
    definition_.name_show = visible;
  }
  void SetNameplateOffset(float offset) {
    nameplate_offset_ = offset;
  }
  bool HasVisibleNameplate() const;
  const std::string& GetNameplate() const {
    return definition_.name;
  }
  const std::string& GetNameplateFont() const {
    return definition_.name_font;
  }
  zCOLOR GetNameplateColor() const {
    return zCOLOR(definition_.name_color[0], definition_.name_color[1], definition_.name_color[2], 255);
  }
  std::uint8_t GetNameplateAlpha(const zVEC3& camera_position) const;
  zVEC3 GetNameplatePosition() const;
  oCNpc* GetNativeNpc() const {
    return npc_;
  }  // Borrowed, never release/delete this pointer.

private:
  MenuNpc(oCWorld& world, const MenuNpcDefinition& definition) : world_(world), definition_(definition) {
    world_.AddRef();
  }
  enum class Equipment { Melee, Ranged, Armor };
  void FailMovement();
  bool BeginWalk(std::vector<zVEC3> route, float speed);
  bool Equip(const char* instance, Equipment kind);
  bool ApplyAnimationMovement(const zCModel& model);

  oCWorld& world_;  // One retained reference, including during partial initialization.
  MenuNpcDefinition definition_;
  bool allow_visual_overrides_ = false;
  oCNpc* npc_ = nullptr;             // Owns the factory's reference, in addition to the world's reference.
  zCAIBase* detached_ai_ = nullptr;  // Held while callback AI is disconnected.
  zVEC3 position_{0.0f, 0.0f, 0.0f};
  std::vector<zVEC3> route_;  // Copied positions; no borrowed waynet pointers.
  size_t route_index_ = 0;
  std::string current_waypoint_;
  std::string destination_waypoint_;
  float feet_offset_ = 0.0f;
  float speed_ = 0.0f;
  float movement_time_left_ = 0.0f;
  Movement movement_ = Movement::Idle;
  int animation_id_ = -1;
  bool loop_animation_ = false;
  bool animation_failed_ = false;
  float nameplate_offset_ = 25.0f;
  mutable bool logged_missing_head_ = false;
};

}  // namespace menu
