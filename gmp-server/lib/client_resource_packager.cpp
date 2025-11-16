#include "client_resource_packager.h"

#include <sodium.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <system_error>

#include "resource/packer.h"

namespace {
namespace fs = std::filesystem;

constexpr const char* kDefaultOutputDir = "public";
constexpr std::array<const char*, 2> kPackableSubdirectories = {"client", "shared"};

void EnsureSodiumInitialized() {
  static bool initialized = [] {
    if (sodium_init() < 0) {
      throw std::runtime_error("Failed to initialize libsodium for resource packing");
    }
    return true;
  }();
  (void)initialized;
}

std::string ComputeFileSHA256(const fs::path& file_path) {
  EnsureSodiumInitialized();

  std::ifstream file(file_path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("Failed to open file for hashing: " + file_path.string());
  }

  crypto_hash_sha256_state state;
  crypto_hash_sha256_init(&state);

  std::array<char, 8192> buffer{};
  while (file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
    crypto_hash_sha256_update(&state, reinterpret_cast<const unsigned char*>(buffer.data()), file.gcount());
  }

  std::array<unsigned char, crypto_hash_sha256_BYTES> hash{};
  crypto_hash_sha256_final(&state, hash.data());

  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for (unsigned char byte : hash) {
    oss << std::setw(2) << static_cast<int>(byte);
  }
  return oss.str();
}

bool HasPackableScripts(const fs::path& resource_root) {
  for (const auto* dir_name : kPackableSubdirectories) {
    const fs::path dir_path = resource_root / dir_name;
    if (!fs::exists(dir_path)) {
      continue;
    }

    for (const auto& entry : fs::recursive_directory_iterator(dir_path)) {
      if (!entry.is_regular_file()) {
        continue;
      }

      std::string ext = entry.path().extension().string();
      std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      if (ext == ".lua") {
        return true;
      }
    }
  }

  return false;
}

}  // namespace

std::vector<ClientResourceDescriptor> ClientResourcePackager::Build(const std::vector<ResourceManager::DiscoveredResource>& resources,
                                                                    const std::string& output_dir) {
  std::vector<ClientResourceDescriptor> descriptors;
  fs::path output_root = output_dir.empty() ? fs::path(kDefaultOutputDir) : fs::path(output_dir);
  std::error_code ec;
  fs::create_directories(output_root, ec);
  if (ec) {
    throw std::runtime_error("Failed to create output directory '" + output_root.string() + "': " + ec.message());
  }

  for (const auto& resource : resources) {
    if (!resource.metadata) {
      SPDLOG_INFO("Skipping client pack for resource '{}' because metadata is missing", resource.name);
      continue;
    }

    if (!HasPackableScripts(resource.root_path)) {
      SPDLOG_INFO("Skipping client pack for resource '{}' because no client/shared Lua scripts were found", resource.name);
      continue;
    }

    gmp::resource::PackOptions options;
    options.name = resource.name;
    options.version = resource.metadata->version;
    options.src_dir = resource.root_path.string();
    options.out_dir = output_root.string();

    SPDLOG_INFO("Packing client resource '{}' version {}", options.name, options.version);
    auto result = gmp::resource::PackResource(options);

    ClientResourceDescriptor descriptor;
    descriptor.name = result.manifest.name;
    descriptor.version = result.manifest.version;
    descriptor.archive_path = fs::path(result.pak_path).filename().string();
    descriptor.archive_sha256 = result.manifest.archive.sha256;
    descriptor.archive_size = result.manifest.archive.size;
    descriptor.manifest_path = fs::path(result.manifest_path).filename().string();
    descriptor.manifest_sha256 = ComputeFileSHA256(result.manifest_path);
    descriptor.author = resource.metadata->author;
    descriptor.description = resource.metadata->description;

    SPDLOG_INFO("  -> wrote {} and {}", descriptor.archive_path, descriptor.manifest_path);
    descriptors.push_back(std::move(descriptor));
  }

  return descriptors;
}
