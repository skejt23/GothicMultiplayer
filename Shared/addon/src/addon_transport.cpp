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

#include "addon/addon_transport.h"

#include <minizip/unzip.h>
#include <minizip/zip.h>
#include <sodium.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include "shared/crypto_utils.h"

namespace gmp::addon {
namespace {

constexpr std::size_t kIoBufferSize = 1024 * 1024;
constexpr std::array<std::uint8_t, 8> kManifestMagic{'G', 'M', 'P', 'A', 'D', 'O', 'N', '1'};

std::string UpperAscii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
  return value;
}

void AppendUint16(std::vector<std::uint8_t>& output, std::uint16_t value) {
  output.push_back(static_cast<std::uint8_t>(value));
  output.push_back(static_cast<std::uint8_t>(value >> 8U));
}

void AppendUint32(std::vector<std::uint8_t>& output, std::uint32_t value) {
  for (unsigned int shift = 0; shift < 32; shift += 8) {
    output.push_back(static_cast<std::uint8_t>(value >> shift));
  }
}

void AppendUint64(std::vector<std::uint8_t>& output, std::uint64_t value) {
  for (unsigned int shift = 0; shift < 64; shift += 8) {
    output.push_back(static_cast<std::uint8_t>(value >> shift));
  }
}

std::vector<std::uint8_t> HexDigestBytes(std::string_view digest) {
  if (digest.size() != 64) {
    throw std::invalid_argument("SHA-256 digest must contain 64 hexadecimal characters");
  }
  const auto nibble = [](char ch) -> int {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
  };
  std::vector<std::uint8_t> bytes(32);
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    const int high = nibble(digest[i * 2]);
    const int low = nibble(digest[i * 2 + 1]);
    if (high < 0 || low < 0) {
      throw std::invalid_argument("SHA-256 digest contains a non-hexadecimal character");
    }
    bytes[i] = static_cast<std::uint8_t>((high << 4) | low);
  }
  return bytes;
}

std::vector<std::uint8_t> ReadFile(const std::filesystem::path& path, std::size_t maximum_size) {
  std::error_code ec;
  const auto size = std::filesystem::file_size(path, ec);
  if (ec || size > maximum_size) {
    return {};
  }
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
  std::ifstream input(path, std::ios::binary);
  if (!input.is_open() || (!bytes.empty() && !input.read(reinterpret_cast<char*>(bytes.data()), bytes.size()))) {
    return {};
  }
  return bytes;
}

void WriteFile(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output.is_open()) {
    throw std::runtime_error("Failed to create addon bundle manifest: " + path.string());
  }
  if (!bytes.empty()) {
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  }
  if (!output) {
    throw std::runtime_error("Failed to write addon bundle manifest: " + path.string());
  }
}

