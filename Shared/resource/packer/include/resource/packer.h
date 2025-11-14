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
#include <string>
#include <vector>

namespace gmp {
namespace resource {

// Metadata for a single file within a resource pack
struct FileMeta {
  std::string path;    // Relative path within the pack (e.g., "client/main.luac")
  std::uint64_t size;  // File size in bytes
  std::string sha256;  // SHA-256 hash of the file content (hex string)
  bool cache;          // Whether client should cache this file on disk
};

// Archive metadata
struct ArchiveMeta {
  std::string path;    // Relative path to the .pak file (e.g., "hud/hud-2025.11.07.pak")
  std::uint64_t size;  // Archive size in bytes
  std::string sha256;  // SHA-256 hash of the entire archive (hex string)
};

// Resource pack manifest structure
struct Manifest {
  std::string name;                      // Resource name (e.g., "hud")
  std::string version;                   // Version string (e.g., "2025.11.07.1800")
  std::string format;                    // Archive format (always "zip")
  ArchiveMeta archive;                   // Archive metadata
  std::vector<FileMeta> files;           // List of files in the pack
  std::vector<std::string> entrypoints;  // Files to preload (e.g., ["client/main.luac"])
  std::string created_utc;               // ISO 8601 timestamp
  std::string signature;                 // Ed25519 signature (empty for v1)
};

// Options for packing a resource
struct PackOptions {
  std::string src_dir;         // Source directory (e.g., "resources_src/hud")
  std::string out_dir;         // Output directory (e.g., "public/hud")
  std::string name;            // Resource name (e.g., "hud")
  std::string version;         // Version string (e.g., "2025.11.07.1800")
  bool compile_lua = true;     // Whether to compile .lua files to .luac
  bool strip_debug = true;     // Strip debug info from compiled Lua bytecode
  int compression_level = 6;   // ZIP compression level (0-9, 0=store, 9=best)
};

// Result of packing a resource
struct PackResult {
  std::string pak_path;       // Absolute path to the generated .pak file
  std::string manifest_path;  // Absolute path to the generated manifest.json
  Manifest manifest;          // The generated manifest structure
};

// Pack a resource according to the provided options.
// This function:
// 1. Scans client/ and shared/ subdirectories
// 2. Compiles .lua files to .luac bytecode (if compile_lua is true)
// 3. Computes SHA-256 hashes for each file
// 4. Creates a .pak ZIP archive with compressed files
// 5. Generates a manifest.json with metadata
//
// Returns PackResult on success, throws std::runtime_error on failure.
PackResult PackResource(const PackOptions& opts);

}  // namespace resource
}  // namespace gmp
