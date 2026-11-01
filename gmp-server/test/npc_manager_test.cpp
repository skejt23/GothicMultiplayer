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

#include <chrono>
#include <cstdint>
#include <limits>
#include <string>

#include "npc_manager.h"
#include "npc_packets.h"

namespace {

using namespace std::chrono_literals;

class NpcManagerTest : public ::testing::Test {
protected:
  NpcManager manager;
  const NpcManager::TimePoint start{10s};
  const NpcManager::NpcId id = manager.Create("Guard", "PC_HERO", "NEWWORLD.ZEN", 20);
};

TEST_F(NpcManagerTest, AllocatesOutsidePlayerSlotsAndNeverReusesDestroyedIds) {
  EXPECT_EQ(id, 21u);
  ASSERT_TRUE(manager.Remove(id));
  EXPECT_FALSE(manager.GetNpc(id));
  EXPECT_FALSE(manager.Remove(id));
  const auto replacement = manager.Create("Guard", "PC_HERO", "NEWWORLD.ZEN", 20);
  EXPECT_EQ(replacement, 22u);
  EXPECT_EQ(manager.GetNpcCount(), 1u);

  const auto snapshot = manager.GetNpcIds();
  ASSERT_EQ(snapshot.size(), 1u);
  EXPECT_EQ(snapshot.front(), replacement);
  ASSERT_TRUE(manager.Remove(replacement));
  EXPECT_EQ(snapshot.front(), replacement);
  EXPECT_FALSE(manager.GetNpc(snapshot.front()));
}

TEST(NpcManagerAllocationTest, DoesNotWrapIdsAtSignedLuaRangeLimit) {
  NpcManager manager;
  constexpr auto max_id = static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max());
  EXPECT_EQ(manager.Create("Guard", "PC_HERO", "NEWWORLD.ZEN", max_id - 1), max_id);
  ASSERT_TRUE(manager.Remove(max_id));
  EXPECT_EQ(manager.Create("Guard", "PC_HERO", "NEWWORLD.ZEN", 20), 0u);
  EXPECT_EQ(manager.Create("Guard", "PC_HERO", "NEWWORLD.ZEN", std::numeric_limits<std::uint32_t>::max()), 0u);
}

TEST_F(NpcManagerTest, RejectsUnserializableCreationDataWithoutConsumingAnId) {
  EXPECT_EQ(manager.Create(std::string(256, 'N'), "PC_HERO", "NEWWORLD.ZEN", 20), 0u);
  EXPECT_EQ(manager.Create("Guard", "", "NEWWORLD.ZEN", 20), 0u);
  EXPECT_EQ(manager.Create("Guard", std::string("PC\0HERO", 7), "NEWWORLD.ZEN", 20), 0u);
  EXPECT_EQ(manager.Create("Guard", "PC_HERO", "", 20), 0u);
  EXPECT_EQ(manager.Create("Guard", "PC_HERO", "NEWWORLD.ZEN", 20), id + 1);
}

TEST_F(NpcManagerTest, RejectsOversizedQueuesAndInvalidAnimationRequests) {
  EXPECT_EQ(manager.QueueAnimation(id, "", 1000, start), 0u);
  EXPECT_EQ(manager.QueueAnimation(id, std::string(kMaxPlayerAnimationNameLength + 1, 'A'), 1000, start), 0u);
  EXPECT_EQ(manager.QueueAnimation(id, std::string("S\0RUN", 5), 1000, start), 0u);
  EXPECT_EQ(manager.QueueAnimation(id, "S_RUN", 0, start), 0u);
  EXPECT_EQ(manager.QueueAnimation(id, "S_RUN", NpcManager::kMaxActionTimeoutMs + 1, start), 0u);
  EXPECT_EQ(manager.QueueAnimation(0, "S_RUN", 1000, start), 0u);
  for (std::size_t index = 0; index < NpcManager::kMaxQueuedActions; ++index) {
    EXPECT_EQ(manager.QueueAnimation(id, "S_RUN", 1000, start), index + 1);
  }
  EXPECT_EQ(manager.QueueAnimation(id, "S_RUN", 1000, start), 0u);
  EXPECT_EQ(manager.GetNpc(id)->get().actions.size(), NpcManager::kMaxQueuedActions);
}

