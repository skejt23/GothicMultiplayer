
/*
MIT License

Copyright (c) 2025 Gothic Multiplayer Team.

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

// clang-format off
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <wincrypt.h>
#include <winhttp.h>
// clang-format on

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "windows_paths.h"

namespace fs = std::filesystem;

#ifndef GMP_UPDATE_SOURCE_URL
#define GMP_UPDATE_SOURCE_URL ""
#endif

namespace {

class ScopedInternetHandle {
public:
  ScopedInternetHandle() = default;
  explicit ScopedInternetHandle(HINTERNET handle) : handle_(handle) {}
  ~ScopedInternetHandle() {
    if (handle_) {
      WinHttpCloseHandle(handle_);
    }
  }

  ScopedInternetHandle(const ScopedInternetHandle&) = delete;
  ScopedInternetHandle& operator=(const ScopedInternetHandle&) = delete;

  operator HINTERNET() const {
    return handle_;
  }

private:
  HINTERNET handle_ = nullptr;
};

}  // namespace

class GMPLauncher {
private:
  struct RuntimeFile {
    std::string fileName;
    fs::path targetPath;
  };

  static constexpr std::array<const wchar_t*, 3> runtimeDlls = {
      L"SDL3.dll", L"BugTrap.dll", L"discord_game_sdk.dll"};

  std::string gothicPath;
  std::string gmpDllPath;
  std::string workingDirectory;
  fs::path launcherDirectory;
  std::string updateSourceUrl;
  std::shared_ptr<spdlog::logger> logger;
  bool waitForDebugger = false;
  bool updateEnabled = true;
  bool initialBreakpointHandled = false;

  void InitializeLogger() {
    try {
      // Create console sink with colors
      auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
      console_sink->set_level(spdlog::level::debug);

      // Create file sink
      auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
          WideToUtf8((launcherDirectory / L"GMPLauncher.log").wstring()), true);
      file_sink->set_level(spdlog::level::debug);

      // Create logger with both sinks
      logger = std::make_shared<spdlog::logger>("GMPLauncher", spdlog::sinks_init_list{console_sink, file_sink});
      logger->set_level(spdlog::level::debug);
      logger->flush_on(spdlog::level::info);

      // Set as default logger
      spdlog::set_default_logger(logger);
    } catch (const spdlog::spdlog_ex& ex) {
      std::cerr << "Log initialization failed: " << ex.what() << std::endl;
      // Fallback to console only
      logger = spdlog::stdout_color_mt("GMPLauncher");
    }
  }

  // Helper function to convert UTF-8 string to UTF-16 for Windows APIs
  std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty())
      return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (size <= 0)
      return {};
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, result.data(), size);
    result.pop_back();
    return result;
  }

  fs::path GetGMPDllPath() {
    return Utf8ToWide(gmpDllPath);
  }

  fs::path GetGMPDllDirectory() {
    fs::path dllPath = GetGMPDllPath();
    fs::path dllDirectory = dllPath.parent_path();
    if (dllDirectory.empty()) {
      dllDirectory = Utf8ToWide(workingDirectory);
    }
    return dllDirectory;
  }

  // Helper function to convert UTF-16 string to UTF-8
  std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty())
      return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0)
      return {};
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, result.data(), size, nullptr, nullptr);
    result.pop_back();
    return result;
  }

  static std::string Trim(const std::string& value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch); });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) { return std::isspace(ch); }).base();
    if (first >= last) {
      return {};
    }
    return std::string(first, last);
  }

  static bool IsHexDigit(char ch) {
    return std::isxdigit(static_cast<unsigned char>(ch)) != 0;
  }

  static std::optional<std::string> ExtractSHA256(const std::string& text) {
    std::string candidate;
    candidate.reserve(64);

    for (char ch : text) {
      if (IsHexDigit(ch)) {
        candidate.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        if (candidate.size() == 64) {
          return candidate;
        }
      } else {
        candidate.clear();
      }
    }

    return std::nullopt;
  }

  static std::string BytesToHex(const BYTE* bytes, DWORD size) {
    static constexpr char hexDigits[] = "0123456789abcdef";
    std::string hex;
    hex.reserve(static_cast<std::size_t>(size) * 2);

    for (DWORD i = 0; i < size; ++i) {
      const BYTE byte = bytes[i];
      hex.push_back(hexDigits[(byte >> 4) & 0x0F]);
      hex.push_back(hexDigits[byte & 0x0F]);
    }

    return hex;
  }

  static bool HashData(HCRYPTHASH hash, const std::uint8_t* data, std::size_t size) {
    constexpr std::size_t maxChunkSize = 64 * 1024;
    std::size_t offset = 0;

    while (offset < size) {
      const std::size_t chunkSize = (std::min)(maxChunkSize, size - offset);
      if (!CryptHashData(hash, reinterpret_cast<const BYTE*>(data + offset), static_cast<DWORD>(chunkSize), 0)) {
        return false;
      }
      offset += chunkSize;
    }

    return true;
  }

  static std::optional<std::string> FinishSHA256Hash(HCRYPTHASH hash) {
    std::array<BYTE, 32> digest{};
    DWORD digestSize = static_cast<DWORD>(digest.size());
    if (!CryptGetHashParam(hash, HP_HASHVAL, digest.data(), &digestSize, 0)) {
      return std::nullopt;
    }

    return BytesToHex(digest.data(), digestSize);
  }

  std::optional<std::string> ComputeSHA256(const std::vector<std::uint8_t>& bytes) {
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;

    if (!CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
      SPDLOG_WARN("Failed to acquire crypto provider. Error: {}", GetLastError());
      return std::nullopt;
    }

    if (!CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash)) {
      SPDLOG_WARN("Failed to create SHA-256 hash. Error: {}", GetLastError());
      CryptReleaseContext(provider, 0);
      return std::nullopt;
    }

    const bool hashed = bytes.empty() || HashData(hash, bytes.data(), bytes.size());
    std::optional<std::string> result;
    if (hashed) {
      result = FinishSHA256Hash(hash);
    }

    CryptDestroyHash(hash);
    CryptReleaseContext(provider, 0);
    return result;
  }

  std::optional<std::string> ComputeFileSHA256(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
      SPDLOG_WARN("Failed to open {} for checksum calculation", WideToUtf8(path.wstring()));
      return std::nullopt;
    }

    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;

    if (!CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
      SPDLOG_WARN("Failed to acquire crypto provider. Error: {}", GetLastError());
      return std::nullopt;
    }

    if (!CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash)) {
      SPDLOG_WARN("Failed to create SHA-256 hash. Error: {}", GetLastError());
      CryptReleaseContext(provider, 0);
      return std::nullopt;
    }

    std::array<std::uint8_t, 64 * 1024> buffer{};
    bool success = true;
    while (input) {
      input.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
      const std::streamsize bytesRead = input.gcount();
      if (bytesRead > 0 && !HashData(hash, buffer.data(), static_cast<std::size_t>(bytesRead))) {
        SPDLOG_WARN("Failed to hash {}", WideToUtf8(path.wstring()));
        success = false;
        break;
      }
    }

    std::optional<std::string> result;
    if (success) {
      result = FinishSHA256Hash(hash);
    }

    CryptDestroyHash(hash);
    CryptReleaseContext(provider, 0);
    return result;
  }

  std::string BuildUpdateUrl(const std::string& fileName) {
    std::string source = Trim(updateSourceUrl);
    while (!source.empty() && (source.back() == '/' || source.back() == '\\')) {
      source.pop_back();
    }

    if (source.empty()) {
      return {};
    }

    return source + "/" + fileName;
  }

  std::optional<std::vector<std::uint8_t>> DownloadUrl(const std::string& url, std::size_t maxBytes = 0) {
    const std::wstring wideUrl = Utf8ToWide(url);
    if (wideUrl.empty()) {
      SPDLOG_WARN("Invalid update URL: {}", url);
      return std::nullopt;
    }

    URL_COMPONENTSW components = {};
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = static_cast<DWORD>(-1);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);

    if (!WinHttpCrackUrl(wideUrl.c_str(), 0, 0, &components)) {
      SPDLOG_WARN("Failed to parse update URL '{}'. Error: {}", url, GetLastError());
      return std::nullopt;
    }

    const bool isHttps = components.nScheme == INTERNET_SCHEME_HTTPS;
    if (!isHttps && components.nScheme != INTERNET_SCHEME_HTTP) {
      SPDLOG_WARN("Unsupported update URL scheme: {}", url);
      return std::nullopt;
    }

    std::wstring host;
    if (components.lpszHostName && components.dwHostNameLength > 0) {
      host.assign(components.lpszHostName, components.dwHostNameLength);
    }

    std::wstring path;
    if (components.lpszUrlPath && components.dwUrlPathLength > 0) {
      path.assign(components.lpszUrlPath, components.dwUrlPathLength);
    }
    if (components.lpszExtraInfo && components.dwExtraInfoLength > 0) {
      path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    }
    if (path.empty()) {
      path = L"/";
    }

    ScopedInternetHandle session(WinHttpOpen(L"GMPLauncher/1.0",
                                             WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                             WINHTTP_NO_PROXY_NAME,
                                             WINHTTP_NO_PROXY_BYPASS,
                                             0));
    if (!session) {
      SPDLOG_WARN("Failed to initialize update HTTP session. Error: {}", GetLastError());
      return std::nullopt;
    }

    WinHttpSetTimeouts(session, 5000, 5000, 5000, 5000);

    ScopedInternetHandle connection(WinHttpConnect(session, host.c_str(), components.nPort, 0));
    if (!connection) {
      SPDLOG_WARN("Failed to connect to update host. Error: {}", GetLastError());
      return std::nullopt;
    }

    ScopedInternetHandle request(WinHttpOpenRequest(connection,
                                                   L"GET",
                                                   path.c_str(),
                                                   nullptr,
                                                   WINHTTP_NO_REFERER,
                                                   WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                   isHttps ? WINHTTP_FLAG_SECURE : 0));
    if (!request) {
      SPDLOG_WARN("Failed to create update request. Error: {}", GetLastError());
      return std::nullopt;
    }

    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

    if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(request, nullptr)) {
      SPDLOG_WARN("Failed to download {}. Error: {}", url, GetLastError());
      return std::nullopt;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    if (!WinHttpQueryHeaders(request,
                             WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX,
                             &statusCode,
                             &statusCodeSize,
                             WINHTTP_NO_HEADER_INDEX)) {
      SPDLOG_WARN("Failed to read update HTTP status. Error: {}", GetLastError());
      return std::nullopt;
    }

    if (statusCode != 200) {
      SPDLOG_WARN("Update request returned HTTP {}", statusCode);
      return std::nullopt;
    }

    std::vector<std::uint8_t> body;
    for (;;) {
      DWORD available = 0;
      if (!WinHttpQueryDataAvailable(request, &available)) {
        SPDLOG_WARN("Failed while reading update response. Error: {}", GetLastError());
        return std::nullopt;
      }

      if (available == 0) {
        break;
      }

      if (maxBytes > 0 && body.size() + available > maxBytes) {
        SPDLOG_WARN("Update response exceeded expected maximum size");
        return std::nullopt;
      }

      const std::size_t offset = body.size();
      body.resize(offset + available);

      DWORD bytesRead = 0;
      if (!WinHttpReadData(request, body.data() + offset, available, &bytesRead)) {
        SPDLOG_WARN("Failed while reading update response body. Error: {}", GetLastError());
        return std::nullopt;
      }

      body.resize(offset + bytesRead);
    }

    return body;
  }

  bool InstallDownloadedFile(const RuntimeFile& file, const std::vector<std::uint8_t>& fileBytes) {
    const fs::path& targetPath = file.targetPath;
    fs::path tempPath = targetPath;
    tempPath += L".download";

    std::error_code ec;
    fs::create_directories(targetPath.parent_path(), ec);
    if (ec) {
      SPDLOG_ERROR("Failed to create runtime directory: {}", ec.message());
      return false;
    }
    fs::remove(tempPath, ec);

    {
      std::ofstream output(tempPath, std::ios::binary | std::ios::trunc);
      if (!output) {
        SPDLOG_ERROR("Failed to create temporary {} download file: {}", file.fileName, WideToUtf8(tempPath.wstring()));
        return false;
      }

      output.write(reinterpret_cast<const char*>(fileBytes.data()), static_cast<std::streamsize>(fileBytes.size()));
      if (!output) {
        SPDLOG_ERROR("Failed to write temporary {} download file: {}", file.fileName, WideToUtf8(tempPath.wstring()));
        return false;
      }
    }

    if (fs::exists(targetPath)) {
      fs::remove(targetPath, ec);
      if (ec) {
        SPDLOG_ERROR("Failed to remove existing {}: {}", file.fileName, ec.message());
        fs::remove(tempPath, ec);
        return false;
      }
    }

    fs::rename(tempPath, targetPath, ec);
    if (ec) {
      SPDLOG_ERROR("Failed to install downloaded {}: {}", file.fileName, ec.message());
      fs::remove(tempPath, ec);
      return false;
    }

    SPDLOG_INFO("Installed updated {}", file.fileName);
    return true;
  }

  bool UpdateRuntimeFile(const RuntimeFile& file, const std::vector<std::uint8_t>* checksumBytesOverride = nullptr) {
    std::optional<std::vector<std::uint8_t>> downloadedChecksumBytes;
    const std::vector<std::uint8_t>* checksumBytes = checksumBytesOverride;

    if (!checksumBytes) {
      downloadedChecksumBytes = DownloadUrl(BuildUpdateUrl(file.fileName + ".sha256"), 4096);
      if (!downloadedChecksumBytes) {
        SPDLOG_ERROR("Failed to download {} checksum from update source", file.fileName);
        return false;
      }
      checksumBytes = &*downloadedChecksumBytes;
    }

    const std::string checksumText(checksumBytes->begin(), checksumBytes->end());
    const auto expectedChecksum = ExtractSHA256(checksumText);
    if (!expectedChecksum) {
      SPDLOG_ERROR("Update source did not provide a valid {} SHA-256 checksum", file.fileName);
      return false;
    }

    if (fs::exists(file.targetPath)) {
      const auto localChecksum = ComputeFileSHA256(file.targetPath);
      if (localChecksum && *localChecksum == *expectedChecksum) {
        SPDLOG_INFO("Local {} is up to date", file.fileName);
        return true;
      }

      SPDLOG_INFO("Local {} checksum differs from update source; downloading replacement", file.fileName);
    } else {
      SPDLOG_INFO("{} is missing; downloading from update source", file.fileName);
    }

    const auto fileBytes = DownloadUrl(BuildUpdateUrl(file.fileName));
    if (!fileBytes) {
      SPDLOG_ERROR("Update source is reachable, but {} download failed", file.fileName);
      return false;
    }

    const auto downloadedChecksum = ComputeSHA256(*fileBytes);
    if (!downloadedChecksum || *downloadedChecksum != *expectedChecksum) {
      SPDLOG_ERROR("Downloaded {} checksum mismatch; refusing to install it", file.fileName);
      return false;
    }

    return InstallDownloadedFile(file, *fileBytes);
  }

  bool UpdateRuntimeFilesIfAvailable() {
    if (!updateEnabled) {
      SPDLOG_INFO("Runtime DLL update check disabled");
      return true;
    }

    if (Trim(updateSourceUrl).empty()) {
      SPDLOG_INFO("No runtime DLL update source configured; skipping update check");
      return true;
    }

    const std::vector<RuntimeFile> runtimeFiles = {
        {"GMP.dll", GetGMPDllPath()},
    };

    SPDLOG_INFO("Checking runtime DLL update source...");
    const auto probeChecksumBytes = DownloadUrl(BuildUpdateUrl(runtimeFiles.front().fileName + ".sha256"), 4096);
    if (!probeChecksumBytes) {
      SPDLOG_WARN("Could not reach runtime DLL update source; using local files if available");
      return true;
    }

    if (!UpdateRuntimeFile(runtimeFiles.front(), &*probeChecksumBytes)) {
      return false;
    }

    for (std::size_t i = 1; i < runtimeFiles.size(); ++i) {
      if (!UpdateRuntimeFile(runtimeFiles[i])) {
        return false;
      }
    }

    return true;
  }

public:
  GMPLauncher() {
    launcherDirectory = gmp::paths::ModulePath().parent_path();
    InitializeLogger();
    // Installed layout: Gothic2/Multiplayer/GMPLauncher.exe and Runtime/GMP.dll.
    const fs::path systemDirectory = launcherDirectory.parent_path() / L"System";
    gothicPath = WideToUtf8((systemDirectory / L"Gothic2.exe").wstring());
    gmpDllPath = WideToUtf8((launcherDirectory / L"Runtime" / L"GMP.dll").wstring());
    workingDirectory = WideToUtf8(systemDirectory.wstring());
    updateSourceUrl = GMP_UPDATE_SOURCE_URL;
  }

  bool ValidatePaths() {
    // Convert to wide strings for filesystem operations
    if (!fs::is_regular_file(Utf8ToWide(gothicPath))) {
      SPDLOG_ERROR("Gothic2.exe not found at: {}", gothicPath);
      return false;
    }

    if (!fs::is_regular_file(Utf8ToWide(gmpDllPath))) {
      SPDLOG_ERROR("GMP.dll not found at: {}", gmpDllPath);
      return false;
    }

    if (!fs::is_directory(Utf8ToWide(workingDirectory))) {
      SPDLOG_ERROR("Game working directory not found: {}", workingDirectory);
      return false;
    }

    return true;
  }

  bool IsExpectedRemoteModule(HANDLE process, DWORD moduleAddress, const fs::path& expectedPath) {
    std::vector<wchar_t> buffer(512);
    for (;;) {
      const DWORD length = GetModuleFileNameExW(process, reinterpret_cast<HMODULE>(static_cast<std::uintptr_t>(moduleAddress)),
                                              buffer.data(), static_cast<DWORD>(buffer.size()));
      if (length == 0) {
        return false;
      }
      if (length < buffer.size()) {
        const fs::path loadedPath(std::wstring(buffer.data(), length));
        std::error_code error;
        if (fs::equivalent(loadedPath, expectedPath, error)) {
          return true;
        }
        SPDLOG_ERROR("DLL resolved to an unexpected location: {}", WideToUtf8(loadedPath.wstring()));
        return false;
      }
      buffer.resize(buffer.size() * 2);
    }
  }

  bool ValidateDependencies() {
    SPDLOG_INFO("Validating GMP.dll dependencies...");

    const fs::path dllDirectory = GetGMPDllDirectory();
    bool allFound = true;
    for (const auto* dll : runtimeDlls) {
      fs::path dllPath = dllDirectory / dll;
      if (!fs::is_regular_file(dllPath)) {
        SPDLOG_ERROR("Required dependency not found: {}", WideToUtf8(dllPath.wstring()));
        allFound = false;
      } else {
        SPDLOG_INFO("Found: {}", WideToUtf8(dllPath.wstring()));
      }
    }

    if (!allFound) {
      SPDLOG_ERROR("Missing dependencies will cause injection to fail.");
      SPDLOG_ERROR("Install the required runtime DLLs alongside GMP.dll before launching.");
    } else {
      SPDLOG_INFO("All dependencies found!");
    }

    return allFound;
  }

  // Inject DLL while acting as the debugger - handles debug events to allow injection
  bool InjectDLL(HANDLE hProcess, DWORD processId, const fs::path& dllPath) {
    SPDLOG_INFO("Loading {} in Gothic2.exe...", WideToUtf8(dllPath.wstring()));

    // Both the launcher and Gothic are x86. Use the wide API for installation paths.
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (!hKernel32) {
      SPDLOG_ERROR("Failed to get handle to kernel32.dll");
      return false;
    }

    FARPROC pLoadLibraryW = GetProcAddress(hKernel32, "LoadLibraryW");
    if (!pLoadLibraryW) {
      SPDLOG_ERROR("Failed to get address of LoadLibraryW");
      return false;
    }

    // Allocate memory in the target process for the DLL path
    SIZE_T dllPathSize = (dllPath.native().size() + 1) * sizeof(wchar_t);
    LPVOID pDllPath = VirtualAllocEx(hProcess, nullptr, dllPathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pDllPath) {
      DWORD error = GetLastError();
      SPDLOG_ERROR("Failed to allocate memory in target process. Error: {}", error);
      return false;
    }

    // Write the DLL path to the allocated memory
    SIZE_T bytesWritten = 0;
    if (!WriteProcessMemory(hProcess, pDllPath, dllPath.c_str(), dllPathSize, &bytesWritten) || bytesWritten != dllPathSize) {
      DWORD error = GetLastError();
      SPDLOG_ERROR("Failed to write DLL path to target process. Error: {}", error);
      VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
      return false;
    }

    // Create a remote thread that calls LoadLibraryW with our absolute DLL path.
    HANDLE hRemoteThread = CreateRemoteThread(hProcess, nullptr, 0, (LPTHREAD_START_ROUTINE)pLoadLibraryW, pDllPath, 0, nullptr);

    if (!hRemoteThread) {
      DWORD error = GetLastError();
      SPDLOG_ERROR("Failed to create remote thread. Error: {}", error);
      VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
      return false;
    }

    SPDLOG_INFO("Remote thread created, pumping debug events...");

    // Pump debug events until the remote thread completes
    // This is necessary because we're the debugger - the process won't run without us
    DEBUG_EVENT debugEvent = {};
    bool injectionComplete = false;
    bool injectionSuccess = false;
    DWORD startTime = GetTickCount();
    const DWORD timeout = 10000;  // 10 seconds

    while (!injectionComplete) {
      if (GetTickCount() - startTime > timeout) {
        SPDLOG_ERROR("DLL injection timed out while pumping debug events");
        break;
      }

      if (WaitForDebugEvent(&debugEvent, 100)) {
        DWORD continueStatus = DBG_CONTINUE;

        switch (debugEvent.dwDebugEventCode) {
          case CREATE_PROCESS_DEBUG_EVENT:
            if (debugEvent.u.CreateProcessInfo.hFile) {
              CloseHandle(debugEvent.u.CreateProcessInfo.hFile);
            }
            break;

          case LOAD_DLL_DEBUG_EVENT:
            if (debugEvent.u.LoadDll.hFile) {
              CloseHandle(debugEvent.u.LoadDll.hFile);
            }
            break;

          case EXCEPTION_DEBUG_EVENT:
            // Consume the loader breakpoint; let Gothic handle other exceptions.
            if (!initialBreakpointHandled && debugEvent.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_BREAKPOINT) {
              initialBreakpointHandled = true;
            } else {
              continueStatus = DBG_EXCEPTION_NOT_HANDLED;
            }
            break;

          case EXIT_THREAD_DEBUG_EVENT:
            // Check if this is our injection thread completing
            // We can't easily match thread IDs, so just check if LoadLibrary is done
            break;

          case EXIT_PROCESS_DEBUG_EVENT:
            SPDLOG_ERROR("Target process exited during injection!");
            injectionComplete = true;
            break;

          default:
            // Handle other events (CREATE_THREAD, LOAD_DLL, etc.)
            break;
        }

        ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId, continueStatus);
      }

      // Check if injection thread has completed
      DWORD waitResult = WaitForSingleObject(hRemoteThread, 0);
      if (waitResult == WAIT_OBJECT_0) {
        injectionComplete = true;

        DWORD exitCode;
        if (GetExitCodeThread(hRemoteThread, &exitCode) && exitCode != 0 && IsExpectedRemoteModule(hProcess, exitCode, dllPath)) {
          SPDLOG_INFO("Loaded {} at 0x{:x}", WideToUtf8(dllPath.filename().wstring()), exitCode);
          injectionSuccess = true;
        } else {
          SPDLOG_ERROR("LoadLibraryW failed for {}. Check its dependencies and x86 architecture.", WideToUtf8(dllPath.wstring()));
        }
      }
    }

    CloseHandle(hRemoteThread);
    // On timeout the remote thread may still be reading its argument. The caller
    // terminates the failed child; leave the allocation alive until then.
    if (injectionComplete) {
      VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
    }

    return injectionSuccess;
  }

  bool LaunchAndInject() {
    SPDLOG_INFO("Starting Gothic2.exe...");

    // Convert UTF-8 strings to UTF-16 for Windows APIs
    std::wstring gothicPathWide = Utf8ToWide(gothicPath);
    std::wstring workingDirWide = Utf8ToWide(workingDirectory);

    // Create the command line (must be mutable for CreateProcess)
    std::wstring cmdLineWide = L"\"" + gothicPathWide + L"\"";

    STARTUPINFOW si = {sizeof(STARTUPINFOW)};
    PROCESS_INFORMATION pi = {};

    // Use DEBUG_ONLY_THIS_PROCESS | CREATE_SUSPENDED to:
    // 1. Prevent VS child process debugging from attaching first
    // 2. Keep the main thread suspended so we control when it runs
    DWORD creationFlags = DEBUG_ONLY_THIS_PROCESS | CREATE_SUSPENDED;

    BOOL success = CreateProcessW(gothicPathWide.c_str(),                     // Application name
                                  const_cast<wchar_t*>(cmdLineWide.c_str()),  // Command line (must be mutable)
                                  nullptr,                                    // Process security attributes
                                  nullptr,                                    // Thread security attributes
                                  FALSE,                                      // Inherit handles
                                  creationFlags,                              // We are the debugger initially
                                  nullptr,                                    // Environment
                                  workingDirWide.c_str(),                     // Current directory
                                  &si,                                        // Startup info
                                  &pi                                         // Process information
    );

    if (!success) {
      DWORD error = GetLastError();
      SPDLOG_ERROR("Failed to create Gothic2.exe process. Error code: {}", error);
      return false;
    }

    SPDLOG_INFO("Gothic2.exe created as debuggee. Process ID: {}", pi.dwProcessId);

    // Preload private imports by absolute path before GMP's import table is
    // resolved. This does not change Gothic's process-wide DLL search order.
    bool injectionSuccess = true;
    const fs::path runtimeDirectory = GetGMPDllDirectory();
    for (const auto* name : runtimeDlls) {
      const fs::path dependency = runtimeDirectory / name;
      if (!InjectDLL(pi.hProcess, pi.dwProcessId, dependency)) {
        injectionSuccess = false;
        break;
      }
    }
    if (injectionSuccess) {
      injectionSuccess = InjectDLL(pi.hProcess, pi.dwProcessId, GetGMPDllPath());
    }

    if (!injectionSuccess) {
      SPDLOG_ERROR("DLL injection failed. Terminating Gothic2.exe process.");
      TerminateProcess(pi.hProcess, 1);
      CloseHandle(pi.hThread);
      CloseHandle(pi.hProcess);
      return false;
    }

    SPDLOG_INFO("DLL injection completed successfully.");

    // Detach from being the debugger first - only one debugger can attach at a time
    SPDLOG_INFO("Detaching launcher debugger...");
    if (!DebugActiveProcessStop(pi.dwProcessId)) {
      DWORD error = GetLastError();
      SPDLOG_ERROR("Failed to detach debugger (error {})", error);
      TerminateProcess(pi.hProcess, 1);
      CloseHandle(pi.hThread);
      CloseHandle(pi.hProcess);
      return false;
    } else {
      SPDLOG_INFO("Debugger detached.");
    }

    // If --debug flag was passed, wait for user to attach external debugger (e.g., IDA)
    if (waitForDebugger) {
      SPDLOG_INFO("");
      SPDLOG_INFO("===============================================================");
      SPDLOG_INFO("  DEBUG MODE: Gothic2.exe with GMP.dll injected");
      SPDLOG_INFO("  Process ID: {}", pi.dwProcessId);
      SPDLOG_INFO("");
      SPDLOG_INFO("  The process is SUSPENDED. Attach debbuger now, then press ENTER.");
      SPDLOG_INFO("===============================================================");
      SPDLOG_INFO("");
      std::cin.get();
    }

    // Resume the main thread so the game starts running
    SPDLOG_INFO("Resuming Gothic2.exe...");
    if (ResumeThread(pi.hThread) == (DWORD)-1) {
      DWORD error = GetLastError();
      SPDLOG_ERROR("Failed to resume main thread. Error: {}", error);
      TerminateProcess(pi.hProcess, 1);
      CloseHandle(pi.hThread);
      CloseHandle(pi.hProcess);
      return false;
    }

    SPDLOG_INFO("Gothic2.exe running with GMP.dll injected!");

    // Monitor the process for a few seconds to ensure it doesn't crash immediately
    DWORD waitResult = WaitForSingleObject(pi.hProcess, 3000);  // Wait up to 3 seconds

    if (waitResult == WAIT_TIMEOUT) {
      // Process is still running after 3 seconds, which is good
      SPDLOG_INFO("Gothic2.exe startup successful!");
      CloseHandle(pi.hThread);
      CloseHandle(pi.hProcess);
      return true;
    } else if (waitResult == WAIT_OBJECT_0) {
      // Process terminated within 3 seconds - something went wrong
      DWORD exitCode;
      GetExitCodeProcess(pi.hProcess, &exitCode);

      SPDLOG_ERROR("Gothic2.exe terminated unexpectedly during startup!");
      SPDLOG_ERROR("Exit code: 0x{:x}", exitCode);

      if (exitCode == 0xC0000135) {
        SPDLOG_ERROR("This usually means a required DLL is missing.");
      } else if (exitCode == 0xC000007B) {
        SPDLOG_ERROR("STATUS_INVALID_IMAGE_FORMAT - Architecture mismatch or missing dependencies.");
      }

      CloseHandle(pi.hThread);
      CloseHandle(pi.hProcess);
      return false;
    } else {
      // Some other error occurred
      DWORD error = GetLastError();
      SPDLOG_ERROR("Error while monitoring process startup. Error code: {}", error);
      CloseHandle(pi.hThread);
      CloseHandle(pi.hProcess);
      return false;
    }
  }

  void ParseCommandLine(int argc, wchar_t* argv[]) {
    bool workdirSpecified = false;
    for (int i = 1; i < argc; i++) {
      std::wstring arg = argv[i];

      if (arg == L"--gothic" && i + 1 < argc) {
        gothicPath = WideToUtf8(argv[++i]);
      } else if (arg == L"--dll" && i + 1 < argc) {
        gmpDllPath = WideToUtf8(argv[++i]);
      } else if (arg == L"--workdir" && i + 1 < argc) {
        workingDirectory = WideToUtf8(argv[++i]);
        workdirSpecified = true;
      } else if (arg == L"--update-source" && i + 1 < argc) {
        updateSourceUrl = WideToUtf8(argv[++i]);
      } else if (arg == L"--no-update") {
        updateEnabled = false;
      } else if (arg == L"--debug" || arg == L"-d") {
        waitForDebugger = true;
      } else if (arg == L"--help" || arg == L"-h") {
        PrintHelp();
        exit(0);
      }
    }
    // Resolve overrides once against the invoking directory. The child's working
    // directory must not change the meaning of --dll or --gothic.
    gothicPath = WideToUtf8(fs::absolute(Utf8ToWide(gothicPath)).lexically_normal().wstring());
    gmpDllPath = WideToUtf8(fs::absolute(GetGMPDllPath()).lexically_normal().wstring());
    workingDirectory = WideToUtf8((workdirSpecified ? fs::absolute(Utf8ToWide(workingDirectory)).lexically_normal()
                                                  : fs::path(Utf8ToWide(gothicPath)).parent_path()).wstring());
  }

  void PrintHelp() {
    SPDLOG_INFO("GMP Launcher - Gothic Multiplayer DLL Injector");
    SPDLOG_INFO("");
    SPDLOG_INFO("Usage: GMPLauncher.exe [options]");
    SPDLOG_INFO("");
    SPDLOG_INFO("Options:");
    SPDLOG_INFO("  --gothic <path>   Path to Gothic2.exe (default: ../System/Gothic2.exe beside Multiplayer)");
    SPDLOG_INFO("  --dll <path>      Path to GMP.dll (default: Runtime/GMP.dll beside the launcher)");
    SPDLOG_INFO("  --workdir <path>  Working directory for Gothic2.exe (default: Gothic2.exe directory)");
    SPDLOG_INFO("                    Relative overrides are resolved against the invoking directory.");
    SPDLOG_INFO("  --update-source <url>");
    SPDLOG_INFO("                    Base HTTP(S) URL containing GMP.dll and its .sha256 file");
    SPDLOG_INFO("  --no-update       Skip the runtime DLL update check");
    SPDLOG_INFO("  --debug, -d       Pause after injection to attach IDA/debugger");
    SPDLOG_INFO("  --help, -h        Show this help message");
    SPDLOG_INFO("");
    SPDLOG_INFO("Example:");
    SPDLOG_INFO("  GMPLauncher.exe --gothic \"C:\\Gothic2\\System\\Gothic2.exe\" --dll \"C:\\GMP\\GMP.dll\"");
    SPDLOG_INFO("");
    SPDLOG_INFO("Debugging with IDA/debugger:");
    SPDLOG_INFO("  GMPLauncher.exe --debug");
    SPDLOG_INFO("  Attach IDA/debugger to the PID shown, then press ENTER to continue");
  }

  int Run(int argc, wchar_t* argv[]) {
    SPDLOG_INFO("Gothic Multiplayer Launcher v1.0");
    SPDLOG_INFO("");

    // Parse command line arguments
    ParseCommandLine(argc, argv);

    if (!UpdateRuntimeFilesIfAvailable()) {
      SPDLOG_ERROR("Runtime DLL update failed. Launch aborted.");
      return 1;
    }

    // Validate that required files exist
    if (!ValidatePaths()) {
      SPDLOG_ERROR("");
      SPDLOG_ERROR("Use --help for usage information.");
      return 1;
    }

    if (!ValidateDependencies()) {
      return 1;
    }

    // Launch Gothic2.exe with GMP.dll injection and monitor startup
    bool launchSuccess = LaunchAndInject();

    if (launchSuccess) {
      SPDLOG_INFO("Launch completed successfully! Gothic2.exe is running.");
    } else {
      SPDLOG_ERROR("Launch failed! Check the error messages above.");
      SPDLOG_ERROR("Common issues:");
      SPDLOG_ERROR("  - Missing Visual C++ Redistributables");
      SPDLOG_ERROR("  - Missing DirectX libraries");
      SPDLOG_ERROR("  - Antivirus blocking the injection");
      SPDLOG_ERROR("  - Incorrect file paths");
    }

    return launchSuccess ? 0 : 1;
  }
};

int wmain(int argc, wchar_t* argv[]) {
  try {
    GMPLauncher launcher;
    return launcher.Run(argc, argv);
  } catch (const std::exception& e) {
    SPDLOG_ERROR("Exception: {}", e.what());
    return 1;
  } catch (...) {
    SPDLOG_ERROR("Unknown exception occurred");
    return 1;
  }
}
