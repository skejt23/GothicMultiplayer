
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

#pragma once

#include <ctime>

#include "CAnimMenu.h"
#include "CChat.h"
#include "CInterpolatePos.h"
#include "CInventory.h"
#include "language.h"
#include "CMap.h"
#include "CPlayerList.h"
#include "CShrinker.h"

enum CURRENT_MAP { MAP_UNKNOWN, MAP_COLONY, MAP_OLDWORLD, MAP_KHORINIS, MAP_JARKENDAR };

class CIngame {
public:
  CIngame();
  ~CIngame(void);
  static void Loop(void);
  void Draw(void);
  void HandleInput(void);
  void CheckForUpdate(void);
  void CheckForHPDiff(void);
  bool ValidatePlayerForHPDiff(Gothic2APlayer*);
  void CheckSwampLights();
  bool IgnoreFirstSync;
  CURRENT_MAP RecognizedMap;
  bool PlayerExists(const char* PlayerName);
  time_t NextTimeSync;
  std::string WhisperingTo;
  CMap* MMap;
  CShrinker* Shrinker;
  CInventory* Inventory;
  std::vector<CInterpolatePos*> Interpolation;

private:
  zSTRING szPing;
  bool SwampLightsOn;
  bool mapusable;
  std::string chatbuffer;
  char buffer[85];
  time_t BuffTimer;
  time_t SecTimer;
  time_t ChatTimer;
  zSTRING ChatTmp;
  oCMsgMovement* Movement;
  void ClearAfterWrite();
  void PrepareForWrite();
  clock_t last_player_update;
  CPlayerList* PList;
  CChat* chat_interface;
  CAnimMenu* AMenu;
  bool WritingOnChat;
};