TEST_F(NpcManagerTest, PersistentAnimationChangesDoNotRestartOrCancelQueuedWork) {
  ASSERT_TRUE(manager.SetHost(id, 1, start));
  const auto action = manager.QueueAnimation(id, "T_STAND_2_SIT", 1000, start);
  const auto epoch = manager.GetNpc(id)->get().control_epoch;
  const auto started_at = manager.GetNpc(id)->get().actions.front().started_at;
  EXPECT_EQ(manager.GetNpc(id)->get().animation_revision, 1u);
  EXPECT_TRUE(manager.GetNpc(id)->get().animation.empty());

  ASSERT_TRUE(manager.SetAnimation(id, "S_FISTRUNL"));
  EXPECT_EQ(manager.GetNpc(id)->get().animation, "S_FISTRUNL");
  EXPECT_EQ(manager.GetNpc(id)->get().animation_revision, 2u);
  ASSERT_TRUE(manager.SetAnimation(id, "S_FISTRUNL"));
  EXPECT_EQ(manager.GetNpc(id)->get().animation_revision, 2u);
  EXPECT_EQ(manager.GetNpc(id)->get().control_epoch, epoch);
  EXPECT_EQ(manager.GetNpc(id)->get().actions.front().started_at, started_at);
  EXPECT_EQ(manager.GetNpc(id)->get().actions.front().id, action);

  ASSERT_TRUE(manager.SetAnimation(id, "S_FISTRUN"));
  EXPECT_EQ(manager.GetNpc(id)->get().animation_revision, 3u);
  EXPECT_TRUE(manager.FinishAction(id, 1, epoch, action, start + 1ms));
  EXPECT_EQ(manager.GetNpc(id)->get().animation, "S_FISTRUN");
  ASSERT_TRUE(manager.ClearActions(id));
  EXPECT_EQ(manager.GetNpc(id)->get().animation, "S_FISTRUN");
  ASSERT_TRUE(manager.SetAnimation(id, ""));
  EXPECT_TRUE(manager.GetNpc(id)->get().animation.empty());
  EXPECT_EQ(manager.GetNpc(id)->get().animation_revision, 4u);
}

TEST_F(NpcManagerTest, PersistentAnimationRejectsInvalidNamesAndNeverWrapsRevision) {
  ASSERT_TRUE(manager.SetAnimation(id, "S_FISTRUNL"));
  const auto revision = manager.GetNpc(id)->get().animation_revision;
  EXPECT_FALSE(manager.SetAnimation(0, "S_FISTRUN"));
  EXPECT_FALSE(manager.SetAnimation(id, std::string(kMaxPlayerAnimationNameLength + 1, 'A')));
  EXPECT_FALSE(manager.SetAnimation(id, std::string("S\0RUN", 5)));
  EXPECT_EQ(manager.GetNpc(id)->get().animation, "S_FISTRUNL");
  EXPECT_EQ(manager.GetNpc(id)->get().animation_revision, revision);

  auto& npc = manager.GetNpc(id)->get();
  npc.animation_revision = std::numeric_limits<std::uint32_t>::max();
  EXPECT_TRUE(manager.SetAnimation(id, "S_FISTRUNL"));
  EXPECT_FALSE(manager.SetAnimation(id, "S_FISTRUN"));
  EXPECT_FALSE(manager.SetAnimation(id, ""));
  EXPECT_EQ(npc.animation, "S_FISTRUNL");
  EXPECT_EQ(npc.animation_revision, std::numeric_limits<std::uint32_t>::max());
}

TEST_F(NpcManagerTest, OnlyCurrentHostAndEpochMayFinishCurrentHeadOnce) {
  ASSERT_TRUE(manager.SetHost(id, 1, start));
  const auto first = manager.QueueAnimation(id, "T_STAND_2_SIT", 1000, start);
  const auto second = manager.QueueAnimation(id, "T_SIT_2_STAND", 1000, start);
  const auto epoch = manager.GetNpc(id)->get().control_epoch;

  EXPECT_FALSE(manager.FinishAction(id, 0, epoch, first, start + 1ms));
  EXPECT_FALSE(manager.FinishAction(id, 2, epoch, first, start + 1ms));
  EXPECT_FALSE(manager.FinishAction(id, 1, epoch - 1, first, start + 1ms));
  EXPECT_FALSE(manager.FinishAction(id, 1, epoch, second, start + 1ms));
  EXPECT_FALSE(manager.FinishAction(0, 1, epoch, first, start + 1ms));
  EXPECT_FALSE(manager.IsActionFinished(id, first));

  const auto finished = manager.FinishAction(id, 1, epoch, first, start + 1ms);
  ASSERT_TRUE(finished);
  EXPECT_EQ(finished->id, first);
  EXPECT_TRUE(manager.IsActionFinished(id, first));
  EXPECT_FALSE(manager.IsActionFinished(id, second));
  EXPECT_FALSE(manager.IsActionFinished(id, 0));
  EXPECT_FALSE(manager.IsActionFinished(id, second + 1));
  EXPECT_EQ(manager.GetNpc(id)->get().actions.front().id, second);
  EXPECT_EQ(manager.GetNpc(id)->get().control_epoch, epoch + 1);
  EXPECT_FALSE(manager.FinishAction(id, 1, epoch, first, start + 2ms));
  EXPECT_FALSE(manager.FinishAction(id, 1, epoch, second, start + 2ms));
  EXPECT_TRUE(manager.FinishAction(id, 1, epoch + 1, second, start + 2ms));
  EXPECT_TRUE(manager.GetNpc(id)->get().actions.empty());
}

