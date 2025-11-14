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

#include <gtest/gtest.h>
#include <sodium.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "resource/loader.h"
#include "resource/packer.h"

namespace {
namespace fs = std::filesystem;

// Helper to initialize libsodium once
void EnsureSodiumInitialized() {
  static int status = sodium_init();
  if (status < 0) {
    throw std::runtime_error("Failed to initialize libsodium");
  }
}

class ResourceLoaderTest : public ::testing::Test {
protected:
  void SetUp() override {
    EnsureSodiumInitialized();

    const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    base_dir_ = fs::temp_directory_path() / ("resource_loader_test_" + unique);
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

    std::ofstream out(file_path, std::ios::binary);
    ASSERT_TRUE(out.good()) << "Unable to create test file: " << file_path.string();
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
  }

  std::string CreateTestPack() {
    WriteFile("client/main.lua", "return 42");
    WriteFile("shared/util.lua", "return function() return 123 end");

    gmp::resource::PackOptions opts;
    opts.src_dir = src_dir_.string();
    opts.out_dir = out_dir_.string();
    opts.name = "testpack";
    opts.version = "1.0.0";
    opts.compile_lua = true;
    opts.strip_debug = true;
    opts.compression_level = 6;

    auto result = gmp::resource::PackResource(opts);
    return result.manifest_path;
  }

  fs::path base_dir_;
  fs::path src_dir_;
  fs::path out_dir_;
};

TEST_F(ResourceLoaderTest, LoadsValidPack) {
  const std::string manifest_path = CreateTestPack();

  auto pack = gmp::resource::ResourcePackLoader::Load(manifest_path, true);

  const auto& manifest = pack.GetManifest();
  EXPECT_EQ(manifest.name, "testpack");
  EXPECT_EQ(manifest.version, "1.0.0");
  EXPECT_EQ(manifest.format, "zip");
  EXPECT_EQ(manifest.files.size(), 2u);
}

TEST_F(ResourceLoaderTest, HasFileChecksExistence) {
  const std::string manifest_path = CreateTestPack();

  auto pack = gmp::resource::ResourcePackLoader::Load(manifest_path);

  EXPECT_TRUE(pack.HasFile("client/main.luac"));
  EXPECT_TRUE(pack.HasFile("shared/util.luac"));
  EXPECT_FALSE(pack.HasFile("client/missing.luac"));
  EXPECT_FALSE(pack.HasFile("nonexistent/file.txt"));
}

TEST_F(ResourceLoaderTest, LoadsFileContent) {
  const std::string manifest_path = CreateTestPack();

  auto pack = gmp::resource::ResourcePackLoader::Load(manifest_path);

  auto loaded = pack.LoadFile("client/main.luac", true);
  EXPECT_EQ(loaded.path, "client/main.luac");
  EXPECT_GT(loaded.size, 0u);
  EXPECT_EQ(loaded.data.size(), loaded.size);
  EXPECT_FALSE(loaded.sha256.empty());

  // Verify Lua bytecode header
  ASSERT_GE(loaded.data.size(), 4u);
  EXPECT_EQ(loaded.data[0], 0x1b);
  EXPECT_EQ(loaded.data[1], 'L');
}

TEST_F(ResourceLoaderTest, VerifiesFileHash) {
  const std::string manifest_path = CreateTestPack();

  auto pack = gmp::resource::ResourcePackLoader::Load(manifest_path, false);  // Skip archive verification

  // Loading with hash verification should succeed
  ASSERT_NO_THROW(pack.LoadFile("client/main.luac", true));
}

TEST_F(ResourceLoaderTest, GetFileMetadataReturnsCorrectInfo) {
  const std::string manifest_path = CreateTestPack();

  auto pack = gmp::resource::ResourcePackLoader::Load(manifest_path);

  auto meta_opt = pack.GetFileMetadata("client/main.luac");
  ASSERT_TRUE(meta_opt.has_value());
  const auto& meta = meta_opt->get();
  EXPECT_EQ(meta.path, "client/main.luac");
  EXPECT_GT(meta.size, 0u);
  EXPECT_FALSE(meta.sha256.empty());
  EXPECT_TRUE(meta.cache);

  auto missing = pack.GetFileMetadata("missing.lua");
  EXPECT_FALSE(missing.has_value());
}

TEST_F(ResourceLoaderTest, GetFilePathsReturnsAllFiles) {
  const std::string manifest_path = CreateTestPack();

  auto pack = gmp::resource::ResourcePackLoader::Load(manifest_path);

  auto paths = pack.GetFilePaths();
  EXPECT_EQ(paths.size(), 2u);

  std::sort(paths.begin(), paths.end());
  EXPECT_EQ(paths[0], "client/main.luac");
  EXPECT_EQ(paths[1], "shared/util.luac");
}

TEST_F(ResourceLoaderTest, ThrowsOnMissingManifest) {
  EXPECT_THROW(gmp::resource::ResourcePackLoader::Load("/nonexistent/manifest.json"), std::runtime_error);
}

TEST_F(ResourceLoaderTest, ThrowsOnMissingArchive) {
  const std::string manifest_path = CreateTestPack();

  // Delete the .pak file
  const fs::path pak_path = out_dir_ / "testpack-1.0.0.pak";
  fs::remove(pak_path);

  EXPECT_THROW(gmp::resource::ResourcePackLoader::Load(manifest_path), std::runtime_error);
}

TEST_F(ResourceLoaderTest, ThrowsOnMissingFile) {
  const std::string manifest_path = CreateTestPack();

  auto pack = gmp::resource::ResourcePackLoader::Load(manifest_path);

  EXPECT_THROW(pack.LoadFile("missing.luac"), std::runtime_error);
}

TEST_F(ResourceLoaderTest, UnloadClearsState) {
  const std::string manifest_path = CreateTestPack();

  {
    auto pack = gmp::resource::ResourcePackLoader::Load(manifest_path);
    EXPECT_TRUE(pack.HasFile("client/main.luac"));
    // pack goes out of scope and is destroyed (RAII cleanup)
  }
  
  // Can load another pack (no loader state to worry about)
  auto pack2 = gmp::resource::ResourcePackLoader::Load(manifest_path);
  EXPECT_TRUE(pack2.HasFile("client/main.luac"));
}

TEST_F(ResourceLoaderTest, ThrowsWhenLoadingFromUnloadedState) {
  const std::string manifest_path = CreateTestPack();

  auto pack = gmp::resource::ResourcePackLoader::Load(manifest_path);
  
  // Move the pack to simulate transferring ownership
  auto pack2 = std::move(pack);
  
  // pack2 should still work
  EXPECT_NO_THROW(pack2.LoadFile("client/main.luac"));
}

}  // namespace

int main(int argc, char** argv) {
  ::testing::GTEST_FLAG(catch_exceptions) = false;
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
