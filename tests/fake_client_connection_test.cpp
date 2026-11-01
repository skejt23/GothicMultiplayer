
/*
MIT License

Copyright (c) 2023 Gothic Multiplayer Team.

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
#include <bitsery/adapter/buffer.h>
#include <bitsery/bitsery.h>

#include <chrono>
#include <initializer_list>
#include <memory>
#include <thread>
#include <vector>

#include "connection_test_workspace.h"
#include "fake_client.h"
#include "game_server.h"
#include "net_enums.h"

namespace {

template <typename Predicate>
bool PumpUntil(std::initializer_list<FakeClient*> clients, Predicate ready) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  do {
    for (auto* client : clients) {
      client->Pulse();
      if (!client->Error().empty()) return false;
    }
    if (ready()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  } while (std::chrono::steady_clock::now() < deadline);
  return false;
}

bool HasLocalSpawn(const FakeClient& client) {
  return client.InitialInfo() && client.FindSpawn(client.InitialInfo()->player_id);
}

class FakeClientConnectionTest : public ::testing::Test {
protected:
  void SetUp() override {
    ASSERT_TRUE(server_.Init());
  }

  ConnectionTestWorkspace workspace_;
  GameServer server_;
};

TEST_F(FakeClientConnectionTest, JoinsAndWorldReadinessProducePeerSpawnSnapshots) {
  FakeClient first("FirstUser");
  FakeClient second("SecondUser");

  ASSERT_TRUE(first.Connect("127.0.0.1", server_.GetPort()));
  // Join broadcasts only target players already spawned in the game. Wait for
  // that server acknowledgment before allowing the second player to join.
  ASSERT_TRUE(PumpUntil({&first}, [&] { return HasLocalSpawn(first); })) << first.Error();
  const auto first_id = first.InitialInfo()->player_id;
  EXPECT_TRUE(first.IsConnected());
  EXPECT_NE(first_id, 0u);

  ASSERT_TRUE(second.Connect("127.0.0.1", server_.GetPort()));
  ASSERT_TRUE(PumpUntil({&first, &second}, [&] {
    return HasLocalSpawn(second) && !first.Joins().empty() &&
           first.FindSpawn(second.InitialInfo()->player_id) && second.FindSpawn(first_id);
  })) << first.Error() << " " << second.Error();

  const auto second_id = second.InitialInfo()->player_id;
  EXPECT_NE(first_id, second_id);
  ASSERT_EQ(first.Joins().size(), 1u);
  EXPECT_EQ(first.Joins().front().player_name, "SecondUser");
  ASSERT_TRUE(first.Joins().front().player_id.has_value());
  EXPECT_EQ(*first.Joins().front().player_id, second_id);

  // Previously joined actors arrive as streamed spawn snapshots once the new
  // client's world is ready; there is no legacy existing-players handshake.
  const auto* peer = second.FindSpawn(first_id);
  ASSERT_NE(peer, nullptr);
  EXPECT_EQ(peer->player_name, "FirstUser");
  EXPECT_FLOAT_EQ(peer->position.x, 0.0f);
  EXPECT_FLOAT_EQ(peer->position.y, 0.0f);
  EXPECT_FLOAT_EQ(peer->position.z, 0.0f);
  EXPECT_EQ(peer->health, peer->max_health);
  EXPECT_GT(peer->max_health, 0);
  EXPECT_GT(peer->state_sequence, 0u);
}

TEST_F(FakeClientConnectionTest, DisconnectNotifiesPeerAndReconnectCanJoinAgain) {
  FakeClient observer("Observer");
  FakeClient reconnecting("ReturningUser");
  ASSERT_TRUE(observer.Connect("127.0.0.1", server_.GetPort()));
  ASSERT_TRUE(PumpUntil({&observer}, [&] { return HasLocalSpawn(observer); })) << observer.Error();
  ASSERT_TRUE(reconnecting.Connect("127.0.0.1", server_.GetPort()));
  ASSERT_TRUE(PumpUntil({&observer, &reconnecting}, [&] {
    return HasLocalSpawn(reconnecting) && observer.FindSpawn(reconnecting.InitialInfo()->player_id);
  })) << observer.Error() << " " << reconnecting.Error();
  const auto previous_id = reconnecting.InitialInfo()->player_id;

  reconnecting.Disconnect();
  EXPECT_FALSE(reconnecting.IsConnected());
  ASSERT_TRUE(PumpUntil({&observer, &reconnecting}, [&] { return !observer.Departures().empty(); })) << observer.Error();
  EXPECT_EQ(observer.Departures().front().disconnected_id, previous_id);

  const auto previous_spawn_count = observer.Spawns().size();
  ASSERT_TRUE(reconnecting.Connect("127.0.0.1", server_.GetPort()));
  ASSERT_TRUE(PumpUntil({&observer, &reconnecting}, [&] {
    return HasLocalSpawn(reconnecting) && observer.Spawns().size() > previous_spawn_count;
  })) << observer.Error() << " " << reconnecting.Error();
  // Player IDs identify active slots and may be reused after disconnect.
  EXPECT_EQ(observer.Spawns().back().player_id, reconnecting.InitialInfo()->player_id);
  EXPECT_TRUE(reconnecting.IsConnected());
}

TEST_F(FakeClientConnectionTest, LuaDisconnectCallbackRetainsTheDepartingPlayersIp) {
  FakeClient observer("Observer");
  FakeClient departing("DepartingUser");
  ASSERT_TRUE(observer.Connect("127.0.0.1", server_.GetPort()));
  ASSERT_TRUE(PumpUntil({&observer}, [&] { return HasLocalSpawn(observer); })) << observer.Error();
  ASSERT_TRUE(departing.Connect("127.0.0.1", server_.GetPort()));
  ASSERT_TRUE(PumpUntil({&observer, &departing}, [&] { return HasLocalSpawn(departing); })) << departing.Error();

  departing.Disconnect();
  // The fixture calls getPlayerIP from Lua's onPlayerDisconnect and sends the
  // result to surviving peers. No server state is read from the test thread.
  ASSERT_TRUE(PumpUntil({&observer}, [&] { return !observer.Messages().empty(); })) << observer.Error();
  EXPECT_EQ(observer.Messages().front().message, "disconnect IP: 127.0.0.1");
}

TEST_F(FakeClientConnectionTest, LuaIdentitySurvivesDisconnectAndReconnect) {
  FakeClient observer("Observer");
  FakeClient returning("IdentityProbe");
  ASSERT_TRUE(observer.Connect("127.0.0.1", server_.GetPort()));
  ASSERT_TRUE(PumpUntil({&observer}, [&] { return HasLocalSpawn(observer); })) << observer.Error();
  ASSERT_TRUE(returning.Connect("127.0.0.1", server_.GetPort()));
  ASSERT_TRUE(PumpUntil({&observer, &returning}, [&] {
    return HasLocalSpawn(returning) && !returning.Messages().empty();
  })) << returning.Error();
  const auto joined_message = returning.Messages().front().message;
  ASSERT_EQ(joined_message.substr(0, 10), "identity: ");
  const auto identity = joined_message.substr(10);
  ASSERT_EQ(identity.size(), 64u);

  returning.Disconnect();
  // Read through Lua messages on the server thread, including the cached value
  // after GNS has released the departing connection. Avoid server data races.
  ASSERT_TRUE(PumpUntil({&observer}, [&] { return observer.Messages().size() >= 2; })) << observer.Error();
  EXPECT_EQ(observer.Messages()[1].message, "disconnect identity: " + identity);
  ASSERT_TRUE(returning.Connect("127.0.0.1", server_.GetPort()));
  ASSERT_TRUE(PumpUntil({&observer, &returning}, [&] {
    return HasLocalSpawn(returning) && !returning.Messages().empty();
  })) << returning.Error();
  EXPECT_EQ(returning.Messages().front().message, joined_message);
}

TEST_F(FakeClientConnectionTest, TrustedItemUseEventReceivesArgumentsAndCancellationPreservesInventory) {
  FakeClient user("UseProbe");
  ASSERT_TRUE(user.Connect("127.0.0.1", server_.GetPort()));
  ASSERT_TRUE(PumpUntil({&user}, [&] { return HasLocalSpawn(user); })) << user.Error();
  ActorActionPacket action;
  action.kind = ActorActionKind::ItemUse;
  action.item_instance = "ITMI_TEST";
  action.quantity = 2;
  action.action_id = 1;
  ASSERT_TRUE(user.SendActorAction(action));
  ASSERT_TRUE(PumpUntil({&user}, [&] { return user.Messages().size() == 1; })) << user.Error();
  EXPECT_EQ(user.Messages().front().message, "item use cancelled");
  action.action_id = 2;
  ASSERT_TRUE(user.SendActorAction(action));
  ASSERT_TRUE(PumpUntil({&user}, [&] { return user.Messages().size() == 2; })) << user.Error();
  EXPECT_EQ(user.Messages().back().message, "item use cancelled");
}

TEST(ServerStateSequenceTest, ReorderedClientSnapshotsCannotOverwriteAcceptedGameplayState) {
  ConnectionTestWorkspace workspace;
  // Do not start the server loop: dispatch the production packet handler on
  // this thread so state assertions are deterministic and free of data races.
  GameServer server;
  constexpr Net::ConnectionHandle connection = 70001;
  const auto id = server.GetPlayerManager().AddPlayer(connection, "StateSender", 1);
  ASSERT_NE(id, 0u);
  auto& player = server.GetPlayerManager().GetPlayer(id)->get();
  player.is_ingame = true;
  player.npc_world_ready = true;
  player.state.nrot = glm::vec3(0.0f, 0.0f, 1.0f);

  const auto deliver = [&](std::uint32_t sequence, float x) {
    PlayerStateUpdatePacket packet{};
    packet.packet_type = Net::PT_ACTUAL_STATISTICS;
    packet.state_sequence = sequence;
    packet.actor_generation = player.actor_generation;
    packet.command_revision = player.command_revision;
    packet.state = player.state;
    packet.state.position.x = x;
    packet.state.source_time_ms = ActorClockMilliseconds();
    std::vector<std::uint8_t> bytes;
    const auto size = bitsery::quickSerialization<bitsery::OutputBufferAdapter<std::vector<std::uint8_t>>>(bytes, packet);
    return server.HandlePacket(connection, bytes.data(), static_cast<std::uint32_t>(size));
  };

  ASSERT_TRUE(deliver(2, 200.0f));
  EXPECT_FLOAT_EQ(player.state.position.x, 200.0f);
  const auto authoritative_sequence = player.state_sequence;
  ASSERT_TRUE(deliver(1, 100.0f));
  ASSERT_TRUE(deliver(2, 999.0f));
  ASSERT_TRUE(deliver(0, 0.0f));
  EXPECT_FLOAT_EQ(player.state.position.x, 200.0f);
  EXPECT_EQ(player.state_sequence, authoritative_sequence);

  ASSERT_TRUE(deliver(3, 300.0f));
  EXPECT_FLOAT_EQ(player.state.position.x, 300.0f);
  EXPECT_NE(player.state_sequence, authoritative_sequence);
}
}  // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
