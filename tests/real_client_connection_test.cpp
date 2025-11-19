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

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <future>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

#include "game_client.hpp"
#include "game_server.h"
#include "task_scheduler.h"

namespace {

namespace fs = std::filesystem;

void CopyDirectoryRecursive(const fs::path& source, const fs::path& destination) {
  if (!fs::exists(source) || !fs::is_directory(source)) {
    throw std::runtime_error("Source directory does not exist: " + source.string());
  }

  fs::create_directories(destination);
  for (const auto& entry : fs::recursive_directory_iterator(source)) {
    const auto relative = fs::relative(entry.path(), source);
    const fs::path target = destination / relative;
    if (entry.is_directory()) {
      fs::create_directories(target);
    } else if (entry.is_regular_file()) {
      fs::create_directories(target.parent_path());
      fs::copy_file(entry.path(), target, fs::copy_options::overwrite_existing);
    }
  }
}

void CopyFileIfExists(const fs::path& source, const fs::path& destination) {
  if (fs::exists(source)) {
    fs::copy_file(source, destination, fs::copy_options::overwrite_existing);
  }
}

std::string GenerateWorkspaceName() {
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  std::ostringstream oss;
  oss << "gmp_test_env_" << now;
  return oss.str();
}

class TestServerWorkspace {
public:
  TestServerWorkspace() {
    original_cwd_ = fs::current_path();
    workspace_dir_ = fs::temp_directory_path() / GenerateWorkspaceName();
    fs::create_directories(workspace_dir_);

    const fs::path repo_resources = original_cwd_ / "gmp-server" / "resources";
    CopyDirectoryRecursive(repo_resources, workspace_dir_ / "resources");

    CopyFileIfExists(repo_resources / "config.toml", workspace_dir_ / "config.toml");
    CopyFileIfExists(repo_resources / "bans.json", workspace_dir_ / "bans.json");

    fs::current_path(workspace_dir_);
  }

  ~TestServerWorkspace() {
    std::error_code ec;
    fs::current_path(original_cwd_, ec);
    fs::remove_all(workspace_dir_, ec);
  }

  const fs::path& root() const {
    return workspace_dir_;
  }

private:
  fs::path original_cwd_;
  fs::path workspace_dir_;
};

void EnsureClientNetworkLoaded() {
  static std::once_flag once;
  std::call_once(once, []() { gmp::client::LoadNetworkLibrary(); });
}

class ImmediateTaskScheduler : public gmp::TaskScheduler {
public:
  void ScheduleOnMainThread(Task task) override {
    task();
  }
};

class RecordingEventObserver : public gmp::client::EventObserver {
public:
  RecordingEventObserver()
      : connected_future_(connected_promise_.get_future()),
        resources_ready_future_(resources_ready_promise_.get_future()),
        local_join_future_(local_join_promise_.get_future()),
        local_spawn_future_(local_spawn_promise_.get_future()),
        failure_future_(failure_promise_.get_future()) {
  }

  std::future<void>& ConnectedFuture() {
    return connected_future_;
  }

  std::future<void>& ResourcesReadyFuture() {
    return resources_ready_future_;
  }

  std::future<std::uint64_t>& LocalJoinedFuture() {
    return local_join_future_;
  }

  std::future<std::uint64_t>& LocalSpawnedFuture() {
    return local_spawn_future_;
  }

  std::future<std::string>& FailureFuture() {
    return failure_future_;
  }

  std::size_t LastResourceCount() const {
    return last_resource_count_.load(std::memory_order_relaxed);
  }

  std::uint64_t LastResourceBytes() const {
    return last_resource_bytes_.load(std::memory_order_relaxed);
  }

  void OnConnected() override {
    Fulfill(connected_promise_, connected_flag_);
  }

  void OnConnectionFailed(const std::string& error) override {
    FulfillValue(failure_promise_, failure_flag_, error);
  }

  bool RequestResourceDownloadConsent(std::size_t resource_count, std::uint64_t total_bytes) override {
    last_resource_count_.store(resource_count, std::memory_order_relaxed);
    last_resource_bytes_.store(total_bytes, std::memory_order_relaxed);
    return true;
  }

  void OnResourcesReady() override {
    Fulfill(resources_ready_promise_, resources_flag_);
  }

  void OnLocalPlayerJoined(gmp::client::Player& player) override {
    FulfillValue(local_join_promise_, local_join_flag_, player.id());
  }

