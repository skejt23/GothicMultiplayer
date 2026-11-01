// SPDX-License-Identifier: MIT
#pragma once

namespace gmp {
inline constexpr bool IsWeaponSkill(int skill) {
  return skill >= 1 && skill <= 4;
}
inline constexpr int WeaponSkillTier(int percentage) {
  return percentage >= 60 ? 2 : percentage >= 30 ? 1 : 0;
}
}  // namespace gmp
