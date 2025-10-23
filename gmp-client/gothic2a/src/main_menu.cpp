
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
#include "CWatch.h"
#include "ExtendedServerList.h"
#include "config.h"
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
#include "menu/states/choose_language_state.hpp"
#include "menu/states/choose_nickname_state.hpp"
#include "menu/states/main_menu_loop_state.hpp"

using namespace Gothic_II_Addon;

std::vector<zSTRING> vec_choose_lang;
std::vector<std::string> vec_lang_files;
extern const char* LANG_DIR;
extern float fWRatio, fHRatio;
zCOLOR Normal = zCOLOR(255, 255, 255);
zCOLOR Highlighted = zCOLOR(128, 180, 128);
zCOLOR Red = zCOLOR(0xFF, 0, 0);
extern zCOLOR Green;
zCOLOR FColor;
constexpr const char* FDefault = "FONT_DEFAULT.TGA";
constexpr const char* WalkAnim = "S_WALKL";

char x[2] = {0, 0};

CMainMenu::CMainMenu() {
  player->SetMovLock(1);
  Patch::PlayerInterfaceEnabled(false);
  LoadConfig();
  hbX = 0;
  hbY = 0;
  RECT wymiary;
  GetWindowRect(Patch::GetHWND(), &wymiary);
  fWRatio = 1280.0f / (float)wymiary.right;  // zalozenie jest takie ze szerokosc jest dopasowywana weddug szerokosci 1280
  fHRatio = 1024.0f / (float)wymiary.top;

  // Disable health bar during menu
  ogame->hpBar->GetSize(hbX, hbY);
  ogame->hpBar->SetSize(0, 0);

  // Create logo view (will be shown by state machine)
  GMPLogo = new zCView(0, 0, 8192, 8192, VIEW_ITEM);
  GMPLogo->SetPos(4000, 200);
  GMPLogo->InsertBack(zSTRING("GMP_LOGO_MENU.TGA"));
  GMPLogo->SetSize(5500, 2000);

  TitleWeaponEnabled = false;
  esl = NULL;

  oCSpawnManager::SetRemoveRange(2097152.0f);
  LaunchMenuScene();
  ogame->GetWorldTimer()->GetTime(Hour, Minute);
  HooksManager* hm = HooksManager::GetInstance();
  hm->AddHook(HT_RENDER, (DWORD)MainMenuLoop, false);
  hm->RemoveHook(HT_AIMOVING, (DWORD)Initialize);
  HeroPos = player->GetPositionWorld();
  Angle = player->trafoObjToWorld.GetAtVector();
  NAngle = player->trafoObjToWorld.GetRightVector();
  ClearNpcTalents(player);

  // Initialize new state machine system
  InitializeStateMachine();
}

void CMainMenu::InitializeStateMachine() {
  SPDLOG_INFO("Initializing menu state machine");

  // Create menu context with all shared resources
  menuContext_ = std::make_unique<menu::MenuContext>(Config::Instance(), Language::Instance(), server_list_);

  // Initialize context with current menu state
  menuContext_->game = ogame;
  menuContext_->player = player;
  menuContext_->input = zinput;
  menuContext_->screen = screen;
  menuContext_->options = zoptions;
  menuContext_->soundSystem = zsound;

  // Menu visual elements (initialized in main constructor)
  menuContext_->logoView = GMPLogo;
  menuContext_->titleWeapon = TitleWeapon;
  menuContext_->cameraWeapon = CamWeapon;
  menuContext_->titleWeaponEnabled = TitleWeaponEnabled;

  // Player state backup
  menuContext_->savedPlayerPosition = HeroPos;
  menuContext_->savedPlayerAngle = Angle;
  menuContext_->savedPlayerNormal = NAngle;
  menuContext_->savedHealthBarX = hbX;
  menuContext_->savedHealthBarY = hbY;

  // Create extended server list
  if (!esl) {
    esl = new ExtendedServerList(server_list_);
  }
  menuContext_->extendedServerList = esl;

  // Create state machine
  stateMachine_ = std::make_unique<menu::MenuStateMachine>();

  // Determine initial state based on config
  if (Config::Instance().IsDefault()) {
    // First launch - start with language selection
    SPDLOG_INFO("First launch detected, starting with language selection");
    stateMachine_->SetState(std::make_unique<menu::states::ChooseLanguageState>(*menuContext_));
  } else {
    // Normal launch - go straight to main menu
    SPDLOG_INFO("Config exists, starting with main menu");
    stateMachine_->SetState(std::make_unique<menu::states::MainMenuLoopState>(*menuContext_));
  }

  SPDLOG_INFO("Menu state machine initialized successfully");
}

CMainMenu::~CMainMenu() {
  // Clean up state machine
  if (stateMachine_) {
    SPDLOG_INFO("Cleaning up menu state machine");
    stateMachine_->Clear();
    stateMachine_.reset();
  }
  menuContext_.reset();

  // Clean up legacy system
  delete esl;
}

