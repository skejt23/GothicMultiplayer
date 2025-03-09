
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

#include "Mod.h"
#include "g2Api.h"
#include "CGmpClient.h"
#include "patch.h"
#include "ExceptionHandler.h"
#include "HooksManager.h"
#include "Interface.h"
#include "InjectMage.h"
#include "CConfig.h"
#include "CServerList.h"
#include "CMainMenu.h"
#include "CActiveAniID.h"
#include "CIngame.h"
#include <ctime>

#pragma warning(disable:4996)
CGmpClient* client=NULL;
CMainMenu* MainMenu=NULL;
extern zCOLOR Red;
extern CLanguage* Lang;
extern zCOLOR Normal;
extern zCView* PrintTimedScreen;
extern CIngame* global_ingame;
zCOLOR Green = zCOLOR(0,255,0);
bool MultiplayerLaunched = false;

// MIEJSCE SKEJTA NA RӯNE ASSEMBLEROWE RZECZY
// CRASHFIXY
DWORD RETURN_CRASHFIX1 = 0x004A4382;
DWORD RETURN_CRASHFIX2 = 0x004A454C;
void _declspec(naked) ZCAICAMERA_CRASHFIX()
{
    _asm
    {
        test    eax, eax
        je     cont 

        fld     [eax+0x68]
		add     eax, 60
        jmp     RETURN_CRASHFIX1
    cont:
        jmp     RETURN_CRASHFIX2
    }
}

