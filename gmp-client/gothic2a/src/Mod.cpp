
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

#include <algorithm>
#include <cmath>
#include <ctime>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "CActiveAniID.h"
#include "CIngame.h"
#include "CServerList.h"
#include "ExceptionHandler.h"
#include "HooksManager.h"
#include "Interface.h"
#include "ZenGin/Gothic_II_Addon/API/zParser_Const.h"
#include "ZenGin/zGothicAPI.h"
#include "actor_sync.h"
#include "audio/gothic_music_bridge.h"
#include "config.h"
#include "dev/dev_tools.h"
#include "gmp_core.h"
#include "hooking/AsmPatch.h"
#include "hooking/MemoryPatch.h"
#include "item_instance.h"
#include "language.h"
#include "main_menu.h"
#include "native_damage_observation.h"
#include "net_game.h"
#include "npc_animation_utils.h"
#include "patch.h"
#include "scripting/gothic_events.h"
#include "scripting/item_ground.h"
#include "scripting/process_input.h"
#include "shared/event.h"
#include "summon_spells.h"
#include "transformation_spells.h"
#include "torch_utils.h"

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
constexpr DWORD kSpellBookCastHookAddress = 0x004767A0;
constexpr DWORD kEndTimedEffectHookAddress = 0x00486E10;
constexpr DWORD kBeamToHookAddress = 0x00736EE0;
constexpr DWORD kTriggerTeleportHookAddress = 0x00614440;
constexpr DWORD kTriggerScriptHookAddress = 0x0043C4C0;
// Hook DoDropVob, not EV_DropVob, so the dropped item has a world transform.
constexpr DWORD kDropItemHookAddress = 0x00744DD0;
constexpr DWORD kTakeItemHookAddress = 0x007534E0;
constexpr DWORD kUseItemHookAddress = 0x00755620;
constexpr DWORD kUseItemToStateHookAddress = 0x007558F0;
constexpr DWORD kNativeUseItemHookAddress = 0x0073BC10;
constexpr DWORD kEquipItemHookAddress = 0x007323C0;
constexpr DWORD kUnequipItemHookAddress = 0x007326C0;
constexpr DWORD kDoDieHookAddress = 0x00736760;
constexpr DWORD kAttackFinishHookAddress = 0x00751AF0;
constexpr DWORD kDropUnconsciousHookAddress = 0x00735EB0;
constexpr DWORD kDropAllInHandHookAddress = 0x007375E0;
constexpr DWORD kDoAIStateHookAddress = 0x0076D1A0;
constexpr DWORD kStandUpHookAddress = 0x00682B40;
constexpr DWORD kCallOnStateFuncHookAddress = 0x00720870;
constexpr DWORD kMobTriggerHookAddress = 0x0071E7D0;
constexpr DWORD kMobUntriggerHookAddress = 0x0071EAC0;
constexpr DWORD kMobCanInteractWithHookAddress = 0x00720F40;
constexpr DWORD kAINormalHookAddress = 0x004A4370;
constexpr DWORD kOnDamageAnimHookAddress = 0x00675BD0;
constexpr DWORD kOnDamageHitHookAddress = 0x00666610;
constexpr DWORD kChangeAttributeHookAddress = 0x0072FF60;
constexpr DWORD kCreateArrowTrailHookAddress = 0x006A0420;
constexpr DWORD kCheckRemoveNpcHookAddress = 0x007792E0;
constexpr DWORD kShootArrowHookAddress = 0x007446B0;

// Gothic II 2.6: OnDamage_Hit caches these Daedalus settings at 0x0066C98C.
// The engine never invalidates them when GMP replaces GOTHIC.DAT/the session.
constexpr DWORD kMinimumDamageSymbolAddress = 0x008AF82C;
constexpr DWORD kMinimumPercentSymbolAddress = 0x008AF830;
constexpr DWORD kMinimumDamageAddress = 0x00AAC610;
constexpr DWORD kMinimumPercentAddress = 0x00AAC614;
constexpr DWORD kMinimumPercentFactorAddress = 0x00AAC618;

void RefreshNativeDamageSettings() {
  // GetParser() returns cur_parser, which can temporarily be the FX parser.
  // OnDamage_Hit reads the game parser at 0x00AB40C0 instead.
  auto* game_parser = Gothic_II_Addon::parser;
  if (!game_parser)
    return;
  const auto read_setting = [game_parser](const char* name, int fallback) {
    const int index = game_parser->GetIndex(zSTRING(name));
    auto* symbol = index >= 0 ? game_parser->GetSymbol(index) : nullptr;
    if (!symbol || symbol->type != zPAR_TYPE_INT || symbol->ele == 0)
      return std::pair{-1, fallback};
    int value = fallback;
    symbol->GetValue(value, 0);
    return std::pair{index, value};
  };
  // Missing constants use Gothic II defaults; explicit script values (including
  // zero) remain authoritative. Refresh values, not just the symbol indices:
  // native lookup only runs once and may have cached a different script set.
  const auto [damage_index, minimum] = read_setting("NPC_MINIMAL_DAMAGE", 5);
  const auto [percent_index, percentage] = read_setting("NPC_MINIMAL_PERCENT", 10);
  auto& cached_damage_index = *reinterpret_cast<int*>(kMinimumDamageSymbolAddress);
  auto& cached_percent_index = *reinterpret_cast<int*>(kMinimumPercentSymbolAddress);
  auto& cached_minimum = *reinterpret_cast<int*>(kMinimumDamageAddress);
  auto& cached_percentage = *reinterpret_cast<int*>(kMinimumPercentAddress);
  if (cached_damage_index != damage_index || cached_percent_index != percent_index || cached_minimum != minimum || cached_percentage != percentage) {
    SPDLOG_INFO("Native damage settings: minimum={}, noncritical percentage={} (active Daedalus scripts)", minimum, percentage);
  }
  cached_damage_index = damage_index;
  cached_percent_index = percent_index;
  cached_minimum = minimum;
  cached_percentage = percentage;
  *reinterpret_cast<float*>(kMinimumPercentFactorAddress) = static_cast<float>(percentage) / 100.0f;
}

using CastSpellOriginalFn = int(__thiscall*)(oCSpell*);
using SpellBookCastOriginalFn = void(__thiscall*)(oCMag_Book*);
using EndTimedEffectOriginalFn = void(__thiscall*)(oCSpell*);
using BeamToOriginalFn = int(__thiscall*)(oCNpc*, const zSTRING&);
using TriggerTeleportOriginalFn = void(__thiscall*)(zCTriggerTeleport*, zCVob*);
using TriggerScriptOriginalFn = void(__thiscall*)(oCTriggerScript*, zCVob*);
using DropItemOriginalFn = int(__thiscall*)(oCNpc*, zCVob*);
using TakeItemOriginalFn = int(__thiscall*)(oCNpc*, oCMsgManipulate*);
using UseItemOriginalFn = int(__thiscall*)(oCNpc*, oCMsgManipulate*);
using UseItemToStateOriginalFn = int(__thiscall*)(oCNpc*, oCMsgManipulate*);
using NativeUseItemOriginalFn = int(__thiscall*)(oCNpc*, oCItem*);
using EquipItemOriginalFn = void(__thiscall*)(oCNpc*, oCItem*);
using UnequipItemOriginalFn = void(__thiscall*)(oCNpc*, oCItem*);
using DoDieOriginalFn = void(__thiscall*)(oCNpc*, oCNpc*);
using AttackFinishOriginalFn = int(__thiscall*)(oCNpc*, oCMsgAttack*);
using DropUnconsciousOriginalFn = void(__thiscall*)(oCNpc*, float, oCNpc*);
using DropAllInHandOriginalFn = void(__thiscall*)(oCNpc*);
using DoAIStateOriginalFn = int(__thiscall*)(oCNpc_States*);
using StandUpOriginalFn = void(__thiscall*)(oCNpc*, int, int);
using CallOnStateFuncOriginalFn = void(__thiscall*)(oCMobInter*, oCNpc*, int);
using MobTriggerOriginalFn = void(__thiscall*)(oCMobInter*, zCVob*, zCVob*);
using MobCanInteractWithOriginalFn = int(__thiscall*)(oCMobInter*, oCNpc*);
using AINormalOriginalFn = void(__thiscall*)(zCAICamera*);
using OnDamageAnimOriginalFn = void(__thiscall*)(oCNpc*, oCNpc::oSDamageDescriptor&);
using OnDamageHitOriginalFn = void(__thiscall*)(oCNpc*, oCNpc::oSDamageDescriptor&);
using ChangeAttributeOriginalFn = void(__thiscall*)(oCNpc*, int, int);
using CreateArrowTrailOriginalFn = void(__thiscall*)(oCAIArrowBase*, zCVob*);
using CheckRemoveNpcOriginalFn = int(__thiscall*)(oCSpawnManager*, oCNpc*);
using ShootArrowOriginalFn = int(__thiscall*)(oCNpc*, int);

