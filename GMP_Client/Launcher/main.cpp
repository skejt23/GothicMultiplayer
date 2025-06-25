
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
#include <tlhelp32.h>
// clang-format on

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace fs = std::filesystem;

class GMPLauncher {
private:
  std::string gothicPath;
  std::string gmpDllPath;
  std::string workingDirectory;
  std::shared_ptr<spdlog::logger> logger;

  void InitializeLogger() {
    try {
      // Create console sink with colors
      auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
      console_sink->set_level(spdlog::level::debug);
      console_sink->set_pattern("[%^%l%$] %v");

      // Create file sink
      auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("gmp_launcher.log", true);
      file_sink->set_level(spdlog::level::debug);
      file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

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
    if (utf8.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (size <= 0) return {};
    std::wstring result(size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &result[0], size);
    return result;
  }

  // Helper function to convert UTF-16 string to UTF-8
  std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &result[0], size, nullptr, nullptr);
    return result;
  }

public:
  GMPLauncher() {
    InitializeLogger();
    // Use UTF-8 encoding for current path
    workingDirectory = WideToUtf8(fs::current_path().wstring());

    // Default paths (can be overridden by command line)
    gothicPath = workingDirectory + "\\Gothic2.exe";
    gmpDllPath = workingDirectory + "\\GMP.dll";
  }

  bool ValidatePaths() {
    // Convert to wide strings for filesystem operations
    if (!fs::exists(Utf8ToWide(gothicPath))) {
      SPDLOG_ERROR("Gothic2.exe not found at: {}", gothicPath);
      return false;
    }

    if (!fs::exists(Utf8ToWide(gmpDllPath))) {
      SPDLOG_ERROR("GMP.dll not found at: {}", gmpDllPath);
      return false;
    }

    return true;
  }

  bool ValidateDependencies() {
    SPDLOG_INFO("Validating GMP.dll dependencies...");

    // Check for required dependencies in the working directory
    std::vector<std::string> requiredDlls = {"SDL3.dll", "BugTrap.dll", "InjectMage.dll"};

    bool allFound = true;
    for (const auto& dll : requiredDlls) {
      fs::path dllPath = fs::path(Utf8ToWide(workingDirectory)) / Utf8ToWide(dll);
      if (!fs::exists(dllPath)) {
        SPDLOG_WARN("Required dependency not found: {}", WideToUtf8(dllPath.wstring()));
        allFound = false;
      } else {
        SPDLOG_INFO("Found: {}", WideToUtf8(dllPath.wstring()));
      }
    }

    if (!allFound) {
      SPDLOG_WARN("Missing dependencies may cause injection to fail.");
      SPDLOG_WARN("Make sure all required DLL files are in the same directory as GMP.dll");
    } else {
      SPDLOG_INFO("All dependencies found!");
    }

    return allFound;
  }

