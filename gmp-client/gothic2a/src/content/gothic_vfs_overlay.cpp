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

#include "content/gothic_vfs_overlay.h"

#include <addon/addon_vfs.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <limits>
#include <stdexcept>

#include "hooking/MemoryPatch.h"

namespace gmp::gothic {

namespace {

using DestructorFn = void(__thiscall*)(zFILE_VDFS*);
using OpenFn = int(__thiscall*)(zFILE_VDFS*, bool);
using OpenPathFn = int(__thiscall*)(zFILE_VDFS*, const zSTRING&, bool);
using ExistsFn = bool(__thiscall*)(zFILE_VDFS*);
using ExistsPathFn = bool(__thiscall*)(zFILE_VDFS*, const zSTRING&);
using IsOpenedFn = bool(__thiscall*)(zFILE_VDFS*);
using CloseFn = int(__thiscall*)(zFILE_VDFS*);
using ResetFn = int(__thiscall*)(zFILE_VDFS*);
using SizeFn = long(__thiscall*)(zFILE_VDFS*);
using PosFn = long(__thiscall*)(zFILE_VDFS*);
using SeekFn = int(__thiscall*)(zFILE_VDFS*, long);
using EofFn = bool(__thiscall*)(zFILE_VDFS*);
using GetStatsFn = int(__thiscall*)(zFILE_VDFS*, zFILE_STATS&);
using ReadFn = long(__thiscall*)(zFILE_VDFS*, void*, long);
using ReadCharsFn = int(__thiscall*)(zFILE_VDFS*, char*);
using ReadStringFn = int(__thiscall*)(zFILE_VDFS*, zSTRING&);
using SearchFileFn = int(__thiscall*)(zFILE_VDFS*, const zSTRING&, const zSTRING&, int);
using FindFirstFn = bool(__thiscall*)(zFILE_VDFS*, const zSTRING&);
using FindNextFn = bool(__thiscall*)(zFILE_VDFS*);

DestructorFn g_destructor = nullptr;
OpenFn g_open = nullptr;
OpenPathFn g_open_path = nullptr;
ExistsFn g_exists = nullptr;
ExistsPathFn g_exists_path = nullptr;
IsOpenedFn g_is_opened = nullptr;
CloseFn g_close = nullptr;
ResetFn g_reset = nullptr;
SizeFn g_size = nullptr;
PosFn g_pos = nullptr;
SeekFn g_seek = nullptr;
EofFn g_eof = nullptr;
GetStatsFn g_get_stats = nullptr;
ReadFn g_read = nullptr;
ReadCharsFn g_read_chars = nullptr;
ReadStringFn g_read_string = nullptr;
SearchFileFn g_search_file = nullptr;
FindFirstFn g_find_first = nullptr;
FindNextFn g_find_next = nullptr;

// The bundled Gothic II 2.6fix executable uses the VC6 _finddata32i64_t
// layout, while the generated API header lays that base member out 0x10
// bytes too large. These derived offsets were verified against the exact
// hooked functions in Gothic2.exe. Accessing the generated vdfEOF or
// find_filedata members corrupts the native vdfFindExt zSTRING.
constexpr std::ptrdiff_t kRuntimeVdfFindDataOffset = 0x29B0;
constexpr std::ptrdiff_t kRuntimeVdfEofOffset = 0x2A04;

static_assert(sizeof(void*) == 4, "The Gothic VFS overlay must remain x86");
static_assert(sizeof(TVDFFINDDATA) == 0x4C, "Unexpected Gothic VDFS find-data layout");

TVDFFINDDATA& RuntimeFindData(zFILE_VDFS* file) {
  auto* storage = reinterpret_cast<std::byte*>(file) + kRuntimeVdfFindDataOffset;
  return *reinterpret_cast<TVDFFINDDATA*>(storage);
}

void SetRuntimeEof(zFILE_VDFS* file, bool eof) {
  auto* storage = reinterpret_cast<std::byte*>(file) + kRuntimeVdfEofOffset;
  *reinterpret_cast<unsigned char*>(storage) = eof ? 1U : 0U;
}

std::string Upper(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
  return value;
}

std::string Basename(const std::string& path) {
  const auto separator = path.find_last_of("/\\");
  return separator == std::string::npos ? path : path.substr(separator + 1);
}

bool WildcardMatch(std::string_view pattern, std::string_view value) {
  std::size_t pattern_index = 0;
  std::size_t value_index = 0;
  std::size_t star = std::string_view::npos;
  std::size_t retry = 0;
  while (value_index < value.size()) {
    if (pattern_index < pattern.size() && (pattern[pattern_index] == '?' || pattern[pattern_index] == value[value_index])) {
      ++pattern_index;
      ++value_index;
    } else if (pattern_index < pattern.size() && pattern[pattern_index] == '*') {
      star = pattern_index++;
      retry = value_index;
    } else if (star != std::string_view::npos) {
      pattern_index = star + 1;
      value_index = ++retry;
    } else {
      return false;
    }
  }
  while (pattern_index < pattern.size() && pattern[pattern_index] == '*') {
    ++pattern_index;
  }
  return pattern_index == pattern.size();
}

template <typename Function>
bool InstallHook(DWORD address, DWORD callback, Function& original, const char* name) {
  if (const auto trampoline = CreateHook(address, callback)) {
    original = reinterpret_cast<Function>(*trampoline);
    return true;
  }
  SPDLOG_ERROR("Failed to install addon VFS hook {} at 0x{:08X}", name, address);
  return false;
}

}  // namespace

GothicVfsOverlay& GothicVfsOverlay::Instance() {
  static GothicVfsOverlay instance;
  return instance;
}

bool GothicVfsOverlay::InstallHooks(std::string& error) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (hooks_installed_) {
    if (!hooks_usable_) {
      error = "One or more zFILE_VDFS hooks could not be installed";
    }
    return hooks_usable_;
  }
  hooks_installed_ = true;