TEST_F(NpcManagerTest, HostHandoverAndBaselineResetInvalidateEarlierReports) {
  ASSERT_TRUE(manager.SetHost(id, 1, start));
  const auto action = manager.QueueAnimation(id, "T_STAND_2_SIT", 1000, start);
  const auto first_epoch = manager.GetNpc(id)->get().control_epoch;
  ASSERT_TRUE(manager.SetHost(id, 2, start + 900ms));
  EXPECT_FALSE(manager.FinishAction(id, 1, first_epoch, action, start + 901ms));
  EXPECT_FALSE(manager.FinishAction(id, 2, first_epoch, action, start + 901ms));
  EXPECT_EQ(manager.GetNpc(id)->get().actions.front().started_at, start);

  ASSERT_TRUE(manager.SetHost(id, 2, start + 950ms));
  EXPECT_EQ(manager.GetNpc(id)->get().control_epoch, first_epoch + 1);
  EXPECT_EQ(manager.GetNpc(id)->get().actions.front().started_at, start);
  ASSERT_TRUE(manager.BumpControlEpoch(id, start + 1000ms));
  EXPECT_FALSE(manager.FinishAction(id, 2, first_epoch + 1, action, start + 1001ms));
  EXPECT_FALSE(manager.FinishAction(id, 2, first_epoch + 2, action, start + 1001ms));
  EXPECT_EQ(manager.ExpireActions(id, start + 1001ms).size(), 1u);
}

TEST_F(NpcManagerTest, HostLossPreservesActionPhaseAndDeadline) {
  ASSERT_TRUE(manager.SetHost(id, 1, start));
  const auto action = manager.QueueAnimation(id, "T_STAND_2_SIT", 1000, start);
  ASSERT_TRUE(manager.SetHost(id, 0, start + 500ms));
  EXPECT_EQ(manager.GetNpc(id)->get().actions.front().started_at, start);
  EXPECT_TRUE(manager.ExpireActions(id, start + 999ms).empty());
  EXPECT_FALSE(manager.FinishAction(id, 0, manager.GetNpc(id)->get().control_epoch, action, start + 999ms));
  const auto expired = manager.ExpireActions(id, start + 1000ms);
  ASSERT_EQ(expired.size(), 1u);
  EXPECT_EQ(expired.front().id, action);
  EXPECT_TRUE(manager.IsActionFinished(id, action));
}

TEST_F(NpcManagerTest, ClearingQueueInvalidatesReportsAndDoesNotReuseActionIds) {
  ASSERT_TRUE(manager.SetHost(id, 1, start));
  const auto first = manager.QueueAnimation(id, "T_STAND_2_SIT", 1000, start);
  const auto second = manager.QueueAnimation(id, "T_SIT_2_STAND", 1000, start);
  const auto epoch = manager.GetNpc(id)->get().control_epoch;
  const auto cleared = manager.ClearActions(id);
  ASSERT_TRUE(cleared);
  ASSERT_EQ(cleared->size(), 2u);
  EXPECT_EQ(cleared->front().id, first);
  EXPECT_EQ(cleared->back().id, second);
  EXPECT_TRUE(manager.IsActionFinished(id, first));
  EXPECT_TRUE(manager.IsActionFinished(id, second));
  EXPECT_EQ(manager.GetNpc(id)->get().control_epoch, epoch + 1);
  EXPECT_FALSE(manager.FinishAction(id, 1, epoch, first, start + 1ms));
  const auto replacement = manager.QueueAnimation(id, "T_STAND_2_SIT", 1000, start + 1ms);
  EXPECT_GT(replacement, second);
  EXPECT_FALSE(manager.IsActionFinished(id, replacement));
  EXPECT_FALSE(manager.ClearActions(0));
}