CastSpellOriginalFn g_originalCastSpell = nullptr;
SpellBookCastOriginalFn g_originalSpellBookCast = nullptr;
EndTimedEffectOriginalFn g_originalEndTimedEffect = nullptr;
BeamToOriginalFn g_originalBeamTo = nullptr;
TriggerTeleportOriginalFn g_originalTriggerTeleport = nullptr;
TriggerScriptOriginalFn g_originalTriggerScript = nullptr;
DropItemOriginalFn g_originalDropItem = nullptr;
TakeItemOriginalFn g_originalTakeItem = nullptr;
UseItemOriginalFn g_originalUseItem = nullptr;
UseItemToStateOriginalFn g_originalUseItemToState = nullptr;
NativeUseItemOriginalFn g_originalNativeUseItem = nullptr;
EquipItemOriginalFn g_originalEquipItem = nullptr;
UnequipItemOriginalFn g_originalUnequipItem = nullptr;
DoDieOriginalFn g_originalDoDie = nullptr;
AttackFinishOriginalFn g_originalAttackFinish = nullptr;
DropUnconsciousOriginalFn g_originalDropUnconscious = nullptr;
DropAllInHandOriginalFn g_originalDropAllInHand = nullptr;
DoAIStateOriginalFn g_originalDoAIState = nullptr;
StandUpOriginalFn g_originalStandUp = nullptr;
CallOnStateFuncOriginalFn g_originalCallOnStateFunc = nullptr;
MobTriggerOriginalFn g_originalMobTrigger = nullptr;
MobTriggerOriginalFn g_originalMobUntrigger = nullptr;
MobCanInteractWithOriginalFn g_originalMobCanInteractWith = nullptr;
AINormalOriginalFn g_originalAINormal = nullptr;
OnDamageAnimOriginalFn g_originalOnDamageAnim = nullptr;
OnDamageHitOriginalFn g_originalOnDamageHit = nullptr;
ChangeAttributeOriginalFn g_originalChangeAttribute = nullptr;
CreateArrowTrailOriginalFn g_originalCreateArrowTrail = nullptr;
CheckRemoveNpcOriginalFn g_originalCheckRemoveNpc = nullptr;
ShootArrowOriginalFn g_originalShootArrow = nullptr;
bool g_damageAnimationsEnabled = true;
bool g_munitionTrailEnabled = true;

int GetInventoryAmount(oCNpc* npc, int instance_id) {
  if (npc == nullptr || instance_id <= 0) {
    return 0;
  }

  std::int64_t amount = 0;
  for (auto* entry = npc->inventory2.inventory.GetNextInList(); entry; entry = entry->GetNextInList()) {
    oCItem* item = entry->GetData();
    if (item && item->GetInstance() == instance_id) amount += std::max(0, item->amount);
  }
  return static_cast<int>(std::min<std::int64_t>(amount, std::numeric_limits<std::int32_t>::max()));
}

bool IsMultiplayerNpc(oCNpc* npc) {
  auto& game = NetGame::Instance();
  return npc && game.IsConnected() && (npc == player || game.GetPlayerIdByNpc(npc).has_value());
}

class ScopedMobScriptFieldBlock {
public:
  explicit ScopedMobScriptFieldBlock(zSTRING& field) : field_(field), saved_(field) {
    field_ = "";
  }
  ~ScopedMobScriptFieldBlock() {
    field_ = saved_;
  }
  ScopedMobScriptFieldBlock(const ScopedMobScriptFieldBlock&) = delete;
  ScopedMobScriptFieldBlock& operator=(const ScopedMobScriptFieldBlock&) = delete;

private:
  zSTRING& field_;
  zSTRING saved_;
};

}  // namespace

void SetDamageAnimationsEnabled(bool enabled) {
  g_damageAnimationsEnabled = enabled;
}

bool AreDamageAnimationsEnabled() {
  return g_damageAnimationsEnabled;
}

void SetMunitionTrailEnabled(bool enabled) {
  g_munitionTrailEnabled = enabled;
}

bool IsMunitionTrailEnabled() {
  return g_munitionTrailEnabled;
}

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

std::string GetItemInstanceNameByIndex(int index) {
  if (index <= 0) {
    return {};
  }
  zCParser* parser = Gothic_II_Addon::parser;
  if (!parser) {
    return {};
  }
  zCPar_Symbol* symbol = parser->GetSymbol(index);
  if (!symbol) {
    return {};
  }
  return symbol->name.ToChar();
}

std::string ResolveItemInstanceName(oCItem* item) {
  if (!item) {
    return {};
  }
  return GetItemInstanceNameByIndex(item->GetInstance());
}

std::string ResolveItemInstanceName(oCMsgManipulate* msg) {
  if (!msg) {
    return {};
  }
  if (auto* item = zDYNAMIC_CAST<oCItem>(msg->targetVob)) {
    return ResolveItemInstanceName(item);
  }
  if (!msg->name.IsEmpty()) {
    return msg->name.ToChar();
  }
  return {};
}

std::string ResolveItemSchemeName(oCMsgManipulate* msg) {
  if (!msg || msg->name.IsEmpty()) {
    return {};
  }
  return msg->name.ToChar();
}

glm::vec3 ToGlmVec3(const zVEC3& vec) {
  return glm::vec3(vec[VX], vec[VY], vec[VZ]);
}

bool IsZeroVector(const glm::vec3& vec) {
  return std::abs(vec.x) < 0.001f && std::abs(vec.y) < 0.001f && std::abs(vec.z) < 0.001f;
}

glm::vec3 GetItemPosition(oCItem* item) {
  if (!item) {
    return glm::vec3{0.0f};
  }

  return ToGlmVec3(item->GetNewTrafoObjToWorld().GetTranslation());
}

glm::vec3 GetItemRotation(oCItem* item) {
  if (!item) {
    return glm::vec3{0.0f};
  }

  const zVEC3 euler = item->GetNewTrafoObjToWorld().GetEulerAngles();
  return glm::vec3(-euler[VX] * DEGREE, -euler[VY] * DEGREE, -euler[VZ] * DEGREE);
}

glm::vec3 GetDropFallbackPosition(oCNpc* npc) {
  if (!npc) {
    return glm::vec3{0.0f};
  }

  glm::vec3 position = ToGlmVec3(npc->GetTrafoModelNodeToWorld("ZS_RIGHTHAND").GetTranslation());
  if (IsZeroVector(position)) {
    position = ToGlmVec3(npc->GetPositionWorld());
  }
  return position;
}

oCNpc* s_observed_hero = nullptr;
zCModel* s_observed_hero_model = nullptr;
bool s_observed_connected = false;

bool s_suppress_local_equip_events = false;
bool s_suppress_local_lifecycle_events = false;

struct FinishingMoveObservation {
  oCMsgAttack* message;
  oCNpc* attacker;
  oCNpc* victim;
  bool reported;
};

std::vector<FinishingMoveObservation> s_finishing_move_observations;
FinishingMoveObservation* s_finishing_move_context = nullptr;

struct ItemUseObservation {
  oCNpc* npc;
  oCMsgManipulate* message;
  std::uint64_t actor_id;
  std::uint64_t generation;
  int instance;
  std::string name;
  std::string scheme;
  int owned_before;
  std::int64_t health_delta{0};
  std::int64_t mana_delta{0};
  bool finished{false};
  std::optional<int> owned_after;
};
std::unordered_map<oCNpc*, ItemUseObservation> s_item_use_observations;
std::vector<ItemUseObservation> s_completed_item_uses;
void FlushCompletedItemUses();

struct ScopedFinishingMoveObservation {
  FinishingMoveObservation* previous;
  explicit ScopedFinishingMoveObservation(FinishingMoveObservation& observation)
      : previous(s_finishing_move_context) {
    s_finishing_move_context = &observation;
  }
  ~ScopedFinishingMoveObservation() { s_finishing_move_context = previous; }
};

void ResetLocalActionObservation() {
  s_finishing_move_observations.clear();
  s_observed_hero = nullptr;
  s_observed_hero_model = nullptr;
  s_observed_connected = false;
  if (auto* animations = CActiveAniID::GetInstance()) animations->Reset();
}

void RefreshLocalActionObservation() {
  zCModel* model = player ? player->GetModel() : nullptr;
  const bool connected = NetGame::Instance().IsConnected();
  if (s_observed_hero != player || s_observed_hero_model != model || s_observed_connected != connected) {
    ResetLocalActionObservation();
    s_observed_hero = player;
    s_observed_hero_model = model;
    s_observed_connected = connected;
  }
  FlushCompletedItemUses();
}

