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

#include <filesystem>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ZenGin/zGothicAPI.h"

namespace gmp::addon {
class AddonFile;
class AddonVfs;
}  // namespace gmp::addon

namespace gmp::gothic {

class GothicVfsOverlay {
public:
  static GothicVfsOverlay& Instance();

  bool InstallHooks(std::string& error);
  bool Activate(const std::vector<std::filesystem::path>& archives, std::string& error);
  void Deactivate();
  bool IsActive() const;
  std::size_t OpenHandleCount() const;
  std::vector<std::string> EnumerateActiveFiles() const;
  std::shared_ptr<gmp::addon::AddonFile> OpenExact(std::string_view path) const;
  // Read through native VDFS without the server overlay's compatibility lookup.
  // Missing files return nullopt; invalid sizes and incomplete reads throw.
  std::optional<std::vector<std::uint8_t>> ReadNativeFile(const std::string& path, std::size_t maximum_size) const;

private:
  struct FindState {
    std::vector<std::string> paths;
    std::size_t index{0};
    std::string native_pattern;
  };

  enum class FindNextResult { OverlayMatch, StartNative, NativeOnly };

  GothicVfsOverlay() = default;

  std::optional<int> TryOpen(zFILE_VDFS* file, const char* path, bool write);
  bool TryClose(zFILE_VDFS* file);
  bool OverlayExists(const char* path) const;
  bool HasOpenFile(zFILE_VDFS* file) const;
  long Read(zFILE_VDFS* file, void* buffer, long length);
  int ReadLine(zFILE_VDFS* file, std::string& line, std::size_t maximum_length);
  int Reset(zFILE_VDFS* file);
  long Size(zFILE_VDFS* file) const;
  long Pos(zFILE_VDFS* file) const;
  int Seek(zFILE_VDFS* file, long position);
  bool Eof(zFILE_VDFS* file) const;
  int GetStats(zFILE_VDFS* file, zFILE_STATS& stats) const;
  int SearchFile(zFILE_VDFS* file, const zSTRING& name, const zSTRING& directory, int mode);
  bool FindFirst(zFILE_VDFS* file, const zSTRING& pattern);
  FindNextResult FindNext(zFILE_VDFS* file, std::string& native_pattern);
  bool PopulateFindData(zFILE_VDFS* file, const std::string& path);

  static void __fastcall HookDestructor(zFILE_VDFS* file, void*);
  static int __fastcall HookOpen(zFILE_VDFS* file, void*, bool write);
  static int __fastcall HookOpenPath(zFILE_VDFS* file, void*, const zSTRING& path, bool write);
  static bool __fastcall HookExists(zFILE_VDFS* file, void*);
  static bool __fastcall HookExistsPath(zFILE_VDFS* file, void*, const zSTRING& path);
  static bool __fastcall HookIsOpened(zFILE_VDFS* file, void*);
  static int __fastcall HookClose(zFILE_VDFS* file, void*);
  static int __fastcall HookReset(zFILE_VDFS* file, void*);
  static long __fastcall HookSize(zFILE_VDFS* file, void*);
  static long __fastcall HookPos(zFILE_VDFS* file, void*);
  static int __fastcall HookSeek(zFILE_VDFS* file, void*, long position);
  static bool __fastcall HookEof(zFILE_VDFS* file, void*);
  static int __fastcall HookGetStats(zFILE_VDFS* file, void*, zFILE_STATS& stats);
  static long __fastcall HookRead(zFILE_VDFS* file, void*, void* buffer, long length);
  static int __fastcall HookReadChars(zFILE_VDFS* file, void*, char* buffer);
  static int __fastcall HookReadString(zFILE_VDFS* file, void*, zSTRING& value);
  static int __fastcall HookSearchFile(zFILE_VDFS* file, void*, const zSTRING& name, const zSTRING& directory, int mode);
  static bool __fastcall HookFindFirst(zFILE_VDFS* file, void*, const zSTRING& pattern);
  static bool __fastcall HookFindNext(zFILE_VDFS* file, void*);

  mutable std::mutex mutex_;
  std::shared_ptr<gmp::addon::AddonVfs> active_vfs_;
  std::unordered_map<zFILE_VDFS*, std::shared_ptr<gmp::addon::AddonFile>> open_files_;
  std::unordered_map<zFILE_VDFS*, FindState> find_states_;
  bool hooks_installed_{false};
  bool hooks_usable_{false};
};

}  // namespace gmp::gothic
