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
#include <minizip/unzip.h>
#include <sodium.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "resource/packer.h"

namespace {
namespace fs = std::filesystem;

constexpr std::size_t kBufferSize = 64 * 1024;

using gmp::resource::FileMeta;
using gmp::resource::PackOptions;
using gmp::resource::PackResource;

class ResourcePackerTest : public ::testing::Test {
protected:
  void SetUp() override {
    EnsureSodiumInitialized();

    const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    base_dir_ = fs::temp_directory_path() / ("resource_packer_test_" + unique);
    src_dir_ = base_dir_ / "src";
    out_dir_ = base_dir_ / "out";

    fs::create_directories(src_dir_);
    fs::create_directories(out_dir_);
  }

  void TearDown() override {
    std::error_code ec;
    fs::remove_all(base_dir_, ec);
  }

  static void EnsureSodiumInitialized() {
    static int status = sodium_init();
    ASSERT_GE(status, 0) << "Failed to initialize libsodium";
  }

  void WriteFile(const fs::path& relative_path, std::string_view content) {
    const fs::path file_path = src_dir_ / relative_path;
    fs::create_directories(file_path.parent_path());

    std::ofstream out(file_path, std::ios::binary);
    ASSERT_TRUE(out.good()) << "Unable to create test file: " << file_path.string();
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
  }

  PackOptions BuildDefaultOptions() const {
    PackOptions opts;
    opts.src_dir = src_dir_.string();
    opts.out_dir = out_dir_.string();
    opts.name = "hud";
    opts.version = "2025.11.07.1800";
    opts.compile_lua = true;
    opts.strip_debug = true;
    opts.compression_level = 6;
    return opts;
  }

  const FileMeta* FindFile(const std::vector<FileMeta>& files, const std::string& path) const {
    const auto it = std::find_if(files.begin(), files.end(), [&](const FileMeta& meta) { return meta.path == path; });
    if (it == files.end()) {
      return nullptr;
    }
    return &(*it);
  }

  static std::string BytesToHex(const std::vector<std::uint8_t>& bytes) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (auto byte : bytes) {
      oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
  }

  static std::string ComputeSHA256(const std::vector<std::uint8_t>& bytes) {
    crypto_hash_sha256_state state;
    crypto_hash_sha256_init(&state);
    if (!bytes.empty()) {
      crypto_hash_sha256_update(&state, bytes.data(), bytes.size());
    }
    std::vector<std::uint8_t> hash(crypto_hash_sha256_BYTES);
    crypto_hash_sha256_final(&state, hash.data());
    return BytesToHex(hash);
  }

  static std::string ComputeFileSHA256(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
      throw std::runtime_error("Unable to open file for hashing: " + path.string());
    }
    crypto_hash_sha256_state state;
    crypto_hash_sha256_init(&state);
    std::array<char, kBufferSize> buffer;
    while (file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
      crypto_hash_sha256_update(&state, reinterpret_cast<const unsigned char*>(buffer.data()), file.gcount());
    }
    std::vector<std::uint8_t> hash(crypto_hash_sha256_BYTES);
    crypto_hash_sha256_final(&state, hash.data());
    return BytesToHex(hash);
  }

  static std::vector<std::uint8_t> ReadFileFromZip(const fs::path& zip_path, const std::string& entry_path) {
    unzFile zip_file = unzOpen64(zip_path.string().c_str());
    if (!zip_file) {
      throw std::runtime_error("Unable to open ZIP archive: " + zip_path.string());
    }

    struct ZipCloser {
      ~ZipCloser() {
        if (file) {
          unzClose(file);
        }
      }
      unzFile file = nullptr;
    } closer{zip_file};

    if (unzLocateFile(zip_file, entry_path.c_str(), 0) != UNZ_OK) {
      throw std::runtime_error("Entry not found in ZIP: " + entry_path);
    }

    if (unzOpenCurrentFile(zip_file) != UNZ_OK) {
      throw std::runtime_error("Unable to open entry: " + entry_path);
    }

    std::vector<std::uint8_t> data;
    std::array<char, kBufferSize> buffer;
    int bytes_read = 0;
    while ((bytes_read = unzReadCurrentFile(zip_file, buffer.data(), buffer.size())) > 0) {
      data.insert(data.end(), buffer.data(), buffer.data() + bytes_read);
    }

    if (bytes_read < 0) {
      unzCloseCurrentFile(zip_file);
      throw std::runtime_error("Failed to read entry: " + entry_path);
    }

    unzCloseCurrentFile(zip_file);
    return data;
  }

  fs::path base_dir_;
  fs::path src_dir_;
  fs::path out_dir_;
};

TEST_F(ResourcePackerTest, PacksOnlyLuaFiles) {
  WriteFile("client/main.lua", "return 42");
  WriteFile("shared/util.lua", "return function() return 123 end");
  WriteFile("shared/config.txt", "important=true");  // Should be ignored.

  auto result = PackResource(BuildDefaultOptions());

  ASSERT_TRUE(fs::exists(result.pak_path));
  ASSERT_TRUE(fs::exists(result.manifest_path));
  ASSERT_EQ(result.manifest.files.size(), 2u);

  const FileMeta* lua_meta = FindFile(result.manifest.files, "client/main.luac");
  ASSERT_NE(lua_meta, nullptr);
  const auto lua_bytes = ReadFileFromZip(result.pak_path, lua_meta->path);
  ASSERT_GE(lua_bytes.size(), 4u);
  EXPECT_EQ(lua_meta->sha256, ComputeSHA256(lua_bytes));
  EXPECT_EQ(lua_bytes[0], 0x1b);
  EXPECT_EQ(lua_bytes[1], 'L');

  const FileMeta* util_meta = FindFile(result.manifest.files, "shared/util.luac");
  ASSERT_NE(util_meta, nullptr);
  const auto util_bytes = ReadFileFromZip(result.pak_path, util_meta->path);
  EXPECT_EQ(util_meta->sha256, ComputeSHA256(util_bytes));

  EXPECT_EQ(FindFile(result.manifest.files, "shared/config.txt"), nullptr);

  EXPECT_EQ(result.manifest.archive.path, "hud/hud-2025.11.07.1800.pak");
  EXPECT_GT(result.manifest.archive.size, 0u);
  EXPECT_EQ(result.manifest.archive.sha256, ComputeFileSHA256(result.pak_path));
}

TEST_F(ResourcePackerTest, SkipsNonLuaFiles) {
  WriteFile("client/readme.txt", "text only");
  WriteFile("shared/image.png", "PNGDATA");

  auto result = PackResource(BuildDefaultOptions());

  EXPECT_TRUE(result.manifest.files.empty());
  EXPECT_EQ(result.manifest.archive.path, "hud/hud-2025.11.07.1800.pak");
}

TEST_F(ResourcePackerTest, ThrowsWhenSourceDirectoryMissing) {
  PackOptions opts = BuildDefaultOptions();
  opts.src_dir = (base_dir_ / "missing").string();

  EXPECT_THROW(PackResource(opts), std::runtime_error);
}

TEST_F(ResourcePackerTest, ThrowsOnInvalidLuaSyntax) {
  WriteFile("client/broken.lua", "function end");  // Invalid Lua syntax

  PackOptions opts = BuildDefaultOptions();
  EXPECT_THROW(PackResource(opts), std::runtime_error);
}

}  // namespace

int main(int argc, char** argv) {
  ::testing::GTEST_FLAG(catch_exceptions) = false;
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
