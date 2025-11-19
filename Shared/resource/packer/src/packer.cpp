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

#include "resource/packer.h"

#include <lua/lua_compiler.h>
#include <minizip/zip.h>
#include <sodium.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace gmp {
namespace resource {

namespace {

constexpr std::size_t kIOBufferSize = 64 * 1024;

// RAII helper that removes a directory tree unless released.
class ScopedDirectoryRemover {
public:
  explicit ScopedDirectoryRemover(fs::path path) : path_(std::move(path)) {
  }
  ~ScopedDirectoryRemover() {
    Reset();
  }

  void Release() {
    path_.clear();
  }

private:
  void Reset() {
    if (path_.empty()) {
      return;
    }
    std::error_code ec;
    fs::remove_all(path_, ec);
    path_.clear();
  }

  fs::path path_;
};

class ZipFileHandle {
public:
  explicit ZipFileHandle(zipFile handle) : handle_(handle) {
  }
  ~ZipFileHandle() {
    if (handle_) {
      zipClose(handle_, nullptr);
    }
  }

  zipFile get() const {
    return handle_;
  }
  zipFile release() {
    zipFile tmp = handle_;
    handle_ = nullptr;
    return tmp;
  }

private:
  zipFile handle_;
};

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

bool StartsWith(std::string_view value, std::string_view prefix) {
  return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

bool EndsWith(std::string_view value, std::string_view suffix) {
  return value.size() >= suffix.size() && value.substr(value.size() - suffix.size()) == suffix;
}

std::vector<std::string> DeriveEntrypoints(const std::vector<FileMeta>& files) {
  std::vector<std::string> entrypoints;

  auto has_path = [&](std::string_view candidate) {
    return std::any_of(files.begin(), files.end(), [&](const FileMeta& meta) { return meta.path == candidate; });
  };

  const std::array<const char*, 2> preferred = {"client/main.luac", "client/main.lua"};
  for (const auto* candidate : preferred) {
    if (has_path(candidate)) {
      entrypoints.emplace_back(candidate);
      break;
    }
  }

  if (entrypoints.empty()) {
    for (const auto& meta : files) {
      if (StartsWith(meta.path, "client/") && (EndsWith(meta.path, ".luac") || EndsWith(meta.path, ".lua"))) {
        entrypoints.push_back(meta.path);
      }
    }
  }

  return entrypoints;
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

// Get current UTC timestamp in ISO 8601 format
std::string GetCurrentUTCTimestamp() {
  auto now = std::chrono::system_clock::now();
  auto time_t_now = std::chrono::system_clock::to_time_t(now);
  std::tm tm_utc;
#ifdef _WIN32
  gmtime_s(&tm_utc, &time_t_now);
#else
  gmtime_r(&time_t_now, &tm_utc);
#endif

  std::ostringstream oss;
  oss << std::put_time(&tm_utc, "%Y-%m-%dT%H:%M:%SZ");
  return oss.str();
}

// Normalize path separators to forward slashes
std::string NormalizePath(const fs::path& path) {
  std::string str = path.generic_string();
  std::replace(str.begin(), str.end(), '\\', '/');
  return str;
}

// Validate that path doesn't contain dangerous patterns
void ValidatePath(const std::string& path) {
  if (path.find("..") != std::string::npos) {
    throw std::runtime_error("Path contains '..' traversal: " + path);
  }
  if (!path.empty() && (path[0] == '/' || path[0] == '\\')) {
    throw std::runtime_error("Path is absolute: " + path);
  }
}

int NormalizeCompressionLevel(int level) {
  return std::clamp(level, 0, 9);
}

FileMeta BuildFileMeta(const fs::path& staged_file, std::string relative_path) {
  FileMeta meta;
  meta.path = std::move(relative_path);
  meta.size = fs::file_size(staged_file);
  meta.sha256 = ComputeFileSHA256(staged_file);
  meta.cache = true;
  return meta;
}

std::vector<FileMeta> StageSourceFiles(const PackOptions& opts, const fs::path& src_path, const fs::path& staging_dir) {
  std::vector<FileMeta> files;
  const std::vector<std::string> include_dirs = {"client", "shared"};

  for (const auto& dir : include_dirs) {
    const fs::path dir_path = src_path / dir;
    if (!fs::exists(dir_path)) {
      continue;
    }

    for (const auto& entry : fs::recursive_directory_iterator(dir_path)) {
      if (!entry.is_regular_file()) {
        continue;
      }

      std::string extension = entry.path().extension().string();
      std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      if (extension != ".lua") {
        continue;  // Only Lua scripts are supported for now.
      }

      fs::path rel_path = fs::relative(entry.path(), src_path);
      std::string normalized_rel_path = NormalizePath(rel_path);
      ValidatePath(normalized_rel_path);

      fs::path staging_file_path = staging_dir / rel_path;
      fs::create_directories(staging_file_path.parent_path());

      if (opts.compile_lua) {
        std::vector<std::uint8_t> bytecode;
        try {
          bytecode = shared::CompileLuaFileToBytecode(entry.path().string(), opts.strip_debug);
        } catch (const std::exception& e) {
          throw std::runtime_error("Failed to compile " + entry.path().string() + ": " + e.what());
        }

        staging_file_path.replace_extension(".luac");
        std::ofstream out(staging_file_path, std::ios::binary);
        if (!out) {
          throw std::runtime_error("Failed to write compiled Lua: " + staging_file_path.string());
        }
        out.write(reinterpret_cast<const char*>(bytecode.data()), bytecode.size());
        out.close();

        rel_path.replace_extension(".luac");
        normalized_rel_path = NormalizePath(rel_path);
      } else {
        fs::copy_file(entry.path(), staging_file_path, fs::copy_options::overwrite_existing);
      }

      files.push_back(BuildFileMeta(staging_file_path, normalized_rel_path));
    }
  }

  std::sort(files.begin(), files.end(), [](const FileMeta& a, const FileMeta& b) { return a.path < b.path; });
  return files;
}

zipFile OpenZipForWriting(const fs::path& pak_path) {
  zipFile handle = zipOpen(pak_path.string().c_str(), APPEND_STATUS_CREATE);
  if (!handle) {
    throw std::runtime_error("Failed to create ZIP archive: " + pak_path.string());
  }
  return handle;
}

void AddFileToZip(zipFile zip_handle, const fs::path& staged_root, const FileMeta& meta, int compression_level) {
  const fs::path staged_file = staged_root / fs::path(meta.path);
  std::ifstream file(staged_file, std::ios::binary);
  if (!file) {
    throw std::runtime_error("Failed to read staged file: " + staged_file.string());
  }

  zip_fileinfo file_info = {};
  const int method = compression_level > 0 ? Z_DEFLATED : 0;

  if (zipOpenNewFileInZip(zip_handle, meta.path.c_str(), &file_info, nullptr, 0, nullptr, 0, nullptr, method, compression_level) != ZIP_OK) {
    throw std::runtime_error("Failed to add file to ZIP: " + meta.path);
  }

  std::array<char, kIOBufferSize> buffer;
  while (file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
    const auto bytes_read = static_cast<unsigned int>(file.gcount());
    if (bytes_read == 0) {
      continue;
    }
    if (zipWriteInFileInZip(zip_handle, buffer.data(), bytes_read) != ZIP_OK) {
      zipCloseFileInZip(zip_handle);
      throw std::runtime_error("Failed to write data for file: " + meta.path);
    }
  }

  if (zipCloseFileInZip(zip_handle) != ZIP_OK) {
    throw std::runtime_error("Failed to close file in ZIP: " + meta.path);
  }
}

void WriteZipArchive(const fs::path& staging_dir, const std::vector<FileMeta>& files, const fs::path& pak_path, int compression_level) {
  ZipFileHandle zip_handle(OpenZipForWriting(pak_path));
  compression_level = NormalizeCompressionLevel(compression_level);

  for (const auto& meta : files) {
    AddFileToZip(zip_handle.get(), staging_dir, meta, compression_level);
  }

  if (zipClose(zip_handle.release(), nullptr) != ZIP_OK) {
    throw std::runtime_error("Failed to finalize ZIP archive");
  }
}

void WriteManifest(const PackResult& result, const fs::path& manifest_path) {
  nlohmann::json manifest_json;
  manifest_json["name"] = result.manifest.name;
  manifest_json["version"] = result.manifest.version;
  manifest_json["format"] = result.manifest.format;
  manifest_json["archive"]["path"] = result.manifest.archive.path;
  manifest_json["archive"]["size"] = result.manifest.archive.size;
  manifest_json["archive"]["sha256"] = result.manifest.archive.sha256;

  manifest_json["files"] = nlohmann::json::array();
  for (const auto& file : result.manifest.files) {
    manifest_json["files"].push_back({{"path", file.path}, {"size", file.size}, {"sha256", file.sha256}, {"cache", file.cache}});
  }

  manifest_json["entrypoints"] = nlohmann::json::array();
  for (const auto& entrypoint : result.manifest.entrypoints) {
    manifest_json["entrypoints"].push_back(entrypoint);
  }
  manifest_json["created_utc"] = result.manifest.created_utc;
  manifest_json["signature"] = nullptr;

  std::ofstream manifest_file(manifest_path);
  if (!manifest_file) {
    throw std::runtime_error("Failed to create manifest file: " + manifest_path.string());
  }

  manifest_file << manifest_json.dump(2);
}

}  // namespace

PackResult PackResource(const PackOptions& opts) {
  // Validate options
  if (opts.name.empty()) {
    throw std::runtime_error("Resource name cannot be empty");
  }
  if (opts.version.empty()) {
    throw std::runtime_error("Resource version cannot be empty");
  }
  if (opts.src_dir.empty()) {
    throw std::runtime_error("Source directory cannot be empty");
  }
  if (opts.out_dir.empty()) {
    throw std::runtime_error("Output directory cannot be empty");
  }

  fs::path src_path(opts.src_dir);
  if (!fs::exists(src_path) || !fs::is_directory(src_path)) {
    throw std::runtime_error("Source directory does not exist: " + opts.src_dir);
  }

  // Create output directory if it doesn't exist
  fs::path out_path(opts.out_dir);
  fs::create_directories(out_path);

  const fs::path staging_dir = out_path / ("staging_" + opts.name + "_" + opts.version);
  if (fs::exists(staging_dir)) {
    fs::remove_all(staging_dir);
  }
  fs::create_directories(staging_dir);
  ScopedDirectoryRemover staging_guard(staging_dir);

  PackResult result;
  result.manifest.name = opts.name;
  result.manifest.version = opts.version;
  result.manifest.format = "zip";
  result.manifest.created_utc = GetCurrentUTCTimestamp();
  result.manifest.signature.clear();

  result.manifest.files = StageSourceFiles(opts, src_path, staging_dir);
  result.manifest.entrypoints = DeriveEntrypoints(result.manifest.files);

  const fs::path pak_path = out_path / (opts.name + "-" + opts.version + ".pak");
  try {
    WriteZipArchive(staging_dir, result.manifest.files, pak_path, opts.compression_level);
  } catch (...) {
    std::error_code ec;
    fs::remove(pak_path, ec);
    throw;
  }

  const fs::path relative_archive_path = fs::path(opts.name) / pak_path.filename();
  result.manifest.archive.path = NormalizePath(relative_archive_path);
  result.manifest.archive.size = fs::file_size(pak_path);
  result.manifest.archive.sha256 = ComputeFileSHA256(pak_path);
  result.pak_path = pak_path.string();

  std::error_code cleanup_ec;
  fs::remove_all(staging_dir, cleanup_ec);
  staging_guard.Release();

  const fs::path manifest_path = out_path / (opts.name + "-" + opts.version + ".manifest.json");
  WriteManifest(result, manifest_path);
  result.manifest_path = manifest_path.string();

  return result;
}

}  // namespace resource
}  // namespace gmp
