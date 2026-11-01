/*
MIT License

Copyright (c) 2025 Gothic Multiplayer Team.

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
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "connection_test_workspace.h"
#include "fake_client.h"
#include "game_client.hpp"
#include "game_server.h"
#include "task_scheduler.h"

namespace {

// Match the client scheduler contract: resource-worker callbacks execute on
// the same thread as network polling and observer assertions.
class TestTaskScheduler : public gmp::TaskScheduler {
public:
  void ScheduleOnMainThread(Task task) override {
    std::lock_guard lock(mutex_);
    pending_.push_back(std::move(task));
  }

  void Dispatch() {
    std::deque<Task> tasks;
    {
      std::lock_guard lock(mutex_);
      tasks.swap(pending_);
    }
    for (auto& task : tasks) task();
  }

private:
  std::mutex mutex_;
  std::deque<Task> pending_;
};

class RecordingEventObserver : public gmp::client::EventObserver {
public:
  void OnConnected() override { ++connected; }
  void OnDisconnected() override { ++disconnected; }
  void OnConnectionFailed(const std::string& reason) override { ++failed; error = reason; }
  void OnConnectionLost() override { error = "Connection lost"; }
  void OnConnectionClosed(const std::string& reason) override { error = reason; }
  void OnResourceDownloadFailed(const std::string& reason) override { error = reason; }
  void OnResourcesReady() override { ++resources_ready; }
  void OnLocalPlayerJoined(gmp::client::Player& player) override { local_id = player.id(); }
  void OnLocalPlayerSpawned(gmp::client::Player& player) override {
    spawned_id = player.id();
    ++spawned;
  }

  int connected = 0;
  int disconnected = 0;
  int failed = 0;
  int resources_ready = 0;
  int spawned = 0;
  std::optional<std::uint64_t> local_id;
  std::optional<std::uint64_t> spawned_id;
  std::string error;
};

class RealClientConnectionTest : public ::testing::Test {
protected:
  explicit RealClientConnectionTest(std::uint32_t slots = 12, bool ban_loopback = false) : workspace_(slots, ban_loopback) {}

  void SetUp() override {
    ASSERT_TRUE(server_.Init());
    client_ = std::make_unique<gmp::client::GameClient>(observer_, scheduler_);
    endpoint_ = "127.0.0.1:" + std::to_string(server_.GetPort());
  }

  void TearDown() override {
    if (client_) {
      client_->Disconnect();
      // Disconnect joins the downloader. Drain its remaining callbacks while
      // both the client and observer still exist.
      scheduler_.Dispatch();
      client_.reset();
    }
  }

  template <typename Predicate>
  bool PumpUntil(Predicate ready, std::chrono::seconds timeout = std::chrono::seconds(5)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    do {
      client_->HandleNetwork();
      scheduler_.Dispatch();
      if (!observer_.error.empty()) return false;
      if (ready()) return true;
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    } while (std::chrono::steady_clock::now() < deadline);
    return false;
  }

  void ExpectConnectionRejected(Net::CloseReason reason) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (observer_.failed == 0 && std::chrono::steady_clock::now() < deadline) {
      client_->HandleNetwork();
      scheduler_.Dispatch();
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    ASSERT_EQ(observer_.failed, 1) << "No connection rejection callback: " << observer_.error;
    EXPECT_EQ(observer_.connected, 0);
    EXPECT_EQ(observer_.resources_ready, 0);
    EXPECT_FALSE(observer_.local_id.has_value());
    EXPECT_FALSE(client_->IsConnected());
    EXPECT_EQ(client_->GetConnectionState(), gmp::client::GameClient::ConnectionState::Failed);
    EXPECT_EQ(client_->GetConnectionCloseReason(), reason);
    EXPECT_FALSE(client_->GetConnectionError().empty());
  }
  ConnectionTestWorkspace workspace_;
  GameServer server_;
  TestTaskScheduler scheduler_;
  RecordingEventObserver observer_;
  std::unique_ptr<gmp::client::GameClient> client_;
  std::string endpoint_;
};

TEST_F(RealClientConnectionTest, GameClientDownloadsResourcesAndJoinsServer) {
  client_->ConnectAsync(endpoint_);
  ASSERT_TRUE(PumpUntil([&] { return observer_.connected == 1; })) << observer_.error;
  EXPECT_TRUE(client_->IsConnected());
  ASSERT_TRUE(PumpUntil([&] { return observer_.resources_ready == 1; }, std::chrono::seconds(15))) << observer_.error;

  // This must transfer a real archive over the resource HTTP endpoint, not
  // pass merely because the server advertised an empty resource list.
  const auto resources = client_->ConsumeDownloadedResources();
  ASSERT_EQ(resources.size(), 1u);
  EXPECT_EQ(resources.front().descriptor.name, "connection_test");
  EXPECT_FALSE(resources.front().manifest_json.empty());
  EXPECT_FALSE(resources.front().archive_bytes.empty());
  ASSERT_TRUE(observer_.local_id.has_value());
  EXPECT_NE(*observer_.local_id, 0u);
  EXPECT_EQ(client_->GetServerName(), "Connection test server");
  EXPECT_EQ(client_->GetMaxSlots(), 12u);

  client_->JoinGame("RealClientUser", "RealClientUser", "", 0, "", 0, 0);
  ASSERT_TRUE(PumpUntil([&] { return observer_.spawned == 1; })) << observer_.error;
  EXPECT_EQ(observer_.local_id, observer_.spawned_id);
  EXPECT_TRUE(client_->player_manager().GetLocalPlayer().has_spawned());
}

TEST_F(RealClientConnectionTest, GameClientDisconnectsAndReconnects) {
  client_->ConnectAsync(endpoint_);
  ASSERT_TRUE(PumpUntil([&] { return observer_.resources_ready == 1 && observer_.connected == 1; }, std::chrono::seconds(15)))
      << observer_.error;
  client_->JoinGame("ReturningClient", "ReturningClient", "", 0, "", 0, 0);
  ASSERT_TRUE(PumpUntil([&] { return observer_.spawned == 1; })) << observer_.error;

  client_->Disconnect();
  EXPECT_EQ(observer_.disconnected, 1);
  EXPECT_FALSE(client_->IsConnected());
  EXPECT_EQ(client_->GetConnectionState(), gmp::client::GameClient::ConnectionState::Disconnected);
  EXPECT_EQ(client_->GetConnectionCloseReason(), Net::CloseReason::Normal);
  EXPECT_FALSE(client_->player_manager().HasLocalPlayer());

  client_->ConnectAsync(endpoint_);
  ASSERT_TRUE(PumpUntil([&] { return observer_.resources_ready == 2 && observer_.connected == 2; }, std::chrono::seconds(15)))
      << observer_.error;
  ASSERT_EQ(client_->ConsumeDownloadedResources().size(), 1u);
  client_->JoinGame("ReturningClient", "ReturningClient", "", 0, "", 0, 0);
  ASSERT_TRUE(PumpUntil([&] { return observer_.spawned == 2; })) << observer_.error;
  EXPECT_EQ(observer_.local_id, observer_.spawned_id);
  EXPECT_TRUE(client_->IsConnected());
}

TEST_F(RealClientConnectionTest, JoinAppearanceIsUsedByTheFirstSpawnAndStreamedToOtherPlayers) {
  FakeClient viewer("AppearanceViewer");
  ASSERT_TRUE(viewer.Connect("127.0.0.1", server_.GetPort()));
  client_->ConnectAsync(endpoint_);
  ASSERT_TRUE(PumpUntil([&] {
    viewer.Pulse();
    return observer_.resources_ready == 1 && viewer.InitialInfo().has_value();
  }, std::chrono::seconds(15))) << observer_.error;
  client_->JoinGame("AppearanceProbe", "AppearanceProbe", "HUM_BODY_BABE0", 12, "HUM_HEAD_THIEF", 162, 1, 0, 0, "HUMANS_BABE.MDS");
  ASSERT_TRUE(PumpUntil([&] { return observer_.spawned == 1; })) << observer_.error;
  const auto& local = client_->player_manager().GetLocalPlayer();
  EXPECT_EQ(local.body_model(), "HUM_BODY_BABE0");
  EXPECT_EQ(local.body_texture(), 12);
  EXPECT_EQ(local.head_model(), "HUM_HEAD_THIEF");
  EXPECT_EQ(local.head_texture(), 162);
  EXPECT_EQ(local.walk_style(), 1);
  EXPECT_EQ(local.overlays(), std::vector<std::string>{"HUMANS_BABE.MDS"});
  client_->SendPlayerWorldEnter("NEWWORLD\\NEWWORLD.ZEN");
  ASSERT_TRUE(PumpUntil([&] {
    viewer.Pulse();
    return viewer.FindSpawn(static_cast<std::uint32_t>(local.id())) != nullptr;
  })) << viewer.Error();
  const auto& remote = *viewer.FindSpawn(static_cast<std::uint32_t>(local.id()));
  EXPECT_EQ(remote.body_model, local.body_model());
  EXPECT_EQ(remote.head_model, local.head_model());
  EXPECT_EQ(remote.overlays, local.overlays());
}

TEST_F(RealClientConnectionTest, ServerCreatorCanOverrideJoinAppearanceBeforeTheFirstSpawn) {
  client_->ConnectAsync(endpoint_);
  ASSERT_TRUE(PumpUntil([&] { return observer_.resources_ready == 1; }, std::chrono::seconds(15))) << observer_.error;
  client_->JoinGame("AppearanceOverrideProbe", "AppearanceOverrideProbe", "HUM_BODY_BABE0", 12, "HUM_HEAD_THIEF", 162, 0, 0, 0,
                    "HUMANS_BABE.MDS");
  ASSERT_TRUE(PumpUntil([&] { return observer_.spawned == 1; })) << observer_.error;
  const auto& local = client_->player_manager().GetLocalPlayer();
  EXPECT_EQ(local.body_model(), "HUM_BODY_NAKED0");
  EXPECT_EQ(local.body_texture(), 2);
  EXPECT_EQ(local.head_model(), "HUM_HEAD_BALD");
  EXPECT_EQ(local.head_texture(), 7);
  EXPECT_TRUE(local.overlays().empty());
}

class FullServerConnectionTest : public RealClientConnectionTest {
protected:
  FullServerConnectionTest() : RealClientConnectionTest(1) {}
};

TEST_F(FullServerConnectionTest, GameClientReceivesServerFullBeforeAcceptance) {
  FakeClient occupying_client("OccupyingUser");
  ASSERT_TRUE(occupying_client.Connect("127.0.0.1", server_.GetPort()));
  ASSERT_TRUE(PumpUntil([&] {
    occupying_client.Pulse();
    return occupying_client.InitialInfo().has_value();
  })) << occupying_client.Error();
  ASSERT_TRUE(occupying_client.IsConnected());
  EXPECT_EQ(occupying_client.InitialInfo()->max_slots, 1u);

  client_->ConnectAsync(endpoint_);
  ExpectConnectionRejected(Net::CloseReason::ServerFull);
  EXPECT_TRUE(occupying_client.IsConnected());
}

class BannedClientConnectionTest : public RealClientConnectionTest {
protected:
  BannedClientConnectionTest() : RealClientConnectionTest(12, true) {}
};

TEST_F(BannedClientConnectionTest, GameServerBanManagerRejectsLoopbackBeforeAcceptance) {
  client_->ConnectAsync(endpoint_);
  ExpectConnectionRejected(Net::CloseReason::Banned);
}
}  // namespace
