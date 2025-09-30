
/*
MIT License

Copyright (c) 2022 Gothic Multiplayer Team (pampi, skejt23, mecio)

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

#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <thread>

#ifdef _WIN32
#include <Windows.h>
#endif

#include "game_server.h"

namespace {

std::atomic<bool> g_should_exit{false};

#ifdef _WIN32
BOOL WINAPI HandlerRoutine(DWORD dwCtrlType) {
  switch (dwCtrlType) {
    case CTRL_CLOSE_EVENT:
      SPDLOG_WARN("Warning! Please use CTRL + C instead of the close button to properly close the application.");
      std::this_thread::sleep_for(std::chrono::milliseconds(1500));
      [[fallthrough]];
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
      g_should_exit.store(true, std::memory_order_release);
      return TRUE;
    default:
      return FALSE;
  }
}

void RegisterSignalHandlers() {
  if (!SetConsoleCtrlHandler(HandlerRoutine, TRUE)) {
    SPDLOG_ERROR("Failed to register console control handler.");
  }
}
#else
void HandlerRoutine(int /*signal*/) {
  g_should_exit.store(true, std::memory_order_release);
}

void RegisterSignalHandlers() {
  std::signal(SIGINT, HandlerRoutine);
  std::signal(SIGTERM, HandlerRoutine);
}
#endif

}  // namespace


int main(int argc, char **argv) {
  srand(static_cast<unsigned int>(time(NULL)));

  RegisterSignalHandlers();

  GameServer serv;
  if (!serv.Init()) {
    SPDLOG_ERROR("Server initialization failed!");
    return 1;
  }

  while (!g_should_exit.load(std::memory_order_acquire)) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  SPDLOG_INFO("Shutting down server...");
  return 0;
}