void SetSuppressLocalEquipEvents(bool suppress) {
  s_suppress_local_equip_events = suppress;
}

bool ShouldSuppressLocalEquipEvents() {
  return s_suppress_local_equip_events;
}

void SetSuppressLocalLifecycleEvents(bool suppress) {
  s_suppress_local_lifecycle_events = suppress;
}

bool ShouldSuppressLocalLifecycleEvents() {
  return s_suppress_local_lifecycle_events;
}

void ClearNpcHands(oCNpc* npc) {
  if (!npc) {
    return;
  }
  ClearTorchHands(npc);

  zCVob* rightVob = npc->GetRightHand();
  zCVob* leftVob = npc->GetLeftHand();
  zSTRING rightSlot(NPC_NODE_RIGHTHAND);
  zSTRING leftSlot(NPC_NODE_LEFTHAND);
  oCItem* rightHand = zDYNAMIC_CAST<oCItem>(npc->RemoveFromSlot(rightSlot, 0, 1));
  oCItem* leftHand = zDYNAMIC_CAST<oCItem>(npc->RemoveFromSlot(leftSlot, 0, 1));
  if (!rightHand) {
    rightHand = zDYNAMIC_CAST<oCItem>(rightVob);
  }
  if (!leftHand) {
    leftHand = zDYNAMIC_CAST<oCItem>(leftVob);
  }
  if (rightHand) {
    rightHand->RemoveVobFromWorld();
  }
  if (leftHand && leftHand != rightHand) {
    leftHand->RemoveVobFromWorld();
  }
}

void __fastcall OnDropAllInHand(oCNpc* npc, void* /*edx*/) {
  // This bypasses DoDropVob and creates unmanaged loot on every client.
  // The death/KO hooks already clear the hands without dropping inventory.
  if (!IsMultiplayerNpc(npc) && g_originalDropAllInHand) {
    g_originalDropAllInHand(npc);
  }
}

int __fastcall OnDoAIState(oCNpc_States* states, void* /*edx*/) {
  if (states && IsMultiplayerNpc(states->npc)) {
    const auto skip_singleplayer_entry = [](TNpcAIState& state) {
      if (state.valid && state.phase == -1 &&
          (state.prgIndex == NPC_AISTATE_DEAD || state.prgIndex == NPC_AISTATE_UNCONSCIOUS)) {
        // ZS_DEAD/ZS_UNCONSCIOUS award XP and change inventory. Keep the
        // native state identity and KO recovery loop, but skip that entry.
        state.phase = 0;
      }
    };
    skip_singleplayer_entry(states->curState);
    skip_singleplayer_entry(states->nextState);
  }
  return g_originalDoAIState ? g_originalDoAIState(states) : FALSE;
}

void __fastcall OnTriggerScript(oCTriggerScript* trigger, void* /*edx*/, zCVob* instigator) {
  if (!NetGame::Instance().IsConnected() && g_originalTriggerScript) {
    g_originalTriggerScript(trigger, instigator);
  }
}

// Hook: oCNpc::EV_AttackFinish - report each native finishing move once.
int __fastcall OnAttackFinish(oCNpc* npc, void* /*edx*/, oCMsgAttack* message) {
  if (!npc || !message || !message->target) return 1;
  auto* victim = zDYNAMIC_CAST<oCNpc>(message->target);
  auto observed = std::find_if(s_finishing_move_observations.begin(), s_finishing_move_observations.end(),
      [message](const auto& observation) { return observation.message == message; });
  if (observed == s_finishing_move_observations.end()) {
    // Cancelled messages may never receive a final call. Keep their records bounded.
    if (s_finishing_move_observations.size() >= 128) s_finishing_move_observations.erase(s_finishing_move_observations.begin());
    s_finishing_move_observations.push_back({message, npc, victim, false});
    observed = s_finishing_move_observations.end() - 1;
  } else if (!message->IsInUse() || observed->attacker != npc || observed->victim != victim) {
    *observed = {message, npc, victim, false};
  }
  if (observed->reported) {
    // Native death normally clears KO, preventing the kill branch from adding
    // blood and death sounds again while the finishing animation is running.
    auto* model = npc->GetModel();
    auto* animation = model ? model->GetAniFromAniID(message->hitAni) : nullptr;
    const int completed = !animation || !model->IsAniActive(animation);
    if (completed) s_finishing_move_observations.erase(observed);
    return completed;
  }

  // Keep the active context on the stack: nested engine calls can grow the records.
  auto context = *observed;
  ScopedFinishingMoveObservation scope(context);
  const int completed = g_originalAttackFinish ? g_originalAttackFinish(npc, message) : 1;
  observed = std::find_if(s_finishing_move_observations.begin(), s_finishing_move_observations.end(),
      [message](const auto& observation) { return observation.message == message; });
  if (observed != s_finishing_move_observations.end()) {
    if (completed) s_finishing_move_observations.erase(observed);
    else observed->reported = context.reported;
  }
  return completed;
}

// Hook: oCNpc::DoDie - clears hands after death
void __fastcall OnDoDie(oCNpc* thisNpc, void* /*edx*/, oCNpc* attacker) {
  if (gmp::NativeDamageObservation::IsActive(thisNpc))
    return;
  if (!ShouldSuppressLocalLifecycleEvents() && s_finishing_move_context &&
      s_finishing_move_context->attacker == attacker && s_finishing_move_context->victim == thisNpc &&
      NetGame::Instance().ReportFinishingMove(attacker, thisNpc, !s_finishing_move_context->reported)) {
    s_finishing_move_context->reported = true;
    return;
  }
  if (g_originalDoDie) {
    g_originalDoDie(thisNpc, attacker);
  }
  ClearNpcHands(thisNpc);
  if (thisNpc == player && NetGame::Instance().IsConnected() && !ShouldSuppressLocalLifecycleEvents()) {
    std::optional<std::uint32_t> killer_id;
    if (auto attacker_id = NetGame::Instance().GetPlayerIdByNpc(attacker); attacker_id.has_value()) {
      killer_id = static_cast<std::uint32_t>(attacker_id.value());
    }
    NetGame::Instance().SendPlayerDeath(killer_id);
  }
}

// Hook: oCNpc::DropUnconscious - clears hands after going unconscious
void __fastcall OnDropUnconscious(oCNpc* thisNpc, void* /*edx*/, float duration, oCNpc* attacker) {
  if (gmp::NativeDamageObservation::IsActive(thisNpc))
    return;
  if (g_originalDropUnconscious) {
    g_originalDropUnconscious(thisNpc, duration, attacker);
  }
  ClearNpcHands(thisNpc);
  if (thisNpc == player && NetGame::Instance().IsConnected() && !ShouldSuppressLocalLifecycleEvents()) {
    std::optional<std::uint32_t> attacker_id;
    if (auto id = NetGame::Instance().GetPlayerIdByNpc(attacker); id.has_value()) {
      attacker_id = static_cast<std::uint32_t>(id.value());
    }
    NetGame::Instance().SendPlayerUnconscious(attacker_id);
  }
}

// Hook: oCNpc::StandUp - notify server when the local player stands up from unconscious
void __fastcall OnStandUp(oCNpc* thisNpc, void* /*edx*/, int param1, int param2) {
  if (g_originalStandUp) {
    g_originalStandUp(thisNpc, param1, param2);
  }
  if (thisNpc == player && NetGame::Instance().IsConnected() && !ShouldSuppressLocalLifecycleEvents()) {
    NetGame::Instance().SendPlayerStandUp();
  }
}

void __fastcall OnMobTrigger(oCMobInter* mob, void* /*edx*/, zCVob* other, zCVob* instigator) {
  if (!mob || !g_originalMobTrigger) return;
  if (!NetGame::Instance().IsConnected()) {
    g_originalMobTrigger(mob, other, instigator);
    return;
  }
  // Keep native mob state changes, but do not dispatch the world's trigger chain.
  ScopedMobScriptFieldBlock block(mob->triggerTarget);
  g_originalMobTrigger(mob, other, instigator);
}

void __fastcall OnMobUntrigger(oCMobInter* mob, void* /*edx*/, zCVob* other, zCVob* instigator) {
  if (!mob || !g_originalMobUntrigger) return;
  if (!NetGame::Instance().IsConnected()) {
    g_originalMobUntrigger(mob, other, instigator);
    return;
  }
  ScopedMobScriptFieldBlock block(mob->triggerTarget);
  g_originalMobUntrigger(mob, other, instigator);
}

int __fastcall OnMobCanInteractWith(oCMobInter* mob, void* /*edx*/, oCNpc* npc) {
  if (!mob || !g_originalMobCanInteractWith) return FALSE;
  if (!NetGame::Instance().IsConnected()) {
    return g_originalMobCanInteractWith(mob, npc);
  }
  // Retain native availability, scheme and useWithItem handling without
  // evaluating a singleplayer quest condition in the Daedalus parser.
  ScopedMobScriptFieldBlock block(mob->conditionFunc);
  return g_originalMobCanInteractWith(mob, npc);
}

