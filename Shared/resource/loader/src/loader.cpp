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

#include "resource/loader.h"

#include <minizip/unzip.h>
#include <sodium.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace fs = std::filesystem;

namespace gmp {
namespace resource {

namespace {

constexpr std::size_t kIOBufferSize = 64 * 1024;

void EnsureSodiumInitialized() {
  static bool sodium_initialized = []() {
    if (sodium_init() < 0) {
      throw std::runtime_error("Failed to initialize libsodium");
    }
    return true;
  }();
  (void)sodium_initialized;
}

// Convert bytes to hex string
std::string BytesToHex(const std::vector<std::uint8_t>& bytes) {
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for (auto byte : bytes) {
    oss << std::setw(2) << static_cast<int>(byte);
  }
  return oss.str();
}

// Compute SHA-256 hash of a file
std::string ComputeFileSHA256(const fs::path& file_path) {
  std::ifstream file(file_path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("Failed to open file for hashing: " + file_path.string());
  }

  EnsureSodiumInitialized();

  crypto_hash_sha256_state state;
  crypto_hash_sha256_init(&state);

  constexpr std::size_t kBufferSize = 8192;
  std::vector<char> buffer(kBufferSize);

  while (file.read(buffer.data(), kBufferSize) || file.gcount() > 0) {
    crypto_hash_sha256_update(&state, reinterpret_cast<const unsigned char*>(buffer.data()), file.gcount());
  }

  std::vector<std::uint8_t> hash(crypto_hash_sha256_BYTES);
  crypto_hash_sha256_final(&state, hash.data());

  return BytesToHex(hash);
}

// Compute SHA-256 hash of a buffer
std::string ComputeBufferSHA256(const std::vector<std::uint8_t>& buffer) {
  EnsureSodiumInitialized();

  std::vector<std::uint8_t> hash(crypto_hash_sha256_BYTES);
  crypto_hash_sha256(hash.data(), buffer.data(), buffer.size());

  return BytesToHex(hash);
}

// RAII wrapper for unzFile
class UnzipFileHandle {
public:
  explicit UnzipFileHandle(unzFile handle) : handle_(handle) {
  }
  ~UnzipFileHandle() {
    if (handle_) {
      unzClose(handle_);
    }
  }

