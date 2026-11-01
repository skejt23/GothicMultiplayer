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

#include <addon/addon_vfs.h>
#include <addon/addon_transport.h>
#include <bitsery/adapter/buffer.h>
#include <bitsery/bitsery.h>
#include <gtest/gtest.h>
#include <zenkit/Stream.hh>
#include <zenkit/Vfs.hh>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "packets.h"
#include "shared/crypto_utils.h"

namespace {

class AddonSupportTest : public ::testing::Test {
protected:
  void SetUp() override {
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    temporary_directory_ = std::filesystem::temp_directory_path() / ("gmp-addon-test-" + std::to_string(nonce));
    std::filesystem::create_directories(temporary_directory_);
  }

  void TearDown() override {
    std::error_code ignored;
    std::filesystem::remove_all(temporary_directory_, ignored);
  }

  std::filesystem::path CreateVdf(const std::string& name,
                                  const std::vector<std::pair<std::string, std::string>>& files,
                                  bool compressed = false) const {
    zenkit::Vfs vfs;
    std::vector<std::vector<std::byte>> storage;
    storage.reserve(files.size());
    for (const auto& [path, value] : files) {
      storage.emplace_back();
      auto& bytes = storage.back();
      bytes.reserve(value.size());
      for (unsigned char ch : value) {
        bytes.push_back(static_cast<std::byte>(ch));
      }

      const auto separator = path.find_last_of('/');
      auto* parent = const_cast<zenkit::VfsNode*>(&vfs.root());
      std::string filename = path;
      if (separator != std::string::npos) {
        parent = &vfs.mkdir(path.substr(0, separator));
        filename = path.substr(separator + 1);
      }
      parent->create(zenkit::VfsNode::file(filename, zenkit::VfsFileDescriptor{bytes.data(), bytes.size(), false}));
    }

    const auto output_path = temporary_directory_ / name;
    auto writer = zenkit::Write::to(output_path);
    if (compressed) {
      vfs.save_compressed(writer.get(), zenkit::GameVersion::GOTHIC_2);
    } else {
      vfs.save(writer.get(), zenkit::GameVersion::GOTHIC_2);
    }
    return output_path;
  }

