
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

/*****************************************************************************
** 	Created by:		06/06/11	-	skejt23
** 	Description:	Multiplayer main menu functionallity
**
*****************************************************************************/

#include "main_menu.h"

#include <spdlog/spdlog.h>
#include <urlmon.h>

#include <fstream>
#include <nlohmann/json.hpp>

#include "CSyncFuncs.h"
#include "ExtendedServerList.h"
#include "config.h"
#include "content/content_transition_manager.h"
#include "interface.h"
#include "keyboard.h"
#include "language.h"
#include "localization_utils.h"
#include "mod.h"
#include "net_game.h"
#include "patch.h"
#include "version.h"
#include "world_utils.hpp"

// New menu state machine includes
#include "menu/menu_context.hpp"
#include "menu/menu_state_machine.hpp"
#include "menu/states/enter_menu_state.hpp"

using namespace Gothic_II_Addon;

extern float fWRatio, fHRatio;
zCOLOR Normal = zCOLOR(255, 255, 255);
zCOLOR Highlighted = zCOLOR(128, 180, 128);
zCOLOR Red = zCOLOR(0xFF, 0, 0);

CMainMenu::CMainMenu() {
  player->SetMovLock(1);
  Patch::PlayerInterfaceEnabled(false);
  RECT wymiary;
  GetWindowRect(Patch::GetHWND(), &wymiary);
  fWRatio = 1280.0f / (float)wymiary.right;  // zalozenie jest takie ze szerokosc jest dopasowywana weddug szerokosci 1280
  fHRatio = 1024.0f / (float)wymiary.top;

  ogame->GetWorldTimer()->GetTime(Hour, Minute);
  HooksManager* hm = HooksManager::GetInstance();
  hm->AddHook(HT_RENDER, (DWORD)MainMenuLoop);
  hm->RemoveHook(HT_AIMOVING, (DWORD)Initialize);

  // Save player state for later restoration
  HeroPos = player->GetPositionWorld();
  Angle = player->trafoObjToWorld.GetAtVector();
  NAngle = player->trafoObjToWorld.GetRightVector();

  // Initialize new state machine system
  InitializeStateMachine();
}

void CMainMenu::InitializeStateMachine() {
  SPDLOG_INFO("Initializing menu state machine");

  // States borrow the context. Destroy them before replacing it on re-entry.
  if (stateMachine_) {
    stateMachine_->Clear();
    stateMachine_.reset();
  }
  menuContext_.reset();

  // Create menu context with all shared resources
  menuContext_ = std::make_unique<menu::MenuContext>(Config::Instance(), Language::Instance(), server_list_);

  // Initialize context with current menu state
  menuContext_->game = ogame;
  menuContext_->player = player;
  menuContext_->input = zinput;
  menuContext_->screen = screen;
  menuContext_->options = zoptions;

  // Menu visual elements - logo will be created by EnterMenuState
  menuContext_->logoView = nullptr;

  // Player state backup
  menuContext_->savedPlayerPosition = HeroPos;
  menuContext_->savedPlayerAngle = Angle;
  menuContext_->savedPlayerNormal = NAngle;

  // Create extended server list
  menuContext_->extendedServerList = new ExtendedServerList(server_list_);

  // Prepare player and HUD before creating state machine
  // This avoids calling GetInstance() from within the constructor
  PrepareForMenuEntry();

  // Create state machine
  stateMachine_ = std::make_unique<menu::MenuStateMachine>();

  // Determine initial flow and let the bootstrap state handle setup
  menu::states::EnterMenuState::InitialFlow initialFlow = menu::states::EnterMenuState::InitialFlow::MainMenu;
  if (Config::Instance().IsDefault()) {
    SPDLOG_INFO("First launch detected, starting with language selection");
    initialFlow = menu::states::EnterMenuState::InitialFlow::ChooseLanguage;
  } else {
    SPDLOG_INFO("Config exists, starting with main menu");
  }

  stateMachine_->SetState(std::make_unique<menu::states::EnterMenuState>(*menuContext_, initialFlow));

  SPDLOG_INFO("Menu state machine initialized successfully");
}

CMainMenu::~CMainMenu() {
  // Clean up state machine (this will also clean up extendedServerList via context)
  if (stateMachine_) {
    SPDLOG_INFO("Cleaning up menu state machine");
    stateMachine_->Clear();
    stateMachine_.reset();
  }
  menuContext_.reset();
}

void CMainMenu::PrepareForMenuEntry() {
  PreparePlayerForMenuReentry();

  // Use the native HUD gate; hiding a bar must not destroy its dimensions.
  if (ogame)
    ogame->SetShowPlayerStatus(FALSE);
}

void __stdcall CMainMenu::ReLaunchMenuCallback() {
  HooksManager::GetInstance()->RemoveHook(HT_AIMOVING, (DWORD)CMainMenu::ReLaunchMenuCallback);

  CMainMenu* menu = CMainMenu::GetInstance();
  if (!menu) {
    SPDLOG_ERROR("ReLaunchMenuCallback invoked without CMainMenu instance");
    return;
  }
  menu->PrepareForMenuEntry();

  HooksManager::GetInstance()->AddHook(HT_RENDER, (DWORD)CMainMenu::MainMenuLoop);

  // Re-initialize state machine for menu re-entry
  menu->InitializeStateMachine();
}

