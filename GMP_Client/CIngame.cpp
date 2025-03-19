
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

#include "CIngame.h"
#include "game_client.h"
#include "CWatch.h"
#include "CConfig.h"
#include "HooksManager.h"
#include "CActiveAniID.h"
#include "keyboard.h"
#include "CMenu.h"
#include "patch.h"
#include "CMainMenu.h"
#include <time.h>
#include "CLocalPlayer.h"

CIngame *global_ingame=NULL;
extern GameClient*client;
extern CLocalPlayer* LocalPlayer;
void InterfaceLoop(void);
zSTRING arrow = "->";
zSTRING DEADB = "S_DEADB";
zSTRING DEAD2 = "S_DEAD";
zSTRING TDEADB = "T_DEADB";
zSTRING TURN = "TURN";
zSTRING MuteTMP;
extern zCOLOR Normal;
zCOLOR AQUA=zCOLOR(0,255,255,255);
zCOLOR RED=zCOLOR(255,0,0,255);
clock_t MuteTimer = 0;
clock_t MsgTimer = 0;
int SpamMessages = 0;
bool MuteCountdown = false;

CIngame::CIngame(CLanguage *pLang){
	this->last_player_update=clock();
	this->lang=pLang;
	this->chat_interface=CChat::GetInstance();
	this->NextTimeSync=time(NULL)+1;
	this->Shrinker = new CShrinker();
	this->Inventory = new CInventory(oCNpc::GetHero()->GetInventory());
	WritingOnChat = false;
	IgnoreFirstSync = true;
	SwampLightsOn = false;
	Movement = NULL;
	RecognizedMap = MAP_UNKNOWN;
	if(!memcmp("OLDVALLEY.ZEN", oCGame::GetGame()->GetWorld()->GetWorldName().ToChar(), 13) || !memcmp("COLONY.ZEN", oCGame::GetGame()->GetWorld()->GetWorldName().ToChar(), 10)) RecognizedMap = MAP_COLONY;
	if(!memcmp("OLDWORLD\\OLDWORLD.ZEN", oCGame::GetGame()->GetWorld()->GetWorldName().ToChar(), 21)) RecognizedMap = MAP_OLDWORLD;
	if(!memcmp("NEWWORLD\\NEWWORLD.ZEN", oCGame::GetGame()->GetWorld()->GetWorldName().ToChar(), 21)) RecognizedMap = MAP_KHORINIS;
	BuffTimer = 0, SecTimer = 0, ChatTimer = 0;
	PList = CPlayerList::GetInstance();
	MMap = CMap::GetInstance();
	AMenu = CAnimMenu::GetInstance();
	mapusable = false;
	if(MMap->CheckMap()) mapusable = true;
	WhisperingTo = "";
	chat_interface->WriteMessage(NORMAL, false, "Gothic Multiplayer");
	global_ingame=this;
	HooksManager::GetInstance()->AddHook(HT_RENDER, (DWORD)CIngame::Loop, false);
}

CIngame::~CIngame(){
	delete Shrinker;
	delete Inventory;
	this->lang=NULL;
	this->chat_interface=NULL;
	this->MMap=NULL;
	this->PList=NULL;
	this->AMenu=NULL;
	this->Shrinker=NULL;
	this->Inventory=NULL;
	global_ingame=NULL;
	HooksManager::GetInstance()->RemoveHook(HT_RENDER, (DWORD)CIngame::Loop);
}

zSTRING LIGHTSSCEMENAME = "PC";
void CIngame::CheckSwampLights()
{
	oCWorldTimer* Timer = oCGame::GetGame()->GetWorldTimer();
	if(!SwampLightsOn){
		if(Timer->IsTimeBetween(20, 00, 05, 00)){
			SwampLightsOn = true;
			oCMobInter::SetAllMobsToState(oCGame::GetGame()->GetGameWorld(), LIGHTSSCEMENAME, 1);
			int h;
			int m;
			Timer->GetTime(h, m);
			oCGame::GetGame()->SetTime(Timer->GetDay(), h, m);
		}
	}
	else{
		if(Timer->IsTimeBetween(05, 00, 20, 00)){
			SwampLightsOn = false;
			oCMobInter::SetAllMobsToState(oCGame::GetGame()->GetGameWorld(), LIGHTSSCEMENAME, 0);
			int h;
			int m;
			Timer->GetTime(h, m);
			oCGame::GetGame()->SetTime(Timer->GetDay(), h, m);
			oCMobInter::SetAllMobsToState(oCGame::GetGame()->GetGameWorld(), LIGHTSSCEMENAME, 0);
		}
	}
};