void WriteZip(const std::filesystem::path& path, const std::vector<AddonTransportSource>& sources) {
  gmp::crypto::EnsureSodiumInitialized();
  zipFile archive = zipOpen64(path.string().c_str(), APPEND_STATUS_CREATE);
  if (!archive) {
    throw std::runtime_error("Failed to create addon bundle: " + path.string());
  }

  std::vector<char> buffer(kIoBufferSize);
  for (const auto& source : sources) {
    zip_fileinfo info{};
    const int zip64 = source.entry.size >= std::numeric_limits<std::uint32_t>::max() ? 1 : 0;
    if (zipOpenNewFileInZip64(archive, source.entry.logical_filename.c_str(), &info, nullptr, 0, nullptr, 0,
                              nullptr, Z_DEFLATED, Z_BEST_COMPRESSION, zip64) != ZIP_OK) {
      zipClose(archive, nullptr);
      throw std::runtime_error("Failed to add '" + source.entry.logical_filename + "' to addons.zip");
    }

    std::ifstream input(source.source_path, std::ios::binary);
    if (!input.is_open()) {
      zipCloseFileInZip(archive);
      zipClose(archive, nullptr);
      throw std::runtime_error("Failed to read addon VDF while packaging: " + source.source_path.string());
    }
    crypto_hash_sha256_state source_hash_state;
    crypto_hash_sha256_init(&source_hash_state);
    std::uint64_t source_bytes = 0;
    while (input.read(buffer.data(), static_cast<std::streamsize>(buffer.size())) || input.gcount() > 0) {
      const auto count = static_cast<unsigned int>(input.gcount());
      if (zipWriteInFileInZip(archive, buffer.data(), count) != ZIP_OK) {
        zipCloseFileInZip(archive);
        zipClose(archive, nullptr);
        throw std::runtime_error("Failed while compressing addon VDF '" + source.entry.logical_filename + "'");
      }
      crypto_hash_sha256_update(&source_hash_state,
                                reinterpret_cast<const unsigned char*>(buffer.data()), count);
      source_bytes += count;
    }
    if (!input.eof()) {
      zipCloseFileInZip(archive);
      zipClose(archive, nullptr);
      throw std::runtime_error("Failed while reading addon VDF '" + source.entry.logical_filename + "'");
    }
    if (zipCloseFileInZip(archive) != ZIP_OK) {
      zipClose(archive, nullptr);
      throw std::runtime_error("Failed to finalize addon VDF entry '" + source.entry.logical_filename + "'");
    }
    std::array<unsigned char, crypto_hash_sha256_BYTES> source_hash{};
    crypto_hash_sha256_final(&source_hash_state, source_hash.data());
    if (source_bytes != source.entry.size ||
        gmp::crypto::BytesToHex(source_hash.data(), source_hash.size()) !=
            gmp::crypto::NormalizeHex(source.entry.sha256)) {
      zipClose(archive, nullptr);
      throw std::runtime_error("Addon VDF changed while addons.zip was being created: '" +
                               source.entry.logical_filename + "'");
    }
  }
  if (zipClose(archive, nullptr) != ZIP_OK) {
    throw std::runtime_error("Failed to finalize addons.zip");
  }
}

}  // namespace

bool IsPortableAddonFilename(std::string_view filename, std::string& error) {
  if (filename.empty() || filename.size() > 192 || filename == "." || filename == ".." ||
      filename.find_first_of("/\\") != std::string_view::npos) {
    error = "addon name must be a single filename between 1 and 192 bytes";
    return false;
  }
  if (filename.back() == ' ' || filename.back() == '.') {
    error = "addon filename may not end in a space or dot";
    return false;
  }
  for (unsigned char ch : filename) {
    if (ch < 32 || ch == '<' || ch == '>' || ch == ':' || ch == '"' || ch == '|' || ch == '?' || ch == '*') {
      error = "addon filename contains a character unsupported by Windows";
      return false;
    }
  }

  const std::filesystem::path path{std::string(filename)};
  if (UpperAscii(path.extension().string()) != ".VDF") {
    error = "addon filename must use the .vdf extension";
    return false;
  }
  const std::string stem = UpperAscii(path.stem().string());
  const bool reserved = stem == "CON" || stem == "PRN" || stem == "AUX" || stem == "NUL" ||
                        (stem.size() == 4 && (stem.starts_with("COM") || stem.starts_with("LPT")) &&
                         stem[3] >= '1' && stem[3] <= '9');
  if (reserved) {
    error = "addon filename uses a reserved Windows device name";
    return false;
  }
  error.clear();
  return true;
}

