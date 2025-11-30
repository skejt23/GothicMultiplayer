
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
** ** *	File name:		CGmpClient/CPlayer.cpp		   							** *
*** *	Created by:		16/12/11	-	skejt23/Pampi							** *
*** *	Description:	Player class	 										** *
***
*****************************************************************************/

#pragma once

#include <cstdint>

#include "ZenGin/zGothicAPI.h"
#include "players.hpp"

class CInterpolatePos;

// Gothic 2 specific player implementation
class Gothic2APlayer {
public:
  enum NpcType {
    NPC_HUMAN,
    NPC_ORCWARRIOR,
    NPC_ORCELITE,
    NPC_ORCSHAMAN,
    NPC_UNDEADORC,
    NPC_SHEEP,
    NPC_DRACONIAN,
    NPC_LESSERSKELETON,
    NPC_SKELETON,
    NPC_SKELETONMAGE,
    NPC_SKELETONLORD
  };
  enum HeadState { HEAD_NONE, HEAD_LEFT, HEAD_RIGHT, HEAD_UP, HEAD_DOWN };

  // Gothic 2 engine object
  oCNpc* npc;
  NpcType Type;

public:
  Gothic2APlayer(gmp::client::Player& base_player, bool is_local_player = false);
  ~Gothic2APlayer();

  void AnalyzePosition(zVEC3& Pos);
  static void DeleteAllPlayers();
  void DisablePlayer();
  int GetHealth();
  static Gothic2APlayer* GetLocalPlayer();
  const char* GetName();
  int GetNameLength();
  inline oCNpc* GetNpc() {
    return this->npc;
  };
  bool IsFighting();
  bool IsLocalPlayer();
  void LeaveGame();
  void RespawnPlayer();
  void SetHealth(int Value);
  void SetName(zSTRING& Name);
  void SetName(const char* Name);
  void SetNameColor(const zCOLOR& color);
  const zCOLOR& GetNameColor() const { return name_color_; }
  void SetNpc(oCNpc* npc);
  void SetNpcType(NpcType Type);
  void SetPosition(zVEC3& pos);
  void SetPosition(float x, float y, float z);
  gmp::client::Player& base_player() {
    return base_player_;
  }

private:
  gmp::client::Player& base_player_;
  bool is_local_player_{false};
  CInterpolatePos* InterPos;
  int ScriptInstance;
  zCOLOR name_color_{255, 255, 255, 255};
};