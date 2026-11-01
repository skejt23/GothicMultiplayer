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

#include "addon/addon_vfs.h"

#include <zenkit/Stream.hh>
#include <zenkit/Vfs.hh>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <fstream>
#include <functional>
#include <limits>
#include <stdexcept>
#include <utility>

#include "shared/crypto_utils.h"

namespace gmp::addon {

struct AddonFile::State {
  zenkit::Vfs vfs;
  std::vector<std::filesystem::path> archives;
};

namespace {

constexpr std::size_t kVdfHeaderSize = 256 + 16 + 6 * 4;
constexpr std::size_t kVdfCatalogEntrySize = 64 + 4 * 4;
constexpr std::uint32_t kMaximumCatalogEntries = 200000;
constexpr std::uint32_t kNormalVolumeFlag = 0x50;
constexpr std::uint32_t kUnionZippedVolumeFlag = 0xA0;

std::uint32_t ReadUint32(const std::byte* data) {
  std::uint32_t value = 0;
  std::memcpy(&value, data, sizeof(value));
  return value;
}

bool ValidateArchiveStructure(const std::filesystem::path& archive, std::string& error) {
  std::ifstream input(archive, std::ios::binary | std::ios::ate);
  if (!input.is_open()) {
    error = "Failed to open VDF archive: " + archive.string();
    return false;
  }
  const auto end = input.tellg();
  if (end < 0 || static_cast<std::uint64_t>(end) < kVdfHeaderSize) {
    error = "VDF archive is smaller than its fixed header";
    return false;
  }
  const auto archive_size = static_cast<std::uint64_t>(end);
  input.seekg(0, std::ios::beg);
  std::array<std::byte, kVdfHeaderSize> header{};
  if (!input.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()))) {
    error = "Failed to read VDF header";
    return false;
  }

  constexpr std::array<std::string_view, 3> signatures = {
      "PSVDSC_V2.00\r\n\r\n", "PSVDSC_V2.00\n\r\n\r", std::string_view("PSVDSC_V2.00\x1A\x1A\x1A\x1A", 16)};
  const std::string_view signature(reinterpret_cast<const char*>(header.data() + 256), 16);
  if (std::find(signatures.begin(), signatures.end(), signature) == signatures.end()) {
    error = "VDF signature is not recognized";
    return false;
  }

  const auto entry_count = ReadUint32(header.data() + 272);
  const auto declared_file_count = ReadUint32(header.data() + 276);
  const auto declared_catalog_offset = ReadUint32(header.data() + 288);
  // Legacy VDFS tools use zero for a catalog immediately after the header.
  // Match ZenKit's fallback before checking the catalog bounds.
  const auto catalog_offset = declared_catalog_offset == 0 ? static_cast<std::uint32_t>(kVdfHeaderSize) : declared_catalog_offset;
  const auto volume_flags = ReadUint32(header.data() + 292);
  if (entry_count == 0 || entry_count > kMaximumCatalogEntries) {
    error = "VDF catalog entry count is outside the supported range";
    return false;
  }
  const bool union_zipped = volume_flags == kUnionZippedVolumeFlag;
  if (volume_flags != kNormalVolumeFlag && !union_zipped) {
    error = "VDF uses unsupported volume flags";
    return false;
  }
  const auto catalog_size = static_cast<std::uint64_t>(entry_count) * kVdfCatalogEntrySize;
  if (catalog_offset < kVdfHeaderSize || catalog_offset > archive_size || catalog_size > archive_size - catalog_offset) {
    error = "VDF catalog lies outside the archive";
    return false;
  }
  std::vector<std::byte> catalog(static_cast<std::size_t>(catalog_size));
  input.seekg(static_cast<std::streamoff>(catalog_offset), std::ios::beg);
  if (!input.read(reinterpret_cast<char*>(catalog.data()), static_cast<std::streamsize>(catalog.size()))) {
    error = "Failed to read complete VDF catalog";
    return false;
  }