  bool InjectDLLManual(HANDLE hProcess, const std::string& dllPath) {
    SPDLOG_INFO("Performing manual DLL injection...");

    // Get the address of LoadLibraryA in kernel32.dll
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (!hKernel32) {
      SPDLOG_ERROR("Failed to get handle to kernel32.dll");
      return false;
    }

    FARPROC pLoadLibraryA = GetProcAddress(hKernel32, "LoadLibraryA");
    if (!pLoadLibraryA) {
      SPDLOG_ERROR("Failed to get address of LoadLibraryA");
      return false;
    }

    // Allocate memory in the target process for the DLL path
    SIZE_T dllPathSize = dllPath.length() + 1;
    LPVOID pDllPath = VirtualAllocEx(hProcess, nullptr, dllPathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pDllPath) {
      DWORD error = GetLastError();
      SPDLOG_ERROR("Failed to allocate memory in target process. Error: {}", error);
      return false;
    }

    // Write the DLL path to the allocated memory
    SIZE_T bytesWritten;
    if (!WriteProcessMemory(hProcess, pDllPath, dllPath.c_str(), dllPathSize, &bytesWritten)) {
      DWORD error = GetLastError();
      SPDLOG_ERROR("Failed to write DLL path to target process. Error: {}", error);
      VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
      return false;
    }

    // Create a remote thread that calls LoadLibraryA with our DLL path
    HANDLE hRemoteThread = CreateRemoteThread(hProcess,                               // Target process handle
                                              nullptr,                                // Security attributes
                                              0,                                      // Stack size (default)
                                              (LPTHREAD_START_ROUTINE)pLoadLibraryA,  // Thread function (LoadLibraryA)
                                              pDllPath,                               // Parameter (DLL path)
                                              0,                                      // Creation flags
                                              nullptr                                 // Thread ID
    );

    if (!hRemoteThread) {
      DWORD error = GetLastError();
      SPDLOG_ERROR("Failed to create remote thread. Error: {}", error);
      VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
      return false;
    }

    SPDLOG_INFO("Remote thread created, waiting for DLL injection to complete...");

    // Wait for the remote thread to complete (LoadLibraryA to finish)
    DWORD waitResult = WaitForSingleObject(hRemoteThread, 10000);  // Wait up to 10 seconds

    if (waitResult == WAIT_TIMEOUT) {
      SPDLOG_ERROR("DLL injection timed out");
      TerminateThread(hRemoteThread, 0);
      CloseHandle(hRemoteThread);
      VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
      return false;
    } else if (waitResult != WAIT_OBJECT_0) {
      DWORD error = GetLastError();
      SPDLOG_ERROR("Error waiting for injection thread. Error: {}", error);
      CloseHandle(hRemoteThread);
      VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
      return false;
    }

    // Get the exit code of the remote thread (return value of LoadLibraryA)
    DWORD exitCode;
    if (!GetExitCodeThread(hRemoteThread, &exitCode)) {
      DWORD error = GetLastError();
      SPDLOG_ERROR("Failed to get injection thread exit code. Error: {}", error);
      CloseHandle(hRemoteThread);
      VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
      return false;
    }

    // Clean up
    CloseHandle(hRemoteThread);
    VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);