  bool success = true;
  success &= InstallHook(0x00448ED0, reinterpret_cast<DWORD>(&HookDestructor), g_destructor, "destructor");
  success &= InstallHook(0x00449120, reinterpret_cast<DWORD>(&HookOpen), g_open, "Open");
  success &= InstallHook(0x00448D30, reinterpret_cast<DWORD>(&HookOpenPath), g_open_path, "Open(path)");
  success &= InstallHook(0x00449020, reinterpret_cast<DWORD>(&HookExists), g_exists, "Exists");
  success &= InstallHook(0x00448DA0, reinterpret_cast<DWORD>(&HookExistsPath), g_exists_path, "Exists(path)");
  success &= InstallHook(0x00448FF0, reinterpret_cast<DWORD>(&HookIsOpened), g_is_opened, "IsOpened");
  success &= InstallHook(0x004493A0, reinterpret_cast<DWORD>(&HookClose), g_close, "Close");
  success &= InstallHook(0x00449400, reinterpret_cast<DWORD>(&HookReset), g_reset, "Reset");
  success &= InstallHook(0x00449410, reinterpret_cast<DWORD>(&HookSize), g_size, "Size");
  success &= InstallHook(0x00449A50, reinterpret_cast<DWORD>(&HookPos), g_pos, "Pos");
  success &= InstallHook(0x00449490, reinterpret_cast<DWORD>(&HookSeek), g_seek, "Seek");
  success &= InstallHook(0x00449470, reinterpret_cast<DWORD>(&HookEof), g_eof, "Eof");
  success &= InstallHook(0x0044A2F0, reinterpret_cast<DWORD>(&HookGetStats), g_get_stats, "GetStats");
  success &= InstallHook(0x0044ABF0, reinterpret_cast<DWORD>(&HookRead), g_read, "Read(bytes)");
  success &= InstallHook(0x0044A8D0, reinterpret_cast<DWORD>(&HookReadChars), g_read_chars, "Read(char*)");
  success &= InstallHook(0x0044AA80, reinterpret_cast<DWORD>(&HookReadString), g_read_string, "Read(zSTRING)");
  success &= InstallHook(0x00449E80, reinterpret_cast<DWORD>(&HookSearchFile), g_search_file, "SearchFile");
  success &= InstallHook(0x0044A300, reinterpret_cast<DWORD>(&HookFindFirst), g_find_first, "FindFirst");
  success &= InstallHook(0x0044A5B0, reinterpret_cast<DWORD>(&HookFindNext), g_find_next, "FindNext");

  hooks_usable_ = success;
  if (!success) {
    error = "One or more zFILE_VDFS hooks could not be installed";
    return false;
  }
  SPDLOG_INFO("Addon VFS: installed zFILE_VDFS overlay hooks");
  error.clear();
  return true;
}