void CIngame::Loop(){
	if(global_ingame){
		if(client->network->IsConnected()){
			if(global_ingame->NextTimeSync==time(NULL)){
				if(!global_ingame->IgnoreFirstSync) client->SyncGameTime();
				else global_ingame->IgnoreFirstSync = false;
				global_ingame->NextTimeSync+=1200;
			}
			client->HandleNetwork();
			if(client->network->IsConnected()){
				client->RestoreHealth();
				global_ingame->CheckForUpdate();
				global_ingame->CheckForHPDiff();
                client->SendVoice();
			}
		}
		// OBSERVE MODE
		if(LocalPlayer){
			if(LocalPlayer->IsInObserveMode()){
				LocalPlayer->GetObserve()->Loop();
			}
		}
		// SENDING MY ANIMATION
		zCModelAni* AniUnusual = oCNpc::GetHero()->GetModel()->GetModelAniActive()->GetUnusualAnim(); // TALK, TURNR ETC
		zCModelAni* Ani = oCNpc::GetHero()->GetModel()->GetModelAniActive()->GetCurrentAni(); // ZWYKLE
		if(Ani){
			if(Ani->GetAniName().Search(TURN) < 2) {
				CActiveAniID::GetInstance()->AddAni(Ani->GetAniID());
			}
			if(AniUnusual && Ani->GetAniID() != AniUnusual->GetAniID()){
				if(AniUnusual->GetAniName().Search(TURN) < 2) {
					CActiveAniID::GetInstance()->AddAni(AniUnusual->GetAniID());
				}
			}
		}
		// KINDA POSITION INTERPOLATION :C
		for (int i = 0; i < (int)global_ingame->Interpolation.size(); i++){
			if(global_ingame->Interpolation[i]->IsInterpolating) global_ingame->Interpolation[i]->DoInterpolate();
		}
		// INVENTORY RENDER
		if(global_ingame->Inventory) global_ingame->Inventory->RenderInventory();
		// START SNOW IF CHRISTMAS
		if(global_ingame->Christmas) oCGame::GetGame()->GetWorld()->StartSnow();
		// RUN SHRINKER
		global_ingame->Shrinker->Loop();
		// CHECK FOR SWAMP LIGHTS STATE 
		if(global_ingame->RecognizedMap == MAP_COLONY) global_ingame->CheckSwampLights();
		// ALL LOCKABLES OPEN
		if(oCNpc::GetHero()->GetFocusVob()){
			if(GetPointerType((DWORD)oCNpc::GetHero()->GetFocusVob()) == VOB_TYPE_VT_OCMOBCONTAINER || GetPointerType((DWORD)oCNpc::GetHero()->GetFocusVob()) == VOB_TYPE_VT_OCMOBDOOR){
				oCMobLockable* Locked = static_cast<oCMobLockable*>(oCNpc::GetHero()->GetFocusVob());
				Locked->SetLocked(0);
			}
		}
		// MUTE 
		if(MuteCountdown)
		{
			long secs_to_unmute=(MuteTimer-clock())/1000;
			char tmp_char[32];
			sprintf(tmp_char, "%s : %d", (*global_ingame->lang)[CLanguage::UNMUTE_TIME].ToChar(), secs_to_unmute);
			MuteTMP = tmp_char;
			zCView::GetScreen()->PrintCXY(MuteTMP);
			MuteTMP.Clear();
			if(secs_to_unmute < 0){
				MuteCountdown = false;
			}
		}
		// DEATH BUG WHEN AIMING FIX
		oCNpc* Npc = oCNpc::GetHero();
		if(Npc->IsDead() && !Npc->GetAnictrl()->IsInWater()){
			if(Npc->GetWeaponMode()== NPC_WEAPON_BOW || Npc->GetWeaponMode() == NPC_WEAPON_MAG || Npc->GetWeaponMode() == NPC_WEAPON_CBOW){
				if(!Npc->GetModel()->IsAnimationActive(DEADB) && !Npc->GetModel()->IsAnimationActive(TDEADB) && !Npc->GetModel()->IsAnimationActive(DEAD2)){
					Npc->GetModel()->StartAnimation(TDEADB);
					Npc->GetAnictrl()->StopTurnAnis();
					 if(Npc){
						 oCItem* RHand = NULL;
						 oCItem* LHand = NULL;
						 if(Npc->GetRightHand()){
							 RHand = Npc->GetRightHand();
						 }
						 if(Npc->GetLeftHand()){
							 LHand = Npc->GetLeftHand();
						 }
						 Npc->DropAllInHand();
						 if(RHand) RHand->RemoveVobFromWorld();
						 if(LHand) LHand->RemoveVobFromWorld();
					 }
				}
			}
		}
		// MAKING SURE THAT TEST MODE IS OFF FOREVER !
		if(*(int*)((DWORD)oCGame::GetGame()+0x0B0) != 0) *(int*)((DWORD)oCGame::GetGame()+0x0B0) = 0;
		global_ingame->HandleInput();
		global_ingame->Draw();
	}
}
void CIngame::ClearAfterWrite(){
	oCNpc* Hero = oCNpc::GetHero();
	if(Hero->IsMovLock()) Hero->SetMovLock(0);
	Patch::PlayerInterfaceEnabled(true);
	if(WritingOnChat) WritingOnChat = false;
}
void CIngame::PrepareForWrite(){
	oCNpc* Hero = oCNpc::GetHero();
	Hero->GetAnictrl()->StopTurnAnis();
	Patch::PlayerInterfaceEnabled(false);
}
bool CIngame::PlayerExists(const char* PlayerName){
	if(client->player.size() > 1){
		for (int i = 1; i < (int)client->player.size(); i++){
			if(client->player[i]->npc){
				if(!memcmp(client->player[i]->npc->GetName().ToChar(), PlayerName, strlen(PlayerName))) return true;
			}
		}
	}
	return false;
}

