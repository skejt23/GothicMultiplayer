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
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace zenkit {
class Read;
class Vfs;
class VfsNode;
}  // namespace zenkit

namespace gmp::addon {

// Converts Gothic/VDF paths to ZenKit's canonical slash-separated form.
// Absolute paths and parent traversal are rejected.
std::optional<std::string> NormalizeVdfPath(std::string_view path);

class AddonFile {
public:
  AddonFile(AddonFile&&) noexcept;
  AddonFile& operator=(AddonFile&&) noexcept;
  ~AddonFile();

  AddonFile(const AddonFile&) = delete;
  AddonFile& operator=(const AddonFile&) = delete;

  std::size_t Read(void* buffer, std::size_t length);
  bool Seek(std::int64_t offset);
  std::size_t Tell() const;
  std::size_t Size() const;
  bool Eof() const;

private:
  struct State;
  friend class AddonVfs;
  AddonFile(std::shared_ptr<const State> state, std::unique_ptr<zenkit::Read> reader, std::size_t size);

  std::shared_ptr<const State> state_;
  std::unique_ptr<zenkit::Read> reader_;
  std::size_t size_{0};
};

class AddonVfs {
public:
  AddonVfs();
  AddonVfs(AddonVfs&&) noexcept;
  AddonVfs& operator=(AddonVfs&&) noexcept;
  ~AddonVfs();

  AddonVfs(const AddonVfs&) = delete;
  AddonVfs& operator=(const AddonVfs&) = delete;

  // Later archives overwrite earlier archives, matching server declaration
  // order and Gothic VDF priority requirements.
  void MountArchives(const std::vector<std::filesystem::path>& archives);
  void MountArchive(const std::filesystem::path& archive);

  std::shared_ptr<AddonFile> Open(std::string_view path) const;
  // Full archive path only; no _WORK/DATA stripping or basename fallback.
  std::shared_ptr<AddonFile> OpenExact(std::string_view path) const;
  bool Exists(std::string_view path) const;
  std::optional<std::size_t> FileSize(std::string_view path) const;
  std::vector<std::string> EnumerateFiles() const;
  std::vector<std::byte> ReadFile(std::string_view path, std::size_t maximum_size) const;
  bool Empty() const;

  // A detached VFS stays alive while AddonFile objects still reference it.
  // This is the lifetime primitive used by the client overlay during unload.
  std::size_t OutstandingReferenceCount() const;

  static bool ValidateArchive(const std::filesystem::path& archive, std::string& error);
  static bool ValidateCachedArchive(const std::filesystem::path& archive, std::uint64_t expected_size,
                                    std::string_view expected_sha256, bool expected_gothic_dat,
                                    std::string& error);

private:
  using State = AddonFile::State;
  const zenkit::VfsNode* ResolveFile(std::string_view path) const;
  std::shared_ptr<AddonFile> OpenNode(const zenkit::VfsNode* node) const;
  std::shared_ptr<State> state_;
};

}  // namespace gmp::addon