// Hook: oCMobInter::CallOnStateFunc - server scripts own interaction effects.
void __fastcall OnCallOnStateFunc(oCMobInter* mob, void* /*edx*/, oCNpc* npc, int state) {
  if (!mob) return;
  auto& game = NetGame::Instance();
  if (!game.IsConnected()) {
    if (g_originalCallOnStateFunc) {
      g_originalCallOnStateFunc(mob, npc, state);
    }
    return;
  }
  // Report the interaction without running the world's Daedalus callback.
  if (npc == player) {
    const std::string name = mob->GetObjectName().ToChar();
    if (!name.empty()) {
      const auto position = mob->GetPositionWorld();
      game.SendMobState(name, glm::vec3(position[VX], position[VY], position[VZ]), state);
    }
  }
}

// Hook: oCNpc::OnDamage_Anim - script-controlled non-local damage animations
void __fastcall OnOnDamageAnim(oCNpc* thisNpc, void* /*edx*/, oCNpc::oSDamageDescriptor& damageDesc) {
  auto& game = NetGame::Instance();
  if (game.IsConnected() && damageDesc.pNpcAttacker != thisNpc && game.GetPlayerIdByNpc(thisNpc).has_value() &&
      game.GetPlayerIdByNpc(damageDesc.pNpcAttacker).has_value()) {
    // The accepted server hit presents this once on every viewer, including
    // the attacker and victim. Native collision prediction must not replay it.
    return;
  }
  const int attackerWeaponMode = damageDesc.pNpcAttacker ? damageDesc.pNpcAttacker->GetWeaponMode() : NPC_WEAPON_NONE;
  const bool alwaysAllowMode =
      attackerWeaponMode == NPC_WEAPON_FIST || (attackerWeaponMode >= NPC_WEAPON_BOW && attackerWeaponMode <= NPC_WEAPON_MAG);
  if (thisNpc != player && !g_damageAnimationsEnabled && !alwaysAllowMode) {
    return;
  }
  if (g_originalOnDamageAnim) {
    g_originalOnDamageAnim(thisNpc, damageDesc);
  }
}

void PlayNetworkDamageAnimation(oCNpc* victim, oCNpc* attacker, int damage, unsigned long damage_type) {
  if (!victim || damage <= 0 || victim->IsDead() || victim->IsUnconscious() || !g_originalOnDamageAnim)
    return;
  const int mode = attacker ? attacker->GetWeaponMode() : NPC_WEAPON_NONE;
  const bool always_allow = mode == NPC_WEAPON_FIST || (mode >= NPC_WEAPON_BOW && mode <= NPC_WEAPON_MAG);
  if (victim != player && !g_damageAnimationsEnabled && !always_allow)
    return;
  oCNpc::oSDamageDescriptor descriptor{};
  descriptor.enuModeDamage = damage_type;
  descriptor.fDamageReal = descriptor.fDamageEffective = descriptor.fDamageTotal = static_cast<float>(damage);
  // Attacker pointers are borrowed. The native descriptor owns only its FX.
  if (attacker) {
    descriptor.pNpcAttacker = attacker;
    descriptor.pVobAttacker = attacker;
  }
  ScopedNetworkObservationSuppression suppress_observation;
  g_originalOnDamageAnim(victim, descriptor);
}

// Hook: oCAIArrowBase::CreateTrail - allow scripts to hide arrow/bolt trails
void __fastcall OnCreateArrowTrail(oCAIArrowBase* ai, void* /*edx*/, zCVob* vob) {
  if (!g_munitionTrailEnabled) {
    return;
  }
  if (g_originalCreateArrowTrail) {
    g_originalCreateArrowTrail(ai, vob);
  }
}

// Native AI checks the camera's removal radius even when automatic spawning is
// disabled. Network actors stay present until the server streams them out.
int __fastcall OnCheckRemoveNpc(oCSpawnManager* spawn_manager, void* /*edx*/, oCNpc* npc) {
  if (!spawn_manager || !npc) {
    return 0;
  }
  if (NetGame::Instance().GetPlayerIdByNpc(npc).has_value()) {
    return 0;
  }
  // Explicit stream-out uses DeleteNpc directly, so it is not intercepted here.
  // Client-local NPCs and other engine actors retain the original behavior.
  return g_originalCheckRemoveNpc ? g_originalCheckRemoveNpc(spawn_manager, npc) : 0;
}

// Only HP changes made inside an observed network hit are intercepted. Native
// damage calculation keeps real HP, maximum HP, protections and spell inputs.
void __fastcall OnChangeAttribute(oCNpc* npc, void* /*edx*/, int attribute, int change) {
  if (npc && attribute == NPC_ATR_HITPOINTS && gmp::NativeDamageObservation::IsActive(npc)) {
    // These are the early-out rules in ZenGin's ChangeAttribute. Preserve
    // native godmode/immunity rather than recording a deduction it would skip.
    const bool godmode = change < 0 && npc == player && oCNpc::godmode;
    const bool immortal = (npc->variousFlags & NPC_FLAG_IMMORTAL) != 0 && change != -999;
    if (!godmode && !immortal)
      gmp::NativeDamageObservation::RecordHealthChange(npc, change);
    return;
  }
  if (g_originalChangeAttribute)
    g_originalChangeAttribute(npc, attribute, change);
}

