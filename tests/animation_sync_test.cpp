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

#include <gtest/gtest.h>

#include <array>
#include <deque>

#include "actor_sync.h"
#include "animation_sync.h"
#include "pose_interpolator.h"

TEST(AnimationSync, GothicStrafeCyclesAreSampledForEveryWeaponAndWalkMode) {
  for (const auto* name : {"T_RUNSTRAFEL", "T_RUNSTRAFER", "T_WALKSTRAFEL", "T_WALKSTRAFER",
                           "T_1HRUNSTRAFEL", "T_2HWALKSTRAFER", "T_BOWSNEAKSTRAFEL",
                           "T_CBOWRUNSTRAFER", "T_MAGWALKSTRAFEL", "T_WALKWSTRAFER"}) {
    EXPECT_TRUE(gmp::IsSampledPlayerAnimation(name, false)) << name;
  }
}

TEST(AnimationSync, RuntimeLoopsAreSampledWithoutDependingOnAnSPrefix) {
  EXPECT_TRUE(gmp::IsSampledPlayerAnimation("T_CUSTOMLOCOMOTION", true));
  EXPECT_FALSE(gmp::IsSampledPlayerAnimation("T_CUSTOMLOCOMOTION", false));
}

TEST(AnimationSync, FiniteAttacksAndStandUpTransitionsRemainActions) {
  for (const auto* name : {"T_1HATTACKMOVE", "T_FISTATTACK", "T_FALLEN_2_STAND", "T_WOUNDED_2_STAND"}) {
    EXPECT_FALSE(gmp::IsSampledPlayerAnimation(name, false)) << name;
  }
}

TEST(AnimationSync, DamageReactionsAreRecognizedWithoutClassifyingAttacksOrRecoveryAsHits) {
  for (const auto* name : {"T_GOTHIT", "T_STUMBLE", "T_STUMBLEB", "T_1HSTUMBLEB", "T_WOLFSTUMBLE"})
    EXPECT_TRUE(gmp::IsDamageReactionAnimation(name)) << name;
  for (const auto* name : {"T_1HATTACKMOVE", "T_WOUNDED_2_STAND", "S_DEAD", "S_RUN", "T_STUMBLEB_2_STAND"})
    EXPECT_FALSE(gmp::IsDamageReactionAnimation(name)) << name;
}

TEST(AnimationSync, TurningIsExcludedWithoutDiscardingNamesContainingReturn) {
  EXPECT_FALSE(gmp::IsSampledPlayerAnimation("T_1HRUNTURNL", true));
  EXPECT_FALSE(gmp::IsSampledPlayerAnimation("T_SWIMTURNR", true));
  EXPECT_TRUE(gmp::IsSampledPlayerAnimation("S_RETURN_TO_CAMP", true));
}

TEST(AnimationSync, LookAndHandOverlaysDoNotHideStrafing) {
  const std::array<gmp::PlayerAnimationSample, 3> animations{{
      {17, "T_RUNSTRAFEL", 1, true},
      {23, "C_LOOK_1", 20, true, false, false},
      {26, "S_TORCH", 5, true},
  }};
  EXPECT_EQ(gmp::SelectPlayerPoseAnimation(animations), 17);
}

TEST(AnimationSync, CastingPhaseTakesPriorityOverUnderlyingMagicLocomotion) {
  const std::array<gmp::PlayerAnimationSample, 3> animations{{
      {17, "S_MAGRUN", 1, true},
      {18, "S_FIBCAST", 1, true, false, true, true},
      {19, "C_LOOK_1", 20, true, false, false},
  }};
  EXPECT_EQ(gmp::SelectPlayerPoseAnimation(animations), 18);
}

TEST(AnimationSync, FadingCastingPhaseDoesNotBlockMovementRecovery) {
  const std::array<gmp::PlayerAnimationSample, 2> animations{{
      {17, "S_MAGRUN", 1, true},
      {18, "S_FIBSHOOT", 1, true, true, true, true},
  }};
  EXPECT_EQ(gmp::SelectPlayerPoseAnimation(animations), 17);
}

