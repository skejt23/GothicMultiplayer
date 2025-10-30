
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
#include "ZenGin/zGothicAPI.h"
#include "net_game.h"
#include "patch.h"
#include "ExceptionHandler.h"
#include "HooksManager.h"
#include "Interface.h"
#include "hooking/MemoryPatch.h"
#include "config.h"
#include "CServerList.h"
#include "main_menu.h"
#include "CActiveAniID.h"
#include "CIngame.h"
#include "language.h"
#include <ctime>
#include <memory>

#pragma warning(disable:4996)

using namespace Gothic_II_Addon;

CMainMenu* MainMenu=NULL;
extern zCOLOR Red;
extern zCOLOR Normal;
extern CIngame* global_ingame;
zCOLOR Green = zCOLOR(0,255,0);
bool MultiplayerLaunched = false;

namespace {

constexpr DWORD kCastSpellHookAddress = 0x00485360;
constexpr DWORD kDropItemHookAddress = 0x00744DD0;
constexpr DWORD kTakeItemHookAddress = 0x007449C0;

using CastSpellOriginalFn = int(__thiscall*)(oCSpell*);
using DropItemOriginalFn = int(__thiscall*)(oCNpc*, zCVob*);
using TakeItemOriginalFn = int(__thiscall*)(oCNpc*, zCVob*);

CastSpellOriginalFn g_originalCastSpell = nullptr;
DropItemOriginalFn g_originalDropItem = nullptr;
TakeItemOriginalFn g_originalTakeItem = nullptr;

} // namespace

// MIEJSCE SKEJTA NA ROZNE ASSEMBLEROWE RZECZY
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
			RHand = dynamic_cast<oCItem*>(Npc->GetRightHand());
		}
		if(Npc->GetLeftHand()){
			LHand = dynamic_cast<oCItem*>(Npc->GetLeftHand());
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
oCMobInter* Mob;
bool _stdcall IsSleepABit()
{
	if(Mob->onStateFuncName.IsEmpty()) return false;
	else if(!memcmp("SLEEPABIT", Mob->onStateFuncName.ToChar(), 9)){
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
void _stdcall CheckDamageForArrows(oCNpc* Npc, int howmuch, oCNpc::oSDamageDescriptor& Des)
{
	if(Des.nSpellID > 0 && Des.nSpellID != -1 && player != Des.pNpcAttacker) return;
    if(Des.pItemWeapon){
		if(!memcmp("ITRW_ARROW", Des.pItemWeapon->GetInstanceName().ToChar(), 10) || !memcmp("ITRW_BOLT", Des.pItemWeapon->GetInstanceName().ToChar(), 9)){
			if(Des.pNpcAttacker) if(player != Des.pNpcAttacker || player == Npc) return;
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
void __stdcall PrepareKillMessage()
{
    if(player->GetFocusNpc()){
        sprintf(bufferTemp, "%s %s", Language::Instance()[Language::KILLEDSOMEONE_MSG].ToChar(), player->GetFocusNpc()->GetName().ToChar());
        TestPrint = bufferTemp;
    }
    else{
        TestPrint = "";
    }
}

DWORD PrepareKillMessageAddr = (DWORD)PrepareKillMessage;

void _declspec(naked) PrintKilledMessage()
{
    __asm 
    {
        pushad
        call PrepareKillMessageAddr
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
int __fastcall OnDropItem(oCNpc* thisNpc, void* /*unusedEdx*/, zCVob* vob)
{   
	oCItem* item = vob ? zDYNAMIC_CAST<oCItem>(vob) : nullptr;
	int result = g_originalDropItem ? g_originalDropItem(thisNpc, vob) : 0;
  SPDLOG_INFO("OnDropItem called for npc {0}", (void*)thisNpc);

	if (thisNpc != player || !item) {
		return result;
	}

	static int dropItemTimeout = 0;
	if (global_ingame && dropItemTimeout < GetTickCount()) {
		if (!NetGame::Instance().DropItemsAllowed) {
			return result;
		}
		NetGame::Instance().SendDropItem(item->GetInstance(), item->amount);
		dropItemTimeout = GetTickCount() + DROP_ITEM_TIMEOUT;
	}

	return result;
}

int __fastcall OnTakeItem(oCNpc* thisNpc, void* /*unusedEdx*/, zCVob* vob)
{   
	int result = g_originalTakeItem ? g_originalTakeItem(thisNpc, vob) : 0;

	if (thisNpc != player) {
		return result;
	}

	oCItem* item = vob ? zDYNAMIC_CAST<oCItem>(vob) : nullptr;
	if (item && global_ingame) {
		if (!NetGame::Instance().DropItemsAllowed) {
			return result;
		}
		NetGame::Instance().SendTakeItem(item->GetInstance());
	}

	return result;
}

int __fastcall OnCastSpell(oCSpell* thisSpell)
{  
	int result = g_originalCastSpell ? g_originalCastSpell(thisSpell) : 0;

	if ((DWORD)thisSpell->spellCasterNpc == (DWORD)player) {
		if (global_ingame) {
			if (thisSpell->spellTargetNpc) {
				if (thisSpell->GetSpellID() == 46 && !global_ingame->Shrinker->IsShrinked(thisSpell->spellTargetNpc)) {
					global_ingame->Shrinker->ShrinkNpc(thisSpell->spellTargetNpc);
				}
				NetGame::Instance().SendCastSpell(thisSpell->spellTargetNpc, thisSpell->GetSpellID());
			} else {
				NetGame::Instance().SendCastSpell(0, thisSpell->GetSpellID());
			}
		}
	}

	return result;
}

// Take distance patch
zSTRING TakeTooFarMessage;
bool _stdcall CheckIfDistanceIsCorrect(oCMsgManipulate* Msg, oCNpc* Npc)
{	   
	if(Npc == player && Msg){
		if(Msg->targetVob){
			if(Npc->GetDistanceToVob(*Msg->targetVob) < 240.0f){
				return true;
			}
			else if(oCItem* Item = zDYNAMIC_CAST<oCItem>(Msg->targetVob)){
				sprintf(bufferTemp, "%s %s", Item->name.ToChar(), Language::Instance()[Language::ITEM_TOOFAR].ToChar());
				TakeTooFarMessage = bufferTemp;
				ogame->array_view[oCGame::GAME_VIEW_SCREEN]->PrintTimedCXY(TakeTooFarMessage, 4000.0f, 0);
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

void Initialize(void)
{
	if(!MultiplayerLaunched){
		MultiplayerLaunched = true;
		HooksManager * hm = HooksManager::GetInstance();
		CActiveAniID *ani_ptr=new CActiveAniID();
		if (auto original = CreateHook(kCastSpellHookAddress, (DWORD)OnCastSpell)) {
			g_originalCastSpell = reinterpret_cast<CastSpellOriginalFn>(*original);
		}
		if (auto original = CreateHook(kDropItemHookAddress, (DWORD)OnDropItem)) {
			g_originalDropItem = reinterpret_cast<DropItemOriginalFn>(*original);
		}
		if (auto original = CreateHook(kTakeItemHookAddress, (DWORD)OnTakeItem)) {
			g_originalTakeItem = reinterpret_cast<TakeItemOriginalFn>(*original);
		}
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
		// Initialize language system
		LanguageManager::Instance().LoadLanguages(".\\Multiplayer\\Localization\\", Config::Instance().lang);
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
