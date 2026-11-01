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

#include "server_addon_context.h"

#include <addon/addon_transport.h>
#include <addon/addon_vfs.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <system_error>
#include <unordered_set>

#include "shared/crypto_utils.h"

namespace {

constexpr std::uint64_t kMaximumAddonArchiveSize = 2ULL * 1024ULL * 1024ULL * 1024ULL;

bool IsWithin(const std::filesystem::path& root, const std::filesystem::path& child) {
  const auto relative = child.lexically_relative(root);
  if (relative.empty() || relative.is_absolute()) {
    return child == root;
  }
  const auto first = *relative.begin();
  return first != "..";
}

std::optional<std::filesystem::path> ResolveConfiguredPath(const std::filesystem::path& root, const std::string& configured,
                                                           std::string& error) {
  namespace fs = std::filesystem;
  fs::path relative(configured);
  if (relative.empty() || relative.is_absolute() || relative.has_root_name() || relative.has_root_directory()) {
    error = "Addon path must be relative to the server directory: '" + configured + "'";
    return std::nullopt;
  }
  for (const auto& part : relative) {
    if (part == "..") {
      error = "Addon path may not contain parent traversal: '" + configured + "'";
      return std::nullopt;
    }
  }

  std::error_code ec;
  const auto canonical_root = fs::weakly_canonical(root, ec);
  if (ec) {
    error = "Failed to resolve server directory: " + ec.message();
    return std::nullopt;
  }
  const auto candidate = fs::weakly_canonical(canonical_root / relative, ec);
  if (ec || !IsWithin(canonical_root, candidate)) {
    error = "Addon path escapes the server directory: '" + configured + "'";
    return std::nullopt;
  }
  if (!fs::is_regular_file(candidate, ec) || ec) {
    error = "Configured addon VDF does not exist or is not a regular file: '" + candidate.string() + "'";
    return std::nullopt;
  }
  return candidate;
}

std::string FormatMiB(std::uint64_t size) {
  std::ostringstream value;
  value << std::fixed << std::setprecision(1) << static_cast<double>(size) / (1024.0 * 1024.0);
  return value.str();
}

std::string CaseInsensitiveFilenameKey(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
  return value;
}

}  // namespace

ServerAddonContext::ServerAddonContext() = default;
ServerAddonContext::~ServerAddonContext() = default;

bool ServerAddonContext::Initialize(const std::vector<std::string>& configured_paths, const std::filesystem::path& server_root,
                                    std::string& error) {
  archives_.clear();
  bundle_ = {};
  merged_vfs_.reset();
  has_gothic_dat_ = false;
  if (configured_paths.empty()) {
    try {
      gmp::addon::RemoveAddonTransportBundle(server_root / "data" / "public");
    } catch (const std::exception& ex) {
      error = std::string("Failed to clear disabled addon bundle cache: ") + ex.what();
      return false;
    }
    SPDLOG_INFO("Server addon: <none>");
    error.clear();
    return true;
  }

  try {
    std::vector<std::filesystem::path> source_paths;
    source_paths.reserve(configured_paths.size());
    std::vector<gmp::addon::AddonTransportSource> transport_sources;
    transport_sources.reserve(configured_paths.size());
    std::unordered_set<std::string> logical_filenames;
    std::size_t gothic_dat_count = 0;

    for (const auto& configured : configured_paths) {
      auto resolved = ResolveConfiguredPath(server_root, configured, error);
      if (!resolved) {
        return false;
      }

      gmp::addon::AddonVfs archive_vfs;
      archive_vfs.MountArchive(*resolved);
      const bool contains_dat = archive_vfs.Exists("GOTHIC.DAT");
      gothic_dat_count += contains_dat ? 1U : 0U;

      const auto size = std::filesystem::file_size(*resolved);
      if (size == 0 || size > kMaximumAddonArchiveSize) {
        error = "Addon VDF size must be between 1 byte and 2 GiB: '" + resolved->string() + "'";
        return false;
      }
      if (size > std::numeric_limits<std::size_t>::max()) {
        error = "Addon VDF is too large for this server build's HTTP stack: '" + resolved->string() + "'";
        return false;
      }
      const auto digest = gmp::crypto::ComputeFileSHA256(*resolved);
      const std::string logical_filename = std::filesystem::path(configured).filename().string();
      std::string filename_error;
      if (!gmp::addon::IsPortableAddonFilename(logical_filename, filename_error)) {
        error = "Invalid addon filename '" + logical_filename + "': " + filename_error;
        return false;
      }
      if (!logical_filenames.insert(CaseInsensitiveFilenameKey(logical_filename)).second) {
        error = "Duplicate addon filename is not allowed: '" + logical_filename + "'";
        return false;
      }

      Archive archive;
      archive.source_path = *resolved;
      archive.descriptor.logical_name = logical_filename;
      archive.descriptor.sha256 = digest;
      archive.descriptor.size = size;
      archive.descriptor.contains_gothic_dat = contains_dat;
      transport_sources.push_back(gmp::addon::AddonTransportSource{
          gmp::addon::AddonTransportEntry{logical_filename, size, digest, contains_dat}, *resolved});
      archives_.push_back(std::move(archive));
      source_paths.push_back(*resolved);

      SPDLOG_INFO("Server addon: {}", configured);
      SPDLOG_INFO("Addon size: {} MiB", FormatMiB(size));
      SPDLOG_INFO("Addon SHA-256: {}", digest);
      if (contains_dat) {
        SPDLOG_INFO("GOTHIC.DAT found in {}", configured);
      }
    }

    if (gothic_dat_count > 1) {
      error = "Configured addon VDFs contain more than one GOTHIC.DAT; exactly zero or one is allowed";
      archives_.clear();
      return false;
    }

    auto merged = std::make_unique<gmp::addon::AddonVfs>();
    merged->MountArchives(source_paths);
    has_gothic_dat_ = gothic_dat_count == 1;
    const auto prepared_bundle =
        gmp::addon::PrepareAddonTransportBundle(server_root / "data" / "public", transport_sources);
    bundle_.archive_path = prepared_bundle.archive_path;
    bundle_.manifest_path = prepared_bundle.manifest_path;
    bundle_.descriptor.archive_path = "addons.zip";
    bundle_.descriptor.archive_sha256 = prepared_bundle.archive_sha256;
    bundle_.descriptor.archive_size = prepared_bundle.archive_size;
    bundle_.descriptor.manifest_path = "addons.bin";
    bundle_.descriptor.manifest_sha256 = prepared_bundle.manifest_sha256;
    bundle_.descriptor.manifest_size = prepared_bundle.manifest_size;
    SPDLOG_INFO("Addon transport bundle: {} VDF(s), {} MiB ZIP", archives_.size(),
                FormatMiB(prepared_bundle.archive_size));

    merged_vfs_ = std::move(merged);
    error.clear();
    return true;
  } catch (const std::exception& ex) {
    archives_.clear();
    bundle_ = {};
    merged_vfs_.reset();
    has_gothic_dat_ = false;
    error = std::string("Failed to initialize server addon VDFs: ") + ex.what();
    return false;
  }
}

const std::vector<ServerAddonContext::Archive>& ServerAddonContext::Archives() const { return archives_; }
const ServerAddonContext::Bundle& ServerAddonContext::TransportBundle() const { return bundle_; }
bool ServerAddonContext::HasGothicDat() const { return has_gothic_dat_; }
bool ServerAddonContext::Empty() const { return archives_.empty(); }
