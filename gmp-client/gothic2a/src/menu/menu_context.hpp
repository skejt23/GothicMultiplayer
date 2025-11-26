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

#include "CServerList.h"
#include "ExtendedServerList.h"
#include "ZenGin/zGothicAPI.h"
#include "config.h"
#include "language.h"
#include "menu/menu_scene.h"

namespace menu {

/**
 * @brief Shared context passed to all menu states
 *
 * This structure contains all the shared resources and references that
 * menu states need to access. Instead of using global variables or
 * singletons directly, states should use this context.
 *
 * This makes testing easier (can mock the context) and makes dependencies
 * explicit.
 */
struct MenuContext {
  // ===== Gothic Engine References =====
  oCGame* game = nullptr;
  oCNpc* player = nullptr;
  zCInput* input = nullptr;
  zCView* screen = nullptr;
  zCOption* options = nullptr;
  zCSoundSystem* soundSystem = nullptr;

  // ===== Menu Visual Elements =====
  zCView* logoView = nullptr;
  oCItem* titleWeapon = nullptr;
  oCItem* cameraWeapon = nullptr;

  // ===== Configuration & Data =====
  Config& config;
  Language& language;
  CServerList& serverList;

  // ===== Menu State =====
  zSTRING selectedServerIP;
  int selectedServerIndex = -1;
  bool titleWeaponEnabled = false;
  bool writingNickname = false;

  // ===== Extended Server List =====
  ExtendedServerList* extendedServerList = nullptr;

  // ===== Menu Scene (for FPS-independent animations) =====
  MenuScene scene;

  // ===== Player State Backup (for restoration) =====
  zVEC3 savedPlayerPosition;
  zVEC3 savedPlayerAngle;
  zVEC3 savedPlayerNormal;
  int savedHealthBarX = 0;
  int savedHealthBarY = 0;

  // ===== Constructor =====
  MenuContext(Config& cfg, Language& lang, CServerList& servers)
      : config(cfg), language(lang), serverList(servers), scene(ogame, nullptr) {
    // Initialize Gothic engine references
    game = ogame;
    player = Gothic_II_Addon::player;
    input = zinput;
    screen = Gothic_II_Addon::screen;
    options = zoptions;
    soundSystem = zsound;
  }

  // ===== Helper Methods =====

  /**
   * @brief Enable the health bar with previously saved dimensions
   */
  void EnableHealthBar() {
    if (game && game->hpBar) {
      game->hpBar->SetSize(savedHealthBarX, savedHealthBarY);
    }
  }

  /**
   * @brief Disable the health bar (save dimensions first)
   */
  void DisableHealthBar() {
    if (game && game->hpBar) {
      int currentX = 0;
      int currentY = 0;
      game->hpBar->GetSize(currentX, currentY);
      if (currentX != 0 || currentY != 0) {
        savedHealthBarX = currentX;
        savedHealthBarY = currentY;
      }
      game->hpBar->SetSize(0, 0);
    }
  }

  /**
   * @brief Insert the title weapon into the world if it is available
   */
  void ShowTitleWeapon() {
    if (!titleWeaponEnabled && titleWeapon) {
      titleWeaponEnabled = true;
      if (game && game->GetWorld()) {
        game->GetWorld()->AddVob(titleWeapon);
      }
      // Update scene to use this weapon
      scene.SetWeapon(titleWeapon);
    }
  }

  /**
   * @brief Remove the title weapon from the world if it is currently shown
   */
  void HideTitleWeapon() {
    if (titleWeaponEnabled && titleWeapon) {
      titleWeapon->RemoveVobFromWorld();
    }
    titleWeaponEnabled = false;
    // Clear weapon from scene
    scene.SetWeapon(nullptr);
  }

  // Destructor to clean up owned resources
  ~MenuContext() {
    if (extendedServerList) {
      delete extendedServerList;
      extendedServerList = nullptr;
    }
  }

  // Disable copy
  MenuContext(const MenuContext&) = delete;
  MenuContext& operator=(const MenuContext&) = delete;
};

}  // namespace menu
