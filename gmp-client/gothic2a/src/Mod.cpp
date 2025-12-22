
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

#include <ctime>
#include <memory>

#include "CActiveAniID.h"
#include "CIngame.h"
#include "CServerList.h"
#include "ExceptionHandler.h"
#include "HooksManager.h"
#include "Interface.h"
#include "dev/dev_tools.h"
#include "ZenGin/zGothicAPI.h"
#include "audio/gothic_music_bridge.h"
#include "config.h"
#include "gmp_core.h"
#include "hooking/AsmPatch.h"
#include "hooking/MemoryPatch.h"
#include "language.h"
#include "main_menu.h"
#include "net_game.h"
#include "patch.h"

#pragma warning(disable : 4996)

using namespace Gothic_II_Addon;

// Legacy global - maintained for compatibility, points to GMPCore-owned instance
// TODO: migrate usages to GMPCore::Instance().GetMainMenu()
CMainMenu* MainMenu = NULL;
extern zCOLOR Red;
extern zCOLOR Normal;
extern CIngame* global_ingame;
zCOLOR Green = zCOLOR(0, 255, 0);
bool MultiplayerLaunched = false;

namespace {

constexpr DWORD kCastSpellHookAddress = 0x00485360;
constexpr DWORD kDropItemHookAddress = 0x00744DD0;
constexpr DWORD kTakeItemHookAddress = 0x007449C0;
constexpr DWORD kDoDieHookAddress = 0x00736760;
constexpr DWORD kDropUnconsciousHookAddress = 0x00735EB0;
constexpr DWORD kCallOnStateFuncHookAddress = 0x00720870;
constexpr DWORD kAINormalHookAddress = 0x004A4370;
constexpr DWORD kOnDamageAnimHookAddress = 0x00675BD0;
constexpr DWORD kOnDamageHitHookAddress = 0x00666610;

using CastSpellOriginalFn = int(__thiscall*)(oCSpell*);
using DropItemOriginalFn = int(__thiscall*)(oCNpc*, zCVob*);
using TakeItemOriginalFn = int(__thiscall*)(oCNpc*, zCVob*);
using DoDieOriginalFn = void(__thiscall*)(oCNpc*, oCNpc*);
using DropUnconsciousOriginalFn = void(__thiscall*)(oCNpc*, float, oCNpc*);
using CallOnStateFuncOriginalFn = void(__thiscall*)(oCMobInter*, oCNpc*, int);
using AINormalOriginalFn = void(__thiscall*)(zCAICamera*);
using OnDamageAnimOriginalFn = void(__thiscall*)(oCNpc*, oCNpc::oSDamageDescriptor&);
using OnDamageHitOriginalFn = void(__thiscall*)(oCNpc*, oCNpc::oSDamageDescriptor&);

CastSpellOriginalFn g_originalCastSpell = nullptr;
DropItemOriginalFn g_originalDropItem = nullptr;
TakeItemOriginalFn g_originalTakeItem = nullptr;
DoDieOriginalFn g_originalDoDie = nullptr;
DropUnconsciousOriginalFn g_originalDropUnconscious = nullptr;
CallOnStateFuncOriginalFn g_originalCallOnStateFunc = nullptr;
AINormalOriginalFn g_originalAINormal = nullptr;
OnDamageAnimOriginalFn g_originalOnDamageAnim = nullptr;
OnDamageHitOriginalFn g_originalOnDamageHit = nullptr;

}  // namespace

// Hook: zCAICamera::AI_Normal - crashfix for null camVob pointer
// The original code dereferences camVob without null check, causing crashes
void __fastcall OnAINormal(zCAICamera* thisCamera, void* /*edx*/) {
  // Check if camVob is valid before calling original
  // AI_Normal dereferences camVob multiple times without null checks
  if (!thisCamera->camVob) {
    return;  // Skip if null - prevents crash
  }
  if (g_originalAINormal) {
    g_originalAINormal(thisCamera);
  }
}

// Helper to clear items from NPC hands after death/unconscious
void ClearNpcHands(oCNpc* npc) {
  if (!npc)
    return;
  oCItem* rightHand = npc->GetRightHand() ? zDYNAMIC_CAST<oCItem>(npc->GetRightHand()) : nullptr;
  oCItem* leftHand = npc->GetLeftHand() ? zDYNAMIC_CAST<oCItem>(npc->GetLeftHand()) : nullptr;
  npc->DropAllInHand();
  if (rightHand)
    rightHand->RemoveVobFromWorld();
  if (leftHand)
    leftHand->RemoveVobFromWorld();
}