  std::filesystem::path temporary_directory_;
};

TEST_F(AddonSupportTest, NormalizesLogicalVdfPathsAndRejectsTraversal) {
  EXPECT_EQ(gmp::addon::NormalizeVdfPath(R"(\_WORK\DATA\Textures\A.TEX)"), "_WORK/DATA/Textures/A.TEX");
  EXPECT_EQ(gmp::addon::NormalizeVdfPath("./SCRIPTS//GOTHIC.DAT"), "SCRIPTS/GOTHIC.DAT");
  EXPECT_FALSE(gmp::addon::NormalizeVdfPath("../outside.vdf"));
  EXPECT_FALSE(gmp::addon::NormalizeVdfPath("C:/outside.vdf"));
  EXPECT_FALSE(gmp::addon::NormalizeVdfPath("/"));
}

TEST_F(AddonSupportTest, LaterArchivesOverrideEarlierArchives) {
  const auto first = CreateVdf("first.vdf", {{"DATA/TEST.TXT", "first"}, {"ONLY_FIRST.TXT", "kept"}});
  const auto second = CreateVdf("second.vdf", {{"DATA/TEST.TXT", "second"}});

  gmp::addon::AddonVfs vfs;
  vfs.MountArchives({first, second});
  auto file = vfs.Open("_work/data/data/test.txt");
  ASSERT_NE(file, nullptr);
  EXPECT_EQ(file->Size(), 6u);

  char bytes[7]{};
  EXPECT_EQ(file->Read(bytes, 3), 3u);
  EXPECT_EQ(std::string(bytes, 3), "sec");
  EXPECT_TRUE(file->Seek(1));
  EXPECT_EQ(file->Tell(), 1u);
  EXPECT_EQ(file->Read(bytes, 5), 5u);
  EXPECT_EQ(std::string(bytes, 5), "econd");
  EXPECT_TRUE(file->Eof());
  EXPECT_TRUE(vfs.Exists("only_first.txt"));
}

TEST_F(AddonSupportTest, OpenFileRetainsArchiveAfterVfsDestruction) {
  const auto archive = CreateVdf("lifetime.vdf", {{"LIFETIME.TXT", "alive"}});
  std::shared_ptr<gmp::addon::AddonFile> file;
  {
    gmp::addon::AddonVfs vfs;
    vfs.MountArchive(archive);
    file = vfs.Open("lifetime.txt");
    ASSERT_NE(file, nullptr);
  }

  char bytes[6]{};
  EXPECT_EQ(file->Read(bytes, 5), 5u);
  EXPECT_EQ(std::string(bytes, 5), "alive");
}

TEST_F(AddonSupportTest, ExactOpenKeepsMusicDirectoriesAndNestedPathsDistinct) {
  const auto archive = CreateVdf("music.vdf", {{"MULTIPLAYER/MUSIC/AMBIENT/THEME.MP3", "multiplayer"},
      {"_WORK/DATA/MUSIC/AMBIENT/THEME.MP3", "gothic"}, {"MUSIC/SHORT/THEME.OGG", "short"},
      {"UNRELATED/MISSING.MP3", "wrong directory"}});
  gmp::addon::AddonVfs vfs;
  vfs.MountArchive(archive);
  const auto first = vfs.OpenExact("./Multiplayer\\Music\\Ambient\\Theme.mp3");
  const auto second = vfs.OpenExact("_Work/Data/Music/Ambient/Theme.mp3");
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  std::array<char, 32> bytes{};
  EXPECT_EQ(first->Read(bytes.data(), bytes.size()), 11u);
  EXPECT_EQ(std::string(bytes.data(), 11), "multiplayer");
  EXPECT_EQ(second->Read(bytes.data(), bytes.size()), 6u);
  EXPECT_EQ(std::string(bytes.data(), 6), "gothic");
  EXPECT_NE(vfs.OpenExact("Music/Short/Theme.ogg"), nullptr);
  EXPECT_EQ(vfs.OpenExact("Multiplayer/Music/Missing.mp3"), nullptr);
  EXPECT_EQ(vfs.OpenExact("_Work/Data/Music/Missing.mp3"), nullptr);
  EXPECT_EQ(vfs.OpenExact("_Work/Data/Music/../UNRELATED/Missing.mp3"), nullptr);
  EXPECT_EQ(vfs.OpenExact("Multiplayer/Music/Ambient/Theme.mp3/child.ogg"), nullptr);
  EXPECT_EQ(vfs.OpenExact("Multiplayer/Music/Ambient"), nullptr);
  // Existing Gothic-compatible readers retain their prior basename fallback.
  EXPECT_NE(vfs.Open("Missing.mp3"), nullptr);
}

TEST_F(AddonSupportTest, MountsVdfsToolArchiveWithImplicitCatalogOffset) {
  const auto archive = CreateVdf(
      "legacy-tool.vdf", {{"_WORK/DATA/WORLDS/COLONY.ZEN", "world data"}, {"_WORK/DATA/TEXTURES/_COMPILED/LOADING_COLONY-C.TEX", "loading texture"}});
  {
    std::fstream output(archive, std::ios::binary | std::ios::in | std::ios::out);
    ASSERT_TRUE(output.is_open());
    constexpr char signature[] = "PSVDSC_V2.00\x1A\x1A\x1A\x1A";
    output.seekp(256);
    output.write(signature, 16);
    // VDFS tool archives can leave both the size and catalog offset at zero.
    constexpr std::array<char, 8> legacy_sizes{};
    output.seekp(284);
    output.write(legacy_sizes.data(), static_cast<std::streamsize>(legacy_sizes.size()));
    ASSERT_TRUE(output.good());
  }

  std::string error;
  ASSERT_TRUE(gmp::addon::AddonVfs::ValidateArchive(archive, error)) << error;
  gmp::addon::AddonVfs vfs;
  ASSERT_NO_THROW(vfs.MountArchive(archive));
  const auto world = vfs.ReadFile("_WORK/DATA/WORLDS/COLONY.ZEN", 1024);
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(world.data()), world.size()), "world data");
  const auto texture = vfs.ReadFile("_WORK/DATA/TEXTURES/_COMPILED/LOADING_COLONY-C.TEX", 1024);
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(texture.data()), texture.size()), "loading texture");
}

