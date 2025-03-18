
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

#include <chrono>
#include <future>

#include "fake_client.h"
#include "game_server.h"

using namespace testing;

class ConnectionTest : public Test {
  void SetUp() override {
    ASSERT_TRUE(server.Init());
  }

public:
  GameServer server;
};

TEST_F(ConnectionTest, MultipleClientsJoin_VerifyPacketsTellingAboutPlayers) {
  FakeObserverMock observer1;
  FakeObserverMock observer2;
  FakeClient client1("TestUser", &observer1);
  FakeClient client2("TestUser2", &observer2);

  std::promise<void> client1_joined_promise;
  std::future<void> client1_joined_future = client1_joined_promise.get_future();

  EXPECT_CALL(observer1, OnJoinGamePacket(_)).WillOnce([&client1_joined_promise](const JoinGamePacket& packet) {
    // TestUser should be informed that TestUser2 joined the game
    EXPECT_EQ(packet.player_name, "TestUser2");
    client1_joined_promise.set_value();
  });

  std::promise<void> client2_existing_players_promise;
  std::future<void> client2_existing_players_future = client2_existing_players_promise.get_future();

  EXPECT_CALL(observer2, OnExistingPlayersPacket(_)).WillOnce([&client2_existing_players_promise](const ExistingPlayersPacket& packet) {
    // TestUser2 should be informed about TestUser
    ASSERT_EQ(packet.existing_players.size(), 1);
    EXPECT_EQ(packet.existing_players[0].player_name, "TestUser");
    client2_existing_players_promise.set_value();
  });

  ASSERT_TRUE(client1.Connect("127.0.0.1"));
  ASSERT_TRUE(client2.Connect("127.0.0.1"));
  ASSERT_TRUE(client1_joined_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
  ASSERT_TRUE(client2_existing_players_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
}
