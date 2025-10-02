
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
** ** *	File name:		Interface/Interface.cpp		   							** *
*** *	Created by:		20/04/11	-	skejt23									** *
*** *	Description:	Interface loops and stuff	 							** *
***
*****************************************************************************/
#include "Interface.h"

#include "CIngame.h"
#include "CLanguage.h"
#include "CLocalPlayer.h"
#include "CMainMenu.h"
#include "CMenu.h"
#include "HooksManager.h"
#include "world-builder\CBuilder.h"
#include "game_client.h"
#include "keyboard.h"
#include "mod.h"

bool HelpOpen = false;
CMenu* MainMenu;
std::vector<CMenu*> MenuList;
zCMenu* Options;
constexpr const char* WriteMapName = "Write map name:";
constexpr const char* MAPSAVED = "Map successfully saved!";
constexpr const char* CANTSAVE = "Saving map failed!";
extern CBuilder* Builder;
char TextFFS[2] = {0, 0};
bool WritingMapSave = false;
bool OrgOptionsOpened = false;
extern CLanguage* Lang;
extern GameClient* client;
extern zCOLOR Red;
extern zCOLOR Normal;
extern CLocalPlayer* LocalPlayer;
constexpr const char* WorldBuilder = "World Builder";
constexpr const char* GothicMP = "Gothic Multiplayer";
bool InWorldBuilder = false;

// MENU FUNCTIONS
void CreateHelpMenu() {
  HelpOpen = true;
}

void ExitMainMenu() {
  // Miejsce na ewentualny kod
}

void ExitToBigMainMenu() {
  auto pos = player->trafoObjToWorld.GetTranslation();
  player->ResetPos(pos);
  player->RefreshNpc();
  MainMenu = NULL;
  client->Disconnect();
  CMainMenu::GetInstance()->ReLaunchMainMenu();
  HooksManager::GetInstance()->RemoveHook(HT_RENDER, (DWORD)InterfaceLoop);
  HooksManager::GetInstance()->RemoveHook(HT_RENDER, (DWORD)CIngame::Loop);
}

void ExitToMainmenu() {
  MainMenu->Open();
}
void ExitGameFromMainMenu() {
  client->Disconnect();
  gameMan->Done();
}
void CreateOptionsMenu() {
  if (MainMenu->IsOpened())
    MainMenu->Close();
  if (!Options)
    Options = zCMenu::Create(zSTRING("MENU_OPTIONS"));
  OrgOptionsOpened = true;
  Options->Run();
}
// END OF MENU FUNCTIONS

void LeaveOptionsMenu() {
  OrgOptionsOpened = false;
  zinput->ClearKeyBuffer();
  Options->Leave();
  gameMan->ApplySomeSettings();
  player->SetMovLock(1);
  CreateMainMenu(InWorldBuilder);
};

void InterfaceLoop(void) {
  if (InWorldBuilder)
    InWorldBuilder = false;
  if (HelpOpen) {
    screen->Print(2500, 2000, (*Lang)[CLanguage::HCONTROLS]);
    screen->Print(2500, 2200, (*Lang)[CLanguage::HCHAT]);
    screen->Print(2500, 2400, (*Lang)[CLanguage::HCHATMAIN]);
    screen->Print(2500, 2600, (*Lang)[CLanguage::HCHATWHISPER]);
    screen->Print(2500, 2800, (*Lang)[CLanguage::HPLAYERLIST]);
    screen->Print(2500, 3000, (*Lang)[CLanguage::HMAP]);
    screen->Print(2500, 3200, (*Lang)[CLanguage::HANIMSMENU]);
    screen->SetFontColor(Red);
    screen->Print(2500, 3400, (*Lang)[CLanguage::SHOWHOW]);
    screen->SetFontColor(Normal);
  }
  if (OrgOptionsOpened) {
    if (!player->IsMovLock())
      player->SetMovLock(1);
    if (memcmp("MENU_OPTIONS", zCMenu::GetActive()->GetName().ToChar(), 12) == 0 && zinput->KeyPressed(KEY_ESCAPE))
      LeaveOptionsMenu();
    if (memcmp("MENUITEM_OPT_BACK", Options->GetActiveItem()->GetName().ToChar(), 17) == 0 && zinput->KeyPressed(KEY_RETURN))
      LeaveOptionsMenu();
  }
  if (MenuList.size() > 0) {
    for (int i = 0; i < (int)MenuList.size(); i++) {
      if (MenuList[i]) {
        if (MenuList[i]->IsOpened())
          MenuList[i]->RenderMenu();
      }
    }
  }
  if (!player->inventory2.IsOpen()) {
    if (zinput->KeyToggled(KEY_ESCAPE) && !OrgOptionsOpened) {
      if (!MainMenu) {
        if (HelpOpen) {
          HelpOpen = false;
        }
        CreateMainMenu(false);
      } else {
        if (MainMenu->IsOpened()) {
          MainMenu->Close();
        } else {
          delete MainMenu;
          MainMenu = NULL;
          if (HelpOpen) {
            HelpOpen = false;
          }
          CreateMainMenu(false);
        }
      }
    }
  }
}