bool GothicVfsOverlay::Activate(const std::vector<std::filesystem::path>& archives, std::string& error) {
  if (archives.empty()) {
    error = "Cannot activate an empty addon VFS overlay";
    return false;
  }
  auto candidate = std::make_shared<gmp::addon::AddonVfs>();
  try {
    candidate->MountArchives(archives);
  } catch (const std::exception& ex) {
    error = ex.what();
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  if (!hooks_usable_) {
    error = "Addon VFS hooks are unavailable";
    return false;
  }
  if (active_vfs_) {
    error = "An addon VFS overlay is already active";
    return false;
  }
  active_vfs_ = std::move(candidate);
  SPDLOG_INFO("Addon VFS: mounted {} archive(s), later declarations have higher precedence", archives.size());
  error.clear();
  return true;
}

void GothicVfsOverlay::Deactivate() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!active_vfs_) {
    return;
  }
  const auto retained_handles = open_files_.size();
  active_vfs_.reset();
  find_states_.clear();
  SPDLOG_INFO("Addon VFS: overlay detached; {} open handle(s) retain archive lifetime until Close", retained_handles);
}

bool GothicVfsOverlay::IsActive() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return active_vfs_ != nullptr;
}

std::size_t GothicVfsOverlay::OpenHandleCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return open_files_.size();
}

std::vector<std::string> GothicVfsOverlay::EnumerateActiveFiles() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return active_vfs_ ? active_vfs_->EnumerateFiles() : std::vector<std::string>{};
}

std::shared_ptr<gmp::addon::AddonFile> GothicVfsOverlay::OpenExact(std::string_view path) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return active_vfs_ ? active_vfs_->OpenExact(path) : nullptr;
}

std::optional<std::vector<std::uint8_t>> GothicVfsOverlay::ReadNativeFile(const std::string& path, std::size_t maximum_size) const {
  if (!zFILE_VDFS::s_vdfsInitialized) return std::nullopt;
  zFILE_VDFS file;
  file.Init(zSTRING(path.c_str()));
  if (!(g_exists ? g_exists(&file) : file.Exists())) return std::nullopt;
  // Calling the original Open directly prevents the addon overlay from
  // substituting an unrelated same-named file. Native VDFS keeps its own rules.
  const auto opened = g_open ? g_open(&file, false) : file.Open(false);
  if (opened != zERR_NONE) throw std::runtime_error("Cannot open native asset: " + path);
  const auto close = [&] { if (g_close) g_close(&file); else file.Close(); };
  try {
    const auto length = g_size ? g_size(&file) : file.Size();
    if (length <= 0 || static_cast<std::size_t>(length) > maximum_size)
      throw std::runtime_error("Invalid native asset size: " + path);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    const auto count = g_read ? g_read(&file, bytes.data(), length) : file.Read(bytes.data(), length);
    if (count != length) throw std::runtime_error("Incomplete native asset read: " + path);
    close();
    return bytes;
  } catch (...) {
    close();
    throw;
  }
}

std::optional<int> GothicVfsOverlay::TryOpen(zFILE_VDFS* file, const char* path, bool write) {
  if (!file || !path || write) {
    return std::nullopt;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  if (!active_vfs_) {
    return std::nullopt;
  }
  auto opened = active_vfs_->Open(path);
  if (!opened) {
    return std::nullopt;
  }
  open_files_[file] = std::move(opened);
  SetRuntimeEof(file, false);
  return zERR_NONE;
}

bool GothicVfsOverlay::TryClose(zFILE_VDFS* file) {
  std::lock_guard<std::mutex> lock(mutex_);
  find_states_.erase(file);
  return open_files_.erase(file) != 0;
}

bool GothicVfsOverlay::OverlayExists(const char* path) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return active_vfs_ && path && active_vfs_->Exists(path);
}

bool GothicVfsOverlay::HasOpenFile(zFILE_VDFS* file) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return open_files_.contains(file);
}

long GothicVfsOverlay::Read(zFILE_VDFS* file, void* buffer, long length) {
  if (length <= 0) {
    return 0;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  const auto found = open_files_.find(file);
  if (found == open_files_.end()) {
    return -1;
  }
  const auto read = found->second->Read(buffer, static_cast<std::size_t>(length));
  SetRuntimeEof(file, found->second->Eof());
  return static_cast<long>(std::min<std::size_t>(read, std::numeric_limits<long>::max()));
}

int GothicVfsOverlay::ReadLine(zFILE_VDFS* file, std::string& line, std::size_t maximum_length) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto found = open_files_.find(file);
  if (found == open_files_.end()) {
    return -1;
  }
  line.clear();
  char ch = 0;
  while (line.size() < maximum_length && found->second->Read(&ch, 1) == 1) {
    if (ch == '\n') {
      break;
    }
    if (ch != '\r') {
      line.push_back(ch);
    }
  }
  const bool eof = found->second->Eof();
  SetRuntimeEof(file, eof);
  return line.empty() && eof ? 0 : static_cast<int>(line.size());
}