  std::vector<std::uint8_t> state(entry_count, 0);
  std::uint32_t actual_file_count = 0;
  std::function<bool(std::uint32_t, std::uint32_t)> validate_list = [&](std::uint32_t start, std::uint32_t depth) {
    if (start >= entry_count || depth > 128) {
      error = depth > 128 ? "VDF directory nesting exceeds 128 levels" : "VDF directory points outside the catalog";
      return false;
    }
    for (std::uint32_t index = start; index < entry_count; ++index) {
      if (state[index] != 0) {
        error = "VDF catalog contains a cycle or duplicate directory reference";
        return false;
      }
      state[index] = 1;
      const auto* entry = catalog.data() + static_cast<std::size_t>(index) * kVdfCatalogEntrySize;
      std::string name(reinterpret_cast<const char*>(entry), 64);
      const auto nul = name.find('\0');
      if (nul != std::string::npos) name.resize(nul);
      while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back())) != 0) name.pop_back();
      if (name.empty() || name == "." || name == ".." || name.find_first_of("/\\:") != std::string::npos) {
        error = "VDF catalog contains an invalid entry name";
        return false;
      }

      const auto offset = ReadUint32(entry + 64);
      const auto size = ReadUint32(entry + 68);
      const auto type = ReadUint32(entry + 72);
      const bool directory = (type & 0x80000000U) != 0;
      const bool last = (type & 0x40000000U) != 0;
      if (directory) {
        if (!validate_list(offset, depth + 1)) {
          return false;
        }
      } else {
        // A Union-compressed catalog stores the uncompressed size here. Its
        // compressed byte extent is described by the ZippedStream at offset,
        // so only the starting offset can be bounded at catalog-validation time.
        const bool payload_outside_archive =
            union_zipped ? offset >= archive_size : (offset > archive_size || size > archive_size - offset);
        if (payload_outside_archive) {
          error = "VDF file payload lies outside the archive";
          return false;
        }
        ++actual_file_count;
      }
      state[index] = 2;
      if (last) {
        return true;
      }
    }
    error = "VDF catalog sibling list is missing its terminating entry";
    return false;
  };

  if (!validate_list(0, 0)) {
    return false;
  }
  if (std::find(state.begin(), state.end(), 0) != state.end()) {
    error = "VDF catalog contains unreachable entries";
    return false;
  }
  if (actual_file_count != declared_file_count) {
    error = "VDF declared file count does not match its catalog";
    return false;
  }
  error.clear();
  return true;
}

std::string Basename(std::string_view normalized) {
  const auto slash = normalized.find_last_of('/');
  return std::string(slash == std::string_view::npos ? normalized : normalized.substr(slash + 1));
}

void EnumerateNode(const zenkit::VfsNode& node, const std::string& parent, std::vector<std::string>& output) {
  if (node.type() == zenkit::VfsNodeType::FILE) {
    output.push_back(parent.empty() ? node.name() : parent + "/" + node.name());
    return;
  }

  const std::string current = node.name() == "/" ? parent : (parent.empty() ? node.name() : parent + "/" + node.name());
  for (const auto& child : node.children()) {
    EnumerateNode(child, current, output);
  }
}

}  // namespace

