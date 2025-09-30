
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
**																			**
**	File name:		CGmpClient/CPlayer.cpp		   							**
**																			**
**	Created by:		16/12/11	-	skejt23/Pampi							**
**																			**
**	Description:	Player class	 										**
**																			**
*****************************************************************************/

#pragma once

#include <cstdint>
#include "ZenGin/zGothicAPI.h"

class CInterpolatePos;

class CPlayer {
private:
  CInterpolatePos* InterPos;
  int ScriptInstance;
  BYTE Head;
  BYTE Skin;
  BYTE Face;

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
  Gothic_II_Addon::oCNpc* npc;
  uint64_t id;
  short hp, enable, update_hp_packet;
  NpcType Type;

public:
  CPlayer();
  ~CPlayer();
  void AnalyzePosition(Gothic_II_Addon::zVEC3& Pos);
  static void DeleteAllPlayers();
  void DisablePlayer();
  void GetAppearance(BYTE& head, BYTE& skin, BYTE& face);
  Gothic_II_Addon::zSTRING GetHeadModelName();
  static Gothic_II_Addon::zSTRING GetHeadModelNameFromByte(BYTE head);
  int GetHealth();
  static CPlayer* GetLocalPlayer();
  const char* GetName();
  int GetNameLength();
  inline Gothic_II_Addon::oCNpc* GetNpc() {
    return this->npc;
  };
  static Gothic_II_Addon::zSTRING GetWalkStyleFromByte(BYTE walkstyle);
  bool IsFighting();
  bool IsLocalPlayer();
  static bool IsPlayerValid(CPlayer* Player);
  void LeaveGame();
  void RespawnPlayer();
  void SetAppearance(BYTE head, BYTE skin, BYTE face);
  void SetHealth(int Value);
  void SetName(Gothic_II_Addon::zSTRING& Name);
  void SetName(const char* Name);
  void SetNpc(Gothic_II_Addon::oCNpc* npc);
  void SetNpcType(NpcType Type);
  void SetPosition(Gothic_II_Addon::zVEC3& pos);
  void SetPosition(float x, float y, float z);
};