zCMaterial* CIngame::GetBarriereMaterial()
{
	zCMaterial* Material = NULL;
	zCMesh* BarrierMesh = oCSkyControler_Barrier::GetCurrent()->GetBarrier()->GetBarrierMesh();
	__asm
	{
		mov eax, BarrierMesh
		mov ebx, eax
		mov ecx, ebx
		mov edx, [ecx+0x44]
		mov eax, [edx]
		mov eax, [eax+0x18]
		mov Material, eax
	}
	return Material;
}
/*oCNpc* pc;
zSTRING NODE = "ZS_TORSO";
extern oCItem* BindArrow;*/
oCNpc* Test;
void CIngame::HandleInput(){
	zCInput* input = zCInput::GetInput();
	if((input->KeyPressed(KEY_LCONTROL) || input->KeyPressed(KEY_RCONTROL)) && (input->KeyPressed(KEY_LALT) || input->KeyPressed(KEY_RALT)) && input->KeyPressed(KEY_F8)){
		if(client->network->IsConnected()){
			client->Disconnect();
			CChat::GetInstance()->WriteMessage(NORMAL, false, zCOLOR(255,0,0,255), "%s", "Disconnected!");
		}
	}
	/*if(input->KeyToggled(KEY_F4)){
		pc = oCGame::GetGame()->GetSpawnManager()->SummonNpc(zCParser::GetParser()->GetIndex("PC_Hero"), oCNpc::GetHero()->GetPosition(), 0);
		pc->SetHealth(10000);
		pc->SetMaxHealth(10000);
	}
	if(pc){
		zCModelNodeInst* Node = pc->GetModel()->SearchNode(NODE);
		zMAT4 Mat = pc->GetTrafoModelNodeToWorld(Node);
		if(BindArrow){
			zVEC3 Pos;
			Pos.x = Mat.m[0][3];
			Pos.y = Mat.m[1][3];
			Pos.z = Mat.m[2][3];
			BindArrow->SetHeadingWorld(Pos);
			//BindArrow->SetPositionWorld(Pos);
		}
		//printf("\nX : %f, Y : %f, Z : %f", Mat.m[0][3], Mat.m[1][3], Mat.m[2][3]);
	}*/
	/*if(input->KeyToggled(KEY_F4)){
		printf("\noCWorld: %X, zCWorld : %X", oCGame::GetGame()->GetGameWorld(), oCGame::GetGame()->GetWorld());
		GetBarriereMaterial()->SetTexture("BARRIERE.TGA");
		Patch::LaunchBarriere();
	}*/
	/*if(input->KeyToggled(KEY_F4)){
		printf("\nItemName : %s", oCNpc::GetHero()->GetSpellItem(19)->GetInstanceName().ToChar());
		//oCSpell* Spell = oCSpell::_CreateNewInstance();
		//Spell->InitValues(13);
		//Spell->Setup(oCNpc::GetHero(), Test, 0);
		//Spell->Cast();
	}
	if(input->KeyToggled(KEY_F5)){
		if(!Test){
		Test = oCGame::GetGame()->GetSpawnManager()->SummonNpc(zCParser::GetParser()->GetIndex("PC_Hero"), oCNpc::GetHero()->GetPosition(), 0);
		Test->SetMaxHealth(70000);
		Test->SetHealth(70000);
		Test->SetTalentSkill(7, 6);
		Test->SetTalentValue(7, 6);
		Test->Equip(Test->GetInventory()->Insert(oCObjectFactory::GetFactory()->CreateItem(zCParser::GetParser()->GetIndex("ITRU_PYROKINESIS"))));
		Test->Equip(Test->GetInventory()->Insert(oCObjectFactory::GetFactory()->CreateItem(zCParser::GetParser()->GetIndex("ITRU_FIREBOLT"))));
		Test->Equip(Test->GetInventory()->Insert(oCObjectFactory::GetFactory()->CreateItem(zCParser::GetParser()->GetIndex("ITRU_SLEEP"))));
		Test->Equip(Test->GetInventory()->Insert(oCObjectFactory::GetFactory()->CreateItem(zCParser::GetParser()->GetIndex("ITRU_GEYSER"))));
		Test->Equip(Test->GetInventory()->Insert(oCObjectFactory::GetFactory()->CreateItem(zCParser::GetParser()->GetIndex("ITRU_WHIRLWIND"))));
		Test->Equip(Test->GetInventory()->Insert(oCObjectFactory::GetFactory()->CreateItem(zCParser::GetParser()->GetIndex("ITRU_ARMYOFDARKNESS"))));
		}
		else{
			printf("\nActiveSpellName : %s", Test->GetSpellBook()->GetSelectedSpell()->GetName().ToChar());
			Test->Equip(Test->GetSpellBook()->GetSpellItem(Test->GetSpellBook()->GetSelectedSpell()));
		}
	}*/
	// PLAYER LIST
	if(input->KeyToggled(KEY_F1)){
		if(!PList->IsPlayerListOpen()) PList->OpenPlayerList();
		else PList->ClosePlayerList();
	}
	if(PList->IsPlayerListOpen()){
		if(input->KeyToggled(KEY_ESCAPE)) PList->ClosePlayerList();
		PList->UpdatePlayerList();
	}
	// MAP
	if(mapusable){
		if(input->KeyToggled(KEY_F2) && (!client->ForceHideMap)){
			if(!MMap->Opened) MMap->Open();
			else MMap->Close();
		}
		if(MMap->Opened){
			if(input->KeyToggled(KEY_ESCAPE) || (client->ForceHideMap)) MMap->Close();
			MMap->PrintMap();
		}
	}
	// ANIM MENU
	if(input->KeyToggled(KEY_F3) && !oCNpc::GetHero()->IsDead()){
			if(!AMenu->Opened)AMenu->Open();
			else AMenu->Close();
		}
	if(AMenu->Opened){
		if(input->KeyToggled(KEY_ESCAPE)) AMenu->Close();
		AMenu->PrintMenu();
	}
	// CHAT
	if(input->KeyPressed(KEY_T) && !WritingOnChat && !PList->IsPlayerListOpen()) {
		input->ClearKeyBuffer();
		WritingOnChat = true;
		PrepareForWrite();
	}
	if(WritingOnChat){
		// CHAT ANIM
		if(!oCNpc::GetHero()->IsMovLock()) oCNpc::GetHero()->SetMovLock(1);
		if(chat_interface->PrintMsgType == NORMAL){int RandomAnim = rand() % 10 + 1;
			chat_interface->StartChatAnimation(RandomAnim);}
		// INPUT
		if(input->KeyToggled(KEY_ESCAPE)) ClearAfterWrite();
		if(input->KeyPressed(KEY_BACKSPACE)){
			if(ChatTimer < clock()){
				if(chatbuffer.length()>0) chatbuffer.erase(chatbuffer.end()-1);
				ChatTimer = clock() + 150;
			}
		}
		char key=GInput::GetCharacterFormKeyboard();
		zCView::GetScreen()->SetFontColor(Normal);
		if (chat_interface->PrintMsgType != WHISPER) zCView::GetScreen()->Print(0,200 * CConfig::GetInstance()->ChatLines, arrow);
		else zCView::GetScreen()->Print(0,200 * (CConfig::GetInstance()->ChatLines + 1), arrow);
		if(key==0x0D){
			if(chatbuffer.length()!=0){
				switch (chat_interface->PrintMsgType){
					case NORMAL:
						if(!memcmp("passwd", chatbuffer.c_str(), 6) || !memcmp("login", chatbuffer.c_str(), 5)) CChat::GetInstance()->WriteMessage(NORMAL, false, zCOLOR(255,0,0), (*lang)[CLanguage::CHAT_WRONGWINDOW].ToChar());
						else{
							if(MuteTimer < clock()){
								if(SpamMessages < 3){
									client->SendMessage(chatbuffer.c_str());
									if(ChatTimer > clock()){
										SpamMessages++;
									}
									else{
										SpamMessages = 0;
									}
									ChatTimer = clock() + 3000;
									}
								else{
									MuteTimer = clock() + 60000;
									SpamMessages = 0;
									MuteCountdown = true;
								}
							}
						}
					break;
					case WHISPER:
						if(chatbuffer[0]=='/'){
							if(!memcmp(oCNpc::GetHero()->GetName().ToChar(), chatbuffer.c_str()+1, strlen(chatbuffer.c_str()+1))) chat_interface->WriteMessage(WHISPER, false, zCOLOR(255,0,0), (*lang)[CLanguage::CHAT_CANTWHISPERTOYOURSELF].ToChar());
							else {
								if(PlayerExists(chatbuffer.c_str()+1)){ WhisperingTo = chatbuffer.c_str()+1;
									chat_interface->SetWhisperTo(WhisperingTo);}
								else chat_interface->WriteMessage(WHISPER, false, zCOLOR(255,0,0), (*lang)[CLanguage::CHAT_PLAYER_DOES_NOT_EXIST].ToChar());
							}
						}
						else if(WhisperingTo.length() > 0){
							client->SendWhisper(WhisperingTo.c_str(), chatbuffer.c_str());
						}
					break;
					case ADMIN:
						client->SendCommand(chatbuffer.c_str());
						CChat::GetInstance()->WriteMessage(ADMIN, false, RED, "%s", chatbuffer.c_str());
						break;
				}
				chatbuffer.clear();
			}
			ClearAfterWrite();
		}
		else if((key>=0x20) || ((key&0x80) && (CConfig::GetInstance()->keyboardlayout == 1))){
			if(chatbuffer.length()<84) chatbuffer+=(char)key;
		}
		if(clock() > BuffTimer && clock() > SecTimer){
			BuffTimer = clock() + 750;
			SecTimer = clock() + 1500;
		}
		if(BuffTimer > clock()) {
			sprintf(buffer, "%s_",  chatbuffer.c_str());
			ChatTmp = buffer;
		}
		else ChatTmp = chatbuffer.c_str();
		if (chat_interface->PrintMsgType != WHISPER) zCView::GetScreen()->Print(200,200 * CConfig::GetInstance()->ChatLines, ChatTmp);
		else zCView::GetScreen()->Print(200,200 * (CConfig::GetInstance()->ChatLines + 1), ChatTmp);
	}
}

