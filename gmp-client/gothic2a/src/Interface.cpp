
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
#include "CMenu.h"
#include "HooksManager.h"
#include "gmp_core.h"
#include "keyboard.h"
#include "language.h"
#include "main_menu.h"
#include "mod.h"
#include "net_game.h"


CMenu* MainMenu;
std::vector<CMenu*> MenuList;
zCMenu* Options;
bool OrgOptionsOpened = false;
bool GMPMenuEnabled = true;
extern CIngame* global_ingame;
constexpr const char* GothicMP = "Gothic Multiplayer";

void ExitMainMenu() {
  // Miejsce na ewentualny kod
}

void ReturnToBigMainMenuAfterDisconnect() {
  if (player) {
    auto pos = player->trafoObjToWorld.GetTranslation();
    player->ResetPos(pos);
    player->RefreshNpc();
  }
  MainMenu = NULL;
  if (global_ingame) {
    delete global_ingame;
    global_ingame = nullptr;
  }
  CMainMenu::GetInstance()->ReLaunchMainMenu();
  HooksManager::GetInstance()->RemoveHook(HT_RENDER, (DWORD)InterfaceLoop);
  HooksManager::GetInstance()->RemoveHook(HT_RENDER, (DWORD)CIngame::Loop);
}

void ExitToBigMainMenu() {
  // Menu actions run during rendering; session teardown must wait until the
  // engine no longer holds the current frame's game session.
  GMPCore::Instance().DeferToNextFrame([]() {
    NetGame::Instance().Disconnect();
    ReturnToBigMainMenuAfterDisconnect();
  });
}

void ExitToMainmenu() {
  MainMenu->Open();
}
void ExitGameFromMainMenu() {
  GMPCore::ExitGame(0);
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
    if (zinput->KeyToggled(KEY_ESCAPE) && !OrgOptionsOpened && GMPMenuEnabled) {
      if (!MainMenu) {
        CreateMainMenu();
      } else {
        if (MainMenu->IsOpened()) {
          MainMenu->Close();
        } else {
          delete MainMenu;
          MainMenu = NULL;
          CreateMainMenu();
        }
      }
    }
  }
}

void EnableGMPMenu(bool enable) {
  GMPMenuEnabled = enable;
}

void OpenGMPMenu() {
  if (!MainMenu) {
    CreateMainMenu();
    return;
  }

  if (!MainMenu->IsOpened()) {
    MainMenu->Open();
  }
}

void CloseGMPMenu() {
  if (MainMenu && MainMenu->IsOpened()) {
    MainMenu->Close();
  }
}

void CreateMainMenu() {
  MainMenu = new CMenu(GothicMP, zCOLOR(0, 128, 128), 3500, 4000);  // MAIN-MENU
  MainMenu->AddMenuItem(Language::Instance()[Language::INGAMEM_BACKTOGAME], (DWORD)ExitMainMenu);
  MainMenu->AddMenuItem(Language::Instance()[Language::MMENU_OPTIONS], (DWORD)CreateOptionsMenu);
  MainMenu->AddMenuItem(Language::Instance()[Language::EXITTOMAINMENU], (DWORD)ExitToBigMainMenu);
  MainMenu->AddMenuItem(Language::Instance()[Language::MMENU_LEAVEGAME], (DWORD)ExitGameFromMainMenu);
};
