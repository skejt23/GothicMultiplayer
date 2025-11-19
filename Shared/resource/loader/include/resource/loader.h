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

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "resource/types.h"

namespace gmp {
namespace resource {

// Represents a loaded file buffer with metadata
struct LoadedFile {
  std::string path;                // Relative path within the pack
  std::vector<std::uint8_t> data;  // File content buffer
  std::uint64_t size;              // Size in bytes
  std::string sha256;              // SHA-256 hash (for verification)
};

// ResourcePack - Represents a loaded resource pack
// This is an immutable handle to a loaded .pak archive. It provides read-only access
// to the manifest and files within the pack. The pack is always in a valid state.
// Thread-safety: Read operations are thread-safe. Multiple threads can query and load files concurrently.
class ResourcePack {
public:
  ~ResourcePack();

  // Non-copyable, movable (move operations declared but defined in .cpp due to pimpl)
  ResourcePack(const ResourcePack&) = delete;
  ResourcePack& operator=(const ResourcePack&) = delete;
  ResourcePack(ResourcePack&&) noexcept;
  ResourcePack& operator=(ResourcePack&&) noexcept;

  // Get the manifest for this resource pack
  const Manifest& GetManifest() const;

  // Check if a file exists in the resource pack
  bool HasFile(const std::string& path) const;

  // Get file metadata without loading content
  // Returns empty optional if file doesn't exist
  std::optional<std::reference_wrapper<const FileMeta>> GetFileMetadata(const std::string& path) const;

  // Load a file's content into memory
  // - path: Relative path within the pack (e.g., "client/main.luac")
  // - verify_hash: If true, verifies the SHA-256 hash matches manifest
  //
  // Returns LoadedFile with the buffer, throws std::runtime_error if file not found or hash mismatch
  LoadedFile LoadFile(const std::string& path, bool verify_hash = true) const;

  // Get list of all file paths in the loaded resource
  std::vector<std::string> GetFilePaths() const;

private:
  friend class ResourcePackLoader;

  // Private constructor - only ResourcePackLoader can create instances
  ResourcePack();

  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// ResourcePackLoader - Factory for loading resource packs
// This is a stateless factory that can be reused to load multiple resource packs.
// Thread-safety: Can be used concurrently from multiple threads.
class ResourcePackLoader {
public:
  ResourcePackLoader() = delete;

  // Load a resource pack from manifest and .pak file
  // - manifest_path: Path to the .manifest.json file
  // - verify_integrity: If true, verifies SHA-256 hashes of archive and files
  //
  // Returns a ResourcePack handle to the loaded resource
  // Throws std::runtime_error on failure (file not found, integrity check failure, etc.)
  static ResourcePack Load(const std::string& manifest_path, bool verify_integrity = true);

  // Load a resource pack directly from manifest JSON content and archive bytes in memory
  // - manifest_json: Raw JSON text of the manifest file
  // - archive_bytes: Contents of the .pak archive
  // - verify_integrity: If true, verifies SHA-256 hash of the archive bytes
  static ResourcePack LoadFromMemory(std::string manifest_json, std::vector<std::uint8_t> archive_bytes, bool verify_integrity = true);
};

}  // namespace resource
}  // namespace gmp