int GothicVfsOverlay::Reset(zFILE_VDFS* file) {
  return Seek(file, 0);
}

long GothicVfsOverlay::Size(zFILE_VDFS* file) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto found = open_files_.find(file);
  return found == open_files_.end() ? -1L : static_cast<long>(found->second->Size());
}

long GothicVfsOverlay::Pos(zFILE_VDFS* file) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto found = open_files_.find(file);
  return found == open_files_.end() ? -1L : static_cast<long>(found->second->Tell());
}

int GothicVfsOverlay::Seek(zFILE_VDFS* file, long position) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto found = open_files_.find(file);
  if (found == open_files_.end()) {
    return -1;
  }
  const bool success = found->second->Seek(position);
  SetRuntimeEof(file, found->second->Eof());
  return success ? zERR_NONE : zERR_DSK_INVAL;
}

bool GothicVfsOverlay::Eof(zFILE_VDFS* file) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto found = open_files_.find(file);
  return found != open_files_.end() && found->second->Eof();
}

int GothicVfsOverlay::GetStats(zFILE_VDFS* file, zFILE_STATS& stats) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto found = open_files_.find(file);
  if (found == open_files_.end()) {
    return zERR_DSK_HANDLE;
  }
  std::memset(&stats, 0, sizeof(stats));
  stats.size = static_cast<unsigned int>(found->second->Size());
  return zERR_NONE;
}

int GothicVfsOverlay::SearchFile(zFILE_VDFS* file, const zSTRING& name, const zSTRING& directory, int) {
  std::string candidate = directory.ToChar();
  if (!candidate.empty() && candidate.back() != '/' && candidate.back() != '\\') {
    candidate.push_back('/');
  }
  candidate += name.ToChar();
  if (!OverlayExists(candidate.c_str()) && !OverlayExists(name.ToChar())) {
    return 0;
  }
  zSTRING resolved(candidate.c_str());
  file->Init(resolved);
  return 1;
}

bool GothicVfsOverlay::PopulateFindData(zFILE_VDFS* file, const std::string& path) {
  if (!active_vfs_) {
    return false;
  }
  const auto size = active_vfs_->FileSize(path);
  if (!size) {
    return false;
  }
  auto& find_data = RuntimeFindData(file);
  std::memset(&find_data, 0, sizeof(find_data));
  const auto name = Basename(path);
  std::strncpy(find_data.name, name.c_str(), sizeof(find_data.name) - 1);
  find_data.size = static_cast<long>(std::min<std::size_t>(*size, std::numeric_limits<long>::max()));
  return true;
}

bool GothicVfsOverlay::FindFirst(zFILE_VDFS* file, const zSTRING& pattern) {
  std::lock_guard<std::mutex> lock(mutex_);
  find_states_.erase(file);
  if (!active_vfs_) {
    return false;
  }
  const auto normalized_pattern = Upper(pattern.ToChar());
  FindState state;
  for (const auto& path : active_vfs_->EnumerateFiles()) {
    if (WildcardMatch(normalized_pattern, Upper(path)) || WildcardMatch(normalized_pattern, Upper(Basename(path)))) {
      state.paths.push_back(path);
    }
  }
  if (state.paths.empty()) {
    return false;
  }
  std::sort(state.paths.begin(), state.paths.end());
  const auto first = state.paths.front();
  state.native_pattern = pattern.ToChar();
  find_states_[file] = std::move(state);
  return PopulateFindData(file, first);
}

GothicVfsOverlay::FindNextResult GothicVfsOverlay::FindNext(zFILE_VDFS* file, std::string& native_pattern) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto found = find_states_.find(file);
  if (found == find_states_.end()) {
    return FindNextResult::NativeOnly;
  }
  if (++found->second.index >= found->second.paths.size()) {
    native_pattern = found->second.native_pattern;
    find_states_.erase(file);
    return FindNextResult::StartNative;
  }
  PopulateFindData(file, found->second.paths[found->second.index]);
  return FindNextResult::OverlayMatch;
}

void __fastcall GothicVfsOverlay::HookDestructor(zFILE_VDFS* file, void*) {
  Instance().TryClose(file);
  g_destructor(file);
}

int __fastcall GothicVfsOverlay::HookOpen(zFILE_VDFS* file, void*, bool write) {
  const zSTRING path = file->GetFullPath();
  if (const auto result = Instance().TryOpen(file, path.ToChar(), write)) {
    return *result;
  }
  return g_open(file, write);
}