std::vector<std::uint8_t> BuildAddonBundleManifest(const std::vector<AddonTransportEntry>& entries,
                                                   std::uint64_t archive_size,
                                                   std::string_view archive_sha256) {
  if (entries.size() > 32) {
    throw std::invalid_argument("Addon bundle cannot contain more than 32 VDFs");
  }
  std::vector<std::uint8_t> output(kManifestMagic.begin(), kManifestMagic.end());
  AppendUint32(output, static_cast<std::uint32_t>(entries.size()));
  for (const auto& entry : entries) {
    std::string filename_error;
    if (!IsPortableAddonFilename(entry.logical_filename, filename_error)) {
      throw std::invalid_argument(filename_error);
    }
    AppendUint16(output, static_cast<std::uint16_t>(entry.logical_filename.size()));
    output.insert(output.end(), entry.logical_filename.begin(), entry.logical_filename.end());
    AppendUint64(output, entry.size);
    const auto digest = HexDigestBytes(entry.sha256);
    output.insert(output.end(), digest.begin(), digest.end());
    output.push_back(entry.contains_gothic_dat ? 1U : 0U);
  }
  AppendUint64(output, archive_size);
  const auto archive_digest = HexDigestBytes(archive_sha256);
  output.insert(output.end(), archive_digest.begin(), archive_digest.end());
  return output;
}

AddonTransportBundle PrepareAddonTransportBundle(const std::filesystem::path& public_directory,
                                                  const std::vector<AddonTransportSource>& sources) {
  if (sources.empty()) {
    throw std::invalid_argument("Cannot prepare an empty addon bundle");
  }
  std::error_code ec;
  std::filesystem::create_directories(public_directory, ec);
  if (ec) {
    throw std::runtime_error("Failed to create public data directory: " + ec.message());
  }

  const auto archive_path = public_directory / "addons.zip";
  const auto manifest_path = public_directory / "addons.bin";
  std::vector<AddonTransportEntry> entries;
  entries.reserve(sources.size());
  for (const auto& source : sources) {
    entries.push_back(source.entry);
  }

  if (std::filesystem::is_regular_file(archive_path, ec) && !ec &&
      std::filesystem::is_regular_file(manifest_path, ec) && !ec) {
    const auto archive_size = std::filesystem::file_size(archive_path, ec);
    if (!ec && archive_size <= std::numeric_limits<std::size_t>::max()) {
      const std::string archive_sha256 = gmp::crypto::ComputeFileSHA256(archive_path);
      const auto expected_manifest = BuildAddonBundleManifest(entries, archive_size, archive_sha256);
      if (ReadFile(manifest_path, expected_manifest.size()) == expected_manifest) {
        return AddonTransportBundle{archive_path, archive_sha256, archive_size, manifest_path,
                                    gmp::crypto::ComputeFileSHA256(manifest_path), expected_manifest.size()};
      }
    }
  }

  RemoveAddonTransportBundle(public_directory);
  auto temporary_archive = archive_path;
  temporary_archive += ".tmp";
  auto temporary_manifest = manifest_path;
  temporary_manifest += ".tmp";
  std::filesystem::remove(temporary_archive, ec);
  std::filesystem::remove(temporary_manifest, ec);
  try {
    WriteZip(temporary_archive, sources);
    const auto archive_size = std::filesystem::file_size(temporary_archive);
    if (archive_size > std::numeric_limits<std::size_t>::max()) {
      throw std::runtime_error("addons.zip is too large for this server build");
    }
    const std::string archive_sha256 = gmp::crypto::ComputeFileSHA256(temporary_archive);
    const auto manifest = BuildAddonBundleManifest(entries, archive_size, archive_sha256);
    WriteFile(temporary_manifest, manifest);
    std::filesystem::rename(temporary_archive, archive_path, ec);
    if (ec) {
      throw std::runtime_error("Failed to commit addons.zip: " + ec.message());
    }
    std::filesystem::rename(temporary_manifest, manifest_path, ec);
    if (ec) {
      std::filesystem::remove(archive_path, ec);
      throw std::runtime_error("Failed to commit addons.bin: " + ec.message());
    }
    return AddonTransportBundle{archive_path, archive_sha256, archive_size, manifest_path,
                                gmp::crypto::ComputeFileSHA256(manifest_path), manifest.size()};
  } catch (...) {
    std::error_code ignored;
    std::filesystem::remove(temporary_archive, ignored);
    std::filesystem::remove(temporary_manifest, ignored);
    throw;
  }
}