TEST_F(AddonSupportTest, RejectsOutOfBoundsCatalogOffsets) {
  const auto archive = CreateVdf("invalid-catalog.vdf", {{"VALUE.TXT", "payload"}});
  const auto archive_size = static_cast<std::uint32_t>(std::filesystem::file_size(archive));
  for (const auto offset : {1U, 295U, archive_size - 1U, archive_size, std::numeric_limits<std::uint32_t>::max()}) {
    SCOPED_TRACE(offset);
    {
      std::fstream output(archive, std::ios::binary | std::ios::in | std::ios::out);
      ASSERT_TRUE(output.is_open());
      const std::array<char, 4> bytes{static_cast<char>(offset & 0xFFU), static_cast<char>((offset >> 8) & 0xFFU),
                                      static_cast<char>((offset >> 16) & 0xFFU), static_cast<char>((offset >> 24) & 0xFFU)};
      output.seekp(288);
      output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
      ASSERT_TRUE(output.good());
    }
    std::string error;
    EXPECT_FALSE(gmp::addon::AddonVfs::ValidateArchive(archive, error));
    EXPECT_NE(error.find("VDF catalog lies outside the archive"), std::string::npos) << error;
  }
}

TEST_F(AddonSupportTest, MountsAndReadsUnionCompressedVdf) {
  const std::string expected(32 * 1024, 'Z');
  const auto archive = CreateVdf("union-compressed.vdf", {{"DATA/COMPRESSED.TXT", expected}}, true);

  std::string error;
  ASSERT_TRUE(gmp::addon::AddonVfs::ValidateArchive(archive, error)) << error;

  gmp::addon::AddonVfs vfs;
  ASSERT_NO_THROW(vfs.MountArchive(archive));
  EXPECT_EQ(vfs.FileSize("data/compressed.txt"), expected.size());
  const auto bytes = vfs.ReadFile("DATA/COMPRESSED.TXT", expected.size());
  ASSERT_EQ(bytes.size(), expected.size());
  EXPECT_TRUE(std::equal(bytes.begin(), bytes.end(), reinterpret_cast<const std::byte*>(expected.data())));
}

TEST_F(AddonSupportTest, ReadsZenKitUnionCompressedFixture) {
  const auto archive = std::filesystem::current_path() / "thirdparty" / "ZenKit" / "tests" / "samples" /
                       "basic_zipped.vdf";
  ASSERT_TRUE(std::filesystem::is_regular_file(archive));

  std::string error;
  ASSERT_TRUE(gmp::addon::AddonVfs::ValidateArchive(archive, error)) << error;

  gmp::addon::AddonVfs vfs;
  ASSERT_NO_THROW(vfs.MountArchive(archive));
  auto file = vfs.Open("config.yml");
  ASSERT_NE(file, nullptr);
  ASSERT_GT(file->Size(), 0u);
  std::vector<std::byte> bytes(file->Size());
  EXPECT_EQ(file->Read(bytes.data(), bytes.size()), bytes.size());
}