void CMainMenu::ReLaunchMainMenu() {
  StopMenuScenes();
  zinput->ClearKeyBuffer();
  PrepareWorldChange();
  const bool reload_base_world = NetGame::Instance().ConsumeBaseWorldReloadRequest();
  if (reload_base_world) {
    SPDLOG_INFO("Recreating the base Gothic game session after addon disconnect");
    std::string error;
    auto* content = NetGame::Instance().content_transition_manager.get();
    if (!content || !content->DeactivateServerContent(error)) {
      SPDLOG_ERROR("Failed to restore the base Gothic game session: {}", error);
      return;
    }
    constexpr int kNewGameSlot = -2;
    gmp::gothic::PrepareDisconnectLoadingScreen();
    ogame->LoadGame(kNewGameSlot, zSTRING("NEWWORLD\\NEWWORLD.ZEN"));
  } else if (ogame && ogame->GetGameWorld() &&
             !memcmp("NEWWORLD\\NEWWORLD.ZEN", ogame->GetGameWorld()->GetWorldFilename().ToChar(), 21)) {
  }  // jest mapa
  else {
    if (!ogame || !ogame->GetGameWorld()) {
      SPDLOG_ERROR("Cannot reload the base world because the Gothic game session is unavailable");
      return;
    }
    gmp::gothic::PrepareDisconnectLoadingScreen();
    Patch::ChangeLevelEnabled(true);
    ogame->ChangeLevel("NEWWORLD\\NEWWORLD.ZEN", zSTRING("????"));
    Patch::ChangeLevelEnabled(false);
  }

  if (!ogame || !ogame->GetGameWorld() || !player) {
    SPDLOG_ERROR("Cannot relaunch the GMP menu because the base world or hero is unavailable");
    return;
  }
  ResetWorldRenderState();
  HeroPos = player->GetPositionWorld();
  Angle = player->trafoObjToWorld.GetAtVector();
  NAngle = player->trafoObjToWorld.GetRightVector();
  HooksManager::GetInstance()->AddHook(HT_AIMOVING, (DWORD)CMainMenu::ReLaunchMenuCallback);
}

void CMainMenu::PreparePlayerForMenuReentry() {
  ClearNpcTalents(player);
  player->SetMovLock(1);
  Patch::PlayerInterfaceEnabled(false);

  if (auto* equippedArmor = player->GetEquippedArmor()) {
    player->UnequipItem(equippedArmor);
  }

  if (auto* rangedWeapon = player->GetEquippedRangedWeapon()) {
    player->UnequipItem(rangedWeapon);
  }

  if (auto* meleeWeapon = player->GetEquippedMeleeWeapon()) {
    player->UnequipItem(meleeWeapon);
  }

  zCVob* rightHand = player->GetRightHand();
  zCVob* leftHand = player->GetLeftHand();
  if (rightHand) {
    zSTRING slot(NPC_NODE_RIGHTHAND);
    player->RemoveFromSlot(slot, 0, 1);
    rightHand->RemoveVobFromWorld();
  }
  if (leftHand && leftHand != rightHand) {
    zSTRING slot(NPC_NODE_LEFTHAND);
    player->RemoveFromSlot(slot, 0, 1);
    leftHand->RemoveVobFromWorld();
  }

  player->SetWeaponMode2(NPC_WEAPON_NONE);
  oCNpcFocus::SetFocusMode(FOCUS_NORMAL);
  player->human_ai->StartStandAni();
  player->inventory2.ClearInventory();
}

void CMainMenu::ClearNpcTalents(oCNpc* Npc) {
  Npc->inventory2.ClearInventory();
  Npc->DestroySpellBook();
  Npc->MakeSpellBook();
  for (int i = 0; i < 21; i++) {
    Npc->SetTalentSkill(i, 0);
    Npc->SetTalentValue(i, 0);
  }
}

void __stdcall CMainMenu::MainMenuLoop() {
  CMainMenu::GetInstance()->RenderMenu();
}

void CMainMenu::RenderMenu() {
  if (!stateMachine_) {
    return;
  }

  // Actors have already moved before the world render. Project their labels
  // here so names and the rendered models use the same camera/pose.
  if (menuContext_) {
    menuContext_->sceneManager.Render();
  }

  // Run state machine update
  bool shouldContinue = stateMachine_->Update();

  // Check if state machine has completed (no more states)
  if (!shouldContinue || !stateMachine_->HasActiveState()) {
    SPDLOG_INFO("State machine completed, cleaning up...");

    // Clear the state machine (calls OnExit on current state if any)
    stateMachine_->Clear();
    stateMachine_.reset();
    menuContext_.reset();

    // Remove the render hook to stop menu rendering
    // This is safe now because we're returning from RenderMenu()
    HooksManager::GetInstance()->RemoveHook(HT_RENDER, (DWORD)CMainMenu::MainMenuLoop);

    SPDLOG_INFO("Menu shutdown complete");
  }
}

void CMainMenu::StopMenuScenes() {
  // GetInstance() would create the menu during startup/shutdown callbacks.
  if (Instance && Instance->menuContext_) {
    if (Instance->stateMachine_) {
      Instance->stateMachine_->StopScene();
    }
    Instance->menuContext_->sceneManager.StopScene();
  }
}

void CMainMenu::UpdateMenuScene() {
  if (Instance && Instance->stateMachine_ && Instance->menuContext_) {
    Instance->menuContext_->sceneManager.Update(ztimer ? ztimer->frameTimeFloatSecs : 0.0f);
    Instance->stateMachine_->UpdateScene(ztimer ? ztimer->frameTimeFloatSecs : 0.0f);
  }
}