// Hook: oCNpc::OnDamage_Hit - filter arrow/spell damage from other players
// In multiplayer, damage from other players' arrows/spells should be server-controlled
void __fastcall OnOnDamageHit(oCNpc* thisNpc, void* /*edx*/, oCNpc::oSDamageDescriptor& damageDesc) {
  if (!thisNpc) return;
  int damage = 0;

  const bool connected = NetGame::Instance().IsConnected();
  const bool attacker_is_local_player = damageDesc.pNpcAttacker == player;
  const bool attacker_is_network_player =
      attacker_is_local_player || NetGame::Instance().GetPlayerIdByNpc(damageDesc.pNpcAttacker).has_value();
  const bool victim_is_network_player = thisNpc == player || NetGame::Instance().GetPlayerIdByNpc(thisNpc).has_value();
  const bool server_controlled_player_damage =
      connected && attacker_is_network_player && victim_is_network_player && damageDesc.pNpcAttacker != thisNpc;

  if (server_controlled_player_damage && !NetGame::Instance().CanReportActorDamage(damageDesc.pNpcAttacker)) {
    // Spectators present the accepted server result. They never execute the
    // native hit gameplay callbacks or calculate another copy of the damage.
    damageDesc.bIsDead = 0;
    damageDesc.bIsUnconscious = 0;
    return;
  }

  if (connected)
    RefreshNativeDamageSettings();

  if (server_controlled_player_damage) {
    // Fail closed if the attribute hook was not installed: native health must
    // never be applied speculatively on top of the server's accepted hit.
    if (g_originalOnDamageHit && g_originalChangeAttribute) {
      thisNpc->AddRef();
      auto release_npc = [](oCNpc* npc) { npc->Release(); };
      std::unique_ptr<oCNpc, decltype(release_npc)> keep_alive(thisNpc, release_npc);
      gmp::NativeDamageObservation calculation(thisNpc);
      g_originalOnDamageHit(thisNpc, damageDesc);
      damage = calculation.damage();
    }
    damageDesc.bIsDead = 0;
    damageDesc.bIsUnconscious = 0;
  } else if (g_originalOnDamageHit) {
    g_originalOnDamageHit(thisNpc, damageDesc);
  }

  // Report the native post-protection deduction; the server applies it once.
  if (server_controlled_player_damage && connected) {
    if (auto victim_id = NetGame::Instance().GetPlayerIdByNpc(thisNpc); victim_id.has_value()) {
      if (damage > 0) {
        NetGame::Instance().SendActorHit(damageDesc.pNpcAttacker, static_cast<std::uint32_t>(victim_id.value()), damage,
                                          static_cast<std::uint32_t>(damageDesc.enuModeDamage),
                                          damageDesc.bDamageDontKill != 0);
      }
    }
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

// Compare completed native mutations, not message addresses or item classes.
// Engine messages are reused, and two equipped rings can share an instance.
int __fastcall OnDropItem(oCNpc* npc, void* /*unusedEdx*/, zCVob* vob) {
  auto* item = vob ? zDYNAMIC_CAST<oCItem>(vob) : nullptr;
  if (npc != player || !item || !NetGame::Instance().IsConnected()) {
    return g_originalDropItem ? g_originalDropItem(npc, vob) : 0;
  }
  item->AddRef();
  const int instance = item->GetInstance();
  int before = GetInventoryAmount(npc, instance);
  const int amount = std::max(1, item->amount);
  const std::string name = ResolveItemInstanceName(item);
  const glm::vec3 fallback = GetDropFallbackPosition(npc);
  const auto event = EventManager::Instance().DispatchEvent(gmp::gothic::kEventOnDropItemName,
      gmp::gothic::OnDropItemEvent{name, amount});
  if (event.cancelled || GetInventoryAmount(npc, instance) < amount) {
    item->Release();
    return 0;
  }
  before = GetInventoryAmount(npc, instance);
  const int result = g_originalDropItem ? g_originalDropItem(npc, vob) : 0;
  const int removed = std::max(0, before - GetInventoryAmount(npc, instance));
  if (removed > 0) {
    auto position = GetItemPosition(item);
    if (IsZeroVector(position)) position = fallback;
    const auto rotation = GetItemRotation(item);
    const bool physics = item->physicsEnabled != 0;
    NetGame::Instance().SendDropItem(instance, removed, position, rotation, physics);
    item->RemoveVobFromWorld();
  }
  item->Release();
  return result;
}

int __fastcall OnTakeItem(oCNpc* npc, void* /*unusedEdx*/, oCMsgManipulate* msg) {
  auto* item = msg && msg->targetVob ? zDYNAMIC_CAST<oCItem>(msg->targetVob) : nullptr;
  if (npc != player || !item || !NetGame::Instance().IsConnected()) {
    return g_originalTakeItem ? g_originalTakeItem(npc, msg) : 0;
  }
  item->AddRef();
  // Copy before native pickup: stacking can retire the world object/message.
  const std::string name = ResolveItemInstanceName(item);
  const int instance = item->GetInstance();
  const int before = GetInventoryAmount(npc, instance);
  const int offered = std::max(1, item->amount);
  const auto ground = gmp::gothic::ClientItemGroundManager::Instance().GetIdByItem(item);
  const auto* ground_record = ground ? gmp::gothic::ClientItemGroundManager::Instance().GetById(*ground) : nullptr;
  const auto restore_ground = ground_record ? std::optional<gmp::gothic::ClientItemGroundManager::ItemGround>(*ground_record) : std::nullopt;
  const auto event = EventManager::Instance().DispatchEvent(gmp::gothic::kEventOnTakeItemName,
      gmp::gothic::OnTakeItemEvent{name, ground.has_value(), offered, ground});
  // EV_TakeVob returns true when its queued job is finished, not when the
  // pickup succeeds. Returning false on rejection blocks every later action.
  if (event.cancelled) {
    item->Release();
    return 1;
  }
  // A native world item without a managed identity has no server ownership or
  // quantity. Server scripts must import it as managed loot before pickup.
  if (!ground || !item->GetHomeWorld()) {
    item->Release();
    return 1;
  }
  const auto* current_ground = gmp::gothic::ClientItemGroundManager::Instance().GetById(*ground);
  if (!current_ground || current_ground->item != item) {
    item->Release();
    return 1;
  }
  const int result = g_originalTakeItem ? g_originalTakeItem(npc, msg) : 0;
  const int gained = std::max(0, GetInventoryAmount(npc, instance) - before);
  if (gained > 0) {
    if (ground) gmp::gothic::ClientItemGroundManager::Instance().DetachItem(*ground);
    if (NetGame::Instance().SendTakeItem(instance, gained, ground) == 0 && restore_ground) {
      const auto& record = *restore_ground;
      gmp::gothic::ClientItemGroundManager::Instance().Upsert(record.id, record.instance, record.amount,
          record.physics_enabled, record.position, record.rotation);
    }
  }
  item->Release();
  return result;
}

int GetOwnedUseItemAmount(oCNpc* npc, int instance) {
  std::int64_t amount = static_cast<std::int64_t>(GetInventoryAmount(npc, instance)) + GetCarriedTorchAmount(npc, instance);
  auto* interaction = npc->interactItem;
  if (interaction && interaction->GetInstance() == instance) {
    bool in_inventory = false;
    for (auto* entry = npc->inventory2.inventory.GetNextInList(); entry; entry = entry->GetNextInList())
      if (entry->GetData() == interaction)
        in_inventory = true;
    // DEF_INSERT_ITEM temporarily removes one owned object from the stack.
    // It becomes consumption only when destroyed, not while held for use.
    if (!in_inventory)
      amount += std::max(0, interaction->amount);
  }
  return static_cast<int>(std::min<std::int64_t>(amount, std::numeric_limits<int>::max()));
}

void FlushCompletedItemUses() {
  auto& game = NetGame::Instance();
  auto completed = std::move(s_completed_item_uses);
  s_completed_item_uses.clear();
  for (auto it = s_item_use_observations.begin(); it != s_item_use_observations.end();) {
    auto* actor = game.FindEngineActor(it->second.actor_id);
    if (!actor || actor->GetNpc() != it->first || actor->base_player().actor_generation() != it->second.generation ||
        !game.CanReportActorDamage(it->first)) {
      it = s_item_use_observations.erase(it);
      continue;
    }
    bool queued = false;
    auto* events = it->first->GetEM();
    for (int index = 0; index < events->GetNumMessages(); ++index)
      if (events->GetEventMessage(index) == it->second.message)
        queued = true;
    if (it->second.finished || !queued) {
      it->second.owned_after = GetOwnedUseItemAmount(it->first, it->second.instance);
      completed.push_back(std::move(it->second));
      it = s_item_use_observations.erase(it);
    } else
      ++it;
  }
  for (const auto& use : completed) {
    auto* actor = game.FindEngineActor(use.actor_id);
    if (!actor || actor->GetNpc() != use.npc || actor->base_player().actor_generation() != use.generation)
      continue;
    const int consumed = std::max(0, use.owned_before - use.owned_after.value_or(use.owned_before));
    const auto clamp_delta = [](std::int64_t value) {
      return static_cast<int>(std::clamp<std::int64_t>(value, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()));
    };
    // Report after all native messages for the frame, including insert/destroy
    // events. Reusable items still notify the server once with quantity zero.
    if (use.instance > 0 && game.IsConnected() && game.CanReportActorDamage(use.npc))
      game.SendItemUsePresentation(use.instance, use.scheme, consumed, clamp_delta(use.health_delta), clamp_delta(use.mana_delta),
                                   static_cast<std::uint32_t>(use.actor_id));
    if (actor->IsLocalPlayer())
      EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnUseItemName, gmp::gothic::OnUseItemEvent{use.name, use.scheme, 0, 0});
  }
}

int ObserveUseItem(oCNpc* npc, oCMsgManipulate* msg, bool to_state) {
  // EV_UseItem/EV_UseItemToState are polled across frames. Keep the original
  // instance/count until the outer message completes; nested calls count once.
  static std::vector<oCNpc*> observed_actors;
  const bool observe = npc && msg && std::find(observed_actors.begin(), observed_actors.end(), npc) == observed_actors.end() &&
                       NetGame::Instance().CanReportActorDamage(npc);
  struct UseObservationScope {
    std::vector<oCNpc*>& actors;
    bool active;
    UseObservationScope(std::vector<oCNpc*>& actors, oCNpc* actor, bool active) : actors(actors), active(active) {
      if (active) actors.push_back(actor);
    }
    ~UseObservationScope() { if (active) actors.pop_back(); }
  } observation_scope(observed_actors, npc, observe);
  if (observe) {
    const auto actor_id = NetGame::Instance().GetPlayerIdByNpc(npc);
    auto* actor = actor_id ? NetGame::Instance().FindEngineActor(*actor_id) : nullptr;
    const auto current = s_item_use_observations.find(npc);
    if (actor && (current == s_item_use_observations.end() || current->second.generation != actor->base_player().actor_generation() ||
                  current->second.message != msg || !msg->IsInUse())) {
      if (current != s_item_use_observations.end() && current->second.generation == actor->base_player().actor_generation()) {
        // A second use can start in the same native frame. Save the first
        // result before another operation changes the same inventory stack.
        current->second.owned_after = GetOwnedUseItemAmount(npc, current->second.instance);
        s_completed_item_uses.push_back(std::move(current->second));
      }
      const auto name = ResolveItemInstanceName(msg);
      auto* item = msg->targetVob ? zDYNAMIC_CAST<oCItem>(msg->targetVob) : nullptr;
      const int instance = item ? item->GetInstance() : Gothic_II_Addon::parser ? Gothic_II_Addon::parser->GetIndex(name.c_str()) : 0;
      if (!item && instance > 0)
        item = npc->inventory2.IsIn(instance, 1);
      const std::string scheme = item ? item->GetSchemeName().ToChar() : ResolveItemSchemeName(msg);
      s_item_use_observations.insert_or_assign(npc, ItemUseObservation{npc, msg, *actor_id, actor->base_player().actor_generation(), instance, name,
                                                                       scheme, GetOwnedUseItemAmount(npc, instance)});
    }
  }
  const int health = observe ? npc->GetAttribute(NPC_ATR_HITPOINTS) : 0;
  const int mana = observe ? npc->GetAttribute(NPC_ATR_MANA) : 0;
  if (observe)
    npc->AddRef();
  const int result = to_state ? (g_originalUseItemToState ? g_originalUseItemToState(npc, msg) : 0)
      : (g_originalUseItem ? g_originalUseItem(npc, msg) : 0);
  if (observe && NetGame::Instance().CanReportActorDamage(npc)) {
    const auto current = s_item_use_observations.find(npc);
    if (current != s_item_use_observations.end() && current->second.message == msg) {
      current->second.health_delta += static_cast<std::int64_t>(npc->GetAttribute(NPC_ATR_HITPOINTS)) - health;
      current->second.mana_delta += static_cast<std::int64_t>(npc->GetAttribute(NPC_ATR_MANA)) - mana;
      current->second.finished = result != 0;
    }
  }
  if (observe)
    npc->Release();
  return result;
}

int __fastcall OnUseItem(oCNpc* npc, void* /*unusedEdx*/, oCMsgManipulate* msg) { return ObserveUseItem(npc, msg, false); }
int __fastcall OnUseItemToState(oCNpc* npc, void* /*unusedEdx*/, oCMsgManipulate* msg) { return ObserveUseItem(npc, msg, true); }

// Torch equipping uses synchronous UseItem, not EV_UseItemToState. 
// Observe it without treating the unlit-to-burning swap as consumption: the held vob reserves that owned torch until it is put away.
int __fastcall OnNativeUseItem(oCNpc* npc, void* /*unusedEdx*/, oCItem* item) {
  if (!npc || !item || !item->HasFlag(ITM_FLAG_TORCH))
    return g_originalNativeUseItem ? g_originalNativeUseItem(npc, item) : 0;
  if (IsTorchUseSuppressed())
    return 0;
  item->AddRef();
  npc->AddRef();
  int instance = item->GetInstance();
  const std::string name = ResolveItemInstanceName(item);
  const bool observe = NetGame::Instance().CanReportActorDamage(npc);
  const auto actor_id = NetGame::Instance().GetPlayerIdByNpc(npc);
  auto* actor = actor_id ? NetGame::Instance().FindEngineActor(*actor_id) : nullptr;
  if (IsBurningTorch(item) && actor && actor->base_player().inventory_count(gmp::gothic::NetworkItemName(instance)) == 0) {
    if (auto* parser = Gothic_II_Addon::parser)
      instance = parser->GetIndex("ITLSTORCH");
  }
  auto* previous_hand = npc->GetLeftHand();
  const bool standing = npc->inventory2.IsOpen() || (npc->GetAnictrl() && npc->GetAnictrl()->IsStanding());
  const int result = g_originalNativeUseItem ? g_originalNativeUseItem(npc, item) : 0;
  if (result && previous_hand != npc->GetLeftHand()) {
    UpdateTorchAnimation(npc, standing);
    if (observe && NetGame::Instance().CanReportActorDamage(npc)) {
      NetGame::Instance().SendItemUsePresentation(instance, {}, 0, 0, 0, static_cast<std::uint32_t>(actor_id.value_or(0)));
      if (npc == player)
        EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnUseItemName, gmp::gothic::OnUseItemEvent{name, {}, 0, 0});
    }
  }
  npc->Release();
  item->Release();
  return result;
}

