
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

#include "CPlayer.h"

#include <spdlog/spdlog.h>

#include <list>

#include "CIngame.h"
#include "CInterpolatePos.h"
#include "CLocalPlayer.h"
#include "config.h"
#include "game_client.h"

// NEEDED FOR SETTING NPC TYPES
#define B_SETVISUALS "B_SETVISUALS"
#define LESSER_SKELETON "LESSER_SKELETON"
#define SKELETON "SKELETON"
#define SKELETON_MAGE "SKELETON_MAGE"
#define SKELETON_LORD "SKELETON_LORD"
static int HeroInstance;

// Externs
extern GameClient* client;
extern CIngame* global_ingame;
extern CLocalPlayer* LocalPlayer;

constexpr const char* BODYMESHNAKED = "HUM_BODY_NAKED0";
// Head models
constexpr const char* HUM_HEAD_FIGHTER = "HUM_HEAD_FIGHTER";
constexpr const char* HUM_HEAD_BALD = "HUM_HEAD_BALD";
constexpr const char* HUM_HEAD_FATBALD = "HUM_HEAD_FATBALD";
constexpr const char* HUM_HEAD_PONY = "HUM_HEAD_PONY";
constexpr const char* HUM_HEAD_PSIONIC = "HUM_HEAD_PSIONIC";
constexpr const char* HUM_HEAD_THIEF = "HUM_HEAD_THIEF";
// Walk styles
constexpr const char* WALK_NONE = "NONE";
constexpr const char* HUMANS_TIRED = "HUMANS_TIRED.MDS";
constexpr const char* HUMANS_RELAXED = "HUMANS_RELAXED.MDS";
constexpr const char* HUMANS_MILITIA = "HUMANS_MILITIA.MDS";
constexpr const char* HUMANS_BABE = "HUMANS_BABE.MDS";
constexpr const char* HUMANS_ARROGANCE = "HUMANS_ARROGANCE.MDS";
constexpr const char* HUMANS_MAGE = "HUMANS_MAGE.MDS";
// NPC INSTANCES
constexpr const char* PCHERO = "PC_HERO";
constexpr const char* ORCWARRIOR = "ORCWARRIOR_ROAM";
constexpr const char* ORCELITE = "ORCELITE_ROAM";
constexpr const char* ORCSHAMAN = "ORCSHAMAN_SIT";
constexpr const char* UNDEADORC = "UNDEADORCWARRIOR";
constexpr const char* SHEEP = "SHEEP";
constexpr const char* DRACONIAN = "DRACONIAN";

CPlayer::CPlayer() {
  this->npc = NULL;
  this->id = NULL;
  this->enable = FALSE;
  this->hp = NULL;
  this->ScriptInstance = NULL;
  this->update_hp_packet = NULL;
  // Checking if CIngame exists therefore skipping add CLocalPlayer to inter list
  if (global_ingame)
    this->InterPos = new CInterpolatePos(this);
  else
    this->InterPos = NULL;
  this->Type = NPC_HUMAN;
};

CPlayer::~CPlayer() {
  this->npc = NULL;
  this->id = NULL;
  this->enable = FALSE;
  this->hp = NULL;
  this->ScriptInstance = NULL;
  this->update_hp_packet = NULL;
  delete this->InterPos;
  this->InterPos = NULL;
};

void CPlayer::AnalyzePosition(zVEC3& Pos) {
  if (!InterPos->IsDistanceSmallerThanRadius(400.0f, npc->GetPositionWorld(), Pos)) {
    npc->trafoObjToWorld.SetTranslation(Pos);
    return;
  }
  if (!InterPos->IsDistanceSmallerThanRadius(50.0f, npc->GetPositionWorld(), Pos)) {
    if (!IsFighting())
      InterPos->UpdateInterpolation(Pos[VX], Pos[VY], Pos[VZ]);
    else
      npc->trafoObjToWorld.SetTranslation(Pos);
  }
};

void CPlayer::DeleteAllPlayers() {
  global_ingame->Shrinker->UnShrinkAll();
  for (size_t i = 1; i < client->players.size(); i++) {
    client->players[i]->npc->GetSpellBook()->Close(1);
    ogame->GetSpawnManager()->DeleteNpc(client->players[i]->npc);
    delete client->players[i];
  }
  client->players.clear();
};

void CPlayer::DisablePlayer() {
  if (enable) {
    npc->GetSpellBook()->Close(1);
    npc->Disable();
    enable = FALSE;
  }
};

void CPlayer::GetAppearance(BYTE& head, BYTE& skin, BYTE& face) {
  head = this->Head;
  skin = this->Skin;
  face = this->Face;
};

zSTRING CPlayer::GetHeadModelName() {
  return GetHeadModelNameFromByte(Head);
};

zSTRING CPlayer::GetHeadModelNameFromByte(BYTE head) {
  switch (head) {
    case 0:
      return HUM_HEAD_FIGHTER;
      break;
    case 1:
      return HUM_HEAD_BALD;
      break;
    case 2:
      return HUM_HEAD_FATBALD;
      break;
    case 3:
      return HUM_HEAD_PONY;
      break;
    case 4:
      return HUM_HEAD_PSIONIC;
      break;
    case 5:
      return HUM_HEAD_THIEF;
      break;
    default:
      return HUM_HEAD_BALD;
      break;
  }
};

