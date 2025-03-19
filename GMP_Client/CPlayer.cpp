
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

#include "CPlayer.h"
#include "CHeroClass.h"
#include <list>
#include "game_client.h"
#include "CIngame.h"
#include "CInterpolatePos.h"
#include "CLocalPlayer.h"
#include "CConfig.h"

#include <spdlog/spdlog.h>


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
extern zSTRING SELF;
extern CConfig* user_config;

// ZSTRINGS
zSTRING BODYMESHNAKED = "HUM_BODY_NAKED0";
  // Rodzaje modelu g�owy
zSTRING HUM_HEAD_FIGHTER = "HUM_HEAD_FIGHTER";
zSTRING HUM_HEAD_BALD = "HUM_HEAD_BALD";
zSTRING HUM_HEAD_FATBALD = "HUM_HEAD_FATBALD";
zSTRING HUM_HEAD_PONY = "HUM_HEAD_PONY";
zSTRING HUM_HEAD_PSIONIC = "HUM_HEAD_PSIONIC";
zSTRING HUM_HEAD_THIEF = "HUM_HEAD_THIEF";
  // Style chodzenia
zSTRING WALK_NONE = "NONE";
zSTRING HUMANS_TIRED = "HUMANS_TIRED.MDS";
zSTRING HUMANS_RELAXED = "HUMANS_RELAXED.MDS";
zSTRING HUMANS_MILITIA = "HUMANS_MILITIA.MDS";
zSTRING HUMANS_BABE = "HUMANS_BABE.MDS";
zSTRING HUMANS_ARROGANCE = "HUMANS_ARROGANCE.MDS";
zSTRING HUMANS_MAGE = "HUMANS_MAGE.MDS";
// NPC INSTANCES
zSTRING PCHERO = "PC_HERO";
zSTRING ORCWARRIOR = "ORCWARRIOR_ROAM";
zSTRING ORCELITE = "ORCELITE_ROAM";
zSTRING ORCSHAMAN = "ORCSHAMAN_SIT";
zSTRING UNDEADORC = "UNDEADORCWARRIOR";
zSTRING SHEEP = "SHEEP";
zSTRING DRACONIAN = "DRACONIAN";
// TEMP
zSTRING TypeTemp;

CPlayer::CPlayer()
{
	this->npc = NULL;
	this->id = NULL;
	this->enable = FALSE;
	this->SameTeamAsLocalPlayer = false;
	this->hp = NULL;
	this->ScriptInstance = NULL;
	this->update_hp_packet = NULL;
	this->char_class = NULL;
	// Checking if CIngame exists therefore skipping add CLocalPlayer to inter list
	if(global_ingame) this->InterPos = new CInterpolatePos(this);
	else this->InterPos = NULL;
	this->Type = NPC_HUMAN;
};

CPlayer::~CPlayer()
{
	this->npc = NULL;
	this->id = NULL;
	this->enable = FALSE;
	this->hp = NULL;
	this->ScriptInstance = NULL;
	this->update_hp_packet = NULL;
	this->char_class = NULL;
	delete this->InterPos;
	this->InterPos = NULL;
};

void CPlayer::AnalyzePosition(zVEC3 & Pos)
{
	if(!InterPos->IsDistanceSmallerThanRadius(400.0f, npc->GetPosition(), Pos)){
		npc->SetPosition(Pos.x, Pos.y, Pos.z);
		return;
	}
	if(!InterPos->IsDistanceSmallerThanRadius(50.0f, npc->GetPosition(), Pos)){
		if(!IsFighting()) InterPos->UpdateInterpolation(Pos.x, Pos.y, Pos.z);
		else npc->SetPosition(Pos.x, Pos.y, Pos.z);
	}
};

void CPlayer::DeleteAllPlayers()
{
	global_ingame->Shrinker->UnShrinkAll();
	for(size_t i=1; i<client->player.size(); i++){
			client->player[i]->npc->GetSpellBook()->Close(1);
			oCGame::GetGame()->GetSpawnManager()->DeleteNpc(client->player[i]->npc);
			delete client->player[i];
	}
	client->player.clear();
};

void CPlayer::DisablePlayer()
{
	if(enable){
		npc->GetSpellBook()->Close(1);
		npc->Disable();
		enable=FALSE;
	}
};

zVEC3 CPlayer::GetAngleN()
{
	return zVEC3(npc->GetAngleNX(), npc->GetAngleNY(), npc->GetAngleNZ());
};

void CPlayer::GetAppearance(BYTE & head, BYTE & skin, BYTE & face)
{
	head = this->Head;
	skin = this->Skin;
	face = this->Face;
};

zSTRING& CPlayer::GetClassName()
{
	return (*client->classmgr)[this->char_class]->class_name;
};

zSTRING& CPlayer::GetHeadModelName()
{
	return GetHeadModelNameFromByte(Head);
};