void __fastcall OnEquipItem(oCNpc* npc, void* /*unusedEdx*/, oCItem* item) {
  if (!item) { if (g_originalEquipItem) g_originalEquipItem(npc, item); return; }
  item->AddRef();
  const bool before = item->HasFlag(ITM_FLAG_ACTIVE) != 0;
  const std::string name = ResolveItemInstanceName(item);
  if (g_originalEquipItem) g_originalEquipItem(npc, item);
  const bool after = item->HasFlag(ITM_FLAG_ACTIVE) != 0;
  item->Release();
  if (npc == player && !ShouldSuppressLocalEquipEvents() && !before && after && !name.empty()) {
    EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnEquipName, gmp::gothic::OnItemEvent{name});
  }
}

void __fastcall OnUnequipItem(oCNpc* npc, void* /*unusedEdx*/, oCItem* item) {
  if (!item) { if (g_originalUnequipItem) g_originalUnequipItem(npc, item); return; }
  item->AddRef();
  const bool before = item->HasFlag(ITM_FLAG_ACTIVE) != 0;
  const std::string name = ResolveItemInstanceName(item);
  if (g_originalUnequipItem) g_originalUnequipItem(npc, item);
  const bool after = item->HasFlag(ITM_FLAG_ACTIVE) != 0;
  item->Release();
  if (npc == player && !ShouldSuppressLocalEquipEvents() && before && !after && !name.empty()) {
    EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnUnequipName, gmp::gothic::OnItemEvent{name});
  }
}

namespace {
class NativeHitBatch {
public:
  explicit NativeHitBatch(bool observe) : client_(observe ? NetGame::Instance().game_client.get() : nullptr) {
    if (client_) client_->BeginHitBatch();
  }
  ~NativeHitBatch() { Finish(false); }
  bool IsCurrent() const { return client_ && NetGame::Instance().game_client.get() == client_; }
  void Finish(bool accepted) {
    if (IsCurrent()) client_->EndHitBatch(accepted);
    client_ = nullptr;
  }
private:
  gmp::client::GameClient* client_;
};
}

int __fastcall OnShootArrow(oCNpc* npc, void* /*unusedEdx*/, int param) {
  const bool observe = NetGame::Instance().CanReportActorDamage(npc);
  NativeHitBatch hits(observe);
  const int mode = npc ? npc->GetWeaponMode() : 0;
  const auto origin = npc ? npc->GetPositionWorld() : zVEC3(0.0f, 0.0f, 0.0f);
  const auto heading = npc ? npc->GetAtVectorWorld() : zVEC3(0.0f, 0.0f, 1.0f);
  const auto target = npc ? NetGame::Instance().GetPlayerIdByNpc(zDYNAMIC_CAST<oCNpc>(npc->GetFocusVob())) : std::nullopt;
  const int result = g_originalShootArrow ? g_originalShootArrow(npc, param) : 0;
  const bool accepted = hits.IsCurrent() && NetGame::Instance().CanReportActorDamage(npc) && result != 0;
  if (accepted) {
    NetGame::Instance().SendShotPresentation(static_cast<std::uint8_t>(mode),
        glm::vec3(origin[VX], origin[VY], origin[VZ]), glm::vec3(heading[VX], heading[VY], heading[VZ]),
        target ? std::optional<std::uint32_t>(static_cast<std::uint32_t>(*target)) : std::nullopt,
        static_cast<std::uint32_t>(NetGame::Instance().GetPlayerIdByNpc(npc).value_or(0)));
  }
  hits.Finish(accepted);
  return result;
}
int __fastcall OnCastSpell(oCSpell* thisSpell) {
  if (!thisSpell)
    return 0;
  auto* caster = thisSpell->spellCasterNpc;
  const bool local = NetGame::Instance().CanReportActorDamage(caster);
  NativeHitBatch hits(local);
  const auto caster_id = NetGame::Instance().GetPlayerIdByNpc(caster);
  const int spell_id = thisSpell->GetSpellID();
  const bool transformation = gmp::GetTransformationForm(static_cast<std::uint16_t>(spell_id)) != nullptr;
  if (transformation && local && NetGame::Instance().HasNativeTransformation())
    return 0;
  auto* actor_before = caster_id ? NetGame::Instance().FindEngineActor(*caster_id) : nullptr;
  // Native transformation calls SetAsPlayer even for an NPC caster. A network
  // proxy must never replace the actual local hero through this path.
  if (transformation && NetGame::Instance().IsConnected() && (!actor_before || !actor_before->IsLocalPlayer()))
    return 0;
  const auto generation = actor_before ? actor_before->base_player().actor_generation() : 0;
  // CastSpecificSpell changes spellCasterNpc and the global hero. The old
  // caster remains the source of the paid mana and owned spell item.
  const auto mana_after_invest = caster ? caster->GetAttribute(NPC_ATR_MANA) : 0;
  if (transformation && caster)
    caster->AddRef();
  const auto spell_level =
      static_cast<std::uint8_t>(std::clamp(thisSpell->effect ? thisSpell->effect->GetLevel() : 1, 0, static_cast<int>(kMaxSpellInvestLevel)));
  const auto target_id = NetGame::Instance().GetPlayerIdByNpc(thisSpell->spellTargetNpc);
  std::string item_instance;
  bool scroll = false;
  if (auto* caster = thisSpell->spellCasterNpc) {
    if (auto* book = caster->GetSpellBook()) {
      if (auto* item = book->GetSpellItem(thisSpell)) {
        item_instance = gmp::gothic::NetworkItemName(item->GetInstance());
        scroll = item->HasFlag(ITM_FLAG_MULTI);
      }
    }
  }
  int result = g_originalCastSpell ? g_originalCastSpell(thisSpell) : 0;
  auto& net = NetGame::Instance();
  auto* actor_after = caster_id ? net.FindEngineActor(*caster_id) : nullptr;
  const bool transformed = transformation && local && hits.IsCurrent() && result != 0 && actor_after &&
                           actor_after->base_player().actor_generation() == generation && thisSpell->saveNpc == caster &&
                           thisSpell->spellCasterNpc == player && player != caster;
  if (transformed) {
    player->attribute[NPC_ATR_HITPOINTS] = gmp::ScaleTransformationHealth(
        caster->GetAttribute(NPC_ATR_HITPOINTS), caster->GetAttribute(NPC_ATR_HITPOINTSMAX), player->GetAttribute(NPC_ATR_HITPOINTSMAX));
    net.OnNativeTransformation(caster, player, static_cast<std::uint16_t>(spell_id), true);
  }
  const bool accepted = hits.IsCurrent() && result != 0 && (transformed || net.CanReportActorDamage(caster));
  if (accepted) {
    const auto* form = gmp::GetTransformationForm(static_cast<std::uint16_t>(spell_id));
    auto* parser = Gothic_II_Addon::parser;
    const bool matching_form = transformed && parser && form && player->GetInstance() == parser->GetIndex(form->instance);
    const auto cast =
        net.game_client->SendCastSpell(static_cast<std::uint32_t>(target_id.value_or(0)), static_cast<std::uint16_t>(spell_id), item_instance,
                                       static_cast<std::uint32_t>(caster_id.value_or(0)),
                                       transformation ? mana_after_invest : caster->GetAttribute(NPC_ATR_MANA), spell_level, matching_form, scroll);
    if (transformed) {
      net.SetNativeTransformationCast(cast);
      if (!cast)
        net.ClearNativeTransformation();
    }
  }
  hits.Finish(accepted);
  if (transformation && caster)
    caster->Release();

  return result;
}

