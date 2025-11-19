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

#include <minizip/ioapi.h>
#include <minizip/unzip.h>
#include <sodium.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace gmp {
namespace resource {

namespace {

struct MemoryZipSource {
  const unsigned char* base{nullptr};
  uLong size{0};
  uLong limit{0};
  uLong offset{0};
};

voidpf ZCALLBACK MemOpenFunc(voidpf opaque, const char*, int) {
  auto* source = static_cast<MemoryZipSource*>(opaque);
  source->offset = 0;
  return opaque;
}

uLong ZCALLBACK MemReadFunc(voidpf opaque, voidpf, void* buf, uLong size) {
  auto* source = static_cast<MemoryZipSource*>(opaque);
  if (source->offset >= source->size) {
    return 0;
  }

  const uLong remaining = source->size - source->offset;
  const uLong to_copy = size > remaining ? remaining : size;
  if (to_copy > 0) {
    std::memcpy(buf, source->base + source->offset, to_copy);
    source->offset += to_copy;
  }
  return to_copy;
}

uLong ZCALLBACK MemWriteFunc(voidpf, voidpf, const void*, uLong) {
  return 0;
}

long ZCALLBACK MemTellFunc(voidpf opaque, voidpf) {
  auto* source = static_cast<MemoryZipSource*>(opaque);
  return static_cast<long>(source->offset);
}

long ZCALLBACK MemSeekFunc(voidpf opaque, voidpf, uLong offset, int origin) {
  auto* source = static_cast<MemoryZipSource*>(opaque);
  uLong new_offset = 0;
  switch (origin) {
    case ZLIB_FILEFUNC_SEEK_CUR:
      new_offset = source->offset + offset;
      break;
    case ZLIB_FILEFUNC_SEEK_END:
      new_offset = source->size + offset;
      break;
    case ZLIB_FILEFUNC_SEEK_SET:
    default:
      new_offset = offset;
      break;
  }

  if (new_offset > source->size) {
    return -1;
  }

  source->offset = new_offset;
  return 0;
}

int ZCALLBACK MemCloseFunc(voidpf, voidpf) {
  return 0;
}

int ZCALLBACK MemErrorFunc(voidpf, voidpf) {
  return 0;
}

void SetupMemoryFileFunc(zlib_filefunc_def& funcs, MemoryZipSource& source) {
  funcs.zopen_file = MemOpenFunc;
  funcs.zread_file = MemReadFunc;
  funcs.zwrite_file = MemWriteFunc;
  funcs.ztell_file = MemTellFunc;
  funcs.zseek_file = MemSeekFunc;
  funcs.zclose_file = MemCloseFunc;
  funcs.zerror_file = MemErrorFunc;
  funcs.opaque = &source;
}

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

class MemoryUnzipHandle {
public:
  explicit MemoryUnzipHandle(const std::vector<std::uint8_t>& buffer) {
    source_.base = buffer.empty() ? nullptr : buffer.data();
    source_.size = static_cast<uLong>(buffer.size());
    source_.limit = source_.size;
    source_.offset = 0;
    SetupMemoryFileFunc(filefunc_, source_);
    handle_ = unzOpen2(nullptr, &filefunc_);
  }

  ~MemoryUnzipHandle() {
    if (handle_) {
      unzClose(handle_);
    }
  }

  unzFile get() const {
    return handle_;
  }

private:
  MemoryZipSource source_{};
  zlib_filefunc_def filefunc_{};
  unzFile handle_{nullptr};
};

Manifest ParseManifestJson(const nlohmann::json& json) {
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

Manifest ParseManifestString(const std::string& manifest_json) {
  try {
    return ParseManifestJson(nlohmann::json::parse(manifest_json));
  } catch (const std::exception& e) {
    throw std::runtime_error("Failed to parse manifest JSON: " + std::string(e.what()));
  }
}

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

  return ParseManifestJson(json);
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
  std::vector<std::uint8_t> pak_memory;
  bool in_memory{false};
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
  auto read_from_zip = [&](unzFile zip) {
    if (unzLocateFile(zip, path.c_str(), 0) != UNZ_OK) {
      throw std::runtime_error("File not found in archive: " + path);
    }

    unz_file_info file_info;
    if (unzGetCurrentFileInfo(zip, &file_info, nullptr, 0, nullptr, 0, nullptr, 0) != UNZ_OK) {
      throw std::runtime_error("Failed to get file info: " + path);
    }

    if (unzOpenCurrentFile(zip) != UNZ_OK) {
      throw std::runtime_error("Failed to open file in archive: " + path);
    }

    std::vector<std::uint8_t> buffer(file_info.uncompressed_size);
    std::size_t total_read = 0;
    while (total_read < buffer.size()) {
      const auto remaining = static_cast<unsigned int>(buffer.size() - total_read);
      const int chunk = unzReadCurrentFile(zip, buffer.data() + total_read, remaining);
      if (chunk < 0) {
        unzCloseCurrentFile(zip);
        throw std::runtime_error("Failed to read file from archive: " + path);
      }
      if (chunk == 0) {
        break;
      }
      total_read += static_cast<std::size_t>(chunk);
    }

    unzCloseCurrentFile(zip);

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
  };

  if (impl_->in_memory) {
    MemoryUnzipHandle handle(impl_->pak_memory);
    if (!handle.get()) {
      throw std::runtime_error("Failed to open in-memory archive for resource: " + impl_->manifest.name);
    }
    return read_from_zip(handle.get());
  }

  UnzipFileHandle handle(unzOpen(impl_->pak_path.string().c_str()));
  if (!handle.get()) {
    throw std::runtime_error("Failed to open archive: " + impl_->pak_path.string());
  }
  return read_from_zip(handle.get());
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

ResourcePack ResourcePackLoader::LoadFromMemory(std::string manifest_json, std::vector<std::uint8_t> archive_bytes, bool verify_integrity) {
  if (manifest_json.empty()) {
    throw std::runtime_error("Manifest JSON payload is empty");
  }

  Manifest manifest = ParseManifestString(manifest_json);
  if (manifest.format != "zip") {
    throw std::runtime_error("Unsupported archive format: " + manifest.format);
  }

  if (manifest.archive.size != archive_bytes.size()) {
    throw std::runtime_error("Archive size mismatch: expected " + std::to_string(manifest.archive.size) + ", got " + std::to_string(archive_bytes.size()));
  }

  if (verify_integrity) {
    const std::string computed_hash = ComputeBufferSHA256(archive_bytes);
    if (computed_hash != manifest.archive.sha256) {
      throw std::runtime_error("Archive integrity check failed: expected " + manifest.archive.sha256 + ", got " + computed_hash);
    }
  }

  ResourcePack pack;
  pack.impl_->manifest = std::move(manifest);
  pack.impl_->pak_memory = std::move(archive_bytes);
  pack.impl_->pak_path.clear();
  pack.impl_->in_memory = true;

  pack.impl_->file_map.reserve(pack.impl_->manifest.files.size());
  for (const auto& file : pack.impl_->manifest.files) {
    pack.impl_->file_map.emplace(file.path, &file);
  }

  return pack;
}

}  // namespace resource
}  // namespace gmp
