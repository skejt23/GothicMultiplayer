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

#pragma once

#include <functional>
#include <memory>
#include <queue>

#include "singleton.h"

// Forward declarations
class CMainMenu;
class CIngame;

namespace Gothic_II_Addon {
class oCGame;
}

/**
 * @brief Central application core for Gothic Multiplayer client.
 *
 * GMPCore is the top-level owner of major subsystems and manages:
 * - Lifecycle of MainMenu, CIngame, and other major components
 * - Deferred actions that must execute outside the render loop
 * - Frame-level update processing
 *
 * This class provides proper ownership semantics instead of scattered
 * global pointers and singletons for core game components.
 */
class GMPCore : public TSingleton<GMPCore> {
public:
  GMPCore();
  ~GMPCore();

  /**
   * @brief Get singleton instance as reference (modern C++ style).
   */
  static GMPCore& Instance() {
    return *GetInstance();
  }

  /**
   * @brief Initialize the core systems. Called once during mod startup.
   */
  void Initialize();

  /**
   * @brief Shutdown and cleanup. Called during mod teardown.
   */
  void Shutdown();

  /**
   * @brief Called at the start of each frame, before Gothic rendering.
   *
   * This is the safe point to execute deferred actions like level changes
   * that would crash if done during rendering.
   */
  void OnFrameStart();

  /**
   * @brief Queue an action to execute at the start of the next frame.
   *
   * Use this for operations that invalidate Gothic engine state (like ChangeLevel)
   * which cannot safely be called during rendering or AI updates.
   *
   * @param action The action to execute. Will be called exactly once.
   */
  void DeferToNextFrame(std::function<void()> action);

  /**
   * @brief Get the main menu instance.
   * @return Pointer to main menu, or nullptr if not yet created.
   */
  CMainMenu* GetMainMenu() const {
    return mainMenu_.get();
  }

  /**
   * @brief Get the ingame instance.
   * @return Pointer to ingame handler, or nullptr if not in game.
   */
  CIngame* GetIngame() const {
    return ingame_.get();
  }

  /**
   * @brief Create and take ownership of the ingame handler.
   * @return Pointer to the newly created ingame handler.
   */
  CIngame* CreateIngame();

  /**
   * @brief Destroy the ingame handler (when returning to menu).
   */
  void DestroyIngame();

  /**
   * @brief Check if currently in an active game session.
   */
  bool IsInGame() const {
    return ingame_ != nullptr;
  }

private:
  std::unique_ptr<CMainMenu> mainMenu_;
  std::unique_ptr<CIngame> ingame_;

  std::queue<std::function<void()>> deferredActions_;

  bool initialized_ = false;
};