  unzFile get() const {
    return handle_;
  }
  unzFile release() {
    unzFile tmp = handle_;
    handle_ = nullptr;
    return tmp;
  }

private:
  unzFile handle_;
};

// Read a JSON manifest from file
Manifest ReadManifest(const fs::path& manifest_path) {
  std::ifstream file(manifest_path);
  if (!file) {
    throw std::runtime_error("Failed to open manifest file: " + manifest_path.string());
  }

  nlohmann::json json;
  try {
    file >> json;
  } catch (const std::exception& e) {
    throw std::runtime_error("Failed to parse manifest JSON: " + std::string(e.what()));
  }

  Manifest manifest;
  try {
    manifest.name = json.at("name").get<std::string>();
    manifest.version = json.at("version").get<std::string>();
    manifest.format = json.at("format").get<std::string>();
    manifest.created_utc = json.at("created_utc").get<std::string>();

    const auto& archive_json = json.at("archive");
    manifest.archive.path = archive_json.at("path").get<std::string>();
    manifest.archive.size = archive_json.at("size").get<std::uint64_t>();
    manifest.archive.sha256 = archive_json.at("sha256").get<std::string>();

    for (const auto& file_json : json.at("files")) {
      FileMeta file_meta;
      file_meta.path = file_json.at("path").get<std::string>();
      file_meta.size = file_json.at("size").get<std::uint64_t>();
      file_meta.sha256 = file_json.at("sha256").get<std::string>();
      file_meta.cache = file_json.at("cache").get<bool>();
      manifest.files.push_back(std::move(file_meta));
    }

    if (json.contains("entrypoints") && json["entrypoints"].is_array()) {
      for (const auto& entrypoint : json["entrypoints"]) {
        manifest.entrypoints.push_back(entrypoint.get<std::string>());
      }
    }

    if (json.contains("signature") && !json["signature"].is_null()) {
      manifest.signature = json["signature"].get<std::string>();
    }
  } catch (const std::exception& e) {
    throw std::runtime_error("Invalid manifest structure: " + std::string(e.what()));
  }

  return manifest;
}

// Verify archive integrity
void VerifyArchiveIntegrity(const fs::path& pak_path, const std::string& expected_hash) {
  const std::string computed_hash = ComputeFileSHA256(pak_path);
  if (computed_hash != expected_hash) {
    throw std::runtime_error("Archive integrity check failed: expected " + expected_hash + ", got " + computed_hash);
  }
}

}  // namespace

struct ResourcePack::Impl {
  Manifest manifest;
  fs::path pak_path;
  std::unordered_map<std::string, const FileMeta*> file_map;
};

ResourcePack::ResourcePack() : impl_(std::make_unique<Impl>()) {
}

ResourcePack::~ResourcePack() = default;

ResourcePack::ResourcePack(ResourcePack&&) noexcept = default;
ResourcePack& ResourcePack::operator=(ResourcePack&&) noexcept = default;

const Manifest& ResourcePack::GetManifest() const {
  return impl_->manifest;
}

bool ResourcePack::HasFile(const std::string& path) const {
  return impl_->file_map.find(path) != impl_->file_map.end();
}

std::optional<std::reference_wrapper<const FileMeta>> ResourcePack::GetFileMetadata(const std::string& path) const {
  auto it = impl_->file_map.find(path);
  if (it == impl_->file_map.end()) {
    return std::nullopt;
  }
  return std::cref(*it->second);
}

LoadedFile ResourcePack::LoadFile(const std::string& path, bool verify_hash) const {
  auto it = impl_->file_map.find(path);
  if (it == impl_->file_map.end()) {
    throw std::runtime_error("File not found in resource: " + path);
  }

  const FileMeta* meta = it->second;

  UnzipFileHandle unz(unzOpen(impl_->pak_path.string().c_str()));
  if (!unz.get()) {
    throw std::runtime_error("Failed to open archive: " + impl_->pak_path.string());
  }

  if (unzLocateFile(unz.get(), path.c_str(), 0) != UNZ_OK) {
    throw std::runtime_error("File not found in archive: " + path);
  }

  unz_file_info file_info;
  if (unzGetCurrentFileInfo(unz.get(), &file_info, nullptr, 0, nullptr, 0, nullptr, 0) != UNZ_OK) {
    throw std::runtime_error("Failed to get file info: " + path);
  }

  if (unzOpenCurrentFile(unz.get()) != UNZ_OK) {
    throw std::runtime_error("Failed to open file in archive: " + path);
  }

  std::vector<std::uint8_t> buffer(file_info.uncompressed_size);
  std::size_t total_read = 0;
  while (total_read < buffer.size()) {
    const auto remaining = static_cast<unsigned int>(buffer.size() - total_read);
    const int chunk = unzReadCurrentFile(unz.get(), buffer.data() + total_read, remaining);
    if (chunk < 0) {
      unzCloseCurrentFile(unz.get());
      throw std::runtime_error("Failed to read file from archive: " + path);
    }
    if (chunk == 0) {
      break;  // Reached EOF before expected number of bytes
    }
    total_read += static_cast<std::size_t>(chunk);
  }

  unzCloseCurrentFile(unz.get());

  if (total_read != buffer.size()) {
    throw std::runtime_error("File size mismatch for " + path + ": expected " + std::to_string(meta->size) + ", got " + std::to_string(total_read));
  }

  if (verify_hash) {
    const std::string computed_hash = ComputeBufferSHA256(buffer);
    if (computed_hash != meta->sha256) {
      throw std::runtime_error("File integrity check failed for " + path + ": expected " + meta->sha256 + ", got " + computed_hash);
    }
  }

  LoadedFile result;
  result.path = path;
  result.data = std::move(buffer);
  result.size = meta->size;
  result.sha256 = meta->sha256;

  return result;
}

std::vector<std::string> ResourcePack::GetFilePaths() const {
  std::vector<std::string> paths;
  paths.reserve(impl_->manifest.files.size());
  for (const auto& file : impl_->manifest.files) {
    paths.push_back(file.path);
  }
  return paths;
}

// ResourcePackLoader implementation

ResourcePack ResourcePackLoader::Load(const std::string& manifest_path, bool verify_integrity) {
  fs::path manifest_file_path(manifest_path);
  if (!fs::exists(manifest_file_path)) {
    throw std::runtime_error("Manifest file not found: " + manifest_path);
  }

  Manifest manifest = ReadManifest(manifest_file_path);

  if (manifest.format != "zip") {
    throw std::runtime_error("Unsupported archive format: " + manifest.format);
  }

  // Resolve .pak path relative to manifest directory
  fs::path manifest_dir = manifest_file_path.parent_path();
  fs::path pak_file_path = manifest_dir / fs::path(manifest.archive.path).filename();

  if (!fs::exists(pak_file_path)) {
    throw std::runtime_error("Archive file not found: " + pak_file_path.string());
  }

  // Verify archive size
  const std::uint64_t actual_size = fs::file_size(pak_file_path);
  if (actual_size != manifest.archive.size) {
    throw std::runtime_error("Archive size mismatch: expected " + std::to_string(manifest.archive.size) + ", got " + std::to_string(actual_size));
  }

  // Verify archive hash
  if (verify_integrity) {
    VerifyArchiveIntegrity(pak_file_path, manifest.archive.sha256);
  }

  // Create and populate the ResourcePack
  ResourcePack pack;
  pack.impl_->manifest = std::move(manifest);
  pack.impl_->pak_path = std::move(pak_file_path);

  pack.impl_->file_map.reserve(pack.impl_->manifest.files.size());
  for (const auto& file : pack.impl_->manifest.files) {
    pack.impl_->file_map.emplace(file.path, &file);
  }

  return pack;
}

}  // namespace resource
}  // namespace gmp