int __fastcall GothicVfsOverlay::HookOpenPath(zFILE_VDFS* file, void*, const zSTRING& path, bool write) {
  if (!write && Instance().OverlayExists(path.ToChar())) {
    file->Init(path);
    if (const auto result = Instance().TryOpen(file, path.ToChar(), false)) {
      return *result;
    }
  }
  return g_open_path(file, path, write);
}

bool __fastcall GothicVfsOverlay::HookExists(zFILE_VDFS* file, void*) {
  const zSTRING path = file->GetFullPath();
  return Instance().OverlayExists(path.ToChar()) || g_exists(file);
}

bool __fastcall GothicVfsOverlay::HookExistsPath(zFILE_VDFS* file, void*, const zSTRING& path) {
  return Instance().OverlayExists(path.ToChar()) || g_exists_path(file, path);
}

bool __fastcall GothicVfsOverlay::HookIsOpened(zFILE_VDFS* file, void*) {
  return Instance().HasOpenFile(file) || g_is_opened(file);
}

int __fastcall GothicVfsOverlay::HookClose(zFILE_VDFS* file, void*) {
  return Instance().TryClose(file) ? zERR_NONE : g_close(file);
}

int __fastcall GothicVfsOverlay::HookReset(zFILE_VDFS* file, void*) {
  return Instance().HasOpenFile(file) ? Instance().Reset(file) : g_reset(file);
}

long __fastcall GothicVfsOverlay::HookSize(zFILE_VDFS* file, void*) {
  return Instance().HasOpenFile(file) ? Instance().Size(file) : g_size(file);
}

long __fastcall GothicVfsOverlay::HookPos(zFILE_VDFS* file, void*) {
  return Instance().HasOpenFile(file) ? Instance().Pos(file) : g_pos(file);
}

int __fastcall GothicVfsOverlay::HookSeek(zFILE_VDFS* file, void*, long position) {
  return Instance().HasOpenFile(file) ? Instance().Seek(file, position) : g_seek(file, position);
}

bool __fastcall GothicVfsOverlay::HookEof(zFILE_VDFS* file, void*) {
  return Instance().HasOpenFile(file) ? Instance().Eof(file) : g_eof(file);
}

int __fastcall GothicVfsOverlay::HookGetStats(zFILE_VDFS* file, void*, zFILE_STATS& stats) {
  return Instance().HasOpenFile(file) ? Instance().GetStats(file, stats) : g_get_stats(file, stats);
}

long __fastcall GothicVfsOverlay::HookRead(zFILE_VDFS* file, void*, void* buffer, long length) {
  return Instance().HasOpenFile(file) ? Instance().Read(file, buffer, length) : g_read(file, buffer, length);
}

int __fastcall GothicVfsOverlay::HookReadChars(zFILE_VDFS* file, void*, char* buffer) {
  if (!Instance().HasOpenFile(file)) {
    return g_read_chars(file, buffer);
  }
  std::string line;
  const int result = Instance().ReadLine(file, line, 1023);
  if (buffer) {
    std::memcpy(buffer, line.c_str(), line.size() + 1);
  }
  return result;
}

int __fastcall GothicVfsOverlay::HookReadString(zFILE_VDFS* file, void*, zSTRING& value) {
  if (!Instance().HasOpenFile(file)) {
    return g_read_string(file, value);
  }
  std::string line;
  const int result = Instance().ReadLine(file, line, 10 * 1024 - 1);
  value = line.c_str();
  return result;
}

int __fastcall GothicVfsOverlay::HookSearchFile(zFILE_VDFS* file, void*, const zSTRING& name, const zSTRING& directory, int mode) {
  const int overlay_result = Instance().SearchFile(file, name, directory, mode);
  return overlay_result != 0 ? overlay_result : g_search_file(file, name, directory, mode);
}

bool __fastcall GothicVfsOverlay::HookFindFirst(zFILE_VDFS* file, void*, const zSTRING& pattern) {
  return Instance().FindFirst(file, pattern) || g_find_first(file, pattern);
}

bool __fastcall GothicVfsOverlay::HookFindNext(zFILE_VDFS* file, void*) {
  std::string native_pattern;
  switch (Instance().FindNext(file, native_pattern)) {
    case FindNextResult::OverlayMatch:
      return true;
    case FindNextResult::StartNative: {
      const zSTRING pattern(native_pattern.c_str());
      return g_find_first(file, pattern);
    }
    case FindNextResult::NativeOnly:
      return g_find_next(file);
  }
  return false;
}

}  // namespace gmp::gothic
