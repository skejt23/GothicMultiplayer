#pragma once

#include <cstdint>

namespace gmp {

// Gothic II Addon spells that create NPCs. SummonSwarm (81) is a visual
// effect spell and must not be converted into an ordinary NPC summon.
inline bool IsNpcSummonSpell(std::uint16_t id) {
  switch (id) {
    case 22:  // Goblin skeleton
    case 25:  // Wolf
    case 31:  // Skeleton
    case 35:  // Golem
    case 40:  // Demon
    case 45:  // Army of darkness
    case 84:  // Guardian
    case 88:  // Zombie
      return true;
    default:
      return false;
  }
}

}  // namespace gmp
