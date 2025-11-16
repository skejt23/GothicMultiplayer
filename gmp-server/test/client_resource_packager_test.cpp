#include "client_resource_packager.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

class ClientResourcePackagerTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto base = fs::current_path() / "test_artifacts" / "client_resource_packager";
    test_root_ = base;
    resources_root_ = test_root_ / "resources";
    output_root_ = test_root_ / "public";

    fs::create_directories(resources_root_);
    fs::create_directories(output_root_);
  }

  void TearDown() override {
    std::error_code ec;
    fs::remove_all(test_root_, ec);
  }

  ResourceManager::DiscoveredResource CreateResource(const std::string& name, bool with_metadata = true) {
    ResourceManager::DiscoveredResource resource;
    resource.name = name;
    resource.root_path = resources_root_ / name;
    fs::create_directories(resource.root_path);

    if (with_metadata) {
      ResourceManager::ResourceMetadata meta;
      meta.version = "1.0.0";
      resource.metadata = meta;

      std::ofstream meta_file(resource.root_path / "resource.toml");
      meta_file << "version = \"1.0.0\"\n";
      meta_file.close();
    }

    return resource;
  }

  void AddClientScript(const ResourceManager::DiscoveredResource& resource, const std::string& script_name, const std::string& contents) {
    auto client_dir = resource.root_path / "client";
    fs::create_directories(client_dir);
    std::ofstream script_file(client_dir / script_name);
    script_file << contents;
    script_file.close();
  }

  std::vector<ClientResourceDescriptor> Build(const std::vector<ResourceManager::DiscoveredResource>& resources) {
    return ClientResourcePackager::Build(resources, output_root_.string());
  }

  fs::path test_root_;
  fs::path resources_root_;
  fs::path output_root_;
};

TEST_F(ClientResourcePackagerTest, SkipsResourcesWithoutMetadata) {
  auto resource = CreateResource("no_meta", /*with_metadata=*/false);

  auto descriptors = Build({resource});

  EXPECT_TRUE(descriptors.empty());
  EXPECT_TRUE(fs::is_empty(output_root_));
}

TEST_F(ClientResourcePackagerTest, SkipsResourcesWithoutClientScripts) {
  auto resource = CreateResource("no_scripts");

  auto descriptors = Build({resource});

  EXPECT_TRUE(descriptors.empty());
  EXPECT_TRUE(fs::is_empty(output_root_));
}

TEST_F(ClientResourcePackagerTest, ProducesDescriptorWhenScriptsExist) {
  auto resource = CreateResource("with_scripts");
  AddClientScript(resource, "main.lua", "print('hello from test')");

  auto descriptors = Build({resource});

  ASSERT_EQ(1u, descriptors.size());
  const auto& descriptor = descriptors.front();

  EXPECT_EQ("with_scripts", descriptor.name);
  EXPECT_EQ("1.0.0", descriptor.version);

  auto manifest_path = output_root_ / descriptor.manifest_path;
  auto pak_path = output_root_ / descriptor.archive_path;

  EXPECT_TRUE(fs::exists(manifest_path));
  EXPECT_TRUE(fs::exists(pak_path));
  EXPECT_GT(descriptor.archive_size, 0u);
  EXPECT_FALSE(descriptor.archive_sha256.empty());
  EXPECT_FALSE(descriptor.manifest_sha256.empty());
}

}  // namespace

int main(int argc, char** argv) {
  ::testing::GTEST_FLAG(catch_exceptions) = false;
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}