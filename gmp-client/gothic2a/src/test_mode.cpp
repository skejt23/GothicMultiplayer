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

#include "test_mode.h"

#include <spdlog/spdlog.h>

#include "CIngame.h"
#include "HooksManager.h"
#include "Interface.h"
#include "benchmark.h"
#include "config.h"
#include "gmp_core.h"
#include "patch.h"
#include "world_utils.hpp"

TestMode::TestMode(GMPCore& core) : core_(core), benchmark_(core) {
}

void TestMode::Initialize() {
  using namespace Gothic_II_Addon;

  const Config::TestModeConfig& testConfig = Config::Instance().GetTestModeConfig();

  // Always initialize benchmark subsystem (available via toggles)
  benchmark_.Initialize();

  SPDLOG_INFO("Initializing test mode: level='{}', spawn=({}, {}, {})", testConfig.level, testConfig.spawn_x, testConfig.spawn_y, testConfig.spawn_z);

  // Change level if needed (different from currently loaded world)
  if (!testConfig.level.empty()) {
    zSTRING levelName = testConfig.level.c_str();
    zSTRING currentWorld = ogame->GetGameWorld()->GetWorldFilename();

    if (currentWorld != levelName) {
      SPDLOG_INFO("Changing level from '{}' to '{}'", currentWorld.ToChar(), levelName.ToChar());
      Patch::ChangeLevelEnabled(true);
      ogame->ChangeLevel(levelName, zSTRING("????"));
      Patch::ChangeLevelEnabled(false);
    } else {
      SPDLOG_INFO("Level '{}' already loaded", levelName.ToChar());
    }
  }

  // Delete all NPCs and disable spawning (like in multiplayer)
  DeleteAllNpcsAndDisableSpawning();

  // Clean up world objects
  CleanupWorldObjects(ogame->GetGameWorld());

  // Defer the player spawn and interface enabling to next frame
  // This ensures Gothic's initialization scripts have finished running
  float spawnX = testConfig.spawn_x;
  float spawnY = testConfig.spawn_y;
  float spawnZ = testConfig.spawn_z;

  core_.DeferToNextFrame([this, spawnX, spawnY, spawnZ]() {
    SPDLOG_INFO("Deferred test mode spawn: setting position to ({}, {}, {})", spawnX, spawnY, spawnZ);

    // Set player position
    zVEC3 spawnPosition(spawnX, spawnY, spawnZ);
    if (player) {
      player->trafoObjToWorld.SetTranslation(spawnPosition);
      player->SetMovLock(0);  // Unlock player movement
      SPDLOG_INFO("Player spawned at ({}, {}, {})", spawnX, spawnY, spawnZ);
    }

    // Enable player interface
    Patch::PlayerInterfaceEnabled(true);

    // Initialize the custom GMP menu/interface
    // We create the Ingame handler and register the interface hook so ESC works
    // and brings up the GMP menu instead of the (disabled) Gothic menu.
    core_.CreateIngame();

    // Register hooks for UI and Interface
    // Note: casts to DWORD are required by the HookManager API
    HooksManager::GetInstance()->AddHook(HT_RENDER, (DWORD)InterfaceLoop);
    HooksManager::GetInstance()->AddHook(HT_RENDER, (DWORD)CIngame::Loop);

    // Add some starter items to the local player's inventory
    auto giveItem = [](oCNpc* npc, const char* name, int amount = 1, bool equip = false) {
      if (!npc)
        return;
      int index = zCParser::GetParser()->GetIndex(name);
      if (index >= 0) {
        oCItem* item = static_cast<oCItem*>(zfactory->CreateItem(index));
        if (item) {
          item->amount = amount;
          npc->inventory2.Insert(item);
          if (equip) {
            npc->Equip(item);
          }
          SPDLOG_INFO("Gave {} x {} to local player (Equip: {})", name, amount, equip);
        }
      } else {
        SPDLOG_WARN("Could not find item index for {}", name);
      }
    };

    giveItem(player, "ITMW_1H_VLK_SWORD");
    giveItem(player, "ITMW_2H_SLD_SWORD");  // Two-handed sword
    giveItem(player, "ITRW_BOW_L_01");
    giveItem(player, "ITRW_CROSSBOW_L_01");  // Crossbow
    giveItem(player, "ITRW_ARROW", 50);      // More arrows
    giveItem(player, "ITRW_BOLT", 30);       // Bolts for crossbow
    giveItem(player, "ITPO_HEALTH_01", 5);
    giveItem(player, "ITPO_HEALTH_02", 3);  // Medium health potions
    giveItem(player, "ITPO_MANA_01", 5);    // Mana potions
    giveItem(player, "ITFO_APPLE", 10);     // Food - apples
    giveItem(player, "ITFO_BREAD", 5);      // Food - bread
    giveItem(player, "ITFO_CHEESE", 3);     // Food - cheese
    giveItem(player, "ITMI_GOLD", 100);     // Gold
    giveItem(player, "ITAR_VLK_L", 1, true);

    SPDLOG_INFO("Test mode spawn complete");
  });

  SPDLOG_INFO("Test mode initialization complete (spawn deferred)");
}

void TestMode::OnFrame() {
  using namespace Gothic_II_Addon;

  // Run benchmark frame logic
  if (benchmark_.IsRunning()) {
    benchmark_.OnFrame();
  }

  // F8 to toggle benchmark
  if (zinput->KeyToggled(KEY_F8)) {
    benchmark_.Toggle();
  }
}