zSTRING& CPlayer::GetHeadModelNameFromByte(BYTE head)
{
		switch(head){
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

int CPlayer::GetHealth()
{
	return this->npc->attribute[NPC_ATR_HITPOINTS];
};

CPlayer* CPlayer::GetLocalPlayer()
{
	return client->player[0];
};

const char* CPlayer::GetName()
{
	return this->npc->GetName().ToChar();
};

int CPlayer::GetNameLength()
{
	return this->npc->GetName().Length();
};

zSTRING& CPlayer::GetWalkStyleFromByte(BYTE walkstyle)
{
		switch(walkstyle){
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
};

bool CPlayer::IsFighting()
{
	if(npc->GetWeaponMode() > 0) return true;
	return false;
};

bool CPlayer::IsLocalPlayer()
{
	if(this == LocalPlayer) return true;
	return false;
};

bool CPlayer::IsPlayerValid(CPlayer* Player)
{
	for(size_t i=0; i<client->player.size(); i++){
		if(Player = client->player[i]) return true;
	}
	return false;
};

void CPlayer::LeaveGame()
{
	if(global_ingame->Shrinker->IsShrinked(npc)) global_ingame->Shrinker->UnShrinkNpc(npc);
	this->npc->GetSpellBook()->Close(1);
	if(this->npc) oCGame::GetGame()->GetSpawnManager()->DeleteNpc(npc);
	else
	{ 
		SPDLOG_ERROR("Error Code: 0x00");
	}
	this->npc = NULL;
};

void CPlayer::RespawnPlayer()
{
	global_ingame->Shrinker->UnShrinkNpc(npc);
	npc->GetSpellBook()->Close(1);
	if(!IsLocalPlayer()){
		hp=static_cast<short>(npc->GetMaxHealth());
		auto player_pos = npc->GetPosition();
		npc->ResetPos(player_pos);
		client->classmgr->EquipNPC(char_class, this, true);
	} else{
		npc->RefreshNpc();
		npc->SetMovLock(0);
		npc->SetWeaponMode(NPC_WEAPON_NONE);
		hp=static_cast<short>(npc->GetMaxHealth());
		client->classmgr->EquipNPC(char_class, this, true);
		if((client->game_mode==1) && (client->spawnpoint->GetSize())){
			std::list<const char*> team_list;
			team_list.push_back((*client->classmgr)[0]->team_name.ToChar());
			for(size_t x=1; x<client->classmgr->GetSize(); x++){
				bool found=false;
				std::list<const char*>::iterator y;
				for(y=team_list.begin(); y!=team_list.end(); y++){
					if(!memcmp((*y), (*client->classmgr)[x]->team_name.ToChar(), strlen((*y)))){ found=true; break;}
				}
				if(!found) team_list.push_back((*client->classmgr)[x]->team_name.ToChar());
			}
			size_t who_am_i=0;
			std::list<const char*>::iterator z;
			for(z=team_list.begin(); z!=team_list.end(); z++){
				if(!memcmp((*z), (*client->classmgr)[char_class]->team_name.ToChar(), strlen((*z))+1)){
					npc->ResetPos(*(*client->spawnpoint)[(rand()%(client->spawnpoint->GetSize()/team_list.size()))*team_list.size()+who_am_i]);
				} else who_am_i++;
			}
			team_list.clear();
		} else{
			if(!client->spawnpoint->GetSize()) {
				auto pos = npc->GetPosition();
				npc->ResetPos(pos);
			}
			else npc->ResetPos(*(*client->spawnpoint)[rand()%client->spawnpoint->GetSize()]);
		}
	}
};

void CPlayer::SetAppearance(BYTE head, BYTE skin, BYTE face)
{
	this->Head = head;
	this->Skin = skin;
	this->Face = face;
	this->npc->SetAdditionalVisuals(BODYMESHNAKED, skin, 0, GetHeadModelNameFromByte(head), face, 0, -1);
};

void CPlayer::SetHealth(int Value)
{
	this->npc->attribute[NPC_ATR_HITPOINTS] = Value;
};

void CPlayer::SetName(zSTRING & Name)
{
	this->npc->name[0].Clear();
	this->npc->name[0].Insert(0, Name);
};

void CPlayer::SetName(const char* Name)
{
	this->npc->name[0].Clear();
	this->npc->name[0] = Name;
};

void CPlayer::SetNpc(oCNpc* Npc)
{
	this->npc = Npc;
	this->ScriptInstance = Npc->GetInstance();
};

void CPlayer::SetNpcType(NpcType TYPE)
{
	if(Type == TYPE) return;
	if(TYPE > NPC_DRACONIAN && Type != NPC_HUMAN){
		SetNpcType(NPC_HUMAN);
	}
	Type = TYPE;
	char buffer[128];
	zCParser::GetParser()->SetInstance(SELF, npc);
	if(npc->GetModel()->HasAppliedModelProtoOverlay(CPlayer::GetWalkStyleFromByte(user_config->walkstyle))) npc->GetModel()->RemoveModelProtoOverlay(CPlayer::GetWalkStyleFromByte(user_config->walkstyle));
	switch(TYPE)
	{
		case NPC_HUMAN:
		{
			oCNpc* New = oCObjectFactory::GetFactory()->CreateNpc(zCParser::GetParser()->GetIndex(PCHERO));
			if(!IsLocalPlayer()) New->startAIState = 0;
			auto position = npc->GetPosition();
			New->Enable(position);
			if(IsLocalPlayer()){
				TypeTemp = "HUM_BODY_NAKED0";
				New->SetAdditionalVisuals(TypeTemp, user_config->skintexture, 0, CPlayer::GetHeadModelNameFromByte(user_config->headmodel), user_config->facetexture, 0, -1);
				New->GetModel()->ApplyModelProtoOverlay(CPlayer::GetWalkStyleFromByte(user_config->walkstyle));
			}
			if(IsLocalPlayer()) New->SetAsPlayer();
			New->SetName(this->npc->GetName());
			oCGame::GetGame()->GetSpawnManager()->DeleteNpc(this->npc);
			SetNpc(New);
		}
		break;
		case NPC_ORCWARRIOR:
		{
			oCNpc* New = oCObjectFactory::GetFactory()->CreateNpc(zCParser::GetParser()->GetIndex(ORCWARRIOR));
			if(!IsLocalPlayer()) New->startAIState = 0;
			auto position = npc->GetPosition();
			New->Enable(position);
			if(IsLocalPlayer()) New->SetAsPlayer();
			New->SetName(this->npc->GetName());
			oCGame::GetGame()->GetSpawnManager()->DeleteNpc(this->npc);
			SetNpc(New);
		}
		break;
		case NPC_ORCELITE:
		{
			oCNpc* New = oCObjectFactory::GetFactory()->CreateNpc(zCParser::GetParser()->GetIndex(ORCELITE));
			if(!IsLocalPlayer()) New->startAIState = 0;
			auto position = npc->GetPosition();
			New->Enable(position);
			if(IsLocalPlayer()) New->SetAsPlayer();
			New->SetName(this->npc->GetName());
			oCGame::GetGame()->GetSpawnManager()->DeleteNpc(npc);
			SetNpc(New);
		}
		break;
		case NPC_ORCSHAMAN:
		{
			oCNpc* New = oCObjectFactory::GetFactory()->CreateNpc(zCParser::GetParser()->GetIndex(ORCSHAMAN));
			if(!IsLocalPlayer()) New->startAIState = 0;
			auto position = npc->GetPosition();
			New->Enable(position);
			if(IsLocalPlayer()) New->SetAsPlayer();
			New->SetName(this->npc->GetName());
			oCGame::GetGame()->GetSpawnManager()->DeleteNpc(npc);
			SetNpc(New);
		}
		break;
		case NPC_UNDEADORC:
		{
			oCNpc* New = oCObjectFactory::GetFactory()->CreateNpc(zCParser::GetParser()->GetIndex(UNDEADORC));
			if(!IsLocalPlayer()) New->startAIState = 0;
			auto position = npc->GetPosition();
			New->Enable(position);
			if(IsLocalPlayer()) New->SetAsPlayer();
			New->SetName(this->npc->GetName());
			oCGame::GetGame()->GetSpawnManager()->DeleteNpc(npc);
			SetNpc(New);
		}
		break;
		case NPC_SHEEP:
		{
			oCNpc* New = oCObjectFactory::GetFactory()->CreateNpc(zCParser::GetParser()->GetIndex(SHEEP));
			if(!IsLocalPlayer()) New->startAIState = 0;
			auto position = npc->GetPosition();
			New->Enable(position);
			if(IsLocalPlayer()) New->SetAsPlayer();
			New->SetName(this->npc->GetName());
			oCGame::GetGame()->GetSpawnManager()->DeleteNpc(npc);
			SetNpc(New);
		}
		break;
		case NPC_DRACONIAN:
		{
			oCNpc* New = oCObjectFactory::GetFactory()->CreateNpc(zCParser::GetParser()->GetIndex(DRACONIAN));
			if(!IsLocalPlayer()) New->startAIState = 0;
			auto position = npc->GetPosition();
			New->Enable(position);
			if(IsLocalPlayer()) New->SetAsPlayer();
			New->SetName(this->npc->GetName());
			oCGame::GetGame()->GetSpawnManager()->DeleteNpc(npc);
			SetNpc(New);
		}
		break;
		case NPC_LESSERSKELETON:
			sprintf(buffer, "%s_%s", B_SETVISUALS, LESSER_SKELETON);
			TypeTemp = buffer;
			zCParser::GetParser()->CallFunc(TypeTemp);
			npc->GetModel()->ApplyModelProtoOverlay(CPlayer::GetWalkStyleFromByte(user_config->walkstyle));
		break;
		case NPC_SKELETON:
			sprintf(buffer, "%s_%s", B_SETVISUALS, SKELETON);
			TypeTemp = buffer;
			zCParser::GetParser()->CallFunc(TypeTemp);
			npc->GetModel()->ApplyModelProtoOverlay(CPlayer::GetWalkStyleFromByte(user_config->walkstyle));
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

void CPlayer::SetPosition(zVEC3 & pos)
{
	this->npc->SetPosition(pos.x, pos.y, pos.z);
};

void CPlayer::SetPosition(float x, float y, float z)
{
	this->npc->SetPosition(x, y, z);
};