TEST_F(AddonSupportTest, FileDigestIsStableAndArchiveValidationRejectsGarbage) {
  const auto archive = CreateVdf("digest.vdf", {{"VALUE.TXT", "digest me"}});
  const auto first = gmp::crypto::ComputeFileSHA256(archive);
  const auto second = gmp::crypto::ComputeFileSHA256(archive);
  EXPECT_EQ(first, second);
  EXPECT_EQ(first.size(), 64u);

  std::string error;
  EXPECT_TRUE(gmp::addon::AddonVfs::ValidateArchive(archive, error));
  EXPECT_TRUE(gmp::addon::AddonVfs::ValidateCachedArchive(
      archive, std::filesystem::file_size(archive), first, false, error));

  {
    std::fstream cached(archive, std::ios::binary | std::ios::in | std::ios::out);
    ASSERT_TRUE(cached.is_open());
    cached.seekp(-1, std::ios::end);
    cached.put('!');
  }
  EXPECT_FALSE(gmp::addon::AddonVfs::ValidateCachedArchive(
      archive, std::filesystem::file_size(archive), first, false, error));
  EXPECT_EQ(error, "SHA-256 mismatch");

  const auto garbage = temporary_directory_ / "garbage.vdf";
  {
    std::ofstream output(garbage, std::ios::binary);
    output << "not a VDF archive";
  }
  EXPECT_FALSE(gmp::addon::AddonVfs::ValidateArchive(garbage, error));
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(gmp::addon::AddonVfs::ValidateCachedArchive(
      garbage, std::filesystem::file_size(garbage), gmp::crypto::ComputeFileSHA256(garbage), false, error));
}

TEST_F(AddonSupportTest, InitialInfoRoundTripsAddonDescriptors) {
  InitialInfoPacket input;
  input.map_name = "NEWWORLD.ZEN";
  input.server_name = "Addon Test";
  input.downloader_group = "MyServer";
  ClientResourceInfoEntry resource;
  resource.name = "client";
  resource.version = "1.0.0";
  resource.manifest_path = "client.manifest.json";
  resource.manifest_sha256 = std::string(64, 'd');
  resource.archive_path = "client.pak";
  resource.archive_sha256 = std::string(64, 'e');
  resource.archive_size = 456789;
  resource.manifest_size = 1234;
  input.client_resources.push_back(resource);
  AddonVdfInfoEntry addon;
  addon.logical_name = "test.vdf";
  addon.sha256 = std::string(64, 'a');
  addon.size = 123456;
  addon.contains_gothic_dat = true;
  input.addon_vdfs.push_back(std::move(addon));
  input.addon_bundle.archive_path = "addons.zip";
  input.addon_bundle.archive_sha256 = std::string(64, 'b');
  input.addon_bundle.archive_size = 65432;
  input.addon_bundle.manifest_path = "addons.bin";
  input.addon_bundle.manifest_sha256 = std::string(64, 'c');
  input.addon_bundle.manifest_size = 117;

  std::vector<std::uint8_t> buffer;
  const auto written = bitsery::quickSerialization<bitsery::OutputBufferAdapter<std::vector<std::uint8_t>>>(buffer, input);
  InitialInfoPacket output;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  const auto state = bitsery::quickDeserialization<InputAdapter>({buffer.data(), written}, output);

  ASSERT_TRUE(state.second);
  ASSERT_EQ(output.addon_vdfs.size(), 1u);
  ASSERT_EQ(output.client_resources.size(), 1u);
  EXPECT_EQ(output.client_resources[0].manifest_size, 1234u);
  EXPECT_EQ(output.downloader_group, "MyServer");
  EXPECT_EQ(output.addon_vdfs[0].logical_name, "test.vdf");
  EXPECT_EQ(output.addon_vdfs[0].sha256, std::string(64, 'a'));
  EXPECT_EQ(output.addon_vdfs[0].size, 123456u);
  EXPECT_TRUE(output.addon_vdfs[0].contains_gothic_dat);
  EXPECT_EQ(output.addon_bundle.archive_path, "addons.zip");
  EXPECT_EQ(output.addon_bundle.archive_sha256, std::string(64, 'b'));
  EXPECT_EQ(output.addon_bundle.archive_size, 65432u);
  EXPECT_EQ(output.addon_bundle.manifest_path, "addons.bin");
  EXPECT_EQ(output.addon_bundle.manifest_sha256, std::string(64, 'c'));
  EXPECT_EQ(output.addon_bundle.manifest_size, 117u);
}