// Hook: oCNpc::DoDie - clears hands after death
void __fastcall OnDoDie(oCNpc* thisNpc, void* /*edx*/, oCNpc* attacker) {
  if (g_originalDoDie) {
    g_originalDoDie(thisNpc, attacker);
  }
  ClearNpcHands(thisNpc);
}

// Hook: oCNpc::DropUnconscious - clears hands after going unconscious
void __fastcall OnDropUnconscious(oCNpc* thisNpc, void* /*edx*/, float duration, oCNpc* attacker) {
  if (g_originalDropUnconscious) {
    g_originalDropUnconscious(thisNpc, duration, attacker);
  }
  ClearNpcHands(thisNpc);
}

// Hook: oCMobInter::CallOnStateFunc - skip SLEEPABIT state to prevent sleep exploit
void __fastcall OnCallOnStateFunc(oCMobInter* mob, void* /*edx*/, oCNpc* npc, int state) {
  // Skip "SLEEPABIT" to prevent sleep exploit in multiplayer
  if (!mob->onStateFuncName.IsEmpty() && memcmp("SLEEPABIT", mob->onStateFuncName.ToChar(), 9) == 0) {
    return;
  }
  if (g_originalCallOnStateFunc) {
    g_originalCallOnStateFunc(mob, npc, state);
  }
}

// Hook: oCNpc::OnDamage_Anim - only play damage animations for the local player
// Other players' damage animations are network-controlled
void __fastcall OnOnDamageAnim(oCNpc* thisNpc, void* /*edx*/, oCNpc::oSDamageDescriptor& damageDesc) {
  // Skip damage animations for non-player NPCs (other players handle their own)
  if (thisNpc != player) {
    return;
  }
  if (g_originalOnDamageAnim) {
    g_originalOnDamageAnim(thisNpc, damageDesc);
  }
}

// Hook: oCNpc::OnDamage_Hit - filter arrow/spell damage from other players
// In multiplayer, damage from other players' arrows/spells should be server-controlled
void __fastcall OnOnDamageHit(oCNpc* thisNpc, void* /*edx*/, oCNpc::oSDamageDescriptor& damageDesc) {
  // Skip spell damage from non-player attackers (server controls this)
  if (damageDesc.nSpellID > 0 && damageDesc.nSpellID != -1 && player != damageDesc.pNpcAttacker) {
    return;
  }
  // Skip arrow/bolt damage from non-player attackers or self-damage
  if (damageDesc.pItemWeapon) {
    const zSTRING weapon_name = damageDesc.pItemWeapon->GetInstanceName();
    if (weapon_name == "ITRW_ARROW" || weapon_name == "ITRW_BOLT") {
      if (damageDesc.pNpcAttacker && (player != damageDesc.pNpcAttacker || player == thisNpc)) {
        return;
      }
    }
  }
  if (g_originalOnDamageHit) {
    g_originalOnDamageHit(thisNpc, damageDesc);
  }
}

char bufferTemp[128];

// Installs a mid-function crashfix for oCNpc::ResetPos using AsmJit
// The original code at 0x006824F6 does: AND [EAX+0xB8], 0xFC
// If EAX (from GetAnictrl) is null, this crashes. We add a null check.
void InstallResetPosCrashfix() {
  using namespace asmjit;
  using namespace asmjit::x86;

  constexpr DWORD kPatchAddress = 0x006824F6;
  constexpr DWORD kContinueAddress = 0x006824FD;  // After the AND instruction
  constexpr DWORD kSkipAddress = 0x006827A9;      // Near function end (cleanup)

  AsmPatch::InstallMidFunctionPatch(kPatchAddress, 7, [](Assembler& a) {
    Label skipLabel = a.newLabel();

    // test eax, eax - check if anictrl is null
    a.test(eax, eax);
    // je skip - if null, skip to function cleanup
    a.je(skipLabel);
    // and byte ptr [eax+0xB8], 0xFC - clear lower 2 bits
    a.and_(byte_ptr(eax, 0xB8), 0xFC);
    // jmp continue - return to normal flow
    a.jmp(kContinueAddress);

    a.bind(skipLabel);
    // jmp cleanup - skip to function end
    a.jmp(kSkipAddress);
  });
}

// Helper function for floor sliding crashfix - checks if address is valid
static bool g_floorSlidingAddressValid = false;
static DWORD g_floorSlidingAddress = 0;

bool __cdecl CheckFloorSlidingAddress(DWORD address) {
  return !IsBadCodePtr(reinterpret_cast<FARPROC>(address));
}

