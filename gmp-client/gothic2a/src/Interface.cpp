
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
#include "language.h"
#include "main_menu.h"
#include "CMenu.h"
#include "HooksManager.h"
#include "net_game.h"
#include "keyboard.h"
#include "mod.h"

bool HelpOpen = false;
CMenu* MainMenu;
std::vector<CMenu*> MenuList;
zCMenu* Options;
bool OrgOptionsOpened = false;
extern zCOLOR Red;
extern zCOLOR Normal;
constexpr const char* GothicMP = "Gothic Multiplayer";

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
  NetGame::Instance().Disconnect();
  CMainMenu::GetInstance()->ReLaunchMainMenu();
  HooksManager::GetInstance()->RemoveHook(HT_RENDER, (DWORD)InterfaceLoop);
  HooksManager::GetInstance()->RemoveHook(HT_RENDER, (DWORD)CIngame::Loop);
}

void ExitToMainmenu() {
  MainMenu->Open();
}
void ExitGameFromMainMenu() {
  NetGame::Instance().Disconnect();
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
  CreateMainMenu();
};

void InterfaceLoop(void) {
  if (HelpOpen) {
    screen->Print(2500, 2000, Language::Instance()[Language::HCONTROLS]);
    screen->Print(2500, 2200, Language::Instance()[Language::HCHAT]);
    screen->Print(2500, 2400, Language::Instance()[Language::HCHATMAIN]);
    screen->Print(2500, 2600, Language::Instance()[Language::HCHATWHISPER]);
    screen->Print(2500, 2800, Language::Instance()[Language::HPLAYERLIST]);
    screen->Print(2500, 3000, Language::Instance()[Language::HMAP]);
    screen->Print(2500, 3200, Language::Instance()[Language::HANIMSMENU]);
    screen->SetFontColor(Red);
    screen->Print(2500, 3400, Language::Instance()[Language::SHOWHOW]);
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
        CreateMainMenu();
      } else {
        if (MainMenu->IsOpened()) {
          MainMenu->Close();
        } else {
          delete MainMenu;
          MainMenu = NULL;
          if (HelpOpen) {
            HelpOpen = false;
          }
          CreateMainMenu();
        }
      }
    }
  }
}

void CreateMainMenu() {
  MainMenu = new CMenu(GothicMP, zCOLOR(0, 128, 128), 3500, 4000);  // MAIN-MENU
  MainMenu->AddMenuItem(Language::Instance()[Language::INGAMEM_BACKTOGAME], (DWORD)ExitMainMenu);
  MainMenu->AddMenuItem(Language::Instance()[Language::INGAMEM_HELP], (DWORD)CreateHelpMenu);
  MainMenu->AddMenuItem(Language::Instance()[Language::MMENU_OPTIONS], (DWORD)CreateOptionsMenu);
  MainMenu->AddMenuItem(Language::Instance()[Language::EXITTOMAINMENU], (DWORD)ExitToBigMainMenu);
  MainMenu->AddMenuItem(Language::Instance()[Language::MMENU_LEAVEGAME], (DWORD)ExitGameFromMainMenu);
};