
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

#include "CAnimMenu.h"

#include <stdio.h>

#include <fstream>
#include <iostream>
#include <string>

#include "CChat.h"
#include "CLanguage.h"
#include "patch.h"

using namespace std;
extern zCOLOR Highlighted;
extern zCOLOR Normal;
extern CLanguage* Lang;
zCOLOR FColors;

CAnimMenu::CAnimMenu() {
  MenuPos = 0, PrintFrom = 0, PrintTo = 0;
  Opened = false;
  ifstream AniNames(".\\Multiplayer\\AnimMenu.txt");
  char _buff[512];
  char AniName[32];
  char AniStart[32];
  char AniLoop[32];
  char AniEnd[32];
  string buffer;
  if (AniNames.good()) {
    while (!AniNames.eof()) {
      memset(AniName, 0, 32);
      memset(AniStart, 0, 32);
      memset(AniLoop, 0, 32);
      memset(AniEnd, 0, 32);
      AniNames.getline(_buff, 512);
      sscanf(_buff, "%s %s %s %s", AniName, AniLoop, AniStart, AniEnd);
      if (strlen(AniName) < 1)
        continue;
      std::string check = AniLoop;
      if (check.find("DIVE") != std::string::npos)
        continue;
      if (check.find("TOUCHPLATE") != std::string::npos)
        continue;
      if (check.find("VWHEEL") != std::string::npos)
        continue;
      if (check.find("RELOAD") != std::string::npos)
        continue;
      if (check.find("LADDER") != std::string::npos)
        continue;
      check = AniStart;
      if (check.find("DIVE") != std::string::npos)
        continue;
      if (check.find("TOUCHPLATE") != std::string::npos)
        continue;
      if (check.find("VWHEEL") != std::string::npos)
        continue;
      if (check.find("RELOAD") != std::string::npos)
        continue;
      if (check.find("LADDER") != std::string::npos)
        continue;
      check = AniEnd;
      if (check.find("DIVE") != std::string::npos)
        continue;
      if (check.find("TOUCHPLATE") != std::string::npos)
        continue;
      if (check.find("VWHEEL") != std::string::npos)
        continue;
      if (check.find("RELOAD") != std::string::npos)
        continue;
      if (check.find("LADDER") != std::string::npos)
        continue;
      Anim anim;
      anim.AniName = AniName;
      anim.AniLoop = AniLoop;
      if (strlen(AniStart) > 0)
        anim.AniStart = AniStart;
      if (strlen(AniEnd) > 0)
        anim.AniEnd = AniEnd;
      AnimVector.push_back(anim);
    }
  } else
    CChat::GetInstance()->WriteMessage(NORMAL, false, "Couldn't find AnimMenu.txt in Multiplayer folder.");
  AniNames.close();
}

void CAnimMenu::Open() {
  if (!AnimVector[MenuPos].AniEnd.IsEmpty()) {
    if (player->GetModel()->IsAnimationActive(AnimVector[MenuPos].AniLoop)) {
      player->GetModel()->StartAnimation(AnimVector[MenuPos].AniEnd);
      return;
    }
  } else if (player->GetModel()->IsAnimationActive(AnimVector[MenuPos].AniLoop)) {
    player->GetModel()->StopAnimation(AnimVector[MenuPos].AniLoop);
    return;
  }
  player->GetAnictrl()->StopTurnAnis();
  Patch::PlayerInterfaceEnabled(false);
  Opened = true;
}

void CAnimMenu::Close() {
  Patch::PlayerInterfaceEnabled(true);
  player->SetMovLock(0);
  Opened = false;
}

void CAnimMenu::RunMenuItem() {
  Patch::PlayerInterfaceEnabled(true);
  if (!AnimVector[MenuPos].AniStart.IsEmpty()) {
    player->GetModel()->StartAnimation(AnimVector[MenuPos].AniStart);
  } else
    player->GetModel()->StartAnimation(AnimVector[MenuPos].AniLoop);
  Close();
}

void CAnimMenu::PrintMenu() {
  if (player->IsDead() || player->GetBodyState() == 22)
    Close();
  if (!player->IsMovLock())
    player->SetMovLock(1);

  // INPUT
  if (zinput->KeyToggled(KEY_UP)) {
    if (MenuPos > 0)
      MenuPos--;
    if (PrintFrom > 0) {
      if (MenuPos > PrintFrom)
        PrintFrom--;
    }
  }
  if (zinput->KeyToggled(KEY_DOWN)) {
    if (MenuPos < (int)AnimVector.size() - 1) {
      MenuPos++;
      if (MenuPos > 9)
        PrintFrom++;
    }
  }
  if (zinput->KeyPressed(KEY_RETURN)) {
    RunMenuItem();
  }

  // PRINT
  screen->SetFontColor(Normal);
  screen->Print(6500, 3200, (*Lang)[CLanguage::ANIMS_MENU]);
  if ((int)AnimVector.size() > 9)
    PrintTo = 10;
  else
    PrintTo = (int)AnimVector.size() - 1;
  int Size = 3400;
  for (int i = PrintFrom; i < PrintFrom + PrintTo; i++) {
    FColors = (MenuPos == i) ? Highlighted : Normal;
    screen->SetFontColor(FColors);
    screen->Print(6500, Size, AnimVector[i].AniName);
    Size += 200;
  }
}