// WORLD BUILDER MENU FUNCTIONS
void SaveMap() {
  zinput->ClearKeyBuffer();
  WritingMapSave = true;
}
void ExitToBigMainMenuFromWB() {
  auto pos = player->trafoObjToWorld.GetTranslation();
  player->ResetPos(pos);
  player->RefreshNpc();
  MainMenu = NULL;
  HooksManager::GetInstance()->RemoveHook(HT_RENDER, (DWORD)RenderEvent);
  HooksManager::GetInstance()->RemoveHook(HT_RENDER, (DWORD)WorldBuilderInterface);
  delete Builder;
  Builder = NULL;
  CMainMenu::GetInstance()->ReLaunchMainMenu();
}
void ExitGameFromMainMenuWB() {
  gameMan->Done();
}
// WORLD BUILDER HELP
constexpr const char* H_CHOBJECT = "Q/E Change object";
constexpr const char* H_UPDOWN = "Z/X Down and up";
constexpr const char* H_ROTY = "NUMPAD 4/8/6/2 Rotations";
constexpr const char* H_ROTYRESET = "NUMPAD 0 Reset rotation";
constexpr const char* H_CAMDIS = "+/- Change camera distance";
constexpr const char* H_UNDO = "F1 - Undo";
constexpr const char* H_SPAWN = "NUMPAD ENTER/ KEY S - SPAWN OBJECT";
constexpr const char* H_SPAWNPLAYER = "G - Spawn player near object";
constexpr const char* H_TEST = "T - Launch test mode";
constexpr const char* H_SPEED = "NUMPAD 1/3 - Decrease/Increase moving speed";
constexpr const char* H_LEFTRIGHT = "DELETE/PAGEDOWN - Move left/right";
constexpr const char* H_COLLIDE = "END - Mob collision ON/OFF";
constexpr const char* H_OBJMENU = "F2 - Objects Menu ON/OFF";
constexpr const char* H_CHANGETYPE = "HOME - Change mob type";
constexpr const char* H_INOBJMENU = "In object menu : DELETE - erase vob, SPACE - Stop rotation";
// WB MENU INTERFACE
void WorldBuilderInterface(void) {
  static zSTRING MapNameTxt;
  if (!InWorldBuilder)
    InWorldBuilder = true;
  if (WritingMapSave) {
    TextFFS[0] = GInput::GetCharacterFormKeyboard();
    if ((TextFFS[0] == 8) && (MapNameTxt.Length() > 0))
      MapNameTxt.DeleteRight(1);
    if ((TextFFS[0] >= 0x20) && (MapNameTxt.Length() < 24))
      MapNameTxt += TextFFS;
    if ((TextFFS[0] == 0x0D) && (!MapNameTxt.IsEmpty())) {
      WritingMapSave = false;
      if (SaveWorld::SaveBuilderMap(Builder->SpawnedVobs, MapNameTxt.ToChar()))
        ogame->array_view[oCGame::GAME_VIEW_SCREEN]->PrintTimedCXY(MAPSAVED, 5000.0f, 0);
      else
        ogame->array_view[oCGame::GAME_VIEW_SCREEN]->PrintTimedCXY(CANTSAVE, 5000.0f, 0);
    }
    screen->PrintCX(3600, WriteMapName);
    screen->PrintCX(4000, MapNameTxt);
    if (zinput->KeyToggled(KEY_ESCAPE)) {
      WritingMapSave = false;
    }
  }
  if (HelpOpen) {
    screen->Print(2500, 2000, (*Lang)[CLanguage::HCONTROLS]);
    screen->Print(2500, 2200, H_CHOBJECT);
    screen->Print(2500, 2400, H_UPDOWN);
    screen->Print(2500, 2600, H_ROTY);
    screen->Print(2500, 2800, H_ROTYRESET);
    screen->Print(2500, 3000, H_CAMDIS);
    screen->Print(2500, 3200, H_UNDO);
    screen->Print(2500, 3400, H_SPAWN);
    screen->Print(2500, 3600, H_SPAWNPLAYER);
    screen->Print(2500, 3800, H_TEST);
    screen->Print(2500, 4000, H_SPEED);
    screen->Print(2500, 4200, H_LEFTRIGHT);
    screen->Print(2500, 4400, H_COLLIDE);
    screen->Print(2500, 4600, H_OBJMENU);
    screen->Print(2500, 4800, H_CHANGETYPE);
    screen->Print(2500, 5000, H_INOBJMENU);
  }
  if (OrgOptionsOpened) {
    if (!player->IsMovLock())
      player->SetMovLock(1);
    if (memcmp("MENU_OPTIONS", zCMenu::GetActive()->GetName().ToChar(), 12) == 0 && zinput->KeyPressed(KEY_ESCAPE))
      LeaveOptionsMenu();
    if (memcmp("MENUITEM_OPT_BACK", Options->GetActiveItem()->GetName().ToChar(), 17) == 0 && zinput->KeyPressed(KEY_RETURN))
      LeaveOptionsMenu();
  }
  if (MenuList.size() > 0) {
    for (int i = 0; i < (int)MenuList.size(); i++) {
      if (MenuList[i]) {
        if (MenuList[i]->IsOpened())
          MenuList[i]->RenderMenu();
      }
    }
  }
  if (!player->inventory2.IsOpen()) {
    if (zinput->KeyToggled(KEY_ESCAPE) && !OrgOptionsOpened && !WritingMapSave) {
      if (!MainMenu) {
        if (HelpOpen) {
          HelpOpen = false;
        }
        CreateMainMenu(true);
      } else {
        if (MainMenu->IsOpened()) {
          MainMenu->Close();
        } else {
          delete MainMenu;
          MainMenu = NULL;
          if (HelpOpen) {
            HelpOpen = false;
          }
          CreateMainMenu(true);
        }
      }
    }
  }
}