void __fastcall OnEndTimedEffect(oCSpell* spell, void* /*unusedEdx*/) {
  const bool transformation = spell && gmp::GetTransformationForm(static_cast<std::uint16_t>(spell->GetSpellID()));
  auto* previous = transformation ? spell->spellCasterNpc : nullptr;
  auto* human = transformation ? spell->saveNpc : nullptr;
  auto& net = NetGame::Instance();
  const auto id = previous ? net.GetPlayerIdByNpc(previous) : std::nullopt;
  auto* actor = id ? net.FindEngineActor(*id) : nullptr;
  const bool observe = actor && actor->IsLocalPlayer() && previous == player && human && previous != human;
  const auto generation = observe ? actor->base_player().actor_generation() : 0;
  const auto spell_id = transformation ? static_cast<std::uint16_t>(spell->GetSpellID()) : 0;
  if (observe) {
    previous->AddRef();
    human->AddRef();
  }
  if (g_originalEndTimedEffect)
    g_originalEndTimedEffect(spell);
  actor = id ? net.FindEngineActor(*id) : nullptr;
  if (observe && actor && actor->base_player().actor_generation() == generation && player == human)
    net.OnNativeTransformation(previous, human, spell_id, false);
  if (observe) {
    human->Release();
    previous->Release();
  }
}

namespace {

std::unordered_set<oCNpc*> g_nativeTeleports;

// Keep the native object alive across script callbacks, but report only if it
// still belongs to the same actor and controlling client after the teleport.
class NativeTeleportObservation {
public:
  NativeTeleportObservation(const NativeTeleportObservation&) = delete;
  NativeTeleportObservation& operator=(const NativeTeleportObservation&) = delete;
  explicit NativeTeleportObservation(oCNpc* npc) {
    auto& net = NetGame::Instance();
    if (!net.CanReportActorDamage(npc) || g_nativeTeleports.count(npc))
      return;
    const auto id = net.GetPlayerIdByNpc(npc);
    auto* actor = id ? net.FindEngineActor(*id) : nullptr;
    if (!actor || actor->GetNpc() != npc)
      return;
    npc_ = npc;
    id_ = *id;
    generation_ = actor->base_player().actor_generation();
    epoch_ = actor->base_player().npc_control_epoch();
    client_ = net.game_client.get();
    npc_->AddRef();
    g_nativeTeleports.insert(npc_);
  }
  ~NativeTeleportObservation() {
    if (!npc_)
      return;
    g_nativeTeleports.erase(npc_);
    npc_->Release();
  }
  bool IsObserving() const {
    return npc_ != nullptr;
  }
  void Finish(bool succeeded, const std::string& destination) {
    if (!npc_ || !succeeded)
      return;
    auto& net = NetGame::Instance();
    auto* actor = net.FindEngineActor(id_);
    if (net.game_client.get() != client_ || !actor || actor->GetNpc() != npc_ || actor->base_player().actor_generation() != generation_ ||
        actor->base_player().npc_control_epoch() != epoch_ || !net.CanReportActorDamage(npc_))
      return;
    net.SendNativeTeleport(npc_, destination);
  }

private:
  oCNpc* npc_{nullptr};
  gmp::client::GameClient* client_{nullptr};
  std::uint64_t id_{0};
  std::uint64_t generation_{0};
  std::uint32_t epoch_{0};
};

}  // namespace

int __fastcall OnBeamTo(oCNpc* npc, void* /*unusedEdx*/, const zSTRING& target) {
  NativeTeleportObservation observation(npc);
  const std::string destination = target.ToChar();
  const int result = g_originalBeamTo ? g_originalBeamTo(npc, target) : 0;
  observation.Finish(result != 0, destination);
  return result;
}

void __fastcall OnTriggerTeleport(zCTriggerTeleport* trigger, void* /*unusedEdx*/, zCVob* vob) {
  auto* npc = zDYNAMIC_CAST<oCNpc>(vob);
  NativeTeleportObservation observation(npc);
  const std::string destination = trigger ? trigger->triggerTarget.ToChar() : "";
  const auto position = observation.IsObserving() ? npc->GetPositionWorld() : zVEC3(0.0f, 0.0f, 0.0f);
  const auto normal = observation.IsObserving() ? npc->GetAtVectorWorld() : zVEC3(0.0f, 0.0f, 0.0f);
  if (g_originalTriggerTeleport)
    g_originalTriggerTeleport(trigger, vob);
  // DoTeleport has no result. Failed activation/target lookup leaves the
  // transform unchanged, so it must not produce a completion notification.
  const bool moved = observation.IsObserving() && (glm::distance(ToGlmVec3(position), ToGlmVec3(npc->GetPositionWorld())) > 0.001f ||
                                                   glm::distance(ToGlmVec3(normal), ToGlmVec3(npc->GetAtVectorWorld())) > 0.001f);
  observation.Finish(moved, destination);
}

// Wld_SpawnNpcRange returns no script value and does not return an NPC handle.
// Consume its arguments without constructing local creatures. The accepted
// cast creates server NPCs instead; native spell cost, FX and book cleanup run.
int __cdecl SuppressNativeSpellSummons() {
  auto* parser = Gothic_II_Addon::parser;
  float range = 0.0f;
  int count = 0, instance = 0;
  parser->GetParameter(range);
  parser->GetParameter(count);
  parser->GetParameter(instance);
  parser->GetInstance();
  return 0;
}

void __fastcall OnSpellBookCast(oCMag_Book* book, void* /*unusedEdx*/) {
  auto* spell = book && book->spellnr >= 0 && book->spellnr < book->GetNoOfSpells() ? book->GetSelectedSpell() : nullptr;
  if (spell && NetGame::Instance().IsConnected() && gmp::GetTransformationForm(static_cast<std::uint16_t>(spell->GetSpellID())) &&
      NetGame::Instance().HasNativeTransformation()) {
    book->StopSelectedSpell();
    return;
  }
  if (!spell || !NetGame::Instance().IsConnected() || !gmp::IsNpcSummonSpell(static_cast<std::uint16_t>(spell->GetSpellID()))) {
    if (g_originalSpellBookCast)
      g_originalSpellBookCast(book);
    return;
  }
  auto* parser = Gothic_II_Addon::parser;
  auto* symbol = parser ? parser->GetSymbol("WLD_SPAWNNPCRANGE") : nullptr;
  if (!symbol || !symbol->HasFlag(zPAR_FLAG_EXTERNAL)) {
    SPDLOG_ERROR("Cannot manage summon spell: Wld_SpawnNpcRange is missing");
    book->StopSelectedSpell();
    return;
  }
  // This is how DefineExternal binds an external in ZenGin. Restore the exact
  // binding on scope exit, including nested casts and single-player fallback.
  struct ExternalScope {
    zCPar_Symbol* symbol;
    int original{0};
    explicit ExternalScope(zCPar_Symbol* symbol) : symbol(symbol) {
      symbol->GetStackPos(original, 0);
      symbol->SetStackPos(reinterpret_cast<int>(&SuppressNativeSpellSummons), 0);
    }
    ~ExternalScope() {
      symbol->SetStackPos(original, 0);
    }
  } scope(symbol);
  if (g_originalSpellBookCast)
    g_originalSpellBookCast(book);
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

  // Replace the complete push/mov/call sequence (1 + 2 + 5 bytes).
  AsmPatch::InstallMidFunctionPatch(kPatchAddress, 8, [](Assembler& a) {
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
    // Complete a rejected pickup job so it cannot block the event queue.
    a.mov(eax, 1);
    a.jmp(kReturnAddress);
  });
}

