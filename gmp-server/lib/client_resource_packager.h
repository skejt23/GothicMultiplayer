#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "resource_manager.h"

struct ClientResourceDescriptor {
  std::string name;
  std::string version;
  std::string manifest_path;
  std::string manifest_sha256;
  std::string archive_path;
  std::string archive_sha256;
  std::uint64_t archive_size{0};
  std::optional<std::string> author;
  std::optional<std::string> description;
};

class ClientResourcePackager {
public:
  // Builds client resource packages from discovered resources.
  // Returns descriptors for resources that were successfully packed.
  static std::vector<ClientResourceDescriptor> Build(const std::vector<ResourceManager::DiscoveredResource>& resources,
                                                     const std::string& output_dir = "public");
};