// Installs a mid-function crashfix for zCAIPlayer::CheckFloorSliding
// The original code calls GetCollisionObject twice - between the first null check
// and the second call, the collision data pointer could become null in multiplayer.
// At 0x0050D5C9: MOV EAX, [EAX+0xD0] - gets the collision data pointer
// At 0x0050D5CF: ADD EAX, 0xC then MOV EDX, [EAX] - crashes if pointer was null.
// Fix: Add null check on the collision data pointer loaded at 0x0050D5C9.
void InstallFloorSlidingCrashfix() {
  using namespace asmjit;
  using namespace asmjit::x86;

  // Patch at 0x0050D5C9 where we load [EAX+0xD0]
  // Original: MOV EAX, [EAX+0xD0]  (8B 80 D0 00 00 00) - 6 bytes
  //           ADD EAX, 0xC         (83 C0 0C)          - 3 bytes
  //           MOV EDX, [EAX]       (8B 10)             - 2 bytes
  // Total: 11 bytes we can use
  constexpr DWORD kPatchAddress = 0x0050D5C9;
  constexpr DWORD kContinueAddress = 0x0050D5D4;  // After MOV EDX, [EAX]
  constexpr DWORD kSkipAddress = 0x0050D610;      // Where original null check jumps to

  AsmPatch::InstallMidFunctionPatch(kPatchAddress, 11, [](Assembler& a) {
    Label validLabel = a.newLabel();

    // mov eax, [eax+0xD0] - get collision data pointer (original instruction)
    a.mov(eax, dword_ptr(eax, 0xD0));
    // test eax, eax - null check (the fix!)
    a.test(eax, eax);
    // jnz valid - continue if not null
    a.jnz(validLabel);
    // jmp skip - go to where the original null check would have jumped
    a.jmp(kSkipAddress);

    a.bind(validLabel);
    // add eax, 0xC - original instruction
    a.add(eax, 0xC);
    // mov edx, [eax] - original instruction (now safe)
    a.mov(edx, dword_ptr(eax));
    // jmp continue
    a.jmp(kContinueAddress);
  });
}

const int DROP_ITEM_TIMEOUT = 200;

