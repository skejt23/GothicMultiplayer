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

#include "gmp_core.h"

#include <spdlog/spdlog.h>
#include <windows.h>

#include "CIngame.h"
#include "config.h"
#include "external_console_window.hpp"
#include "main_menu.h"
#include "mcp/mcp_pipe_handler.h"
#include "net_game.h"
#include "test_mode.h"

// For now, maintain compatibility with existing global_ingame pattern
extern CIngame* global_ingame;

GMPCore::GMPCore() {
}

GMPCore::~GMPCore() {
  Shutdown();
}

void GMPCore::Initialize() {
  if (initialized_) {
    SPDLOG_WARN("GMPCore::Initialize called multiple times");
    return;
  }

  if (Config::Instance().IsMCPPipeEnabled()) {
    SPDLOG_INFO("MCP Pipe enabled in config - starting server");
    gmp::mcp::MCPPipeHandler::Instance().Start();
  }

  // Check for test mode BEFORE creating the main menu
  if (Config::Instance().IsTestModeEnabled()) {
    SPDLOG_INFO("Test mode enabled - skipping main menu entirely");
    testMode_ = std::make_unique<TestMode>(*this);
    testMode_->Initialize();
    initialized_ = true;
    return;
  }

  // Normal mode: create the main menu
  // Note: CMainMenu is still a TSingleton, we just reference it here
  // Future: migrate to owned unique_ptr when singleton pattern is removed
  mainMenu_.reset(CMainMenu::GetInstance());

  initialized_ = true;
}

void GMPCore::Shutdown() {
  if (!initialized_) {
    return;
  }

  SPDLOG_INFO("GMPCore shutting down...");

  // Release references (don't delete - CMainMenu is still singleton-managed)
  // Future: properly destroy when we own these
  testMode_.reset();
  ingame_.release();
  mainMenu_.release();

  // Clear any pending deferred actions
  while (!deferredActions_.empty()) {
    deferredActions_.pop();
  }

  // Stop MCP Pipe Server
  if (Config::Instance().IsMCPPipeEnabled()) {
    gmp::mcp::MCPPipeHandler::Instance().Stop();
  }

  initialized_ = false;
  SPDLOG_INFO("GMPCore shutdown complete");
}

void GMPCore::OnFrameStart() {
  // Execute all deferred actions at the start of the frame.
  // This runs on the main thread, BEFORE Gothic's rendering.
  // Safe for operations like ChangeLevel that invalidate world state.
  while (!deferredActions_.empty()) {
    auto action = std::move(deferredActions_.front());
    deferredActions_.pop();
    action();
  }

  // Process MCP commands and state updates
  if (Config::Instance().IsMCPPipeEnabled()) {
    gmp::mcp::MCPPipeHandler::Instance().ProcessPendingMessages();
  }

  // Update TestMode (e.g. Benchmark)
  if (testMode_) {
    testMode_->OnFrame();
  }
}

void GMPCore::DeferToNextFrame(std::function<void()> action) {
  deferredActions_.push(std::move(action));
  SPDLOG_DEBUG("GMPCore: Queued deferred action for next frame");
}

CIngame* GMPCore::CreateIngame() {
  if (ingame_) {
    SPDLOG_WARN("GMPCore::CreateIngame called but ingame already exists");
    return ingame_.get();
  }

  SPDLOG_INFO("GMPCore: Creating ingame handler");
  ingame_ = std::make_unique<CIngame>();
  return ingame_.get();
}

void GMPCore::DestroyIngame() {
  if (!ingame_) {
    return;
  }

  SPDLOG_INFO("GMPCore: Destroying ingame handler");
  ingame_.reset();
}

void GMPCore::ExitGame(int exitCode) {
  SPDLOG_INFO("GMPCore::ExitGame called with exitCode={}", exitCode);

  // Save console window position before exit
  ExternalConsoleWindow::SavePosition();

  // Disconnect from server (quick operation, doesn't block)
  NetGame::Instance().Disconnect();

  if (Config::Instance().IsMCPPipeEnabled()) {
    gmp::mcp::MCPPipeHandler::Instance().Stop();
  }

  // Terminate the process immediately.
  // The current attempts at graceful shutdown sometimes hang or crash,
  // so for now we just exit immediately. TODO: revisit proper shutdown later.
  TerminateProcess(GetCurrentProcess(), static_cast<UINT>(exitCode));
}
