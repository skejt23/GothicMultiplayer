
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
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>

#include "fake_client.h"
#include "game_server.h"

using namespace testing;

namespace fs = std::filesystem;

namespace {
class TestServerWorkspace {
public:
  TestServerWorkspace() {
    original_cwd_ = fs::current_path();
    workspace_dir_ = fs::temp_directory_path() / fs::path("gmp_test_env_fake_client");
    workspace_dir_ += std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    fs::create_directories(workspace_dir_);

    // Minimal config to ensure the server binds on a free port.
    std::ofstream cfg(workspace_dir_ / "config.toml", std::ios::binary);
    cfg << "port = 0\n";
    cfg << "public = false\n";

    fs::current_path(workspace_dir_);
  }

  ~TestServerWorkspace() {
    std::error_code ec;
    fs::current_path(original_cwd_, ec);
    fs::remove_all(workspace_dir_, ec);
  }

private:
  fs::path original_cwd_;
  fs::path workspace_dir_;
};
}  // namespace

class FakeClientConnectionTest : public Test {
  void SetUp() override {
    workspace_ = std::make_unique<TestServerWorkspace>();
    server_ = std::make_unique<GameServer>();
    ASSERT_TRUE(server_->Init());
  }

public:
  std::unique_ptr<TestServerWorkspace> workspace_;
  std::unique_ptr<GameServer> server_;
};

TEST_F(FakeClientConnectionTest, MultipleClientsJoin_VerifyPacketsTellingAboutPlayers) {
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
    const auto& p = packet.existing_players[0];
    EXPECT_EQ(p.player_name, "TestUser");

    // Baseline join state
    EXPECT_FLOAT_EQ(p.position.x, 0.0f);
    EXPECT_FLOAT_EQ(p.position.y, 0.0f);
    EXPECT_FLOAT_EQ(p.position.z, 0.0f);
    EXPECT_EQ(p.left_hand_item_instance, 0);
    EXPECT_EQ(p.right_hand_item_instance, 0);
    EXPECT_EQ(p.equipped_armor_instance, 0);
    EXPECT_EQ(p.body_model, "");
    EXPECT_EQ(p.body_texture, 0);
    EXPECT_EQ(p.head_model, "");
    EXPECT_EQ(p.head_texture, 0);
    EXPECT_EQ(p.walk_style, 0);

    // Expanded snapshot fields (defaults for a newly-joined player)
    EXPECT_EQ(p.instance, "");
    EXPECT_EQ(p.name_color_r, 255);
    EXPECT_EQ(p.name_color_g, 255);
    EXPECT_EQ(p.name_color_b, 255);

    EXPECT_EQ(p.strength, 0);
    EXPECT_EQ(p.dexterity, 0);
    EXPECT_EQ(p.level, 0);
    EXPECT_EQ(p.exp, 0);
    EXPECT_EQ(p.next_level_exp, 0);
    EXPECT_EQ(p.learn_points, 0);
    EXPECT_EQ(p.health, 0);
    EXPECT_EQ(p.max_health, 100);
    EXPECT_EQ(p.mana, 0);
    EXPECT_EQ(p.max_mana, 100);

    EXPECT_FLOAT_EQ(p.fatness, 1.0f);
    EXPECT_FLOAT_EQ(p.scale.x, 1.0f);
    EXPECT_FLOAT_EQ(p.scale.y, 1.0f);
    EXPECT_FLOAT_EQ(p.scale.z, 1.0f);
    EXPECT_TRUE(p.weapon_skills.empty());
    EXPECT_TRUE(p.talents.empty());
    EXPECT_TRUE(p.overlays.empty());
    client2_existing_players_promise.set_value();
  });

  ASSERT_TRUE(client1.Connect("127.0.0.1", server_->GetPort()));
  ASSERT_TRUE(client2.Connect("127.0.0.1", server_->GetPort()));
  ASSERT_TRUE(client1_joined_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
  ASSERT_TRUE(client2_existing_players_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}