std::optional<std::string> NormalizeVdfPath(std::string_view path) {
  if (path.empty()) {
    return std::nullopt;
  }

  std::string normalized;
  normalized.reserve(path.size());
  for (char ch : path) {
    normalized.push_back(ch == '\\' ? '/' : ch);
  }

  // zFILE_VDFS commonly supplies paths rooted at the Gothic working tree.
  // They are logical VDF paths, not host paths, so leading separators are
  // harmless and are removed before traversal validation.
  while (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  if (normalized.size() >= 2 && std::isalpha(static_cast<unsigned char>(normalized[0])) && normalized[1] == ':') {
    return std::nullopt;
  }

  std::vector<std::string> components;
  std::size_t start = 0;
  while (start <= normalized.size()) {
    const auto end = normalized.find('/', start);
    std::string component = normalized.substr(start, end == std::string::npos ? std::string::npos : end - start);
    if (!component.empty() && component != ".") {
      if (component == "..") {
        return std::nullopt;
      }
      components.push_back(std::move(component));
    }
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }

  if (components.empty()) {
    return std::nullopt;
  }
  std::string result;
  for (const auto& component : components) {
    if (!result.empty()) {
      result.push_back('/');
    }
    result += component;
  }
  return result;
}

AddonFile::AddonFile(std::shared_ptr<const State> state, std::unique_ptr<zenkit::Read> reader, std::size_t size)
    : state_(std::move(state)), reader_(std::move(reader)), size_(size) {}
AddonFile::AddonFile(AddonFile&&) noexcept = default;
AddonFile& AddonFile::operator=(AddonFile&&) noexcept = default;
AddonFile::~AddonFile() = default;

std::size_t AddonFile::Read(void* buffer, std::size_t length) {
  if (!reader_ || buffer == nullptr || length == 0) {
    return 0;
  }
  return reader_->read(buffer, length);
}

bool AddonFile::Seek(std::int64_t offset) {
  if (!reader_ || offset < 0 || static_cast<std::uint64_t>(offset) > size_) {
    return false;
  }
  reader_->seek(static_cast<std::ptrdiff_t>(offset), zenkit::Whence::BEG);
  return reader_->tell() == static_cast<std::size_t>(offset);
}

std::size_t AddonFile::Tell() const { return reader_ ? reader_->tell() : 0; }
std::size_t AddonFile::Size() const { return size_; }
bool AddonFile::Eof() const { return !reader_ || reader_->eof() || Tell() >= size_; }

AddonVfs::AddonVfs() : state_(std::make_shared<State>()) {}
AddonVfs::AddonVfs(AddonVfs&&) noexcept = default;
AddonVfs& AddonVfs::operator=(AddonVfs&&) noexcept = default;
AddonVfs::~AddonVfs() = default;

void AddonVfs::MountArchives(const std::vector<std::filesystem::path>& archives) {
  auto candidate = std::make_shared<State>();
  for (const auto& archive : archives) {
    std::string validation_error;
    if (!ValidateArchiveStructure(archive, validation_error)) {
      throw std::runtime_error("Invalid VDF '" + archive.string() + "': " + validation_error);
    }
    candidate->vfs.mount_disk(archive, zenkit::VfsOverwriteBehavior::ALL);
    candidate->archives.push_back(archive);
  }
  state_ = std::move(candidate);
}

void AddonVfs::MountArchive(const std::filesystem::path& archive) {
  std::vector<std::filesystem::path> archives = state_->archives;
  archives.push_back(archive);
  MountArchives(archives);
}

const zenkit::VfsNode* AddonVfs::ResolveFile(std::string_view path) const {
  const auto normalized = NormalizeVdfPath(path);
  if (!normalized || !state_) {
    return nullptr;
  }

  auto is_file = [](const zenkit::VfsNode* node) { return node != nullptr && node->type() == zenkit::VfsNodeType::FILE; };
  if (const auto* direct = state_->vfs.resolve(*normalized); is_file(direct)) {
    return direct;
  }

  // ZenGin may ask for _WORK/DATA/... while an archive stores the same path
  // without that prefix (or vice versa).  VDFS also supports global filename
  // lookup, which ZenKit exposes via find().
  constexpr std::string_view work_prefix = "_WORK/DATA/";
  std::string upper = *normalized;
  std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
  if (upper.starts_with(work_prefix)) {
    if (const auto* stripped = state_->vfs.resolve(normalized->substr(work_prefix.size())); is_file(stripped)) {
      return stripped;
    }
  }

  const auto* by_name = state_->vfs.find(Basename(*normalized));
  return is_file(by_name) ? by_name : nullptr;
}

std::shared_ptr<AddonFile> AddonVfs::Open(std::string_view path) const {
  return OpenNode(ResolveFile(path));
}

std::shared_ptr<AddonFile> AddonVfs::OpenExact(std::string_view path) const {
  const auto normalized = NormalizeVdfPath(path);
  if (!normalized || !state_) return nullptr;
  const auto* node = &state_->vfs.root();
  for (std::size_t start = 0; start < normalized->size();) {
    if (!node || node->type() != zenkit::VfsNodeType::DIRECTORY) return nullptr;
    const auto end = normalized->find('/', start);
    node = node->child(normalized->substr(start, end == std::string::npos ? end : end - start));
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return OpenNode(node);
}

std::shared_ptr<AddonFile> AddonVfs::OpenNode(const zenkit::VfsNode* node) const {
  if (!node || node->type() != zenkit::VfsNodeType::FILE) {
    return nullptr;
  }
  auto reader = node->open_read();
  if (!reader) {
    return nullptr;
  }
  reader->seek(0, zenkit::Whence::END);
  const auto size = reader->tell();
  reader->seek(0, zenkit::Whence::BEG);
  return std::shared_ptr<AddonFile>(new AddonFile(state_, std::move(reader), size));
}

bool AddonVfs::Exists(std::string_view path) const { return ResolveFile(path) != nullptr; }

std::optional<std::size_t> AddonVfs::FileSize(std::string_view path) const {
  auto file = Open(path);
  return file ? std::optional<std::size_t>(file->Size()) : std::nullopt;
}

std::vector<std::string> AddonVfs::EnumerateFiles() const {
  std::vector<std::string> files;
  if (state_) {
    EnumerateNode(state_->vfs.root(), {}, files);
  }
  return files;
}

std::vector<std::byte> AddonVfs::ReadFile(std::string_view path, std::size_t maximum_size) const {
  auto file = Open(path);
  if (!file) {
    throw std::runtime_error("VDF file not found: " + std::string(path));
  }
  if (file->Size() > maximum_size) {
    throw std::runtime_error("VDF file exceeds validation size limit: " + std::string(path));
  }
  std::vector<std::byte> bytes(file->Size());
  if (!bytes.empty() && file->Read(bytes.data(), bytes.size()) != bytes.size()) {
    throw std::runtime_error("Failed to read complete VDF file: " + std::string(path));
  }
  return bytes;
}

bool AddonVfs::Empty() const { return !state_ || state_->archives.empty(); }

std::size_t AddonVfs::OutstandingReferenceCount() const {
  if (!state_) {
    return 0;
  }
  const auto references = state_.use_count();
  return references > 0 ? references - 1 : 0;
}

bool AddonVfs::ValidateArchive(const std::filesystem::path& archive, std::string& error) {
  try {
    AddonVfs vfs;
    vfs.MountArchive(archive);
    error.clear();
    return true;
  } catch (const std::exception& ex) {
    error = ex.what();
    return false;
  }
}

bool AddonVfs::ValidateCachedArchive(const std::filesystem::path& archive, std::uint64_t expected_size,
                                     std::string_view expected_sha256, bool expected_gothic_dat,
                                     std::string& error) {
  std::error_code ec;
  if (!std::filesystem::is_regular_file(archive, ec) || ec) {
    error = "cache file does not exist or is not a regular file";
    return false;
  }
  if (std::filesystem::file_size(archive, ec) != expected_size || ec) {
    error = "size mismatch";
    return false;
  }

  try {
    if (gmp::crypto::NormalizeHex(gmp::crypto::ComputeFileSHA256(archive)) !=
        gmp::crypto::NormalizeHex(expected_sha256)) {
      error = "SHA-256 mismatch";
      return false;
    }

    AddonVfs vfs;
    vfs.MountArchive(archive);
    const bool contains_gothic_dat = vfs.Exists("GOTHIC.DAT");
    if (contains_gothic_dat != expected_gothic_dat) {
      error = contains_gothic_dat
                  ? "archive contains GOTHIC.DAT but its descriptor does not declare it"
                  : "descriptor declares GOTHIC.DAT but the archive does not contain it";
      return false;
    }
    error.clear();
    return true;
  } catch (const std::exception& ex) {
    error = ex.what();
    return false;
  }
}

}  // namespace gmp::addon