TEST(AnimationSync, FadingPreviousMovementCannotReplaceActiveMovement) {
  const std::array<gmp::PlayerAnimationSample, 3> animations{{
      {17, "T_RUNSTRAFEL", 1, true},
      {18, "T_RUNSTRAFER", 1, true, true},
      {19, "T_1HATTACK", 5, false},
  }};
  EXPECT_EQ(gmp::SelectPlayerPoseAnimation(animations), 17);
}

TEST(AnimationSync, PersistentStationaryStatesRemainAvailable) {
  const std::array<gmp::PlayerAnimationSample, 2> animations{{
      {10, "S_BENCH_S1", 1, true},
      {11, "C_LOOK_1", 20, true, false, false},
  }};
  EXPECT_EQ(gmp::SelectPlayerPoseAnimation(animations), 10);
}

TEST(AnimationSync, StandaloneSkeletalAnimationsRetainTheirNameAndLayer) {
  const std::array<gmp::PlayerAnimationSample, 2> animations{{
      {10, "C_CUSTOMGESTURE", 4, true},
      {11, "C_LOOK_1", 20, true, false, false},
  }};
  EXPECT_EQ(gmp::SelectPlayerPoseAnimation(animations), 10);
}

TEST(AnimationSync, FiniteAnimationIsNeverSelectedForPoseRecovery) {
  const std::array<gmp::PlayerAnimationSample, 1> animations{{
      {19, "T_1HATTACK", 1, false},
  }};
  EXPECT_EQ(gmp::SelectPlayerPoseAnimation(animations), -1);
}

TEST(AnimationSync, WoundedAndDeadAnimationsAreOwnedByLifecycleSynchronization) {
  for (const auto* name : {"S_WOUNDED", "S_WOUNDEDB", "S_DEAD", "S_DEADB", "T_DEAD", "T_DEADB",
                           "T_STAND_2_WOUNDED", "T_STAND_2_WOUNDEDB", "T_WOUNDED_2_DEAD",
                           "T_WOUNDEDB_2_DEADB", "T_WOUNDED_TRY", "T_WOUNDEDB_TRY",
                           "T_WOUNDED_2_STAND", "T_WOUNDEDB_2_STAND"}) {
    EXPECT_TRUE(gmp::IsPlayerLifecycleAnimation(name)) << name;
    EXPECT_FALSE(gmp::IsSampledPlayerAnimation(name, true)) << name;
  }
}

TEST(AnimationSync, WoundedPoseCannotReplaceRecoveredLocomotion) {
  const std::array<gmp::PlayerAnimationSample, 2> animations{{
      {10, "S_RUN", 1, true},
      {11, "S_WOUNDEDB", 20, true},
  }};
  EXPECT_EQ(gmp::SelectPlayerPoseAnimation(animations), 10);
}

TEST(AnimationSync, OrdinaryFallsAndUnrelatedStatesAreNotTreatedAsUnconsciousness) {
  for (const auto* name : {"S_FALLEN", "S_FALLENB", "T_FALLEN_2_STAND", "T_FALLENB_2_STAND",
                           "S_SLEEP", "S_HIT", "S_DEADLIFT", "S_WOUNDED_POSE"}) {
    EXPECT_FALSE(gmp::IsPlayerLifecycleAnimation(name)) << name;
  }
  EXPECT_TRUE(gmp::IsSampledPlayerAnimation("S_FALLEN", true));
  EXPECT_TRUE(gmp::IsSampledPlayerAnimation("S_FALLENB", true));
}

TEST(AnimationSync, GetUpDetectionIncludesNormalFallsWithoutAssigningTerminalLife) {
  for (const auto* name : {"T_WOUNDED_2_STAND", "T_WOUNDEDB_2_STAND", "T_FALLEN_2_STAND",
                           "T_FALLENB_2_STAND", "T_FALLDN_2_STAND"}) {
    EXPECT_TRUE(gmp::IsPlayerGetUpAnimation(name)) << name;
  }
  EXPECT_FALSE(gmp::IsPlayerGetUpAnimation("S_SLEEP"));
  EXPECT_FALSE(gmp::IsPlayerGetUpAnimation("T_POTION_S0_2_STAND"));
}