TEST_F(AddonSupportTest, PackagesReusesAndExtractsOrderedAddonBundle) {
  const auto first_source =
      CreateVdf("first addon.vdf", {{"DATA/FIRST.TXT", std::string(128 * 1024, 'A')}});
  const auto second_source =
      CreateVdf("second addon.vdf", {{"DATA/SECOND.TXT", std::string(128 * 1024, 'B')}});
  const auto make_source = [](const std::filesystem::path& source, bool contains_gothic_dat) {
    return gmp::addon::AddonTransportSource{
        {source.filename().string(), std::filesystem::file_size(source),
         gmp::crypto::ComputeFileSHA256(source), contains_gothic_dat},
        source};
  };
  std::vector sources{make_source(first_source, false), make_source(second_source, true)};
  const auto public_directory = temporary_directory_ / "data" / "public";

  const auto first = gmp::addon::PrepareAddonTransportBundle(public_directory, sources);
  EXPECT_EQ(first.archive_path.filename(), "addons.zip");
  EXPECT_EQ(first.manifest_path.filename(), "addons.bin");
  EXPECT_EQ(first.archive_sha256, gmp::crypto::ComputeFileSHA256(first.archive_path));
  EXPECT_EQ(first.manifest_sha256, gmp::crypto::ComputeFileSHA256(first.manifest_path));
  EXPECT_LT(first.archive_size,
            std::filesystem::file_size(first_source) + std::filesystem::file_size(second_source));

  const auto extracted_directory = temporary_directory_ / "extracted";
  std::filesystem::create_directories(extracted_directory);
  std::string error;
  ASSERT_TRUE(gmp::addon::ExtractAddonTransportBundle(
      first.archive_path, extracted_directory, {sources[0].entry, sources[1].entry}, error))
      << error;
  EXPECT_EQ(gmp::crypto::ComputeFileSHA256(extracted_directory / "first addon.vdf"),
            sources[0].entry.sha256);
  EXPECT_EQ(gmp::crypto::ComputeFileSHA256(extracted_directory / "second addon.vdf"),
            sources[1].entry.sha256);

  const auto reused = gmp::addon::PrepareAddonTransportBundle(public_directory, sources);
  EXPECT_EQ(reused.archive_sha256, first.archive_sha256);
  EXPECT_EQ(reused.manifest_sha256, first.manifest_sha256);

  std::reverse(sources.begin(), sources.end());
  const auto reordered = gmp::addon::PrepareAddonTransportBundle(public_directory, sources);
  EXPECT_NE(reordered.manifest_sha256, first.manifest_sha256);

  gmp::addon::RemoveAddonTransportBundle(public_directory);
  EXPECT_FALSE(std::filesystem::exists(reordered.archive_path));
  EXPECT_FALSE(std::filesystem::exists(reordered.manifest_path));
}

TEST_F(AddonSupportTest, RejectsNonPortableOrDuplicateProneAddonNames) {
  std::string error;
  EXPECT_TRUE(gmp::addon::IsPortableAddonFilename("Colony Origins.vdf", error));
  EXPECT_FALSE(gmp::addon::IsPortableAddonFilename("folder/addon.vdf", error));
  EXPECT_FALSE(gmp::addon::IsPortableAddonFilename("addon.zip", error));
  EXPECT_FALSE(gmp::addon::IsPortableAddonFilename("CON.vdf", error));
}

}  // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
