#include <gtest/gtest.h>

#include "player_manager.h"

TEST(PlayerManagerConnectionTest, KeepsSessionIdSeparateFromTransportHandle) {
  PlayerManager players;
  const auto id = players.AddPlayer(70001, "first", 2);
  EXPECT_EQ(id, 1u);
  ASSERT_TRUE(players.GetPlayerId(70001));
  EXPECT_EQ(*players.GetPlayerId(70001), id);
  ASSERT_TRUE(players.GetConnectionHandle(id));
  EXPECT_EQ(*players.GetConnectionHandle(id), 70001u);
}

TEST(PlayerManagerConnectionTest, FullServerDoesNotCreatePlayerZeroOrOverwriteExistingPlayer) {
  PlayerManager players;
  const auto first = players.AddPlayer(70001, "first", 1);
  ASSERT_NE(first, 0u);
  EXPECT_EQ(players.AddPlayer(70002, "second", 1), 0u);
  EXPECT_EQ(players.GetPlayerCount(), 1u);
  EXPECT_FALSE(players.HasPlayer(0));
  EXPECT_FALSE(players.HasConnection(70002));
  ASSERT_TRUE(players.GetPlayer(first));
  EXPECT_EQ(players.GetPlayer(first)->get().name, "first");
}

TEST(PlayerManagerConnectionTest, RejectsInvalidAndDuplicateConnectionsWithoutAllocatingIds) {
  PlayerManager players;
  EXPECT_EQ(players.AddPlayer(Net::kInvalidConnection, "invalid", 2), 0u);
  const auto first = players.AddPlayer(70001, "first", 2);
  EXPECT_EQ(players.AddPlayer(70001, "duplicate", 2), 0u);
  EXPECT_EQ(players.GetPlayerCount(), 1u);
  ASSERT_TRUE(players.GetPlayer(first));
  EXPECT_EQ(players.GetPlayer(first)->get().name, "first");
}

TEST(PlayerManagerConnectionTest, DisconnectAndReconnectReplacesOnlyTransportMapping) {
  PlayerManager players;
  const auto first = players.AddPlayer(70001, "first", 1);
  ASSERT_TRUE(players.RemovePlayerByConnection(70001));
  EXPECT_FALSE(players.GetPlayerId(70001));
  EXPECT_FALSE(players.GetConnectionHandle(first));
  const auto reconnected = players.AddPlayer(80001, "reconnected", 1);
  EXPECT_NE(reconnected, 0u);
  EXPECT_FALSE(players.HasConnection(70001));
  ASSERT_TRUE(players.GetConnectionHandle(reconnected));
  EXPECT_EQ(*players.GetConnectionHandle(reconnected), 80001u);
}

TEST(PlayerManagerStateSequenceTest, RejectsZeroDuplicateAndOlderClientSnapshots) {
  PlayerManager::Player player;
  EXPECT_FALSE(player.AcceptClientStateSequence(0));
  EXPECT_TRUE(player.AcceptClientStateSequence(10));
  EXPECT_FALSE(player.AcceptClientStateSequence(10));
  EXPECT_FALSE(player.AcceptClientStateSequence(9));
  EXPECT_TRUE(player.AcceptClientStateSequence(11));
}

TEST(PlayerManagerStateSequenceTest, AcceptsCounterWrapAndRejectsOldAndAmbiguousCounters) {
  PlayerManager::Player player;
  EXPECT_TRUE(player.AcceptClientStateSequence(0xfffffffeu));
  EXPECT_TRUE(player.AcceptClientStateSequence(0xffffffffu));
  EXPECT_TRUE(player.AcceptClientStateSequence(1));
  EXPECT_FALSE(player.AcceptClientStateSequence(0xffffffffu));
  EXPECT_FALSE(player.AcceptClientStateSequence(0x80000001u));
  EXPECT_TRUE(player.AcceptClientStateSequence(2));
}