// DROP & TAKE
int __fastcall OnDropItem(oCNpc* thisNpc, void* /*unusedEdx*/, zCVob* vob) {
  oCItem* item = vob ? zDYNAMIC_CAST<oCItem>(vob) : nullptr;
  int result = g_originalDropItem ? g_originalDropItem(thisNpc, vob) : 0;

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

int __fastcall OnTakeItem(oCNpc* thisNpc, void* /*unusedEdx*/, zCVob* vob) {
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

int __fastcall OnCastSpell(oCSpell* thisSpell) {
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

// Take distance patch - C++ callback for distance check
zSTRING TakeTooFarMessage;
bool __stdcall CheckIfDistanceIsCorrect(oCMsgManipulate* Msg, oCNpc* Npc) {
  if (Npc == player && Msg) {
    if (Msg->targetVob) {
      if (Npc->GetDistanceToVob(*Msg->targetVob) < 240.0f) {
        return true;
      } else if (oCItem* Item = zDYNAMIC_CAST<oCItem>(Msg->targetVob)) {
        sprintf(bufferTemp, "%s %s", Item->name.ToChar(), Language::Instance()[Language::ITEM_TOOFAR].ToChar());
        TakeTooFarMessage = bufferTemp;
        ogame->array_view[oCGame::GAME_VIEW_SCREEN]->PrintTimedCXY(TakeTooFarMessage, 4000.0f, 0);
        return false;
      }
    }
  }
  return true;
}

// Installs the take distance patch using AsmJit
// This patch intercepts item pickup to check if player is close enough
void InstallDistanceTakeFix() {
  using namespace asmjit;
  using namespace asmjit::x86;

  constexpr DWORD kPatchAddress = 0x0074C37C;
  constexpr DWORD kReturnAddress = 0x0074C6C4;
  constexpr DWORD kEvTakeVobAddress = 0x007534E0;  // oCNpc::EV_TakeVob

  AsmPatch::InstallMidFunctionPatch(kPatchAddress, 6, [](Assembler& a) {
    Label skipLabel = a.newLabel();

    // Set up call to CheckIfDistanceIsCorrect(Msg, Npc)
    // At this point: ESI = this (oCNpc*), EBP = oCMsgManipulate*
    a.mov(ecx, esi);  // Npc in ecx (but we push it)
    a.push(ecx);      // Push Npc (2nd arg - stdcall)
    a.push(ebp);      // Push Msg (1st arg - stdcall)
    a.call(reinterpret_cast<uint64_t>(&CheckIfDistanceIsCorrect));
    // stdcall cleans up stack automatically

    // Check result
    a.test(al, al);
    a.je(skipLabel);

    // Distance OK - call EV_TakeVob
    a.push(ebp);      // Push Msg parameter
    a.mov(ecx, esi);  // this pointer
    a.call(kEvTakeVobAddress);
    a.jmp(kReturnAddress);

    a.bind(skipLabel);
    // Distance too far - skip taking the item
    a.jmp(kReturnAddress);
  });
}

void Initialize(void) {
  if (!MultiplayerLaunched) {
    MultiplayerLaunched = true;
    HooksManager* hm = HooksManager::GetInstance();

    // Initialize dev tools hooks (weather override, etc.)
    debug::DevTools::Instance().InitHooks();

    // Register task scheduler render hook - processes queued main-thread tasks every frame
    hm->AddHook(HT_RENDER, (DWORD)NetGame::ProcessTaskScheduler);

    CActiveAniID* ani_ptr = new CActiveAniID();
    if (auto original = CreateHook(kCastSpellHookAddress, (DWORD)OnCastSpell)) {
      g_originalCastSpell = reinterpret_cast<CastSpellOriginalFn>(*original);
    }
    if (auto original = CreateHook(kDropItemHookAddress, (DWORD)OnDropItem)) {
      g_originalDropItem = reinterpret_cast<DropItemOriginalFn>(*original);
    }
    if (auto original = CreateHook(kTakeItemHookAddress, (DWORD)OnTakeItem)) {
      g_originalTakeItem = reinterpret_cast<TakeItemOriginalFn>(*original);
    }
    // oCNpc::DoDie - clear hands after death
    if (auto original = CreateHook(kDoDieHookAddress, (DWORD)OnDoDie)) {
      g_originalDoDie = reinterpret_cast<DoDieOriginalFn>(*original);
    }
    // oCNpc::DropUnconscious - clear hands after going unconscious
    if (auto original = CreateHook(kDropUnconsciousHookAddress, (DWORD)OnDropUnconscious)) {
      g_originalDropUnconscious = reinterpret_cast<DropUnconsciousOriginalFn>(*original);
    }
    // oCMobInter::CallOnStateFunc - skip SLEEPABIT to prevent sleep exploit
    if (auto original = CreateHook(kCallOnStateFuncHookAddress, (DWORD)OnCallOnStateFunc)) {
      g_originalCallOnStateFunc = reinterpret_cast<CallOnStateFuncOriginalFn>(*original);
    }
    // zCAICamera::AI_Normal - crashfix for null pointer dereference
    if (auto original = CreateHook(kAINormalHookAddress, (DWORD)OnAINormal)) {
      g_originalAINormal = reinterpret_cast<AINormalOriginalFn>(*original);
    }
    // oCNpc::OnDamage_Anim - only play damage anims for local player
    if (auto original = CreateHook(kOnDamageAnimHookAddress, (DWORD)OnOnDamageAnim)) {
      g_originalOnDamageAnim = reinterpret_cast<OnDamageAnimOriginalFn>(*original);
    }
    // oCNpc::OnDamage_Hit - filter arrow/spell damage from other players
    if (auto original = CreateHook(kOnDamageHitHookAddress, (DWORD)OnOnDamageHit)) {
      g_originalOnDamageHit = reinterpret_cast<OnDamageHitOriginalFn>(*original);
    }
    // Patch for FindMobInter
    EraseMemory(0x00740006, 0x6A, 1);
    EraseMemory(0x00740007, 0x00, 1);
    EraseMemory(0x00740008, 0x8B, 1);
    EraseMemory(0x00740009, 0xCE, 1);
    CallPatch(0x0074000A, 0x00719620, 8);
    // ResetPos CrashFix
    InstallResetPosCrashfix();
    // Take distance patch
    InstallDistanceTakeFix();
    // Floor Sliding Crashfix
    InstallFloorSlidingCrashfix();
    SetupExceptionHandler();
    // Initialize language system
    LanguageManager::Instance().LoadLanguages(".\\Multiplayer\\Localization\\", Config::Instance().lang);
    // Initialize music bridge for zCOptions integration
    gmp::audio::GothicMusicBridge::Initialize();
    // Initialize GMPCore - the central application owner
    GMPCore::Instance().Initialize();
    MainMenu = GMPCore::Instance().GetMainMenu();  // Legacy compatibility
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