    if (exitCode == 0) {
      SPDLOG_ERROR("LoadLibraryA returned NULL - DLL injection failed");
      SPDLOG_ERROR("This usually means:");
      SPDLOG_ERROR("  1. The DLL file could not be found");
      SPDLOG_ERROR("  2. The DLL has missing dependencies");
      SPDLOG_ERROR("  3. The DLL's DllMain function returned FALSE");
      SPDLOG_ERROR("  4. Architecture mismatch (32-bit vs 64-bit)");
      return false;
    } else {
      SPDLOG_INFO("DLL injection successful! LoadLibraryA returned: 0x{:x}", exitCode);
      return true;
    }
  }

  bool LaunchAndInject() {
    SPDLOG_INFO("Starting Gothic2.exe in suspended mode...");

    // Convert UTF-8 strings to UTF-16 for Windows APIs
    std::wstring gothicPathWide = Utf8ToWide(gothicPath);
    std::wstring workingDirWide = Utf8ToWide(workingDirectory);
    
    // Create the command line (must be mutable for CreateProcess)
    std::wstring cmdLineWide = L"\"" + gothicPathWide + L"\"";

    // Create process in suspended state using Unicode API
    STARTUPINFOW si = {sizeof(STARTUPINFOW)};
    PROCESS_INFORMATION pi = {};

    BOOL success = CreateProcessW(gothicPathWide.c_str(),            // Application name
                                  const_cast<wchar_t*>(cmdLineWide.c_str()),  // Command line (must be mutable)
                                  nullptr,                           // Process security attributes
                                  nullptr,                           // Thread security attributes
                                  FALSE,                             // Inherit handles
                                  CREATE_SUSPENDED,                  // Creation flags - SUSPENDED!
                                  nullptr,                           // Environment
                                  workingDirWide.c_str(),           // Current directory
                                  &si,                              // Startup info
                                  &pi                               // Process information
    );

    if (!success) {
      DWORD error = GetLastError();
      SPDLOG_ERROR("Failed to create Gothic2.exe process. Error code: {}", error);
      return false;
    }

    SPDLOG_INFO("Gothic2.exe created in suspended mode. Process ID: {}", pi.dwProcessId);

    // Inject the DLL while the process is suspended
    bool injectionSuccess = InjectDLLManual(pi.hProcess, gmpDllPath);

    if (!injectionSuccess) {
      SPDLOG_ERROR("DLL injection failed. Terminating Gothic2.exe process.");
      TerminateProcess(pi.hProcess, 1);
      CloseHandle(pi.hThread);
      CloseHandle(pi.hProcess);
      return false;
    }

    SPDLOG_INFO("DLL injection completed successfully. Resuming Gothic2.exe...");

    // Resume the main thread to start execution
    if (ResumeThread(pi.hThread) == (DWORD)-1) {
      DWORD error = GetLastError();
      SPDLOG_ERROR("Failed to resume Gothic2.exe main thread. Error: {}", error);
      TerminateProcess(pi.hProcess, 1);
      CloseHandle(pi.hThread);
      CloseHandle(pi.hProcess);
      return false;
    }

    SPDLOG_INFO("Gothic2.exe resumed and running with GMP.dll injected!");

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
    for (int i = 1; i < argc; i++) {
      std::wstring arg = argv[i];

      if (arg == L"--gothic" && i + 1 < argc) {
        gothicPath = WideToUtf8(argv[++i]);
      } else if (arg == L"--dll" && i + 1 < argc) {
        gmpDllPath = WideToUtf8(argv[++i]);
      } else if (arg == L"--workdir" && i + 1 < argc) {
        workingDirectory = WideToUtf8(argv[++i]);
      } else if (arg == L"--help" || arg == L"-h") {
        PrintHelp();
        exit(0);
      }
    }
  }

  void PrintHelp() {
    SPDLOG_INFO("GMP Launcher - Gothic Multiplayer DLL Injector");
    SPDLOG_INFO("");
    SPDLOG_INFO("Usage: GMPLauncher.exe [options]");
    SPDLOG_INFO("");
    SPDLOG_INFO("Options:");
    SPDLOG_INFO("  --gothic <path>   Path to Gothic2.exe (default: Gothic2.exe in launcher directory)");
    SPDLOG_INFO("  --dll <path>      Path to GMP.dll (default: GMP.dll in launcher directory)");
    SPDLOG_INFO("  --workdir <path>  Working directory for Gothic2.exe (default: launcher directory)");
    SPDLOG_INFO("  --help, -h        Show this help message");
    SPDLOG_INFO("");
    SPDLOG_INFO("Example:");
    SPDLOG_INFO("  GMPLauncher.exe --gothic \"C:\\Gothic2\\Gothic2.exe\" --dll \"C:\\GMP\\GMP.dll\"");
  }

  int Run(int argc, wchar_t* argv[]) {
    SPDLOG_INFO("Gothic Multiplayer Launcher v1.0");
    SPDLOG_INFO("");

    // Parse command line arguments
    ParseCommandLine(argc, argv);

    // Validate that required files exist
    if (!ValidatePaths()) {
      SPDLOG_ERROR("");
      SPDLOG_ERROR("Use --help for usage information.");
      return 1;
    }

    // Check for dependencies (warning only, don't fail)
    ValidateDependencies();

    // Launch Gothic2.exe with GMP.dll injection and monitor startup
    bool launchSuccess = LaunchAndInject();

    if (launchSuccess) {
      SPDLOG_INFO("Launch completed successfully! Gothic2.exe is running.");
      SPDLOG_INFO("You can now close this launcher window.");
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
