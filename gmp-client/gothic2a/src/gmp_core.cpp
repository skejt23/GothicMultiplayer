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

#include "CIngame.h"
#include "main_menu.h"

// For now, maintain compatibility with existing global_ingame pattern
extern CIngame* global_ingame;

GMPCore::GMPCore() {
}

GMPCore::~GMPCore() {
  Shutdown();
}

void GMPCore::Initialize() {
  if (this->initialized_) {
    SPDLOG_WARN("GMPCore::Initialize called multiple times");
    return;
  }

  // Note: CMainMenu is still a TSingleton, we just reference it here
  // Future: migrate to owned unique_ptr when singleton pattern is removed
  this->mainMenu_.reset(CMainMenu::GetInstance());

  this->initialized_ = true;
}

void GMPCore::Shutdown() {
  if (!this->initialized_) {
    return;
  }

  SPDLOG_INFO("GMPCore shutting down...");

  // Release references (don't delete - CMainMenu is still singleton-managed)
  // Future: properly destroy when we own these
  this->ingame_.release();
  this->mainMenu_.release();

  // Clear any pending deferred actions
  while (!this->deferredActions_.empty()) {
    this->deferredActions_.pop();
  }

  this->initialized_ = false;
  SPDLOG_INFO("GMPCore shutdown complete");
}

void GMPCore::OnFrameStart() {
  // Execute all deferred actions at the start of the frame.
  // This runs on the main thread, BEFORE Gothic's rendering.
  // Safe for operations like ChangeLevel that invalidate world state.
  while (!this->deferredActions_.empty()) {
    auto action = std::move(this->deferredActions_.front());
    this->deferredActions_.pop();
    action();
  }
}

void GMPCore::DeferToNextFrame(std::function<void()> action) {
  this->deferredActions_.push(std::move(action));
  SPDLOG_DEBUG("GMPCore: Queued deferred action for next frame");
}

CIngame* GMPCore::CreateIngame() {
  if (this->ingame_) {
    SPDLOG_WARN("GMPCore::CreateIngame called but ingame already exists");
    return this->ingame_.get();
  }

  SPDLOG_INFO("GMPCore: Creating ingame handler");
  this->ingame_ = std::make_unique<CIngame>();
  return this->ingame_.get();
}

void GMPCore::DestroyIngame() {
  if (!this->ingame_) {
    return;
  }

  SPDLOG_INFO("GMPCore: Destroying ingame handler");
  this->ingame_.reset();
}