void CreateMainMenu(bool InWorldBuilder) {
  if (!InWorldBuilder) {
    MainMenu = new CMenu(GothicMP, zCOLOR(0, 128, 128), 3500, 4000);  // MAIN-MENU
    MainMenu->AddMenuItem((*Lang)[CLanguage::INGAMEM_BACKTOGAME], (DWORD)ExitMainMenu);
    MainMenu->AddMenuItem((*Lang)[CLanguage::INGAMEM_HELP], (DWORD)CreateHelpMenu);
    MainMenu->AddMenuItem((*Lang)[CLanguage::MMENU_OPTIONS], (DWORD)CreateOptionsMenu);
    MainMenu->AddMenuItem((*Lang)[CLanguage::EXITTOMAINMENU], (DWORD)ExitToBigMainMenu);
    MainMenu->AddMenuItem((*Lang)[CLanguage::MMENU_LEAVEGAME], (DWORD)ExitGameFromMainMenu);
  } else {
    MainMenu = new CMenu(WorldBuilder, zCOLOR(0, 128, 128), 3500, 4000);  // MAIN-MENU
    MainMenu->AddMenuItem((*Lang)[CLanguage::INGAMEM_BACKTOGAME], (DWORD)ExitMainMenu);
    MainMenu->AddMenuItem((*Lang)[CLanguage::WB_SAVEMAP], (DWORD)SaveMap);
    MainMenu->AddMenuItem((*Lang)[CLanguage::INGAMEM_HELP], (DWORD)CreateHelpMenu);
    MainMenu->AddMenuItem((*Lang)[CLanguage::MMENU_OPTIONS], (DWORD)CreateOptionsMenu);
    MainMenu->AddMenuItem((*Lang)[CLanguage::EXITTOMAINMENU], (DWORD)ExitToBigMainMenuFromWB);
    MainMenu->AddMenuItem((*Lang)[CLanguage::MMENU_LEAVEGAME], (DWORD)ExitGameFromMainMenuWB);
  }
};