int CPlayer::GetHealth() {
  return this->npc->attribute[NPC_ATR_HITPOINTS];
};

CPlayer* CPlayer::GetLocalPlayer() {
  return client->players[0];
};

const char* CPlayer::GetName() {
  return this->npc->GetName().ToChar();
};

int CPlayer::GetNameLength() {
  return this->npc->GetName().Length();
};

zSTRING CPlayer::GetWalkStyleFromByte(BYTE walkstyle) {
  switch (walkstyle) {
    case 0:
      return WALK_NONE;
      break;
    case 1:
      return HUMANS_TIRED;
      break;
    case 2:
      return HUMANS_RELAXED;
      break;
    case 3:
      return HUMANS_MILITIA;
      break;
    case 4:
      return HUMANS_BABE;
      break;
    case 5:
      return HUMANS_ARROGANCE;
      break;
    case 6:
      return HUMANS_MAGE;
      break;
    default:
      return WALK_NONE;
      break;
  }
}

bool CPlayer::IsFighting() {
  if (npc->GetWeaponMode() > 0)
    return true;
  return false;
};

bool CPlayer::IsLocalPlayer() {
  if (this == LocalPlayer)
    return true;
  return false;
};

bool CPlayer::IsPlayerValid(CPlayer* Player) {
  for (size_t i = 0; i < client->players.size(); i++) {
    if (Player = client->players[i])
      return true;
  }
  return false;
};

void CPlayer::LeaveGame() {
  if (global_ingame->Shrinker->IsShrinked(npc))
    global_ingame->Shrinker->UnShrinkNpc(npc);
  this->npc->GetSpellBook()->Close(1);
  if (this->npc)
    ogame->GetSpawnManager()->DeleteNpc(npc);
  else {
    SPDLOG_ERROR("Error Code: 0x00");
  }
  this->npc = NULL;
};

void CPlayer::RespawnPlayer() {
  global_ingame->Shrinker->UnShrinkNpc(npc);
  npc->GetSpellBook()->Close(1);
  if (!IsLocalPlayer()) {
    hp = static_cast<short>(npc->attribute[NPC_ATR_HITPOINTSMAX]);
    auto player_pos = npc->GetPositionWorld();
    npc->ResetPos(player_pos);
  } else {
    npc->RefreshNpc();
    npc->SetMovLock(0);
    npc->SetWeaponMode(NPC_WEAPON_NONE);
    hp = static_cast<short>(npc->attribute[NPC_ATR_HITPOINTSMAX]);
    auto pos = npc->GetPositionWorld();
    npc->ResetPos(pos);
  }
};

void CPlayer::SetAppearance(BYTE head, BYTE skin, BYTE face) {
  this->Head = head;
  this->Skin = skin;
  this->Face = face;
  static zSTRING body_mesh = BODYMESHNAKED;
  zSTRING head_model = GetHeadModelNameFromByte(head);
  this->npc->SetAdditionalVisuals(body_mesh, skin, 0, head_model, face, 0, -1);
};

void CPlayer::SetHealth(int Value) {
  this->npc->attribute[NPC_ATR_HITPOINTS] = Value;
};

void CPlayer::SetName(zSTRING& Name) {
  this->npc->name[0].Clear();
  this->npc->name[0].Insert(0, Name);
};

void CPlayer::SetName(const char* Name) {
  this->npc->name[0].Clear();
  this->npc->name[0] = Name;
};

void CPlayer::SetNpc(oCNpc* Npc) {
  this->npc = Npc;
  this->ScriptInstance = Npc->GetInstance();
};

