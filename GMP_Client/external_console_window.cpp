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

#include "external_console_window.hpp"

#include <windows.h>

#include <memory>
#include <mutex>
#include <exception>

#include "CConfig.h"

namespace {
static std::unique_ptr<ExternalConsoleWindow> g_instance;
static std::once_flag g_once;
static HWND hwnd = nullptr;
void SaveConsoleConfigOnExit() {
  try {
    RECT rc{};
    if (::GetWindowRect(hwnd, &rc)) {
      auto *user_config = CConfig::GetInstance();
      user_config->SetConsolePosition({rc.left, rc.top});
      user_config->SaveConfigToFile();
    }

  } catch (const std::exception &ex) {
  }
}

}  // namespace

void ExternalConsoleWindow::Init() {
  std::call_once(g_once, [] { g_instance = std::unique_ptr<ExternalConsoleWindow>(new ExternalConsoleWindow()); });
}

ExternalConsoleWindow::ExternalConsoleWindow() {
  if (EnsureConsoleAvailable()) {
    RedirectStdStreamsToConsole();

    if (auto &opt_pos = CConfig::GetInstance()->GetConsolePosition(); opt_pos) {
      ::SetWindowPos(hwnd, nullptr, opt_pos->x, opt_pos->y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    std::atexit(SaveConsoleConfigOnExit);
  }
}

ExternalConsoleWindow::~ExternalConsoleWindow() {
  if (hwnd != nullptr) {
    ::FreeConsole();
  }
}

bool ExternalConsoleWindow::EnsureConsoleAvailable() {
  if (::AllocConsole()) {
    hwnd = ::GetConsoleWindow();
    return true;
  }
  return false;
}

void ExternalConsoleWindow::RedirectStdStreamsToConsole() {
  // Reopen stdout, stderr to console
  FILE *fp;
  // stdout
  freopen_s(&fp, "CONOUT$", "w", stdout);
  setvbuf(stdout, nullptr, _IONBF, 0);
  // stderr
  freopen_s(&fp, "CONOUT$", "w", stderr);
  setvbuf(stderr, nullptr, _IONBF, 0);

  // stdin (optional, but useful for interactive cases)
  freopen_s(&fp, "CONIN$", "r", stdin);
  setvbuf(stdin, nullptr, _IONBF, 0);
}