// HOOKZ
void _stdcall PutToInv(DWORD Ptr)
{
	oCNpc* Npc = (oCNpc*)Ptr;
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

DWORD FuncName = (DWORD)PutToInv;
DWORD RETURN_FROMCLEAR = 0x0073689F;
void _declspec(naked) ClearHandsAfterDeath()
{
	_asm
	{
		push ecx
		call FuncName
		jmp RETURN_FROMCLEAR
	}
}
DWORD RETURN_TOUNC = 0x00735FB2;
void _declspec(naked) ClearHandsAfterUnc()
{
	_asm
	{
		push ecx
		call FuncName
		jmp RETURN_TOUNC
	}
}

// DAMAGE FIX
DWORD RETURN_TOONDAMAGE = 0x00666535;
DWORD DAMAGEONANIM = 0x00675BD0;
void _declspec(naked) CheckDamageIfForHero()
{
	_asm
	{
		test ecx, DWORD PTR DS:[0x00AB2684]
		jne retrn
		push esi
		call DAMAGEONANIM
		jmp retrn
	retrn:
		jmp RETURN_TOONDAMAGE
	}
}
// NO SLEEP FIX
DWORD RETURN_TOCSTATE = 0x00720877;
DWORD RETURN_TOENDCSTATE = 0x00720AC1;
oCMob* Mob;
bool _stdcall IsSleepABit()
{
	if(Mob->GetStateFuncName().IsEmpty()) return false;
	else if(!memcmp("SLEEPABIT", Mob->GetStateFuncName().ToChar(), 9)){
		return true;
	}
	return false;
}
DWORD CheckForSleep = (DWORD)IsSleepABit;
void _declspec(naked) CheckCallStateFunc()
{
	_asm mov Mob, ecx
	if(IsSleepABit()){
		_asm jmp RETURN_TOENDCSTATE;
		}
	else{
	_asm
	{
		mov ecx, Mob
		push -1
		push 0x00822829
		jmp RETURN_TOCSTATE
	}
	}
}

// Patch for arrows damage
void _stdcall CheckDamageForArrows(oCNpc* Npc, int howmuch, oSDamageDescriptor& Des)
{
	if(Des.nSpellID > 0 && Des.nSpellID != -1 && oCNpc::GetHero() != Des.pNpcAttacker) return;
    if(Des.pItemWeapon){
		if(!memcmp("ITRW_ARROW", Des.pItemWeapon->GetInstanceName().ToChar(), 10) || !memcmp("ITRW_BOLT", Des.pItemWeapon->GetInstanceName().ToChar(), 9)){
			/*BindArrow = oCObjectFactory::GetFactory()->CreateItem(Des.pItemWeapon->GetInstance());
			oCGame::GetGame()->GetWorld()->AddVob(BindArrow);
			BindArrow->SetCollDet(0);
			BindArrow->SetTrafoObjToWorld(Des.pItemWeapon->trafoObjToWorld);
			//BindArrow->SetPositionWorld(Des.pItemWeapon->GetPositionWorld());
			zSTRING NODE = "ZS_TORSO";
			zCModelNodeInst* Node = Npc->GetModel()->SearchNode(NODE);
			zMAT4 Mat = Npc->GetTrafoModelNodeToWorld(Node);
			printf("\nX : %f, Y : %f, Z : %f", Mat.m[0][3], Mat.m[1][3], Mat.m[2][3]);
			//Npc->GetModel()->AttachChildVobToNode(BindArrow, Node);
			//Npc->GetModel()->UpdateAttachedVobs();
			oCNpc::GetHero()->SetPosition(BindArrow->GetPositionWorld().x, BindArrow->GetPositionWorld().y, BindArrow->GetPositionWorld().z);*/
			if(Des.pNpcAttacker) if(oCNpc::GetHero() != Des.pNpcAttacker || oCNpc::GetHero() == Npc) return;
		}
	}
	Npc->ChangeAttribute(0, howmuch);
}
DWORD Damagefuncadr = (DWORD)CheckDamageForArrows;
DWORD Backtofunc = 0x0066CAD3;
void _declspec(naked) CheckDamage()
{
	__asm 
	{
		mov eax, [esp+0x284]
		push eax
		neg edi
		push edi
		mov edx, ebp
		push edx
		call Damagefuncadr
		jmp Backtofunc
	}
}


// SHOW WHO KILLED PATCH
DWORD RETURNTOPRINT = 0x006E2FC2;
zSTRING TestPrint = "";
zSTRING* Wat = &TestPrint;
char bufferTemp[128];
void _declspec(naked) PrintKilledMessage()
{
	__asm pushad
	if(oCNpc::GetHero()->GetFocusNpc()){
		sprintf(bufferTemp, "%s %s", (*Lang)[CLanguage::KILLEDSOMEONE_MSG].ToChar(), oCNpc::GetHero()->GetFocusNpc()->GetName().ToChar());
		TestPrint = bufferTemp;
	}
	else{
		TestPrint = "";
	}
	__asm
	{
		popad
		mov edx, Wat
		fstp DWORD PTR DS:[esp]
		jmp RETURNTOPRINT
	}
};

// CRASHFIX RESETPOS
DWORD RETURN_RESETCRASHFIX1 = 0x006824FD;
DWORD RETURN_RESETCRASHFIX2 = 0x006827A9;
void _declspec(naked) RESETPOS_CRASHFIX()
{
    _asm
    {
        test    eax, eax
        je     cont 

//		and BYTE PTR DS:[eax+0x0B8], 252   <- tej wersji cos visual nie moze przetrawic
		and		[eax+0x0B8], 252
        jmp     RETURN_RESETCRASHFIX1
    cont:
		//pushad
		//push ebx
		//call RESTARTNPC
		//popad
        jmp     RETURN_RESETCRASHFIX2
    }
}

// CRASHFIX FLOORSLIDING/NO IDEA WHY IT CRASHES.
DWORD RETURN_SLIDINGCRASHFIX1 = 0x0050D5D4;
DWORD RETURN_SLIDINGCRASHFIX2 = 0x0050D56A;
FARPROC ADDRESS;
void _declspec(naked) FLOORSLIDING_CRASHFIX() // 0x0050D5CF
{
	_asm 
	{
		add eax, 12
		mov ADDRESS, eax
		pushad
	}
	if(IsBadCodePtr(ADDRESS))
	{
		_asm
		{
			popad
			jmp RETURN_SLIDINGCRASHFIX2
		}
	}
	else{
		_asm
		{
			popad
			mov edx, [eax]
			jmp RETURN_SLIDINGCRASHFIX1
		}
	}
};

const int DROP_ITEM_TIMEOUT = 200;

// DROP & TAKE
void _stdcall OnDropItem(sRegs & regs, DWORD & item)
{	   
	if ((DWORD)regs.ECX != (DWORD)oCNpc::GetHero()) {
		return;
	}
	oCItem* DroppedItem = (oCItem*)item;
	short amount = DroppedItem->GetAmount();
	static int dropItemTimeout = 0;
	if(client && global_ingame && dropItemTimeout < GetTickCount()){
		if(!client->DropItemsAllowed) return;
		client->SendDropItem(DroppedItem->GetInstance(), amount);
		dropItemTimeout = GetTickCount() + DROP_ITEM_TIMEOUT;
	}
}

void _stdcall OnTakeItem(sRegs & regs, DWORD & item)
{	   
		if((DWORD)regs.ECX == (DWORD)oCNpc::GetHero()){
				oCItem* TakenItem = (oCItem*)item;
				if(client && global_ingame){
					if(!client->DropItemsAllowed) return;
					client->SendTakeItem(TakenItem->GetInstance());
				}
		}
}

void _stdcall OnCastSpell(sRegs & regs)
{	  
	oCSpell* CastedSpell = (oCSpell*)regs.ECX;
	if((DWORD)CastedSpell->GetCaster() == (DWORD)oCNpc::GetHero()){
			if(client && global_ingame){
				if(CastedSpell->GetTarget()){
					if(CastedSpell->GetSpellID() == 46) if(!global_ingame->Shrinker->IsShrinked(CastedSpell->GetTarget())) global_ingame->Shrinker->ShrinkNpc(CastedSpell->GetTarget());
					client->SendCastSpell(CastedSpell->GetTarget(), CastedSpell->GetSpellID());
				}
				else client->SendCastSpell(0, CastedSpell->GetSpellID());
			}
	}
}

// Take distance patch
zSTRING TakeTooFarMessage;
bool _stdcall CheckIfDistanceIsCorrect(oCMsgManipulate* Msg, oCNpc* Npc)
{	   
	if(Npc == oCNpc::GetHero() && Msg){
		if(Msg->GetVob()){
			if(Npc->GetDistanceToVob(Msg->GetVob()) < 240.0f){
				return true;
			}
			else if(GetPointerType((DWORD)Msg->GetVob()) == VOB_TYPE_VT_OCITEM){
				oCItem* Item = static_cast<oCItem*>(Msg->GetVob());
				sprintf(bufferTemp, "%s %s", Item->GetItemName().ToChar(), (*Lang)[CLanguage::ITEM_TOOFAR].ToChar());
				TakeTooFarMessage = bufferTemp;
				PrintTimedScreen->PrintTimedCXY(TakeTooFarMessage, 4000.0f, 0);
				return false;
			}
		}
	}
	return true;
}
DWORD RETURN_TOTAKEVOB = 0x0074C6C4;
DWORD EV_TAKEVOB = 0x007534E0;
DWORD CHECKDISTANCEADRESS = (DWORD)CheckIfDistanceIsCorrect;
void _declspec(naked) DISTANCE_TAKEFIX()
{
    _asm
    {
		mov ecx, esi
		push ecx
		push ebp
		call CHECKDISTANCEADRESS
		test al, al
		je nextlbl
		push ebp
		mov ecx, esi
		call EV_TAKEVOB
		jmp RETURN_TOTAKEVOB
nextlbl:
		jmp RETURN_TOTAKEVOB
    }
}


zCMover* Vob;
void _stdcall OnTrigger(sRegs & regs, DWORD & Vob1, DWORD & Vob2)
{	   
	oCMobInter* TriggerMob = (oCMobInter*)regs.ECX;
	printf("\nPOINTER : %p, TriggerName : %s", TriggerMob, TriggerMob->GetTriggerName().ToChar());
	Vob = static_cast<zCMover*>(oCGame::GetGame()->GetGameWorld()->SearchVobByName(TriggerMob->GetTriggerName()));
	printf("\nVobPos : %f,%f,%f", Vob->GetPositionWorld().x, Vob->GetPositionWorld().y, Vob->GetPositionWorld().z);

}
/*void _stdcall OnRenderBarrier(sRegs & regs, DWORD & RenderContext, int & Val1, int & Val2)
{	  
	//printf("\nRNDPTR : %X, Value : %d, %d", RenderContext, Val1, Val2);
	//RenderContext.World = oCGame::GetGame()->GetGameWorld();
	int lol = 1;
	zTRenderContext Con = *(zTRenderContext*)RenderContext;
	printf("\nWorld : %X", Con.World);
	oCSkyControler_Barrier::GetCurrent()->GetBarrier()->RenderLayer(*(zTRenderContext*)RenderContext, 0, lol);
}*/

// Walnij CreateHook(0x0057B0C0, (DWORD)OnStartAni, 2, true); tam gdzie chcesz żeby zaczeło pobierać animacje
/*zSTRING TURN = "TURN";
void _stdcall OnStartAni(sRegs & regs, DWORD & ModelAni, int arg2)
{	   
	if(oCNpc::GetHero()){
		if((DWORD)regs.ECX == (DWORD)oCNpc::GetHero()->GetModel()){
			zCModelAni* Ani = (zCModelAni*)ModelAni;
			printf("\nAniName : %s, PTR : %X, ANIACTIVE : %X, MODEL : %X", Ani->GetAniName().ToChar(), Ani, oCNpc::GetHero()->GetModel()->GetModelAniActive(), oCNpc::GetHero()->GetModel());
			//printf("\nAniId : %d", Ani->GetAniID());
			if(Ani){
				if(Ani->GetAniName().Search(TURN) < 2) {
						CActiveAniID::GetInstance()->AddAni(Ani->GetAniID());
						if(!memcmp("T_WALKL_2_RUNL", Ani->GetAniName().ToChar(), 14) || !memcmp("T_JUMP_2_RUNL", Ani->GetAniName().ToChar(), 13)) CActiveAniID::GetInstance()->AddAni(266);
				}
			}
			// PRZEKAZANIE ID DO FUNKCJI CZY COS
		}
	}
}*/

// BLOKOWANIE CHEAT ENGINE, OLLYDBG
BOOL CALLBACK CheckIfCheatEngineIsRunning(HWND hWnd, LPARAM lParam)
{
	char title[128];
    ZeroMemory(title, sizeof(title));
    GetWindowTextA(hWnd, title, sizeof(title));
    BYTE buffer[4] = {0xB0, 0x93, 0x93, 0x86};
	for(int i=0; i<4; i++){
		buffer[i]^=0xFF;
	}
	char Text[32];
	ZeroMemory(Text, 32);
	for(int i=0; i<4; i++){
		Text[i]=buffer[i];
	}
	// CHEATENGINE
	BYTE buffer1[5]={0xBC, 0x97, 0x9A, 0x9E, 0x8B};
	for(int i=0; i<5; i++){
		buffer1[i]^=0xFF;
	}
	char TextCE[32];
	ZeroMemory(TextCE, 32);
	for(int i=0; i<5; i++){
		TextCE[i]=buffer1[i];
	}
    if (strstr(title, TextCE) || strstr(title, Text))
	{
		DWORD* processID = new DWORD;
		GetWindowThreadProcessId(hWnd, processID);
		HANDLE hProcess = OpenProcess(PROCESS_TERMINATE,0, *processID);
		TerminateProcess(hProcess,0);
		CloseHandle(hProcess);
		exit(1);
	}
	ZeroMemory(Text, 32);
	ZeroMemory(TextCE, 32);
    return TRUE;
}

time_t CECheck = 0;
DWORD WINAPI CheckForBadApps(PVOID pvParam)
{
	while(true)
	{
		if(CECheck < clock()){
			EnumWindows(CheckIfCheatEngineIsRunning, 0);
			CECheck = clock() + 5000;
		}
		Sleep(1);
	}
	return 0;
};

void Initialize(void)
{
	if(!MultiplayerLaunched){
		MultiplayerLaunched = true;
		HooksManager * hm = HooksManager::GetInstance();
		CActiveAniID *ani_ptr=new CActiveAniID();
		//CreateHook(0x0071E7D0, (DWORD)OnTrigger, 2, true);
		CreateHook(0x00485360, (DWORD)OnCastSpell, 0, true);
		CreateHook(0x00744DD6, (DWORD)OnDropItem, 1, true);
		CreateHook(0x007449C0, (DWORD)OnTakeItem, 1, true);
		//CreateHook(0x0057B0C0, (DWORD)OnStartAni, 2, true);
		//CreateHook(0x006B9F30, (DWORD)OnRenderBarrier, 3, true);
		// zCAICamera::AI_Normal(void) CrashFix
		JmpPatch(0x004A437C, (DWORD)ZCAICAMERA_CRASHFIX);
		EraseMemory(0x004A4381, 0x90, 1);
		// ClearInv after death fix
		JmpPatch(0x0073689A, (DWORD)ClearHandsAfterDeath);
		JmpPatch(0x00735FAD, (DWORD)ClearHandsAfterUnc);
		// Patch for Damage Anims
		EraseMemory(0x0066652D, 0x90, 1);
		JmpPatch(0x00666530, (DWORD)CheckDamageIfForHero);
		// Patch for CallStateFunc
		EraseMemory(0x00720870, 0x90, 2);
		JmpPatch(0x00720872, (DWORD)CheckCallStateFunc);
		// Patch for arrwos damage
		EraseMemory(0x0066CAC7, 0x90, 7);
		JmpPatch(0x0066CACE, (DWORD)CheckDamage);
		// Patch for FindMobInter
		EraseMemory(0x00740006, 0x6A, 1);
		EraseMemory(0x00740007, 0x00, 1);
		EraseMemory(0x00740008, 0x8B, 1);
		EraseMemory(0x00740009, 0xCE, 1);
		CallPatch(0x0074000A, 0x00719620, 8);
		// PrintTimed EXP Patch
		EraseMemory(0x006E2FC0, 0x90, 2);
		JmpPatch(0x006E2FBB, (DWORD)PrintKilledMessage);
		// ResetPos CrashFix
		EraseMemory(0x006824FB, 0x90, 2);
		JmpPatch(0x006824F6, (DWORD)RESETPOS_CRASHFIX);
		// Take distance patch
		EraseMemory(0x0074C37F, 0x90, 3);
		JmpPatch(0x0074C37C, (DWORD)DISTANCE_TAKEFIX);
		// Floor Sliding Crashfix
		JmpPatch(0x0050D5CF, (DWORD)FLOORSLIDING_CRASHFIX);
		SetupExceptionHandler();
		MainMenu = CMainMenu::GetInstance();
		Patch::FixSetTime();
		Patch::DisableCheat();
		Patch::DisablePause();
		Patch::FixLights();
		Patch::FixApplyOverlay();
		Patch::EraseCastSecurity();
		Patch::DisableGothicMainMenu();
		Patch::DisableWriteSavegame();
		Patch::DisableChangeSightKeys();
		Patch::ChangeLevelEnabled(false);
		Patch::SetLogScreenEnabled(false);
		Patch::DisableInjection();
	}
}