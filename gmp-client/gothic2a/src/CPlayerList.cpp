
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

#include "CPlayerList.h"

#include "CChat.h"
#include "CIngame.h"
#include "game_client.h"

extern GameClient* client;
extern CLanguage* Lang;
extern zCOLOR Normal;
extern zCOLOR Highlighted;
extern CIngame* global_ingame;
zCOLOR FColors1;

CPlayerList::CPlayerList() {
  PlayerListBackground = new zCView(0, 0, 8192, 8192, VIEW_ITEM);
  PlayerListBackground->SetPos(2500, 2000);
  x = 2500, y = 2200;
  MenuPos = 1, PrintTo = 0, PrintFrom = 1;
  Opened = false;
  PlayerOptions = false;
  PlayerListBackground->SetSize(3500, 4000);
  PlayerListBackground->InsertBack(zSTRING("MENU_INGAME.TGA"));
};

CPlayerList::~CPlayerList() {
  delete PlayerListBackground;
};

bool CPlayerList::IsPlayerListOpen() {
  return Opened;
};

bool CPlayerList::OpenPlayerList() {
  if (!Opened) {
    screen->InsertItem(PlayerListBackground);
    player->GetAnictrl()->StopTurnAnis();
    MenuPos = 1, PrintTo = 0, PrintFrom = 1;
    Opened = true;
    return true;
  }
  return false;
};

bool CPlayerList::ClosePlayerList() {
  if (Opened) {
    ChPlayerNpc = NULL;
    screen->RemoveItem(PlayerListBackground);
    player->SetMovLock(0);
    PlayerOptions = false;
    Opened = false;
    return true;
  }
  return false;
};

void CPlayerList::RunPlayerListItem() {
  std::string buffer;
  switch (MenuPos) {
    case 0:
      MenuPos = 1;
      PlayerOptions = false;
      break;
    case 1:
      global_ingame->WhisperingTo = ChosenPlayer.ToChar();
      CChat::GetInstance()->SetWhisperTo(global_ingame->WhisperingTo);
      CChat::GetInstance()->PrintMsgType = WHISPER;
      ClosePlayerList();
      break;
    case 2:
      buffer = "kick " + std::string(ChosenPlayer.ToChar());
      client->SendCommand(buffer.c_str());
      ClosePlayerList();
      break;
    case 3:
      buffer = "ban " + std::string(ChosenPlayer.ToChar());
      client->SendCommand(buffer.c_str());
      ClosePlayerList();
      break;
    case 4:
      buffer = "kill " + std::string(ChosenPlayer.ToChar());
      client->SendCommand(buffer.c_str());
      ClosePlayerList();
      break;
    case 5:
      if (!player->IsDead()) {
        zVEC3 pos = ChPlayerNpc->trafoObjToWorld.GetTranslation();
        player->trafoObjToWorld.SetTranslation(zVEC3(pos[VX], pos[VY] + 100, pos[VZ]));
      }
      ClosePlayerList();
      break;
  }
};

void CPlayerList::UpdatePlayerList() {
  if (!player->IsMovLock())
    player->SetMovLock(1);
  if (!PlayerOptions) {
    // INIT
    if (MenuPos > (int)client->players.size() - 1)
      MenuPos = (int)client->players.size() - 1;
    if (PrintFrom > (int)client->players.size() - 1)
      PrintFrom--;
    // INPUT
    if (client->players.size() > 1) {
      if (zinput->KeyToggled(KEY_UP)) {
        if (MenuPos > 1)
          MenuPos--;
        if (PrintFrom > 1) {
          if (MenuPos > PrintFrom)
            PrintFrom--;
        }
      }
      if (zinput->KeyToggled(KEY_DOWN)) {
        if (MenuPos < (int)client->players.size() - 1) {
          MenuPos++;
          if (MenuPos > 17)
            PrintFrom++;
        }
      }
      if (zinput->KeyPressed(KEY_RETURN)) {
        zinput->ClearKeyBuffer();
        ChosenPlayer = client->players[MenuPos]->npc->GetName();
        ChPlayerNpc = client->players[MenuPos]->npc;
        PlayerOptions = true;
        MenuPos = 0;
      }
    }
    // PRINT
    zCView* Screen = screen;
    Screen->SetFontColor(Normal);
    Screen->Print(x + 400, y, (*Lang)[CLanguage::SRV_PLAYERS]);
    char buffer[128];
    sprintf(buffer, "%d", client->players.size());
    zSTRING NoOfPlayers = buffer;
    Screen->Print(x + 3000, y, NoOfPlayers);
    int Size = 2400;
    if (client->players.size() > 1) {
      if ((int)client->players.size() > 17)
        PrintTo = 17;
      else
        PrintTo = (int)client->players.size() - 1;
      for (int i = PrintFrom; i < PrintFrom + PrintTo; i++) {
        if (i > (int)client->players.size() - 1) {
          MenuPos = (int)client->players.size() - 1;
          if (PrintFrom > (int)client->players.size() - 1)
            PrintFrom--;
          break;
        }
        if (client->players[i]->npc) {
          FColors1 = (MenuPos == i) ? Highlighted : Normal;
          Screen->SetFontColor(FColors1);
          ZeroMemory(buffer, 128);
          std::string display_name = client->players[i]->GetName();
          if (display_name.length() > 20)
            display_name.resize(20);
          sprintf(buffer, "%s", display_name.c_str());
          NoOfPlayers = buffer;
          Screen->Print(x + 400, Size, NoOfPlayers);
          Size += 200;
        }
      }
    } else
      Screen->Print(x + 400, y + 200, (*Lang)[CLanguage::NOPLAYERS]);
  } else {
    // INPUT
    if (zinput->KeyToggled(KEY_UP)) {
      if (MenuPos > 0)
        MenuPos--;
    }
    if (zinput->KeyToggled(KEY_DOWN)) {
      if (client->IsAdminOrModerator) {
        if (MenuPos < 5)
          MenuPos++;
      } else {
        if (MenuPos < 1)
          MenuPos++;
      }
    }
    if (zinput->KeyPressed(KEY_RETURN)) {
      zinput->ClearKeyBuffer();
      RunPlayerListItem();
    }
    // PRINT
    zCView* Screen = screen;
    Screen->SetFontColor(Normal);
    Screen->Print(x + 400, y, ChosenPlayer);
    FColors1 = (MenuPos == 0) ? Highlighted : Normal;
    Screen->SetFontColor(FColors1);
    Screen->Print(x + 400, y + 200, (*Lang)[CLanguage::MMENU_BACK]);
    FColors1 = (MenuPos == 1) ? Highlighted : Normal;
    Screen->SetFontColor(FColors1);
    Screen->Print(x + 400, y + 400, (*Lang)[CLanguage::PLIST_PM]);
    if (client->IsAdminOrModerator) {
      FColors1 = (MenuPos == 2) ? Highlighted : Normal;
      Screen->SetFontColor(FColors1);
      Screen->Print(x + 400, y + 800, "Kick");
      FColors1 = (MenuPos == 3) ? Highlighted : Normal;
      Screen->SetFontColor(FColors1);
      Screen->Print(x + 400, y + 1000, "Ban");
      FColors1 = (MenuPos == 4) ? Highlighted : Normal;
      Screen->SetFontColor(FColors1);
      Screen->Print(x + 400, y + 1200, (*Lang)[CLanguage::KILL_PLAYER]);
      FColors1 = (MenuPos == 5) ? Highlighted : Normal;
      Screen->SetFontColor(FColors1);
      Screen->Print(x + 400, y + 1400, (*Lang)[CLanguage::GOTO_PLAYER]);
    }
  }
};