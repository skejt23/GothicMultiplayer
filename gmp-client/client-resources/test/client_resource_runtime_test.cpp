/*
MIT License

Copyright (c) 2025 Gothic Multiplayer Team

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

#include "client_resources/client_resource_runtime.h"

#include <gtest/gtest.h>
#include <sodium.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "resource/packer.h"

namespace {

namespace fs = std::filesystem;

void EnsureSodiumInitialized() {
  static bool initialized = []() {
    if (sodium_init() < 0) {
      throw std::runtime_error("Failed to initialize libsodium");
    }
    return true;
  }();
  (void)initialized;
}

class ClientResourceRuntimeTest : public ::testing::Test {
protected:
  void SetUp() override {
    EnsureSodiumInitialized();

    const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    base_dir_ = fs::temp_directory_path() / ("client_resource_runtime_test_" + unique);
    src_dir_ = base_dir_ / "src";
    out_dir_ = base_dir_ / "out";

    fs::create_directories(src_dir_);
    fs::create_directories(out_dir_);
  }

  void TearDown() override {
    std::error_code ec;
    fs::remove_all(base_dir_, ec);
  }

  void WriteFile(const fs::path& relative_path, std::string_view content) {
    const fs::path file_path = src_dir_ / relative_path;
    fs::create_directories(file_path.parent_path());

    std::ofstream ofs(file_path, std::ios::binary);
    ASSERT_TRUE(ofs.good()) << "Unable to write test file: " << file_path.string();
    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
  }

  gmp::client::GameClient::ResourcePayload BuildPayload(const std::string& resource_name) {
    gmp::resource::PackOptions opts;
    opts.src_dir = src_dir_.string();
    opts.out_dir = out_dir_.string();
    opts.name = resource_name;
    opts.version = "1.0.0";
    opts.compile_lua = true;
    opts.strip_debug = true;
    opts.compression_level = 6;

    auto result = gmp::resource::PackResource(opts);

    gmp::client::GameClient::ResourcePayload payload;
    payload.descriptor.name = result.manifest.name;
    payload.descriptor.version = result.manifest.version;
    payload.descriptor.manifest_path = result.manifest_path;
    payload.descriptor.manifest_sha256 = result.manifest.signature;
    payload.descriptor.archive_path = result.pak_path;
    payload.descriptor.archive_sha256 = result.manifest.archive.sha256;
    payload.descriptor.archive_size = result.manifest.archive.size;
    payload.manifest_json = ReadTextFile(result.manifest_path);
    payload.archive_bytes = ReadBinaryFile(result.pak_path);

    return payload;
  }

  static std::string ReadTextFile(const fs::path& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
      throw std::runtime_error("Unable to open file: " + path.string());
    }
    return std::string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
  }

  static std::vector<std::uint8_t> ReadBinaryFile(const fs::path& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
      throw std::runtime_error("Unable to open file: " + path.string());
    }
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
  }

  fs::path base_dir_;
  fs::path src_dir_;
  fs::path out_dir_;
};

TEST_F(ClientResourceRuntimeTest, LoadsResourceAndExposesExports) {
  WriteFile("client/main.lua", R"(
    exports = {}

    function exports.greet(name)
      return "Hello, " .. name
    end

    function onResourceStart()
      exports.ready = true
    end

    return exports
  )");

  auto payload = BuildPayload("greetings");

  ClientResourceRuntime runtime;
  std::string error;
  ASSERT_TRUE(runtime.LoadResources({payload}, error)) << error;

  auto exports_opt = runtime.GetExports("greetings");
  ASSERT_TRUE(exports_opt.has_value());

  sol::table exports = exports_opt.value();
  EXPECT_TRUE(exports.get_or("ready", false));

  sol::function greet = exports["greet"];
  ASSERT_TRUE(greet.valid());
  const std::string greeting = greet("World");
  EXPECT_EQ(greeting, "Hello, World");
}

TEST_F(ClientResourceRuntimeTest, UnloadResourcesClearsExportsAndInvokesStop) {
  WriteFile("client/main.lua", R"(
    exports = { counter = 0 }

    function onResourceStart()
      exports.counter = exports.counter + 1
    end

    function onResourceStop()
      exports.counter = -1
    end

    return exports
  )");

  auto payload = BuildPayload("lifecycle_test");

  ClientResourceRuntime runtime;
  std::string error;
  ASSERT_TRUE(runtime.LoadResources({payload}, error)) << error;

  auto exports_opt = runtime.GetExports("lifecycle_test");
  ASSERT_TRUE(exports_opt.has_value());

  sol::table exports = exports_opt.value();
  EXPECT_EQ(exports.get<int>("counter"), 1);

  runtime.UnloadResources();

  EXPECT_EQ(exports.get<int>("counter"), -1);
  EXPECT_FALSE(runtime.GetExports("lifecycle_test").has_value());
}

TEST_F(ClientResourceRuntimeTest, LoadFailsWhenEntrypointThrows) {
  WriteFile("client/main.lua", R"(
    error("intentional failure")
  )");

  auto payload = BuildPayload("broken");

  ClientResourceRuntime runtime;
  std::string error;
  EXPECT_FALSE(runtime.LoadResources({payload}, error));
  EXPECT_NE(error.find("broken"), std::string::npos);
  EXPECT_NE(error.find("intentional failure"), std::string::npos);

  auto exports_opt = runtime.GetExports("broken");
  EXPECT_FALSE(exports_opt.has_value());
}

TEST_F(ClientResourceRuntimeTest, RequireLoadsModulesFromClientAndSharedDirs) {
  WriteFile("client/main.lua", R"(
    local util = require("shared.util")
    local constants = require("client.constants")

    exports = {}

    function exports.compute(a, b)
      return util.sum(a, b) + constants.offset
    end

    return exports
  )");

  WriteFile("shared/util.lua", R"(
    local M = {}
    function M.sum(a, b)
      return a + b
    end
    return M
  )");

  WriteFile("client/constants.lua", R"(
    local C = { offset = 5 }
    return C
  )");

  auto payload = BuildPayload("require_test");

  ClientResourceRuntime runtime;
  std::string error;
  ASSERT_TRUE(runtime.LoadResources({payload}, error)) << error;

  auto exports_opt = runtime.GetExports("require_test");
  ASSERT_TRUE(exports_opt.has_value());

  sol::table exports = exports_opt.value();
  sol::function compute = exports["compute"];
  ASSERT_TRUE(compute.valid());
  int result = compute(3, 4);
  EXPECT_EQ(result, 12);
}

}  // namespace

int main(int argc, char** argv) {
  ::testing::GTEST_FLAG(catch_exceptions) = false;
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