TEST(PlayerManagerStateSequenceTest, ReusedPlayerSlotStartsNewClientSequenceHistory) {
  PlayerManager players;
  const auto old_id = players.AddPlayer(70001, "first", 1);
  ASSERT_NE(old_id, 0u);
  ASSERT_TRUE(players.GetPlayer(old_id)->get().AcceptClientStateSequence(100));
  ASSERT_TRUE(players.RemovePlayerByConnection(70001));
  const auto new_id = players.AddPlayer(80001, "reconnected", 1);
  ASSERT_EQ(new_id, old_id);
  EXPECT_TRUE(players.GetPlayer(new_id)->get().AcceptClientStateSequence(1));
  EXPECT_FALSE(players.GetPlayer(new_id)->get().AcceptClientStateSequence(1));
}
TEST(PlayerManagerStateSequenceTest, CommandsAndIncarnationsRejectOldEchoesWithoutConsumingNewSequence) {
  PlayerManager::Player player;
  const auto generation = player.actor_generation;
  player.command_revision = 10;
  EXPECT_FALSE(player.AcceptClientState(generation, 9, 40));
  EXPECT_FALSE(player.AcceptClientState(generation + 1, 10, 40));
  EXPECT_FALSE(player.last_client_state_sequence);
  EXPECT_TRUE(player.AcceptClientState(generation, 10, 40));
  player.command_revision = 11;
  EXPECT_FALSE(player.AcceptClientState(generation, 10, 41));
  EXPECT_TRUE(player.AcceptClientState(generation, 11, 41));
}

TEST(PlayerManagerConnectionTest, ReusedSlotAndManagerResetNeverReuseActorGeneration) {
  PlayerManager players;
  const auto id = players.AddPlayer(70001, "first", 1);
  const auto generation = players.GetPlayer(id)->get().actor_generation;
  ASSERT_TRUE(players.RemovePlayerByConnection(70001));
  const auto reused = players.AddPlayer(80001, "second", 1);
  ASSERT_EQ(reused, id);
  EXPECT_NE(players.GetPlayer(reused)->get().actor_generation, generation);
  players.Clear();
  const auto after_reset = players.AddPlayer(90001, "third", 1);
  EXPECT_NE(players.GetPlayer(after_reset)->get().actor_generation, generation);
}

TEST(ActorComponentSequenceTest, OlderIndependentComponentIsDeliveredWithoutRewindingMovement) {
  ActorComponentSequences sequences;
  EXPECT_EQ(sequences.Accept(kSyncPosition, 11), kSyncPosition);
  EXPECT_EQ(sequences.Accept(kSyncHealth, 10), kSyncHealth);
  EXPECT_EQ(sequences.Accept(kSyncPosition | kSyncHealth, 10), 0);
  EXPECT_EQ(sequences.Accept(kSyncPosition | kSyncHealth, 12), kSyncPosition | kSyncHealth);
  EXPECT_EQ(sequences.Accept(kSyncHealth, 12), 0);
}

TEST(ActorComponentSequenceTest, WrapDoesNotAcceptAmbiguousOrPreviousEraRevision) {
  ActorComponentSequences sequences;
  sequences.Reset(0xfffffffe);
  EXPECT_EQ(sequences.Accept(kSyncEquipment, 1), kSyncEquipment);
  EXPECT_EQ(sequences.Accept(kSyncEquipment, 0xfffffffe), 0);
  EXPECT_EQ(sequences.Accept(kSyncEquipment, 0x80000001), 0);
}

TEST(ActorComponentSequenceTest, DeferredPresentationSurvivesUnrelatedCorrectionsOnly) {
  ActorComponentSequences sequences;
  sequences.Reset(10);
  const auto captured = sequences;
  const auto presentation = kSyncEquipment | kSyncCombat | kSyncPose;
  sequences.Accept(kSyncHealth, 11);
  EXPECT_EQ(sequences.CurrentComponents(presentation, captured), presentation);
  sequences.Accept(kSyncCombat, 12);
  EXPECT_EQ(sequences.CurrentComponents(presentation, captured), kSyncEquipment | kSyncPose);
  sequences.Reset();
  EXPECT_EQ(sequences.CurrentComponents(presentation, captured), 0);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
