#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace gmp::paths {

inline std::string WideToUtf8(std::wstring_view value) {
  if (value.empty()) {
    return {};
  }
  const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
  if (size <= 0) {
    throw std::system_error(GetLastError(), std::system_category(), "WideCharToMultiByte");
  }
  std::string result(static_cast<std::size_t>(size), '\0');
  if (WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr) != size) {
    throw std::system_error(GetLastError(), std::system_category(), "WideCharToMultiByte");
  }
  return result;
}

// A null module means the process executable, independent of its working directory.
inline std::filesystem::path ModulePath(HMODULE module = nullptr) {
  std::vector<wchar_t> buffer(512);
  for (;;) {
    const DWORD length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0) {
      throw std::system_error(GetLastError(), std::system_category(), "GetModuleFileNameW");
    }
    if (length < buffer.size()) {
      return std::filesystem::path(std::wstring(buffer.data(), length));
    }
    buffer.resize(buffer.size() * 2);
  }
}

inline std::filesystem::path ContainingModulePath(const void* address) {
  HMODULE module = nullptr;
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCWSTR>(address), &module)) {
    throw std::system_error(GetLastError(), std::system_category(), "GetModuleHandleExW");
  }
  return ModulePath(module);
}

}  // namespace gmp::paths