TEST(AnimationSync, BufferedAnimationsKeepPlayingWith80msSamplesAnd120msDelay) {
  gmp::PoseInterpolator::Settings settings;
  settings.delay_ms = 120.0;
  gmp::PoseInterpolator timeline(settings);
  ActorComponentSequences received_sequences;
  gmp::ActorPresentationEpochs correction_epochs;
  struct PendingPose {
    std::uint64_t source_time;
    ActorComponentSequences received;
    gmp::ActorPresentationEpochs corrections;
  };
  std::deque<PendingPose> pending;
  std::size_t presented = 0;
  for (std::uint32_t sample = 0; sample < 10; ++sample) {
    const std::uint64_t now = 1000 + sample * 80;
    ASSERT_TRUE(timeline.Push({{static_cast<float>(sample * 10), 0, 0}, {0, 0, 1}}, now, now));
    ASSERT_EQ(received_sequences.Accept(kSyncAll, sample + 1), kSyncAll);
    pending.push_back({now, received_sequences, correction_epochs});
    // Even frequent resource corrections must not starve unrelated animations.
    correction_epochs.Invalidate(kSyncHealth | kSyncMana);
    while (!pending.empty() && pending.front().source_time <= timeline.RenderTime(now)) {
      const auto due = pending.front();
      pending.pop_front();
      EXPECT_EQ(received_sequences.CurrentComponents(kSyncPose, due.received), 0);
      EXPECT_EQ(correction_epochs.CurrentComponents(kSyncPose, due.corrections), kSyncPose);
      ++presented;
    }
  }
  EXPECT_EQ(presented, 8);
  EXPECT_EQ(pending.size(), 2);
}

TEST(AnimationSync, ExplicitAnimationCorrectionInvalidatesAlreadyBufferedPose) {
  gmp::PoseInterpolator::Settings settings;
  settings.delay_ms = 120.0;
  gmp::PoseInterpolator timeline(settings);
  gmp::ActorPresentationEpochs correction_epochs;
  ASSERT_TRUE(timeline.Push({{0, 0, 0}, {0, 0, 1}}, 1000, 1000));
  const auto captured = correction_epochs;
  ASSERT_TRUE(timeline.Push({{10, 0, 0}, {0, 0, 1}}, 1080, 1080));
  EXPECT_LT(timeline.RenderTime(1080), 1000);
  correction_epochs.Invalidate(kSyncPose);
  EXPECT_EQ(timeline.RenderTime(1120), 1000);
  EXPECT_EQ(correction_epochs.CurrentComponents(kSyncPose, captured), 0);
  const auto following_command = correction_epochs;
  EXPECT_EQ(correction_epochs.CurrentComponents(kSyncPose, following_command), kSyncPose);
}

TEST(AnimationSync, EquipmentCorrectionInvalidatesOnlyBufferedEquipmentAndCombat) {
  gmp::PoseInterpolator::Settings settings;
  settings.delay_ms = 120.0;
  gmp::PoseInterpolator timeline(settings);
  gmp::ActorPresentationEpochs correction_epochs;
  constexpr std::uint16_t presentation = kSyncEquipment | kSyncCombat | kSyncPose;
  ASSERT_TRUE(timeline.Push({{0, 0, 0}, {0, 0, 1}}, 1000, 1000));
  const auto captured = correction_epochs;
  ASSERT_TRUE(timeline.Push({{10, 0, 0}, {0, 0, 1}}, 1080, 1080));
  correction_epochs.Invalidate(kSyncEquipment | kSyncCombat);
  EXPECT_EQ(timeline.RenderTime(1120), 1000);
  EXPECT_EQ(correction_epochs.CurrentComponents(presentation, captured), kSyncPose);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
