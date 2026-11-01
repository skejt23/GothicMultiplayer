
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
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include "ban_manager.h"

namespace {

class BanListTest : public ::testing::Test {
protected:
  const std::filesystem::path storage_path = std::filesystem::current_path() /
      ("bans-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".json");
  BanManager manager{storage_path};

  void WriteBanFile(const std::string& content) {
    std::ofstream ofs(storage_path, std::ios::binary);
    ASSERT_TRUE(ofs.good());
    ofs << content;
  }

  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove(storage_path, ec);
  }
};

TEST_F(BanListTest, LoadsAndEnforcesValidBanEntries) {
  WriteBanFile(R"([{"Nickname":"ExamplePlayer","IP":"198.51.100.10","Date":"2025-08-30","Reason":"Testing"}])");
  ASSERT_TRUE(manager.Load());
  ASSERT_EQ(manager.GetBanList().size(), 1u);
  EXPECT_EQ(manager.GetBanList().front().nickname, "ExamplePlayer");
  EXPECT_EQ(manager.GetBanList().front().date, "2025-08-30");
  EXPECT_EQ(manager.GetBanList().front().reason, "Testing");
  EXPECT_TRUE(manager.IsBanned("198.51.100.10"));
  EXPECT_FALSE(manager.IsBanned("198.51.100.11"));
  EXPECT_FALSE(manager.IsBanned(""));
}

TEST_F(BanListTest, RejectsNonArrayAndMalformedJsonWithoutRemovingActiveBans) {
  manager.AddBan({"player", "203.0.113.1", "", "existing"});
  WriteBanFile(R"({"IP":"203.0.113.1"})");
  EXPECT_FALSE(manager.Load());
  EXPECT_TRUE(manager.IsBanned("203.0.113.1"));
  WriteBanFile("not json");
  EXPECT_FALSE(manager.Load());
  EXPECT_TRUE(manager.IsBanned("203.0.113.1"));
}

TEST_F(BanListTest, SkipsMalformedAndEmptyIpEntries) {
  WriteBanFile(R"([{"Nickname":"NoIP"},{"IP":""},{"IP":123},false,{"Nickname":"Valid","IP":"192.0.2.55"}])");
  ASSERT_TRUE(manager.Load());
  ASSERT_EQ(manager.GetBanList().size(), 1u);
  EXPECT_EQ(manager.GetBanList().front().nickname, "Valid");
  EXPECT_TRUE(manager.IsBanned("192.0.2.55"));
}

TEST_F(BanListTest, PersistsUpdatesAndUnbansWithoutTransportState) {
  manager.AddBan({"first", "192.0.2.55", "", "first reason"});
  manager.AddBan({"second", "192.0.2.55", "", "updated reason"});
  ASSERT_EQ(manager.GetBanList().size(), 1u);
  ASSERT_TRUE(manager.Save());

  BanManager restored(storage_path);
  ASSERT_TRUE(restored.Load());
  EXPECT_TRUE(restored.IsBanned("192.0.2.55"));
  ASSERT_EQ(restored.GetBanList().size(), 1u);
  EXPECT_EQ(restored.GetBanList().front().nickname, "second");
  EXPECT_EQ(restored.GetBanList().front().reason, "updated reason");
  EXPECT_TRUE(restored.RemoveBan("192.0.2.55"));
  EXPECT_FALSE(restored.IsBanned("192.0.2.55"));
  EXPECT_FALSE(restored.RemoveBan("192.0.2.55"));
  ASSERT_TRUE(restored.Save());
  ASSERT_TRUE(manager.Load());
  EXPECT_FALSE(manager.IsBanned("192.0.2.55"));
}

TEST_F(BanListTest, TemporaryIntegrityBanExpiresAndIsNotPersisted) {
  const BanManager::Clock::time_point now{std::chrono::seconds(10)};
  manager.AddTemporaryBan("192.0.2.55", std::chrono::hours(1), now);
  EXPECT_TRUE(manager.IsBanned("192.0.2.55", now));
  EXPECT_TRUE(manager.IsBanned("192.0.2.55", now + std::chrono::minutes(59)));
  EXPECT_FALSE(manager.IsBanned("192.0.2.55", now + std::chrono::hours(1)));
  ASSERT_TRUE(manager.Save());
  BanManager restored(storage_path);
  ASSERT_TRUE(restored.Load());
  EXPECT_FALSE(restored.IsBanned("192.0.2.55", now));
}

TEST_F(BanListTest, RemovesBothPermanentAndTemporaryBans) {
  const auto now = BanManager::Clock::now();
  manager.AddBan({"player", "192.0.2.55", "", "reason"});
  manager.AddTemporaryBan("192.0.2.55", std::chrono::hours(1), now);
  EXPECT_TRUE(manager.RemoveBan("192.0.2.55"));
  EXPECT_FALSE(manager.IsBanned("192.0.2.55", now));
}

TEST_F(BanListTest, ShorterTemporaryBanDoesNotReduceExistingDuration) {
  const BanManager::Clock::time_point now{std::chrono::seconds(10)};
  manager.AddTemporaryBan("192.0.2.55", std::chrono::hours(1), now);
  manager.AddTemporaryBan("192.0.2.55", std::chrono::minutes(1), now);
  EXPECT_TRUE(manager.IsBanned("192.0.2.55", now + std::chrono::minutes(30)));
  manager.AddTemporaryBan("", std::chrono::hours(1), now);
  EXPECT_FALSE(manager.IsBanned("", now));
}

}  // namespace

int main(int argc, char** argv) {
  ::testing::GTEST_FLAG(catch_exceptions) = false;
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