TEST_F(NpcManagerTest, TimeoutExpiresHeadAndStartsNextActionWithFreshEpoch) {
  ASSERT_TRUE(manager.SetHost(id, 1, start));
  const auto first = manager.QueueAnimation(id, "T_STAND_2_SIT", 1000, start);
  const auto second = manager.QueueAnimation(id, "T_SIT_2_STAND", 1000, start);
  const auto epoch = manager.GetNpc(id)->get().control_epoch;
  EXPECT_TRUE(manager.ExpireActions(id, start + 999ms).empty());
  EXPECT_FALSE(manager.FinishAction(id, 1, epoch, first, start + 1000ms));

  const auto expired = manager.ExpireActions(id, start + 1000ms);
  ASSERT_EQ(expired.size(), 1u);
  EXPECT_EQ(expired.front().id, first);
  EXPECT_TRUE(manager.IsActionFinished(id, first));
  EXPECT_FALSE(manager.IsActionFinished(id, second));
  EXPECT_EQ(manager.GetNpc(id)->get().actions.front().started_at, start + 1000ms);
  EXPECT_EQ(manager.GetNpc(id)->get().control_epoch, epoch + 1);
  EXPECT_TRUE(manager.FinishAction(id, 1, epoch + 1, second, start + 1999ms));
}

TEST_F(NpcManagerTest, RepeatedHandoverCannotExtendTotalLifetime) {
  const auto action = manager.QueueAnimation(id, "S_RUN", 30000, start);
  ASSERT_TRUE(manager.SetHost(id, 1, start + 20s));
  ASSERT_TRUE(manager.SetHost(id, 2, start + 40s));
  ASSERT_TRUE(manager.SetHost(id, 1, start + 49s));
  EXPECT_TRUE(manager.ExpireActions(id, start + 49999ms).empty());
  const auto expired = manager.ExpireActions(id, start + 50s);
  ASSERT_EQ(expired.size(), 1u);
  EXPECT_EQ(expired.front().id, action);
}

TEST_F(NpcManagerTest, ExpiringQueuedActionDoesNotMarkAnEarlierPendingActionFinished) {
  const auto first = manager.QueueAnimation(id, "S_RUN", 30000, start);
  const auto second = manager.QueueAnimation(id, "S_RUN", 30000, start);
  // Exercise completion bookkeeping independently of FIFO enqueue timing.
  // A queued entry can be cancelled/expired without completing its predecessor.
  manager.GetNpc(id)->get().actions.back().queued_at = start - 60s;
  const auto epoch = manager.GetNpc(id)->get().control_epoch;
  const auto expired = manager.ExpireActions(id, start);
  ASSERT_EQ(expired.size(), 1u);
  EXPECT_EQ(expired.front().id, second);
  EXPECT_FALSE(manager.IsActionFinished(id, first));
  EXPECT_TRUE(manager.IsActionFinished(id, second));
  EXPECT_EQ(manager.GetNpc(id)->get().control_epoch, epoch);
}

TEST_F(NpcManagerTest, ExhaustedEpochAndActionIdsAreRejectedWithoutWrapping) {
  constexpr auto exhausted = std::numeric_limits<std::uint32_t>::max();
  auto& npc = manager.GetNpc(id)->get();
  npc.next_action_id = exhausted - 1;
  EXPECT_EQ(manager.QueueAnimation(id, "S_RUN", 1000, start), exhausted - 1);
  EXPECT_EQ(manager.QueueAnimation(id, "S_RUN", 1000, start), 0u);
  EXPECT_EQ(npc.next_action_id, exhausted);

  ASSERT_TRUE(manager.SetHost(id, 1, start));
  npc.control_epoch = exhausted;
  EXPECT_FALSE(manager.SetHost(id, 2, start));
  EXPECT_FALSE(manager.BumpControlEpoch(id, start));
  EXPECT_FALSE(manager.ClearActions(id));
  EXPECT_FALSE(manager.FinishAction(id, 1, exhausted, exhausted - 1, start + 1ms));
  EXPECT_TRUE(manager.ExpireActions(id, start + 1s).empty());
  EXPECT_EQ(npc.control_epoch, exhausted);
  EXPECT_EQ(npc.host_player_id, 1u);
  EXPECT_EQ(npc.actions.size(), 1u);
}