  void OnLocalPlayerSpawned(gmp::client::Player& player) override {
    FulfillValue(local_spawn_promise_, local_spawn_flag_, player.id());
  }

private:
  static void Fulfill(std::promise<void>& promise, std::once_flag& flag) {
    std::call_once(flag, [&promise]() { promise.set_value(); });
  }

  template <typename T>
  static void FulfillValue(std::promise<T>& promise, std::once_flag& flag, T value) {
    std::call_once(flag, [&promise, value = std::move(value)]() mutable { promise.set_value(std::move(value)); });
  }

  std::promise<void> connected_promise_;
  std::promise<void> resources_ready_promise_;
  std::promise<std::uint64_t> local_join_promise_;
  std::promise<std::uint64_t> local_spawn_promise_;
  std::promise<std::string> failure_promise_;

  std::future<void> connected_future_;
  std::future<void> resources_ready_future_;
  std::future<std::uint64_t> local_join_future_;
  std::future<std::uint64_t> local_spawn_future_;
  std::future<std::string> failure_future_;

  std::once_flag connected_flag_;
  std::once_flag resources_flag_;
  std::once_flag local_join_flag_;
  std::once_flag local_spawn_flag_;
  std::once_flag failure_flag_;

  std::atomic<std::size_t> last_resource_count_{0};
  std::atomic<std::uint64_t> last_resource_bytes_{0};
};

std::string BuildFailureMessage(const char* phase, std::future<std::string>& failure_future) {
  if (failure_future.valid() && failure_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
    return std::string("Client reported failure while waiting for ") + phase + ": " + failure_future.get();
  }
  return std::string("Timed out while waiting for ") + phase;
}

template <typename Future>
bool WaitForFutureWithPump(Future& future, gmp::client::GameClient& client, std::chrono::milliseconds timeout,
                           std::future<std::string>* failure_future) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
      return true;
    }

    if (failure_future && failure_future->valid() &&
        failure_future->wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
      return false;
    }

    client.HandleNetwork();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  return future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
}

}  // namespace

class RealClientConnectionTest : public ::testing::Test {
protected:
  void SetUp() override {
    EnsureClientNetworkLoaded();
    ASSERT_TRUE(server_.Init());
  }

  void TearDown() override {
    if (client_) {
      client_->Disconnect();
      client_.reset();
    }
  }

  TestServerWorkspace workspace_;
  GameServer server_;
  ImmediateTaskScheduler scheduler_;
  RecordingEventObserver observer_;
  std::unique_ptr<gmp::client::GameClient> client_;
};

TEST_F(RealClientConnectionTest, GameClientDownloadsResourcesAndJoinsServer) {
  client_ = std::make_unique<gmp::client::GameClient>(observer_, scheduler_);

  std::ostringstream endpoint;
  endpoint << "127.0.0.1:" << server_.GetPort();

  auto& failure_future = observer_.FailureFuture();
  auto& connected_future = observer_.ConnectedFuture();
  auto& resources_future = observer_.ResourcesReadyFuture();
  auto& joined_future = observer_.LocalJoinedFuture();
  auto& spawned_future = observer_.LocalSpawnedFuture();

  client_->ConnectAsync(endpoint.str());

  ASSERT_TRUE(WaitForFutureWithPump(connected_future, *client_, std::chrono::seconds(5), &failure_future))
      << BuildFailureMessage("connection", failure_future);
  connected_future.get();

    ASSERT_TRUE(WaitForFutureWithPump(resources_future, *client_, std::chrono::seconds(15), &failure_future))
      << BuildFailureMessage("resource preparation", failure_future);
  resources_future.get();
    EXPECT_GT(observer_.LastResourceCount(), 0u);
    EXPECT_GT(observer_.LastResourceBytes(), 0u);

  client_->JoinGame("RealClientUser", "RealClientUser", 0, 0, 0, 0);

  ASSERT_TRUE(WaitForFutureWithPump(joined_future, *client_, std::chrono::seconds(5), &failure_future))
      << BuildFailureMessage("join acknowledgment", failure_future);
  auto local_player_id = joined_future.get();

    ASSERT_TRUE(WaitForFutureWithPump(spawned_future, *client_, std::chrono::seconds(10), &failure_future))
      << BuildFailureMessage("spawn event", failure_future);
  auto spawned_player_id = spawned_future.get();

  EXPECT_EQ(local_player_id, spawned_player_id);
  EXPECT_EQ(server_.GetPlayerManager().GetPlayerCount(), 1u);
}
