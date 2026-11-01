#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

namespace gmp {

// Creature movement animations use Gothic's FIST weapon mode, even without items.
inline constexpr std::uint8_t kTransformationWeaponMode = 1;

struct TransformationForm {
  const char* instance;
  std::int32_t max_health;
  std::int32_t strength;
  std::int32_t dexterity;
  std::array<std::int32_t, 8> protection;
  std::uint32_t damage_type{1u << 2};
};

inline const TransformationForm* GetTransformationForm(std::uint16_t spell) {
  // Gothic II Addon Spell_Transform.d and the corresponding monster prototypes.
  // Protection indices: barrier, blunt, edge, fire, fly, magic, point, fall.
  static constexpr TransformationForm forms[] = {{"SHEEP", 10, 5, 5, {}},
                                                 {"SCAVENGER", 70, 35, 35, {0, 35, 35, 35, 35, 0, 0, 0}},
                                                 {"GIANT_RAT", 30, 15, 15, {0, 15, 15, 15, 15, 0, 0, 0}},
                                                 {"GIANT_BUG", 80, 40, 40, {0, 40, 40, 40, 40, 0, 40, 0}},
                                                 {"WOLF", 60, 30, 30, {0, 30, 30, 30, 30, 0, 0, 0}},
                                                 {"WARAN", 120, 60, 60, {0, 100, 100, 100, 100, 0, 100, 0}},
                                                 {"SNAPPER", 120, 60, 60, {0, 60, 60, 60, 60, 0, 60, 0}},
                                                 {"WARG", 300, 150, 150, {0, 125, 125, 125, 125, 0, 75, 0}},
                                                 {"FIREWARAN", 300, 150, 150, {0, 150, 150, 150, 150, 0, 150, 0}, 1u << 3},
                                                 {"LURKER", 120, 60, 60, {0, 60, 60, 60, 60, 0, 60, 0}},
                                                 {"SHADOWBEAST", 300, 150, 150, {0, 125, 125, 125, 125, 20, 75, 0}},
                                                 {"DRAGONSNAPPER", 400, 200, 200, {0, 150, 150, 150, 150, 0, 150, 0}}};
  return spell >= 47 && spell <= 58 ? &forms[spell - 47] : nullptr;
}

inline std::int32_t ScaleTransformationHealth(std::int32_t health, std::int32_t maximum, std::int32_t next_maximum) {
  if (health <= 0 || next_maximum <= 0)
    return 0;
  return static_cast<std::int32_t>(
      std::clamp<std::int64_t>(static_cast<std::int64_t>(health) * next_maximum / std::max(1, maximum), 1, next_maximum));
}

}  // namespace gmp
