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

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace gmp::addon {

struct AddonTransportEntry {
  std::string logical_filename;
  std::uint64_t size{0};
  std::string sha256;
  bool contains_gothic_dat{false};
};

struct AddonTransportSource {
  AddonTransportEntry entry;
  std::filesystem::path source_path;
};

struct AddonTransportBundle {
  std::filesystem::path archive_path;
  std::string archive_sha256;
  std::uint64_t archive_size{0};
  std::filesystem::path manifest_path;
  std::string manifest_sha256;
  std::uint64_t manifest_size{0};
};

// Restricts announced names to one portable Windows filename. The name is
// preserved verbatim in the client Store directory.
bool IsPortableAddonFilename(std::string_view filename, std::string& error);

// Canonical binary manifest shared by server and client. Entry order is part
// of the identity and therefore changes the manifest bytes.
std::vector<std::uint8_t> BuildAddonBundleManifest(const std::vector<AddonTransportEntry>& entries,
                                                   std::uint64_t archive_size,
                                                   std::string_view archive_sha256);

// Creates data/public/addons.zip and addons.bin, or reuses them when the
// current ordered VDF identities and cached ZIP checksum still match.
AddonTransportBundle PrepareAddonTransportBundle(const std::filesystem::path& public_directory,
                                                  const std::vector<AddonTransportSource>& sources);

// Removes the bundle cache when no addon VDFs are configured.
void RemoveAddonTransportBundle(const std::filesystem::path& public_directory);

// Extracts every ordered entry into an existing empty directory. ZIP CRC,
// names, order, counts, and uncompressed sizes are checked; VDF SHA-256 is not
// recomputed after extraction because the authenticated bundle covers it.
bool ExtractAddonTransportBundle(const std::filesystem::path& archive_path,
                                 const std::filesystem::path& output_directory,
                                 const std::vector<AddonTransportEntry>& entries,
                                 std::string& error);

}  // namespace gmp::addon