template <typename Packet>
std::vector<std::uint8_t> EncodeNpcPacket(const Packet& packet) {
  std::vector<std::uint8_t> bytes;
  const auto size = bitsery::quickSerialization<bitsery::OutputBufferAdapter<std::vector<std::uint8_t>>>(bytes, packet);
  bytes.resize(size);
  return bytes;
}

TEST(NpcPacketTest, SpawnPreservesActorSnapshotAndControlBaseline) {
  NpcSpawnPacket original;
  original.actor.packet_type = Net::PT_PLAYER_SPAWN;
  original.actor.player_id = 21;
  original.actor.player_name = "Guard";
  original.actor.instance = "PC_HERO";
  original.actor.max_health = 250;
  original.actor.health = 125;
  original.actor.overlays = {"HUMANS_MILITIA.MDS"};
  original.control.npc_id = 21;
  original.control.control_epoch = 9;
  original.control.host_player_id = 2;
  original.control.action_id = 7;
  original.control.animation = "T_STAND_2_SIT";
  original.control.timeout_ms = 8500;
  original.control.position = {100.0f, 200.0f, 300.0f};
  original.animation.npc_id = 21;
  original.animation.revision = 4;
  original.animation.animation = "S_FISTRUNL";
  auto bytes = EncodeNpcPacket(original);
  NpcSpawnPacket decoded;
  using Adapter = bitsery::InputBufferAdapter<std::uint8_t*>;
  const auto result = bitsery::quickDeserialization<Adapter>({bytes.data(), bytes.size()}, decoded);
  ASSERT_EQ(result.first, bitsery::ReaderError::NoError);
  ASSERT_TRUE(result.second);
  EXPECT_EQ(decoded.packet_type, Net::PT_NPC_SPAWN);
  EXPECT_EQ(decoded.actor.player_id, original.actor.player_id);
  EXPECT_EQ(decoded.actor.instance, original.actor.instance);
  EXPECT_EQ(decoded.actor.health, original.actor.health);
  EXPECT_EQ(decoded.actor.max_health, original.actor.max_health);
  EXPECT_EQ(decoded.actor.overlays, original.actor.overlays);
  EXPECT_EQ(decoded.control.npc_id, original.control.npc_id);
  EXPECT_EQ(decoded.control.control_epoch, original.control.control_epoch);
  EXPECT_EQ(decoded.control.host_player_id, original.control.host_player_id);
  EXPECT_EQ(decoded.control.action_id, original.control.action_id);
  EXPECT_EQ(decoded.control.animation, original.control.animation);
  EXPECT_EQ(decoded.control.timeout_ms, original.control.timeout_ms);
  EXPECT_FLOAT_EQ(decoded.control.position.y, original.control.position.y);
  EXPECT_EQ(decoded.animation.packet_type, Net::PT_NPC_ANIMATION);
  EXPECT_EQ(decoded.animation.npc_id, original.animation.npc_id);
  EXPECT_EQ(decoded.animation.revision, original.animation.revision);
  EXPECT_EQ(decoded.animation.animation, original.animation.animation);
}

TEST(NpcPacketTest, PersistentAnimationRoundTripsAndRejectsTruncationOrTrailingData) {
  NpcAnimationPacket original;
  original.npc_id = 21;
  original.revision = 9;
  original.animation = std::string(kMaxPlayerAnimationNameLength, 'A');
  auto bytes = EncodeNpcPacket(original);
  using Adapter = bitsery::InputBufferAdapter<std::uint8_t*>;
  for (std::size_t length = 0; length < bytes.size(); ++length) {
    NpcAnimationPacket decoded;
    const auto result = bitsery::quickDeserialization<Adapter>({bytes.data(), length}, decoded);
    EXPECT_TRUE(result.first != bitsery::ReaderError::NoError || !result.second) << "prefix: " << length;
  }
  NpcAnimationPacket decoded;
  const auto valid = bitsery::quickDeserialization<Adapter>({bytes.data(), bytes.size()}, decoded);
  ASSERT_EQ(valid.first, bitsery::ReaderError::NoError);
  ASSERT_TRUE(valid.second);
  EXPECT_EQ(decoded.npc_id, original.npc_id);
  EXPECT_EQ(decoded.revision, original.revision);
  EXPECT_EQ(decoded.animation, original.animation);
  bytes.push_back(0);
  const auto extra = bitsery::quickDeserialization<Adapter>({bytes.data(), bytes.size()}, decoded);
  EXPECT_FALSE(extra.second);

  original.animation.clear();
  bytes = EncodeNpcPacket(original);
  const auto cleared = bitsery::quickDeserialization<Adapter>({bytes.data(), bytes.size()}, decoded);
  ASSERT_EQ(cleared.first, bitsery::ReaderError::NoError);
  ASSERT_TRUE(cleared.second);
  EXPECT_TRUE(decoded.animation.empty());
}

