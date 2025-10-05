
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

#include "CMap.h"

#include "config.h"
#include "net_game.h"

CMap::CMap() {
  Opened = false;
  PlayerMap = new zCView(0, 0, 8192, 8192, VIEW_ITEM);
  PlayerMap->SetSize(8200, 8200);
};

CMap::~CMap() {
  Opened = false;
  delete PlayerMap;
  PlayerMap = NULL;
};

void CMap::Open() {
  if (Config::Instance().watch)
    Config::Instance().watch = false;
  screen->InsertItem(PlayerMap);
  Opened = true;
};
void CMap::Close() {
  if (watchwastrue)
    Config::Instance().watch = true;
  screen->RemoveItem(PlayerMap);
  Opened = false;
};

bool CMap::CheckMap() {
  if (Config::Instance().watch)
    watchwastrue = true;
  else
    watchwastrue = false;
  if (!memcmp("NEWWORLD\\NEWWORLD.ZEN", ogame->GetGameWorld()->GetWorldFilename().ToChar(), 21)) {
    PlayerMap->InsertBack(zSTRING("MAP_NEWWORLD.TGA"));
    Map = MNEWWORLD;
    return true;
  } else if (!memcmp("OLDWORLD\\OLDWORLD.ZEN", ogame->GetGameWorld()->GetWorldFilename().ToChar(), 21)) {
    PlayerMap->InsertBack(zSTRING("MAP_OLDWORLD.TGA"));
    Map = MOLDWORLD;
    return true;
  } else if (strstr(ogame->GetGameWorld()->GetWorldFilename().ToChar(), "ADDONWORLD")) {
    PlayerMap->InsertBack(zSTRING("MAP_ADDONWORLD.TGA"));
    Map = MJARKENDAR;
    return true;
  } else if (!memcmp("OLDVALLEY.ZEN", ogame->GetGameWorld()->GetWorldFilename().ToChar(), 13)) {
    PlayerMap->InsertBack(zSTRING("MAP_WORLD_ORC.TGA"));
    Map = MOLDVALLEY;
    return true;
  } else if (strstr(ogame->GetGameWorld()->GetWorldFilename().ToChar(), "COLONY")) {
    PlayerMap->InsertBack(zSTRING("MAP_WORLD_ORC.TGA"));
    Map = MCOLONY;
    return true;
  }
  return false;
};

void CMap::PrintMap() {
  if (!Opened || !PlayerMap)
    return;
  PlayerMap->ClrPrintwin();
  switch (Map) {
    // NEWWORLD
    case MNEWWORLD:
      for (int i = 0; i < (int)NetGame::Instance().players.size(); i++) {
        if (NetGame::Instance().players[i]->npc) {
          zVEC3 MapPos = NetGame::Instance().players[i]->npc->GetPositionWorld();
          zSTRING Name = NetGame::Instance().players[i]->npc->GetName();
          int x = (int)floor((MapPos[VX] / 15) + 1850);
          int y = (int)floor((MapPos[VZ] / 12));
          sprintf(buffer, "+ %s", Name.ToChar());
          PlayerMap->Print(x, 4415 - y, buffer);
        }
      }
      break;
    // OLDWORLD
    case MOLDWORLD:
      for (int i = 0; i < (int)NetGame::Instance().players.size(); i++) {
        if (NetGame::Instance().players[i]->npc) {
          zVEC3 MapPos = NetGame::Instance().players[i]->npc->GetPositionWorld();
          zSTRING Name = NetGame::Instance().players[i]->npc->GetName();
          int x = (int)floor((MapPos[VX] / 16) + 4830);
          int y = (int)floor((MapPos[VZ] / 12));
          sprintf(buffer, "+ %s", Name.ToChar());
          PlayerMap->Print(x, 3855 - y, buffer);
        }
      }
      break;
    // JARKENDAR
    case MJARKENDAR:
      for (int i = 0; i < (int)NetGame::Instance().players.size(); i++) {
        if (NetGame::Instance().players[i]->npc) {
          zVEC3 MapPos = NetGame::Instance().players[i]->npc->GetPositionWorld();
          zSTRING Name = NetGame::Instance().players[i]->npc->GetName();
          int x = (int)floor((MapPos[VX] / 11) + 4290);
          int y = (int)floor((MapPos[VZ] / 10));
          sprintf(buffer, "+ %s", Name.ToChar());
          PlayerMap->Print(x, 4420 - y, buffer);
        }
      }
      break;
    // OLD VALLEY
    case MOLDVALLEY:
      for (int i = 0; i < (int)NetGame::Instance().players.size(); i++) {
        if (NetGame::Instance().players[i]->npc) {
          zVEC3 MapPos = NetGame::Instance().players[i]->npc->GetPositionWorld();
          zSTRING Name = NetGame::Instance().players[i]->npc->GetName();
          int x = (int)floor((MapPos[VX] / 24) + 4045);
          int y = (int)floor((MapPos[VZ] / 16));
          sprintf(buffer, "+ %s", Name.ToChar());
          PlayerMap->Print(x, 4105 - y, buffer);
        }
      }
      break;
    // COLONY.ZEN
    case MCOLONY:
      for (int i = 0; i < (int)NetGame::Instance().players.size(); i++) {
        if (NetGame::Instance().players[i]->npc) {
          zVEC3 MapPos = NetGame::Instance().players[i]->npc->GetPositionWorld();
          zSTRING Name = NetGame::Instance().players[i]->npc->GetName();
          int x = (int)floor((MapPos[VX] / 24) + 4045);
          int y = (int)floor((MapPos[VZ] / 16));
          sprintf(buffer, "+ %s", Name.ToChar());
          PlayerMap->Print(x, 4105 - y, buffer);
        }
      }
      break;
  }
}