void CIngame::Draw(){
	this->chat_interface->PrintChat();
	if(CConfig::GetInstance()->watch) CWatch::GetInstance()->PrintWatch();
	/*if(client->IsConnected()){
			char buffer[32];
			sprintf(buffer, "Your ping: %d", client->GetPing());
			szPing=buffer;
			zCView::GetScreen()->Print(5000,0, szPing);
	}*/
}

void CIngame::CheckForUpdate(){
	if(clock()-this->last_player_update>80){
		client->UpdatePlayerStats(static_cast<short>(CActiveAniID::GetInstance()->GetAniID()));
		this->last_player_update=clock();
	}
}

void CIngame::CheckForHPDiff(){
	for(size_t i=0; i<client->player.size(); i++){
		if(client->player[i]->hp!=static_cast<short>(client->player[i]->npc->GetHealth())){
			if (!ValidatePlayerForHPDiff(client->player[i])) {
				if (client->player[i]->npc->GetHealth() <= 0) {
					client->player[i]->RespawnPlayer();
				}
				client->player[i]->npc->SetHealth(static_cast<int>(client->player[i]->hp));
			}
			client->SendHPDiff(i, static_cast<short>(static_cast<short>(client->player[i]->npc->GetHealth())-client->player[i]->hp));
			client->player[i]->hp=static_cast<short>(client->player[i]->npc->GetHealth());
		}
	}
}

bool CIngame::ValidatePlayerForHPDiff(CPlayer* player)
{
	oCNpc* hero = oCNpc::GetHero();
	if (hero == player->npc) {
		return true;
	}
	if (hero->GetFocusNpc() == player->npc && hero->GetWeaponMode() > NPC_WEAPON_NONE && hero->GetWeaponMode() < NPC_WEAPON_MAG) {
		return true;
	}
	if (hero->GetWeaponMode() == NPC_WEAPON_BOW || hero->GetWeaponMode() == NPC_WEAPON_CBOW || hero->GetWeaponMode() == NPC_WEAPON_MAG) {
		return hero->GetDistanceToVob(player->npc) < 5000.0f;
	}
	return false;
}