TEST(NpcPacketTest, TruncatedControlAndResultPacketsCannotBeAccepted) {
  NpcControlPacket original;
  original.npc_id = 21;
  original.control_epoch = 3;
  original.host_player_id = 1;
  original.action_id = 1;
  original.animation = std::string(kMaxPlayerAnimationNameLength, 'A');
  original.timeout_ms = 1000;
  auto bytes = EncodeNpcPacket(original);
  using Adapter = bitsery::InputBufferAdapter<std::uint8_t*>;
  for (std::size_t length = 0; length < bytes.size(); ++length) {
    NpcControlPacket decoded;
    const auto result = bitsery::quickDeserialization<Adapter>({bytes.data(), length}, decoded);
    EXPECT_TRUE(result.first != bitsery::ReaderError::NoError || !result.second) << "prefix: " << length;
  }
  NpcActionResultPacket report;
  report.npc_id = 21;
  report.control_epoch = 3;
  report.action_id = 1;
  report.success = 1;
  bytes = EncodeNpcPacket(report);
  for (std::size_t length = 0; length < bytes.size(); ++length) {
    NpcActionResultPacket decoded;
    const auto result = bitsery::quickDeserialization<Adapter>({bytes.data(), length}, decoded);
    EXPECT_TRUE(result.first != bitsery::ReaderError::NoError || !result.second) << "prefix: " << length;
  }
  NpcActionResultPacket decoded;
  const auto valid = bitsery::quickDeserialization<Adapter>({bytes.data(), bytes.size()}, decoded);
  ASSERT_EQ(valid.first, bitsery::ReaderError::NoError);
  ASSERT_TRUE(valid.second);
  EXPECT_EQ(decoded.success, 1);
  bytes.push_back(0);
  const auto extra = bitsery::quickDeserialization<Adapter>({bytes.data(), bytes.size()}, decoded);
  EXPECT_FALSE(extra.second);
}

TEST_F(NpcManagerTest, HostChangeResetsReportedActionsAndProjectileApproval) {
  ASSERT_TRUE(manager.SetHost(id, 1, start));
  auto& npc = manager.GetNpc(id)->get();
  npc.last_actor_action_id = 90;
  npc.last_cast_action_id = 91;
  npc.last_hit_action_id = 92;
  npc.hit_victims.insert(5);
  npc.approved_projectiles.push_back(Character::ApprovedAttack{});

  ASSERT_TRUE(manager.SetHost(id, 1, start + 100ms));
  EXPECT_FALSE(npc.approved_projectiles.empty());
  EXPECT_EQ(npc.last_actor_action_id, 90u);
  ASSERT_TRUE(manager.SetHost(id, 2, start + 200ms));
  EXPECT_FALSE(npc.last_actor_action_id);
  EXPECT_FALSE(npc.last_cast_action_id);
  EXPECT_FALSE(npc.last_hit_action_id);
  EXPECT_TRUE(npc.hit_victims.empty());

  EXPECT_TRUE(npc.approved_projectiles.empty());
}

TEST(ActorPacketTest, PacketTypeRemainsFirstByteForNewCommandsAndAcknowledgements) {
  const auto assert_type = [](const auto& packet, std::uint8_t type) {
    const auto bytes = EncodeNpcPacket(packet);
    ASSERT_FALSE(bytes.empty());
    EXPECT_EQ(bytes.front(), type);
  };
  UnequipItemPacket unequip{};
  unequip.packet_type = Net::PT_UNEQUIPITEM;
  unequip.slot_id = Net::EQUIP_SLOT_RING_LEFT;
  assert_type(unequip, Net::PT_UNEQUIPITEM);
  PlayerRespawnInfoPacket respawn{};
  respawn.packet_type = Net::PT_RESPAWN;
  respawn.health = 50000;
  respawn.mana = 60000;
  assert_type(respawn, Net::PT_RESPAWN);
  assert_type(ActorActionPacket{}, Net::PT_ACTOR_ACTION);
  assert_type(ItemTransactionResultPacket{}, Net::PT_ITEM_TRANSACTION_RESULT);
  assert_type(EquipmentResultPacket{}, Net::PT_EQUIPMENT_RESULT);
}

}  // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
