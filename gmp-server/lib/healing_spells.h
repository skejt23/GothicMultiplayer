#pragma once

#include <cstdint>
#include <optional>

// Gothic II Addon Spell_Heal.d. Custom resource handlers can cancel the native
// cast and provide their own health/mana changes through onPlayerCastSpell.
struct HealingSpell {
  std::int32_t health;
  std::int32_t mana;
};

inline std::optional<HealingSpell> GetHealingSpell(std::uint16_t id) {
  switch (id) {
    case 1:   // Paladin light heal
    case 21:  // Light heal
      return HealingSpell{200, 10};
    case 3:   // Paladin medium heal
    case 28:  // Medium heal
      return HealingSpell{400, 25};
    case 5:   // Paladin full heal
    case 41:  // Full heal
      return HealingSpell{800, 50};
    default:
      return std::nullopt;
  }
}