static void ReLaunchPart2() {
  HooksManager::GetInstance()->RemoveHook(HT_AIMOVING, (DWORD)ReLaunchPart2);
  CMainMenu* ReMenu = CMainMenu::GetInstance();
  ReMenu->ClearNpcTalents(player);
  player->SetMovLock(1);
  Patch::PlayerInterfaceEnabled(false);
  if (player->GetEquippedArmor())
    player->UnequipItem(player->GetEquippedArmor());
  if (player->GetEquippedRangedWeapon())
    player->UnequipItem(player->GetEquippedRangedWeapon());
  if (player->GetEquippedMeleeWeapon())
    player->UnequipItem(player->GetEquippedMeleeWeapon());
  if (player->GetRightHand()) {
    zCVob* Ptr = player->GetRightHand();
    zCVob* PtrLeft = player->GetLeftHand();
    player->DropAllInHand();
    if (PtrLeft)
      PtrLeft->RemoveVobFromWorld();
    Ptr->RemoveVobFromWorld();
  }
  player->GetModel()->StartAnimation("S_RUN");
  player->SetWeaponMode(NPC_WEAPON_NONE);
  player->inventory2.ClearInventory();

  // Disable health bar during menu
  ogame->hpBar->GetSize(ReMenu->hbX, ReMenu->hbY);
  ogame->hpBar->SetSize(0, 0);

  // Create logo view (will be shown by state machine)
  ReMenu->GMPLogo = new zCView(0, 0, 8192, 8192, VIEW_ITEM);
  ReMenu->GMPLogo->SetPos(4000, 200);
  ReMenu->GMPLogo->InsertBack(zSTRING("GMP_LOGO_MENU.TGA"));
  ReMenu->GMPLogo->SetSize(5500, 2000);

  HooksManager::GetInstance()->AddHook(HT_RENDER, (DWORD)CMainMenu::MainMenuLoop, false);
  ReMenu->TitleWeaponEnabled = false;
  ReMenu->LaunchMenuScene();

  // Re-initialize state machine for menu re-entry
  ReMenu->InitializeStateMachine();
};

void CMainMenu::ReLaunchMainMenu() {
  zinput->ClearKeyBuffer();
  if (!memcmp("NEWWORLD\\NEWWORLD.ZEN", ogame->GetGameWorld()->GetWorldFilename().ToChar(), 21)) {
  }  // jest mapa
  else {
    Patch::ChangeLevelEnabled(true);
    ogame->ChangeLevel("NEWWORLD\\NEWWORLD.ZEN", zSTRING("????"));
    Patch::ChangeLevelEnabled(false);
  }
  HooksManager::GetInstance()->AddHook(HT_AIMOVING, (DWORD)ReLaunchPart2, false);
}

void CMainMenu::LaunchMenuScene() {
  // Just a placeholder weapon for the camera (the actual object is not relevant)
  CamWeapon = zfactory->CreateItem(zCParser::GetParser()->GetIndex("ItMw_1h_Mil_Sword"));
  CamWeapon->name.Clear();
  CamWeapon->SetPositionWorld(zVEC3((float)13354.502930, 2040.0, (float)-1141.678467));
  CamWeapon->RotateWorldY(-150);
  ogame->CamInit(CamWeapon, zCCamera::activeCam);
  TitleWeapon = zfactory->CreateItem(zCParser::GetParser()->GetIndex("ItMw_1H_Blessed_03"));
  TitleWeapon->SetPositionWorld(zVEC3((float)13346.502930, 2006.0, (float)-1240.678467));
};

void CMainMenu::LoadConfig() {
  if (!Config::Instance().IsDefault()) {
    auto head_model = Gothic2APlayer::GetHeadModelNameFromByte(Config::Instance().headmodel);
    auto walkstyle = Gothic2APlayer::GetWalkStyleFromByte(Config::Instance().walkstyle);
    player->SetAdditionalVisuals("HUM_BODY_NAKED0", Config::Instance().skintexture, 0, head_model, Config::Instance().facetexture, 0, -1);
    player->ApplyOverlay(walkstyle);
  }
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

  // Reset player position if needed
  if (player->GetPositionWorld()[VX] != HeroPos[VX]) {
    player->trafoObjToWorld.SetTranslation(HeroPos);
    player->trafoObjToWorld.SetAtVector(Angle);
    player->trafoObjToWorld.SetRightVector(NAngle);
  }

  // Sync menu context with current state (bidirectional)
  menuContext_->titleWeapon = TitleWeapon;
  // Let states control titleWeaponEnabled - don't overwrite it
  // menuContext_->titleWeaponEnabled = TitleWeaponEnabled;

  // Sync back from context (states can modify this)
  TitleWeaponEnabled = menuContext_->titleWeaponEnabled;

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
