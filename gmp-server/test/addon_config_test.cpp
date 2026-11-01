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
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "config.h"

namespace {

class AddonConfigTest : public ::testing::Test {
protected:
  void SetUp() override {
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    directory_ = std::filesystem::temp_directory_path() / ("gmp-addon-config-test-" + std::to_string(nonce));
    std::filesystem::create_directories(directory_);
    config_path_ = directory_ / "config.toml";
  }

  void TearDown() override {
    std::error_code ignored;
    std::filesystem::remove_all(directory_, ignored);
  }

  void WriteConfig(const std::string& body, const std::string& seed = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=") const {
    std::ofstream output(config_path_, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(output.is_open());
    output << "server_identity_seed = \"" << seed << "\"\n";
    output << body;
    ASSERT_TRUE(output.good());
  }

  std::filesystem::path directory_;
  std::filesystem::path config_path_;
};

TEST_F(AddonConfigTest, ParsesOrderedAddonsAndDownloaderSettings) {
  WriteConfig(R"(
addon_vdfs = ["addons/base.vdf", "", "addons/base.vdf", "addons/override.vdf"]
downloader_file_max_chunk = 2097152
downloader_rate_limit = 0
downloader_group = "MyServer"
downloader_download_timeout_seconds = 45
)");

  const Config config(config_path_);
  EXPECT_EQ(config.Get<std::vector<std::string>>("addon_vdfs"), (std::vector<std::string>{"addons/base.vdf", "addons/override.vdf"}));
  EXPECT_EQ(config.Get<std::int32_t>("downloader_file_max_chunk"), 2097152);
  EXPECT_EQ(config.Get<std::int32_t>("downloader_rate_limit"), 0);
  EXPECT_EQ(config.Get<std::string>("downloader_group"), "MyServer");
  EXPECT_EQ(config.Get<std::int32_t>("downloader_download_timeout_seconds"), 45);
}

TEST_F(AddonConfigTest, PreservesExistingWebsiteTokenAcrossReloads) {
  WriteConfig("public = true\n");

  const Config config(config_path_);
  EXPECT_EQ(config.Get<std::string>("server_identity_seed"), "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=");

  const Config reloaded(config_path_);
  EXPECT_EQ(reloaded.Get<std::string>("server_identity_seed"), config.Get<std::string>("server_identity_seed"));
}

TEST_F(AddonConfigTest, PersistsGeneratedWebsiteTokenWithoutLosingServerSettings) {
  WriteConfig("name = \"Website Token Test\"\npublic = true\n", "");

  const Config config(config_path_);
  const auto token = config.Get<std::string>("server_identity_seed");
  ASSERT_EQ(token.size(), 44);

  const Config reloaded(config_path_);
  EXPECT_EQ(reloaded.Get<std::string>("server_identity_seed"), token);
  EXPECT_EQ(reloaded.Get<std::string>("name"), "Website Token Test");
  EXPECT_TRUE(reloaded.Get<bool>("public"));
}

}  // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
