#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <limits>

#include "native_damage_observation.h"

namespace {
struct Victim {
  int health;
  int maximum_health;
};

void ChangeHealth(Victim& victim, int change) {
  if (gmp::NativeDamageObservation::RecordHealthChange(&victim, change))
    return;
  // ZenGin clamps ChangeAttribute to maximum HP, even for deductions.
  victim.health = static_cast<int>(std::clamp<std::int64_t>(static_cast<std::int64_t>(victim.health) + change, 0, victim.maximum_health));
}
}  // namespace

TEST(NativeDamageObservation, ProtectedSpellDoesNotTurnMaximumHealthClampingIntoDamage) {
  Victim legacy{1000000000, 1000};
  ChangeHealth(legacy, -30);
  ASSERT_EQ(legacy.health, 1000);
  ASSERT_EQ(1000000000 - legacy.health, 999999000);

  for (const int health : {50, 100, 1000}) {
    Victim victim{health, 1000};
    {
      gmp::NativeDamageObservation observation(&victim);
      // A spell with 80 damage hits a victim with 50 protection.
      ChangeHealth(victim, -30);
      EXPECT_EQ(observation.damage(), 30);
      EXPECT_EQ(victim.health, health);
      EXPECT_EQ(victim.maximum_health, 1000);
    }
    EXPECT_FALSE(gmp::NativeDamageObservation::IsActive(&victim));
  }
}

TEST(NativeDamageObservation, HealthDependentSpellsReadRealHealthAndCanStillBeLethal) {
  Victim victim{160, 1000};
  gmp::NativeDamageObservation observation(&victim);
  ChangeHealth(victim, -(victim.health / 4));
  EXPECT_EQ(observation.damage(), 40);
  EXPECT_EQ(victim.health, 160);
  ChangeHealth(victim, -500);
  EXPECT_EQ(observation.damage(), 540);
  EXPECT_EQ(victim.health, 160);
}

TEST(NativeDamageObservation, ZeroDamageAndHealingCannotBecomeHitReports) {
  Victim victim{100, 1000};
  gmp::NativeDamageObservation observation(&victim);
  ChangeHealth(victim, 0);
  ChangeHealth(victim, 50);
  EXPECT_EQ(observation.damage(), 0);
  EXPECT_EQ(victim.health, 100);
}

TEST(NativeDamageObservation, OtherVictimsAndNormalAttributeChangesKeepNativeBehavior) {
  Victim observed{100, 1000};
  Victim other{100, 1000};
  {
    gmp::NativeDamageObservation observation(&observed);
    ChangeHealth(other, -30);
    EXPECT_EQ(other.health, 70);
    EXPECT_EQ(observation.damage(), 0);
  }
  ChangeHealth(observed, -30);
  EXPECT_EQ(observed.health, 70);
}

TEST(NativeDamageObservation, NestedCalculationsRestoreThePreviousVictimAndDoNotDoubleCount) {
  Victim first{100, 1000};
  Victim second{100, 1000};
  gmp::NativeDamageObservation outer(&first);
  ChangeHealth(first, -10);
  {
    gmp::NativeDamageObservation inner(&second);
    ChangeHealth(first, -20);
    ChangeHealth(second, -30);
    EXPECT_EQ(inner.damage(), 30);
    EXPECT_EQ(outer.damage(), 30);
    {
      gmp::NativeDamageObservation nested(&first);
      ChangeHealth(first, -40);
      EXPECT_EQ(nested.damage(), 40);
      EXPECT_EQ(outer.damage(), 30);
    }
  }
  EXPECT_FALSE(gmp::NativeDamageObservation::IsActive(&second));
  ChangeHealth(first, -50);
  EXPECT_EQ(outer.damage(), 80);
  EXPECT_EQ(first.health, 100);
  EXPECT_EQ(second.health, 100);
}

TEST(NativeDamageObservation, ExtremeDeductionsSaturateWithoutSignedOverflow) {
  Victim victim{100, 1000};
  gmp::NativeDamageObservation observation(&victim);
  ChangeHealth(victim, std::numeric_limits<std::int32_t>::min());
  ChangeHealth(victim, -100);
  EXPECT_EQ(observation.damage(), std::numeric_limits<std::int32_t>::max());
  EXPECT_EQ(victim.health, 100);
}
