
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

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include <gmock/gmock.h>

#include "ban_manager.h"
#include "znet_server.h"

namespace {

class MockNetServer : public Net::NetServer {
public:
  MOCK_METHOD(void, Pulse, (), (override));
  MOCK_METHOD(bool, Start, (std::uint32_t, std::uint32_t), (override));
  MOCK_METHOD(bool, Send, (unsigned char*, std::uint32_t, Net::PacketPriority, Net::PacketReliability, std::uint32_t, Net::PlayerId), (override));
  MOCK_METHOD(bool, Send, (const char*, std::uint32_t, Net::PacketPriority, Net::PacketReliability, std::uint32_t, Net::PlayerId), (override));
  MOCK_METHOD(void, AddToBanList, (const char*, std::uint32_t), (override));
  MOCK_METHOD(void, AddToBanList, (Net::PlayerId, std::uint32_t), (override));
  MOCK_METHOD(void, RemoveFromBanList, (const char*), (override));
  MOCK_METHOD(bool, IsBanned, (const char*), (override));
  MOCK_METHOD(const char*, GetPlayerIp, (Net::PlayerId), (override));
  MOCK_METHOD(void, AddPacketHandler, (Net::PacketHandler&), (override));
  MOCK_METHOD(void, RemovePacketHandler, (Net::PacketHandler&), (override));
  MOCK_METHOD(std::uint32_t, GetPort, (), (const override));
};

class BanListTest : public ::testing::Test {
protected:
  void WriteBanFile(const std::string& content) {
    std::ofstream ofs(std::filesystem::current_path() / "bans.json", std::ios::binary);
    ASSERT_TRUE(ofs.good()) << "Unable to open bans.json for writing";
    ofs << content;
  }

  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove(std::filesystem::current_path() / "bans.json", ec);
  }

public:
  MockNetServer net_server;
  BanManager manager{net_server};
};

TEST_F(BanListTest, LoadsValidBanEntries) {
  constexpr char kJson[] = R"([
    {
      "Nickname": "ExamplePlayer",
      "IP": "198.51.100.10",
      "Date": "2025-08-30",
      "Reason": "Testing"
    }
  ])";

  WriteBanFile(kJson);

  EXPECT_CALL(net_server, AddToBanList(testing::Matcher<const char*>(testing::StrEq("198.51.100.10")), 0)).Times(1);

  EXPECT_TRUE(manager.Load());

  ASSERT_EQ(1u, manager.GetBanList().size());
  EXPECT_EQ("ExamplePlayer", manager.GetBanList().front().nickname);
  EXPECT_EQ("198.51.100.10", manager.GetBanList().front().ip);
  EXPECT_EQ("2025-08-30", manager.GetBanList().front().date);
  EXPECT_EQ("Testing", manager.GetBanList().front().reason);
}

TEST_F(BanListTest, ReturnsEarlyWhenJsonIsNotArray) {
  constexpr char kJson[] = R"({
    "IP": "203.0.113.1"
  })";

  WriteBanFile(kJson);

  EXPECT_CALL(net_server, AddToBanList(testing::Matcher<const char*>(testing::_), testing::_)).Times(0);

  EXPECT_FALSE(manager.Load());
  EXPECT_TRUE(manager.GetBanList().empty());
}

TEST_F(BanListTest, SkipsEntriesWithoutValidIp) {
  constexpr char kJson[] = R"([
    {
      "Nickname": "NoIP",
      "Reason": "missing ip"
    },
    {
      "Nickname": "Valid",
      "IP": "192.0.2.55",
      "Reason": "ok"
    }
  ])";

  WriteBanFile(kJson);

  EXPECT_CALL(net_server, AddToBanList(testing::Matcher<const char*>(testing::StrEq("192.0.2.55")), 0)).Times(1);

  EXPECT_TRUE(manager.Load());

  ASSERT_EQ(1u, manager.GetBanList().size());
  EXPECT_EQ("Valid", manager.GetBanList().front().nickname);
  EXPECT_EQ("192.0.2.55", manager.GetBanList().front().ip);
}
}  // namespace

int main(int argc, char** argv) {
  ::testing::GTEST_FLAG(catch_exceptions) = false;
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