void CPlayer::SetNpcType(NpcType TYPE) {
  if (Type == TYPE)
    return;
  if (TYPE > NPC_DRACONIAN && Type != NPC_HUMAN) {
    SetNpcType(NPC_HUMAN);
  }
  Type = TYPE;
  char buffer[128];
  zSTRING TypeTemp;
  zCParser::GetParser()->SetInstance("SELF", npc);
  if (npc->GetModel()->HasAppliedModelProtoOverlay(CPlayer::GetWalkStyleFromByte(Config::Instance().walkstyle)))
    npc->GetModel()->RemoveModelProtoOverlay(CPlayer::GetWalkStyleFromByte(Config::Instance().walkstyle));
  switch (TYPE) {
    case NPC_HUMAN: {
      oCNpc* New = zfactory->CreateNpc(zCParser::GetParser()->GetIndex(PCHERO));
      if (!IsLocalPlayer())
        New->startAIState = 0;
      auto position = npc->GetPositionWorld();
      New->Enable(position);
      if (IsLocalPlayer()) {
        TypeTemp = "HUM_BODY_NAKED0";
        zSTRING headmodel_tmp = CPlayer::GetHeadModelNameFromByte(Config::Instance().headmodel);
        New->SetAdditionalVisuals(TypeTemp, Config::Instance().skintexture, 0, headmodel_tmp, Config::Instance().facetexture, 0, -1);
        New->GetModel()->ApplyModelProtoOverlay(CPlayer::GetWalkStyleFromByte(Config::Instance().walkstyle));
      }
      if (IsLocalPlayer())
        New->SetAsPlayer();
      New->name[0] = this->npc->GetName();
      ogame->GetSpawnManager()->DeleteNpc(this->npc);
      SetNpc(New);
    } break;
    case NPC_ORCWARRIOR: {
      oCNpc* New = zfactory->CreateNpc(zCParser::GetParser()->GetIndex(ORCWARRIOR));
      if (!IsLocalPlayer())
        New->startAIState = 0;
      auto position = npc->GetPositionWorld();
      New->Enable(position);
      if (IsLocalPlayer())
        New->SetAsPlayer();
      New->name[0] = this->npc->GetName();
      ogame->GetSpawnManager()->DeleteNpc(this->npc);
      SetNpc(New);
    } break;
    case NPC_ORCELITE: {
      oCNpc* New = zfactory->CreateNpc(zCParser::GetParser()->GetIndex(ORCELITE));
      if (!IsLocalPlayer())
        New->startAIState = 0;
      auto position = npc->GetPositionWorld();
      New->Enable(position);
      if (IsLocalPlayer())
        New->SetAsPlayer();
      New->name[0] = this->npc->GetName();
      ogame->GetSpawnManager()->DeleteNpc(npc);
      SetNpc(New);
    } break;
    case NPC_ORCSHAMAN: {
      oCNpc* New = zfactory->CreateNpc(zCParser::GetParser()->GetIndex(ORCSHAMAN));
      if (!IsLocalPlayer())
        New->startAIState = 0;
      auto position = npc->GetPositionWorld();
      New->Enable(position);
      if (IsLocalPlayer())
        New->SetAsPlayer();
      New->name[0] = this->npc->GetName();
      ogame->GetSpawnManager()->DeleteNpc(npc);
      SetNpc(New);
    } break;
    case NPC_UNDEADORC: {
      oCNpc* New = zfactory->CreateNpc(zCParser::GetParser()->GetIndex(UNDEADORC));
      if (!IsLocalPlayer())
        New->startAIState = 0;
      auto position = npc->GetPositionWorld();
      New->Enable(position);
      if (IsLocalPlayer())
        New->SetAsPlayer();
      New->name[0] = this->npc->GetName();
      ogame->GetSpawnManager()->DeleteNpc(npc);
      SetNpc(New);
    } break;
    case NPC_SHEEP: {
      oCNpc* New = zfactory->CreateNpc(zCParser::GetParser()->GetIndex(SHEEP));
      if (!IsLocalPlayer())
        New->startAIState = 0;
      auto position = npc->GetPositionWorld();
      New->Enable(position);
      if (IsLocalPlayer())
        New->SetAsPlayer();
      New->name[0] = this->npc->GetName();
      ogame->GetSpawnManager()->DeleteNpc(npc);
      SetNpc(New);
    } break;
    case NPC_DRACONIAN: {
      oCNpc* New = zfactory->CreateNpc(zCParser::GetParser()->GetIndex(DRACONIAN));
      if (!IsLocalPlayer())
        New->startAIState = 0;
      auto position = npc->GetPositionWorld();
      New->Enable(position);
      if (IsLocalPlayer())
        New->SetAsPlayer();
      New->name[0] = this->npc->GetName();
      ogame->GetSpawnManager()->DeleteNpc(npc);
      SetNpc(New);
    } break;
    case NPC_LESSERSKELETON:
      sprintf(buffer, "%s_%s", B_SETVISUALS, LESSER_SKELETON);
      TypeTemp = buffer;
      zCParser::GetParser()->CallFunc(TypeTemp);
      npc->GetModel()->ApplyModelProtoOverlay(CPlayer::GetWalkStyleFromByte(Config::Instance().walkstyle));
      break;
    case NPC_SKELETON:
      sprintf(buffer, "%s_%s", B_SETVISUALS, SKELETON);
      TypeTemp = buffer;
      zCParser::GetParser()->CallFunc(TypeTemp);
      npc->GetModel()->ApplyModelProtoOverlay(CPlayer::GetWalkStyleFromByte(Config::Instance().walkstyle));
      break;
    case NPC_SKELETONMAGE:
      sprintf(buffer, "%s_%s", B_SETVISUALS, SKELETON_MAGE);
      TypeTemp = buffer;
      zCParser::GetParser()->CallFunc(TypeTemp);
      break;
    case NPC_SKELETONLORD:
      sprintf(buffer, "%s_%s", B_SETVISUALS, SKELETON_LORD);
      TypeTemp = buffer;
      zCParser::GetParser()->CallFunc(TypeTemp);
      break;
    default:
      break;
  };
};

void CPlayer::SetPosition(zVEC3& pos) {
  this->npc->trafoObjToWorld.SetTranslation(pos);
};

void CPlayer::SetPosition(float x, float y, float z) {
  this->npc->trafoObjToWorld.SetTranslation(zVEC3(x, y, z));
};