void RemoveAddonTransportBundle(const std::filesystem::path& public_directory) {
  std::error_code ec;
  std::filesystem::remove(public_directory / "addons.zip", ec);
  if (ec) {
    throw std::runtime_error("Failed to remove stale addons.zip: " + ec.message());
  }
  std::filesystem::remove(public_directory / "addons.bin", ec);
  if (ec) {
    throw std::runtime_error("Failed to remove stale addons.bin: " + ec.message());
  }
}

bool ExtractAddonTransportBundle(const std::filesystem::path& archive_path,
                                 const std::filesystem::path& output_directory,
                                 const std::vector<AddonTransportEntry>& entries,
                                 std::string& error) {
  std::error_code ec;
  if (!std::filesystem::is_directory(output_directory, ec) || ec) {
    error = "addon extraction directory must exist and be empty";
    return false;
  }
  const std::filesystem::directory_iterator directory_begin(output_directory, ec);
  if (ec || directory_begin != std::filesystem::directory_iterator{}) {
    error = "addon extraction directory must exist and be empty";
    return false;
  }

  unzFile archive = unzOpen64(archive_path.string().c_str());
  if (!archive) {
    error = "failed to open addons.zip";
    return false;
  }
  std::vector<std::filesystem::path> created_files;
  std::vector<char> buffer(kIoBufferSize);
  bool success = true;
  for (std::size_t index = 0; index < entries.size(); ++index) {
    const int position_result = index == 0 ? unzGoToFirstFile(archive) : unzGoToNextFile(archive);
    if (position_result != UNZ_OK) {
      error = "addons.zip contains fewer entries than announced";
      success = false;
      break;
    }

    unz_file_info64 info{};
    std::array<char, 512> entry_name{};
    if (unzGetCurrentFileInfo64(archive, &info, entry_name.data(), static_cast<uLong>(entry_name.size()), nullptr, 0,
                                nullptr, 0) != UNZ_OK || std::string_view(entry_name.data()) != entries[index].logical_filename ||
        info.uncompressed_size != entries[index].size) {
      error = "addons.zip entry name, order, or size differs from the manifest";
      success = false;
      break;
    }
    if (unzOpenCurrentFile(archive) != UNZ_OK) {
      error = "failed to open VDF entry in addons.zip";
      success = false;
      break;
    }

    const auto output_path = output_directory / entries[index].logical_filename;
    std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
      unzCloseCurrentFile(archive);
      error = "failed to create extracted VDF '" + entries[index].logical_filename + "'";
      success = false;
      break;
    }
    created_files.push_back(output_path);
    std::uint64_t extracted = 0;
    while (true) {
      const int count = unzReadCurrentFile(archive, buffer.data(), static_cast<unsigned int>(buffer.size()));
      if (count < 0) {
        error = "failed while decompressing VDF '" + entries[index].logical_filename + "'";
        success = false;
        break;
      }
      if (count == 0) break;
      output.write(buffer.data(), count);
      extracted += static_cast<std::uint64_t>(count);
      if (!output || extracted > entries[index].size) {
        error = "failed while writing VDF '" + entries[index].logical_filename + "'";
        success = false;
        break;
      }
    }
    output.close();
    if (unzCloseCurrentFile(archive) != UNZ_OK && success) {
      error = "VDF '" + entries[index].logical_filename + "' failed its ZIP CRC check";
      success = false;
    }
    if (!success || extracted != entries[index].size) {
      if (success) error = "extracted VDF size mismatch";
      success = false;
      break;
    }
  }
  if (success && unzGoToNextFile(archive) != UNZ_END_OF_LIST_OF_FILE) {
    error = "addons.zip contains more entries than announced";
    success = false;
  }
  unzClose(archive);
  if (!success) {
    for (const auto& path : created_files) {
      std::filesystem::remove(path, ec);
    }
    return false;
  }
  error.clear();
  return true;
}

}  // namespace gmp::addon