void Initialize(void) {
  if (!MultiplayerLaunched) {
    MultiplayerLaunched = true;
    HooksManager* hm = HooksManager::GetInstance();
    InstallNpcAnimationFixes();
    gmp::gothic::InstallChatInputHooks();

    // Register task scheduler render hook - processes queued main-thread tasks every frame
    hm->AddHook(HT_RENDER, (DWORD)NetGame::ProcessTaskScheduler);

    CActiveAniID* ani_ptr = new CActiveAniID();
    if (auto original = CreateHook(kCastSpellHookAddress, (DWORD)OnCastSpell)) {
      g_originalCastSpell = reinterpret_cast<CastSpellOriginalFn>(*original);
    }
    if (auto original = CreateHook(kSpellBookCastHookAddress, (DWORD)OnSpellBookCast)) {
      g_originalSpellBookCast = reinterpret_cast<SpellBookCastOriginalFn>(*original);
    }
    if (auto original = CreateHook(kEndTimedEffectHookAddress, (DWORD)OnEndTimedEffect)) {
      g_originalEndTimedEffect = reinterpret_cast<EndTimedEffectOriginalFn>(*original);
    }
    if (auto original = CreateHook(kBeamToHookAddress, (DWORD)OnBeamTo)) {
      g_originalBeamTo = reinterpret_cast<BeamToOriginalFn>(*original);
    }
    if (auto original = CreateHook(kTriggerTeleportHookAddress, (DWORD)OnTriggerTeleport)) {
      g_originalTriggerTeleport = reinterpret_cast<TriggerTeleportOriginalFn>(*original);
    }
    if (auto original = CreateHook(kTriggerScriptHookAddress, (DWORD)OnTriggerScript)) {
      g_originalTriggerScript = reinterpret_cast<TriggerScriptOriginalFn>(*original);
    }
    if (auto original = CreateHook(kShootArrowHookAddress, (DWORD)OnShootArrow)) {
      g_originalShootArrow = reinterpret_cast<ShootArrowOriginalFn>(*original);
    }
    if (auto original = CreateHook(kDropItemHookAddress, (DWORD)OnDropItem)) {
      g_originalDropItem = reinterpret_cast<DropItemOriginalFn>(*original);
    }
    if (auto original = CreateHook(kTakeItemHookAddress, (DWORD)OnTakeItem)) {
      g_originalTakeItem = reinterpret_cast<TakeItemOriginalFn>(*original);
    }
    if (auto original = CreateHook(kUseItemHookAddress, (DWORD)OnUseItem)) {
      g_originalUseItem = reinterpret_cast<UseItemOriginalFn>(*original);
    }
    if (auto original = CreateHook(kUseItemToStateHookAddress, (DWORD)OnUseItemToState)) {
      g_originalUseItemToState = reinterpret_cast<UseItemToStateOriginalFn>(*original);
    }
    if (auto original = CreateHook(kNativeUseItemHookAddress, (DWORD)OnNativeUseItem)) {
      g_originalNativeUseItem = reinterpret_cast<NativeUseItemOriginalFn>(*original);
    }
    if (auto original = CreateHook(kEquipItemHookAddress, (DWORD)OnEquipItem)) {
      g_originalEquipItem = reinterpret_cast<EquipItemOriginalFn>(*original);
    }
    if (auto original = CreateHook(kUnequipItemHookAddress, (DWORD)OnUnequipItem)) {
      g_originalUnequipItem = reinterpret_cast<UnequipItemOriginalFn>(*original);
    }
    // oCNpc::DoDie - clear hands after death
    if (auto original = CreateHook(kDoDieHookAddress, (DWORD)OnDoDie)) {
      g_originalDoDie = reinterpret_cast<DoDieOriginalFn>(*original);
    }
    if (auto original = CreateHook(kAttackFinishHookAddress, (DWORD)OnAttackFinish)) {
      g_originalAttackFinish = reinterpret_cast<AttackFinishOriginalFn>(*original);
    }
    // oCNpc::DropUnconscious - clear hands after going unconscious
    if (auto original = CreateHook(kDropUnconsciousHookAddress, (DWORD)OnDropUnconscious)) {
      g_originalDropUnconscious = reinterpret_cast<DropUnconsciousOriginalFn>(*original);
    }
    if (auto original = CreateHook(kDropAllInHandHookAddress, (DWORD)OnDropAllInHand)) {
      g_originalDropAllInHand = reinterpret_cast<DropAllInHandOriginalFn>(*original);
    }
    if (auto original = CreateHook(kDoAIStateHookAddress, (DWORD)OnDoAIState)) {
      g_originalDoAIState = reinterpret_cast<DoAIStateOriginalFn>(*original);
    }
    // oCNpc::StandUp - notify server when local player stands up
    if (auto original = CreateHook(kStandUpHookAddress, (DWORD)OnStandUp)) {
      g_originalStandUp = reinterpret_cast<StandUpOriginalFn>(*original);
    }
    // oCMobInter::CallOnStateFunc - suppress singleplayer interaction scripts.
    if (auto original = CreateHook(kCallOnStateFuncHookAddress, (DWORD)OnCallOnStateFunc)) {
      g_originalCallOnStateFunc = reinterpret_cast<CallOnStateFuncOriginalFn>(*original);
    }
    if (auto original = CreateHook(kMobTriggerHookAddress, (DWORD)OnMobTrigger)) {
      g_originalMobTrigger = reinterpret_cast<MobTriggerOriginalFn>(*original);
    }
    if (auto original = CreateHook(kMobUntriggerHookAddress, (DWORD)OnMobUntrigger)) {
      g_originalMobUntrigger = reinterpret_cast<MobTriggerOriginalFn>(*original);
    }
    if (auto original = CreateHook(kMobCanInteractWithHookAddress, (DWORD)OnMobCanInteractWith)) {
      g_originalMobCanInteractWith = reinterpret_cast<MobCanInteractWithOriginalFn>(*original);
    }
    // zCAICamera::AI_Normal - crashfix for null pointer dereference
    if (auto original = CreateHook(kAINormalHookAddress, (DWORD)OnAINormal)) {
      g_originalAINormal = reinterpret_cast<AINormalOriginalFn>(*original);
    }
    // oCNpc::OnDamage_Anim - script-controlled non-local damage animations
    if (auto original = CreateHook(kOnDamageAnimHookAddress, (DWORD)OnOnDamageAnim)) {
      g_originalOnDamageAnim = reinterpret_cast<OnDamageAnimOriginalFn>(*original);
    }
    // Install the health observer before enabling speculative damage calculation.
    if (auto original = CreateHook(kChangeAttributeHookAddress, (DWORD)OnChangeAttribute)) {
      g_originalChangeAttribute = reinterpret_cast<ChangeAttributeOriginalFn>(*original);
    }
    // oCNpc::OnDamage_Hit - filter arrow/spell damage from other players
    if (auto original = CreateHook(kOnDamageHitHookAddress, (DWORD)OnOnDamageHit)) {
      g_originalOnDamageHit = reinterpret_cast<OnDamageHitOriginalFn>(*original);
    }
    // oCAIArrowBase::CreateTrail - script-controlled arrow/bolt trail visibility
    if (auto original = CreateHook(kCreateArrowTrailHookAddress, (DWORD)OnCreateArrowTrail)) {
      g_originalCreateArrowTrail = reinterpret_cast<CreateArrowTrailOriginalFn>(*original);
    }
    if (auto original = CreateHook(kCheckRemoveNpcHookAddress, (DWORD)OnCheckRemoveNpc)) {
      g_originalCheckRemoveNpc = reinterpret_cast<CheckRemoveNpcOriginalFn>(*original);
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
    LanguageManager::Instance().LoadLanguages(".\\Multiplayer\\Localization\\", Config::Instance().language);
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
