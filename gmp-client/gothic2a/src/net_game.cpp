/*
MIT License

Copyright (c) 2022 Gothic Multiplayer Team.

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

#include "net_game.h"

#include <bitsery/adapter/buffer.h>
#include <bitsery/bitsery.h>
#include <bitsery/ext/std_optional.h>
#include <bitsery/traits/array.h>
#include <bitsery/traits/string.h>
#include <bitsery/traits/vector.h>
#include <spdlog/spdlog.h>
#include <wincrypt.h>

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <glm/glm.hpp>
#include <iomanip>
#include <limits>
#include <list>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "CActiveAniID.h"
#include "CChat.h"
#include "CIngame.h"
#include "Interface.h"
#include "Mod.h"
#include "Shared/lua_runtime/lua_value_codec.h"
#include "ZenGin/zGothicAPI.h"
#include "actor_sync.h"
#include "animation_sync.h"
#include "config.h"
#include "character_appearance.h"
#include "dev/dev_tools.h"
#include "gmp_core.h"
#include "hooking/MemoryPatch.h"
#include "language.h"
#include "magic_sync.h"
#include "item_instance.h"
#include "main_menu.h"
#include "net_enums.h"
#include "npc_animation_utils.h"
#include "npc_weapon_utils.h"
#include "packets.h"
#include "patch.h"
#include "player_name_utils.hpp"
#include "script_instance_scope.h"
#include "scripting/gothic_bindings.h"
#include "scripting/gothic_events.h"
#include "scripting/item_ground.h"
#include "scripting/lua_camera.h"
#include "scripting/lua_sound3d.h"
#include "scripting/lua_world_label.h"
#include "scripting/process_input.h"
#include "shared/event.h"
#include "sky_utils.h"
#include "sol/sol.hpp"
#include "torch_utils.h"
#include "transformation_spells.h"
#include "version.h"
#include "weapon_skills.h"
#include "world_utils.hpp"

const char* LANG_DIR = ".\\Multiplayer\\Localization\\";

float fWRatio, fHRatio;
extern CIngame* global_ingame;

using namespace Net;

namespace {
using gmp::gothic::NetworkItemName;
using gmp::gothic::LocalItemIndex;

constexpr auto kVoiceActivityTimeout = std::chrono::milliseconds(400);

bool IsValidVoiceChannelName(const std::string& channel) {
  if (channel.empty() || channel.size() > kMaxVoiceChannelNameLength) {
    return false;
  }
  return std::all_of(channel.begin(), channel.end(), [](unsigned char character) {
    return character >= 0x20 && character != 0x7f;
  });
}

oCMenu_Status* GetStatusMenu() {
  zSTRING status_menu_name("MENU_STATUS");
  if (auto* menu = dynamic_cast<oCMenu_Status*>(zCMenu::GetByName(status_menu_name))) {
    return menu;
  }

  zSTRING fallback_name("STATUS");
  return dynamic_cast<oCMenu_Status*>(zCMenu::GetByName(fallback_name));
}

class ScopedLocalLifecycleEventSuppression {
public:
  ScopedLocalLifecycleEventSuppression() : previous_(ShouldSuppressLocalLifecycleEvents()) {
    SetSuppressLocalLifecycleEvents(true);
  }

  ~ScopedLocalLifecycleEventSuppression() {
    SetSuppressLocalLifecycleEvents(previous_);
  }

private:
  bool previous_;
};

std::uint8_t NormalizeLifeState(std::uint8_t life_state, std::int32_t health) {
  switch (life_state) {
    case PLAYER_LIFE_DEAD:
    case PLAYER_LIFE_UNCONSCIOUS:
      return life_state;
    case PLAYER_LIFE_ALIVE:
      return health <= 0 ? PLAYER_LIFE_DEAD : PLAYER_LIFE_ALIVE;
    default:
      break;
  }

  return health <= 0 ? PLAYER_LIFE_DEAD : PLAYER_LIFE_ALIVE;
}

bool IsTerminalLifeState(std::uint8_t life_state) {
  return life_state == PLAYER_LIFE_DEAD || life_state == PLAYER_LIFE_UNCONSCIOUS;
}

void CloseSpellBook(oCNpc* npc) {
  if (!npc) {
    return;
  }

  if (auto* spell_book = npc->GetSpellBook()) {
    spell_book->Close(1);
  }
}

oCItem* RemoveHandItem(oCNpc* npc, bool right_hand) {
  if (!npc) {
    return nullptr;
  }

  zCVob* current_vob = right_hand ? npc->GetRightHand() : npc->GetLeftHand();
  if (!current_vob) {
    return nullptr;
  }
  if (RemoveTorchFromHand(npc, right_hand)) return nullptr;

  zSTRING slot(right_hand ? NPC_NODE_RIGHTHAND : NPC_NODE_LEFTHAND);
  oCItem* removed_item = dynamic_cast<oCItem*>(npc->RemoveFromSlot(slot, 0, 1));
  return removed_item ? removed_item : dynamic_cast<oCItem*>(current_vob);
}

void RemoveItemFromHandSlots(oCNpc* npc, oCItem* item) {
  if (!npc || !item) {
    return;
  }

  if (npc->GetRightHand() == item) {
    RemoveHandItem(npc, true);
  }

  if (npc->GetLeftHand() == item) {
    RemoveHandItem(npc, false);
  }
}

void ClearNpcHandsForLifecycle(oCNpc* npc) {
  if (!npc) {
    return;
  }

  oCItem* right_hand = RemoveHandItem(npc, true);
  oCItem* left_hand = RemoveHandItem(npc, false);
  if (right_hand) {
    right_hand->RemoveVobFromWorld();
  }
  if (left_hand && left_hand != right_hand) {
    left_hand->RemoveVobFromWorld();
  }
}

void SetNpcHealth(oCNpc* npc, int health) {
  if (npc) {
    npc->SetAttribute(NPC_ATR_HITPOINTS, health);
  }
}

int ClampAliveHealth(oCNpc* npc, int health) {
  if (!npc) {
    return std::max(1, health);
  }

  const int max_health = std::max(1, npc->GetAttribute(NPC_ATR_HITPOINTSMAX));
  return std::clamp(health, 1, max_health);
}



void TriggerMobTarget(oCMobInter* mob) {
  if (!mob || mob->triggerTarget.IsEmpty() || !ogame || !ogame->GetGameWorld()) {
    return;
  }

  zCMover* mover = dynamic_cast<zCMover*>(ogame->GetGameWorld()->SearchVobByName(mob->triggerTarget));
  if (mover) {
    mover->TriggerMover(mover);
  }
}

oCItem* GetNpcSlotItem(oCNpc* npc, const char* slot_name) {
  if (!npc) {
    return nullptr;
  }

  zSTRING slot(slot_name);
  return npc->GetSlotItem(slot);
}

std::int32_t GetSlotItemInstance(oCNpc* npc, const char* slot_name) {
  if (auto* item = GetNpcSlotItem(npc, slot_name)) {
    return item->GetInstance();
  }

  return 0;
}

bool IsNetworkItemInstance(std::int32_t instance) {
  return gmp::gothic::IsItemInstanceIndex(instance);
}

oCItem* CreateNetworkItem(std::int32_t instance) {
  if (!IsNetworkItemInstance(instance) || !zfactory) {
    return nullptr;
  }

  auto* parser = Gothic_II_Addon::parser;
  std::optional<ScriptInstanceScope> scope;
  if (parser) {
    scope.emplace(*parser, instance);
  }
  oCItem* item = zfactory->CreateItem(static_cast<int>(instance));
  if (!item) {
    SPDLOG_WARN("Failed to create item instance {}", instance);
  }
  return item;
}

oCItem* InsertOwnedItem(oCNpc* npc, oCItem* item, bool distinct = false, bool presentation = true) {
  if (!npc || !item) {
    return nullptr;
  }
  // Insert owns a reference and can coalesce a stack. Use its returned object;
  // the factory reference is ours to release even when coalescing occurred.
  const int flags = item->flags;
  if (distinct) {
    item->flags &= ~ITM_FLAG_MULTI;
  }
  auto* inserted = npc->inventory2.Insert(item);
  if (presentation && inserted == item) NetGame::Instance().TrackProxyPresentationItem(npc, inserted);
  if (distinct) {
    item->flags = flags;
    if (inserted) {
      inserted->flags = flags;
    }
  }
  item->Release();
  return inserted;
}

int GetCurrentAnimationId() {
  CActiveAniID* active_ani_id = CActiveAniID::GetInstance();
  if (!active_ani_id) {
    return -1;
  }

  return active_ani_id->GetAniID();
}

std::string GetAnimationNameById(oCNpc* npc, int animation_id) {
  if (!npc || animation_id < 0) {
    return {};
  }

  zCModel* model = npc->GetModel();
  zCModelAni* animation = model ? model->GetAniFromAniID(static_cast<int>(animation_id)) : nullptr;
  if (!animation || animation->GetAniName().IsEmpty()) {
    return {};
  }

  return animation->GetAniName().ToChar();
}

bool IsLoopingModelAnimation(zCModel* model, zCModelAni* animation) {
  if (!model || !animation) return false;
  if (auto* active = model->GetActiveAni(animation); active && !active->isFadingOut)
    return (active->nextAniOverride ? active->nextAniOverride : active->nextAni) == animation;
  auto* next = animation->nextAni;
  const int id = animation->GetAniID();
  if (!next && model->modelProtoList.GetNumInList() > 0) {
    auto* base = model->modelProtoList[0];
    if (base && id >= 0 && id < base->protoAnis.GetNumInList() && base->protoAnis[id]) next = base->protoAnis[id]->nextAni;
  }
  return next && model->GetAniFromAniID(next->GetAniID()) == animation;
}

bool IsPlayingDamageReaction(zCModel* model) {
  if (!model)
    return false;
  for (int index = 0; index < std::clamp(model->numActiveAnis, 0, zMDL_MAX_ANIS_PARALLEL); ++index) {
    auto* active = model->aniChannels[index];
    if (active && !active->isFadingOut && active->protoAni && gmp::IsDamageReactionAnimation(active->protoAni->GetAniName().ToChar()))
      return true;
  }
  return false;
}

std::optional<int> FindParserIndex(const char* instance_name) {
  if (!instance_name || instance_name[0] == '\0') {
    return std::nullopt;
  }

  // GetParser() exposes cur_parser, which native FX/music can leave pointing
  // at a different symbol table. NPC/item indices belong to the game parser.
  zCParser* parser = Gothic_II_Addon::parser;
  if (!parser) {
    return std::nullopt;
  }

  zSTRING name(instance_name);
  const int index = parser->GetIndex(name);
  if (index < 0) {
    return std::nullopt;
  }

  return index;
}

std::optional<std::string> FindParserInstanceName(std::int32_t index) {
  if (!IsNetworkItemInstance(index)) {
    return std::nullopt;
  }

  zCParser* parser = Gothic_II_Addon::parser;
  zCPar_Symbol* symbol = parser ? parser->GetSymbol(index) : nullptr;
  if (!symbol || symbol->type != zPAR_TYPE_INSTANCE || symbol->name.IsEmpty()) {
    return std::nullopt;
  }
  return std::string(symbol->name.ToChar());
}

void ApplyAuthoritativeNpcPosition(oCNpc* npc, const zVEC3& position) {
  if (!npc) {
    return;
  }

  const int coll_det_stat = npc->collDetectionStatic ? 1 : 0;
  const int coll_det_dyn = npc->collDetectionDynamic ? 1 : 0;
  npc->SetCollDet(0);
  npc->SetPositionWorld(position);
  npc->SetCollDetStat(coll_det_stat);
  npc->SetCollDetDyn(coll_det_dyn);
}

void SuppressServerNpcAI(oCNpc* npc) {
  if (!npc) {
    return;
  }
  npc->startAIState = 0;
  npc->daily_routine = 0;
  npc->state.SetRoutine(nullptr, nullptr);
  if (rtnMan) {
    rtnMan->RemoveRoutine(npc);
  }
  npc->state.hasRoutine = false;
  npc->state.ClearAIState();
  npc->respawnOn = false;
  npc->ClearPerception();
  npc->ClearPerceptionLists();
  npc->SetMovLock(true);
  if (auto* model = npc->GetModel()) {
    model->SetRandAnisEnabled(false);
    model->doVobRot = false;
  }
}

bool IsHelmetItem(oCItem* item) {
  return item && (item->wear & ITM_WEAR_HEAD) != 0;
}

bool IsShieldItem(oCItem* item) {
  return item && item->HasFlag(ITM_FLAG_SHIELD);
}

bool IsArmorItem(oCItem* item) {
  return item && (item->wear & ITM_WEAR_TORSO) != 0;
}

bool IsMeleeWeaponItem(oCItem* item) {
  return item && (item->HasFlag(ITM_FLAG_SWD) || item->HasFlag(ITM_FLAG_DAG) || item->HasFlag(ITM_FLAG_AXE) || item->HasFlag(ITM_FLAG_2HD_SWD) ||
                  item->HasFlag(ITM_FLAG_2HD_AXE));
}

bool IsRangedWeaponItem(oCItem* item) {
  return item && (item->HasFlag(ITM_FLAG_BOW) || item->HasFlag(ITM_FLAG_CROSSBOW));
}

oCItem* GetCurrentMeleeWeapon(oCNpc* npc) {
  if (!npc) {
    return nullptr;
  }

  // Hand and mode updates can arrive separately. Observe the equipped object,
  // not a mode that may still describe the preceding draw/sheath transition.
  auto* held = dynamic_cast<oCItem*>(npc->GetRightHand());
  if (IsMeleeWeaponItem(held) && held->HasFlag(ITM_FLAG_ACTIVE)) {
    return held;
  }
  return npc->GetEquippedMeleeWeapon();
}

oCItem* GetCurrentRangedWeapon(oCNpc* npc) {
  if (!npc) {
    return nullptr;
  }

  for (auto* held : {npc->GetLeftHand(), npc->GetRightHand()}) {
    auto* item = dynamic_cast<oCItem*>(held);
    if (IsRangedWeaponItem(item) && item->HasFlag(ITM_FLAG_ACTIVE)) {
      return item;
    }
  }
  return npc->GetEquippedRangedWeapon();
}

bool HasItemFlag(oCItem* item, int item_flag) {
  return item && item->HasFlag(item_flag);
}

std::vector<oCItem*> GetActiveInventoryItems(oCNpc* npc, int item_flag) {
  std::vector<oCItem*> items;
  if (!npc) {
    return items;
  }

  for (zCListSort<oCItem>* entry = npc->inventory2.inventory.GetNextInList(); entry; entry = entry->GetNextInList()) {
    oCItem* item = entry->GetData();
    if (HasItemFlag(item, item_flag) && item->HasFlag(ITM_FLAG_ACTIVE)) {
      items.push_back(item);
    }
  }

  return items;
}

std::int32_t GetEquippedInventoryItemInstance(oCNpc* npc, int item_flag) {
  const auto items = GetActiveInventoryItems(npc, item_flag);
  if (items.empty()) {
    return 0;
  }

  return items.front()->GetInstance();
}

std::array<std::int32_t, 2> GetEquippedRingInstances(oCNpc* npc, const std::array<std::int32_t, 2>& previous_rings) {
  std::array<std::int32_t, 2> rings{0, 0};
  std::vector<std::int32_t> active_instances;
  for (auto* item : GetActiveInventoryItems(npc, ITM_FLAG_RING)) {
    active_instances.push_back(item->GetInstance());
  }

  for (std::size_t i = 0; i < previous_rings.size(); ++i) {
    const auto previous = previous_rings[i];
    if (previous == 0) {
      continue;
    }

    const auto it = std::find(active_instances.begin(), active_instances.end(), previous);
    if (it != active_instances.end()) {
      rings[i] = previous;
      active_instances.erase(it);
    }
  }

  for (const auto instance : active_instances) {
    const auto empty_slot = std::find(rings.begin(), rings.end(), 0);
    if (empty_slot == rings.end()) {
      break;
    }

    *empty_slot = instance;
  }

  return rings;
}

std::int32_t GetActiveSpellItemInstance(oCNpc* npc) {
  auto* spell = GetSelectedMagicSpell(npc);
  auto* book = npc ? npc->GetSpellBook() : nullptr;
  if (spell && npc->GetWeaponMode() == NPC_WEAPON_MAG) {
    auto* item = book->GetSpellItem(spell);
    if (!item)
      return 0;
    return item->GetInstance();
  }

  return 0;
}

int GetInventoryAmount(oCNpc* npc, int instance) {
  if (npc == nullptr || instance <= 0) {
    return 0;
  }

  std::int64_t total = 0;
  for (auto* entry = npc->inventory2.inventory.GetNextInList(); entry; entry = entry->GetNextInList()) {
    if (auto* item = entry->GetData(); item && item->GetInstance() == instance) total += std::max(0, item->amount);
  }
  return static_cast<int>(std::min<std::int64_t>(total, std::numeric_limits<std::int32_t>::max()));
}

void ReconcileInventoryAmount(oCNpc* npc, int instance, int desired) {
  if (!npc || instance <= 0 || (desired > 0 && !IsNetworkItemInstance(instance))) return;
  desired = std::max(0, desired);
  const int carried_torches = GetCarriedTorchAmount(npc, instance);
  if (desired < carried_torches) {
    ClearTorchHands(npc);
    ReconcileInventoryAmount(npc, instance, desired);
    return;
  }
  desired -= carried_torches;
  std::int64_t current = 0;
  std::vector<oCItem*> items;
  for (auto* entry = npc->inventory2.inventory.GetNextInList(); entry; entry = entry->GetNextInList()) {
    if (auto* item = entry->GetData(); item && item->GetInstance() == instance) {
      current += std::max(0, item->amount);
      items.push_back(item);
    }
  }
  auto* interaction = npc->interactItem;
  if (interaction && interaction->GetInstance() == instance && std::find(items.begin(), items.end(), interaction) == items.end()) {
    // Native item interaction reserves an owned object outside inventory2.
    // A server acknowledgement must not create another copy of that object.
    if (desired < std::max(0, interaction->amount)) {
      npc->ClearEM();
      npc->SetInteractItem(nullptr);
      ReconcileInventoryAmount(npc, instance, desired);
      return;
    }
    desired = std::max(0, desired - std::max(0, interaction->amount));
  }
  if (current < desired) {
    if (auto* created = CreateNetworkItem(instance)) {
      created->amount = static_cast<int>(desired - current);
      InsertOwnedItem(npc, created, false, false);
    }
    return;
  }
  // Remove inactive stack elements first. Clearing the last owned element also
  // clears native equipment/hands, so a rejected pickup cannot leave a phantom.
  std::stable_sort(items.begin(), items.end(), [](auto* left, auto* right) {
    return (left->HasFlag(ITM_FLAG_ACTIVE) != 0) < (right->HasFlag(ITM_FLAG_ACTIVE) != 0);
  });
  for (auto* item : items) {
    const int count = static_cast<int>(std::min<std::int64_t>(current - desired, std::max(0, item->amount)));
    if (count <= 0) break;
    if (count == item->amount) {
      npc->UnequipItem(item);
      if ((item->mainflag & (ITM_CAT_RUNE | ITM_CAT_MAGIC)) != 0) {
        UnregisterMagicItem(npc, item);
      }
      RemoveItemFromHandSlots(npc, item);
      npc->inventory2.Remove(item);
    } else {
      item->amount -= count;
    }
    current -= count;
  }
}

oCItem* GetOrCreateInventoryItem(oCNpc* npc, std::int32_t instance, int required_flag) {
  if (!npc || !IsNetworkItemInstance(instance)) {
    return nullptr;
  }

  oCItem* item = npc->inventory2.IsIn(static_cast<int>(instance), 1);
  if (!item) {
    item = CreateNetworkItem(instance);
    if (!item) {
      return nullptr;
    }

    if (!HasItemFlag(item, required_flag)) {
      item->Release();
      return nullptr;
    }

    item = InsertOwnedItem(npc, item);
  } else if (!HasItemFlag(item, required_flag)) {
    return nullptr;
  }

  return item;
}

oCItem* GetOrCreateValidInventoryItem(oCNpc* npc, std::int32_t instance, bool (*is_valid_item)(oCItem*)) {
  if (!npc || !IsNetworkItemInstance(instance) || !is_valid_item) {
    return nullptr;
  }

  oCItem* item = npc->inventory2.IsIn(static_cast<int>(instance), 1);
  if (!item) {
    item = CreateNetworkItem(instance);
    if (!item) {
      return nullptr;
    }

    if (!is_valid_item(item)) {
      item->Release();
      return nullptr;
    }

    item = InsertOwnedItem(npc, item);
  } else if (!is_valid_item(item)) {
    return nullptr;
  }

  return item;
}

void SyncEquippedCombatItem(oCNpc* npc, std::int32_t instance, oCItem* current, bool (*is_valid_item)(oCItem*)) {
  if (!npc) {
    return;
  }

  if (instance == 0 || !IsNetworkItemInstance(instance)) {
    if (current) {
      npc->UnequipItem(current);
      RemoveItemFromHandSlots(npc, current);
    }
    return;
  }

  if (current && current->GetInstance() == static_cast<int>(instance) && current->HasFlag(ITM_FLAG_ACTIVE) &&
      (!is_valid_item || is_valid_item(current))) {
    return;
  }

  if (current) {
    npc->UnequipItem(current);
    RemoveItemFromHandSlots(npc, current);
  }

  if (auto* item = GetOrCreateValidInventoryItem(npc, instance, is_valid_item)) {
    // Equip toggles an already active item off. Its body slot may be empty
    // precisely because the same equipped object is currently held.
    const char* sheathed_slot = GetSheathedWeaponSlot(item);
    if (sheathed_slot && item->HasFlag(ITM_FLAG_ACTIVE)) {
      if (npc->GetLeftHand() != item && npc->GetRightHand() != item) {
        MoveInventoryWeaponToSlot(npc, item, sheathed_slot);
      }
    } else {
      npc->Equip(item);
    }
  }
}

void SyncEquippedInventoryItem(oCNpc* npc, std::int32_t instance, int item_flag) {
  if (!npc) {
    return;
  }

  const auto active_items = GetActiveInventoryItems(npc, item_flag);
  for (auto* item : active_items) {
    if (instance == 0 || item->GetInstance() != static_cast<int>(instance)) {
      npc->UnequipItem(item);
    }
  }

  if (instance == 0) {
    return;
  }

  const auto remaining_items = GetActiveInventoryItems(npc, item_flag);
  const auto already_equipped = std::any_of(remaining_items.begin(), remaining_items.end(),
                                            [instance](oCItem* item) { return item && item->GetInstance() == static_cast<int>(instance); });
  if (already_equipped) {
    return;
  }

  if (auto* item = GetOrCreateInventoryItem(npc, instance, item_flag)) {
    npc->Equip(item);
  }
}

void SyncEquippedRingItems(oCNpc* npc, std::int32_t left_instance, std::int32_t right_instance) {
  if (!npc) {
    return;
  }

  std::array<std::int32_t, 2> desired{left_instance, right_instance};
  std::array<bool, 2> matched{false, false};

  const auto active_rings = GetActiveInventoryItems(npc, ITM_FLAG_RING);
  for (auto* item : active_rings) {
    bool keep = false;
    for (std::size_t i = 0; i < desired.size(); ++i) {
      if (!matched[i] && desired[i] != 0 && item->GetInstance() == static_cast<int>(desired[i])) {
        matched[i] = true;
        keep = true;
        break;
      }
    }

    if (!keep) {
      npc->UnequipItem(item);
    }
  }

  for (std::size_t i = 0; i < desired.size(); ++i) {
    if (matched[i] || desired[i] == 0) {
      continue;
    }

    oCItem* item = nullptr;
    for (auto* entry = npc->inventory2.inventory.GetNextInList(); entry; entry = entry->GetNextInList()) {
      auto* candidate = entry->GetData();
      if (candidate && candidate->GetInstance() == desired[i] && candidate->HasFlag(ITM_FLAG_RING) &&
          !candidate->HasFlag(ITM_FLAG_ACTIVE)) {
        item = candidate;
        break;
      }
    }
    if (!item) {
      // Gothic can retain an active stack with amount > 1. Reserve a second
      // element from that owned quantity before inserting a distinct object.
      oCItem* stack = nullptr;
      for (auto* entry = npc->inventory2.inventory.GetNextInList(); entry; entry = entry->GetNextInList()) {
        auto* candidate = entry->GetData();
        if (candidate && candidate->GetInstance() == desired[i] && candidate->amount > 1) { stack = candidate; break; }
      }
      auto* created = CreateNetworkItem(desired[i]);
      if (created && !created->HasFlag(ITM_FLAG_RING)) {
        created->Release();
        created = nullptr;
      }
      if (created && stack) --stack->amount;
      item = InsertOwnedItem(npc, created, true);
      if (!item && stack) ++stack->amount;
    }
    if (item) {
      npc->Equip(item);
    }
  }
}

void SyncEquippedSlotItem(oCNpc* npc, std::int32_t instance, const char* slot_name, bool (*is_valid_slot_item)(oCItem*)) {
  if (!npc) {
    return;
  }

  oCItem* current = GetNpcSlotItem(npc, slot_name);
  if (instance == 0) {
    if (current) {
      npc->UnequipItem(current);
    }
    return;
  }

  if (!IsNetworkItemInstance(instance)) {
    return;
  }

  if (current && current->GetInstance() == static_cast<int>(instance)) {
    return;
  }

  if (current) {
    npc->UnequipItem(current);
  }

  oCItem* item = npc->inventory2.IsIn(static_cast<int>(instance), 1);
  if (!item) {
    item = CreateNetworkItem(instance);
    if (!item) {
      return;
    }

    if (!is_valid_slot_item(item)) {
      item->Release();
      return;
    }

    item = InsertOwnedItem(npc, item);
  } else if (!is_valid_slot_item(item)) {
    return;
  }

  npc->Equip(item);
}

bool SyncEquippedPacketSlot(oCNpc* npc, std::int32_t instance, std::int16_t slot_id) {
  switch (slot_id) {
    case EQUIP_SLOT_ARMOR:
      SyncEquippedCombatItem(npc, instance, npc ? npc->GetEquippedArmor() : nullptr, IsArmorItem);
      return true;
    case EQUIP_SLOT_MELEE_WEAPON:
      SyncEquippedCombatItem(npc, instance, GetCurrentMeleeWeapon(npc), IsMeleeWeaponItem);
      return true;
    case EQUIP_SLOT_RANGED_WEAPON:
      SyncEquippedCombatItem(npc, instance, GetCurrentRangedWeapon(npc), IsRangedWeaponItem);
      return true;
    case EQUIP_SLOT_HELMET:
      SyncEquippedSlotItem(npc, instance, NPC_NODE_HELMET, IsHelmetItem);
      return true;
    case EQUIP_SLOT_SHIELD:
      SyncEquippedSlotItem(npc, instance, NPC_NODE_SHIELD, IsShieldItem);
      return true;
    case EQUIP_SLOT_AMULET:
      SyncEquippedInventoryItem(npc, instance, ITM_FLAG_AMULET);
      return true;
    case EQUIP_SLOT_BELT:
      SyncEquippedInventoryItem(npc, instance, ITM_FLAG_BELT);
      return true;
    default:
      return false;
  }
}

std::int32_t ObserveEquipmentSlot(Gothic2APlayer& actor, std::int16_t slot, std::int32_t requested) {
  auto* npc = actor.GetNpc();
  if (!npc) return 0;
  switch (slot) {
    case EQUIP_SLOT_ARMOR: return npc->GetEquippedArmor() ? npc->GetEquippedArmor()->GetInstance() : 0;
    case EQUIP_SLOT_HELMET: return GetSlotItemInstance(npc, NPC_NODE_HELMET);
    case EQUIP_SLOT_SHIELD: return GetSlotItemInstance(npc, NPC_NODE_SHIELD);
    case EQUIP_SLOT_MELEE_WEAPON: return GetCurrentMeleeWeapon(npc) ? GetCurrentMeleeWeapon(npc)->GetInstance() : 0;
    case EQUIP_SLOT_RANGED_WEAPON: return GetCurrentRangedWeapon(npc) ? GetCurrentRangedWeapon(npc)->GetInstance() : 0;
    case EQUIP_SLOT_AMULET: return GetEquippedInventoryItemInstance(npc, ITM_FLAG_AMULET);
    case EQUIP_SLOT_BELT: return GetEquippedInventoryItemInstance(npc, ITM_FLAG_BELT);
    case EQUIP_SLOT_RING_LEFT:
    case EQUIP_SLOT_RING_RIGHT: {
      const auto rings = GetEquippedRingInstances(npc, {LocalItemIndex(actor.base_player().equipped_ring_left()), LocalItemIndex(actor.base_player().equipped_ring_right())});
      return rings[slot == EQUIP_SLOT_RING_LEFT ? 0 : 1];
    }
    default:
      for (auto* entry = npc->inventory2.inventory.GetNextInList(); entry; entry = entry->GetNextInList()) {
        if (auto* item = entry->GetData(); item && item->GetInstance() == requested && item->HasFlag(ITM_FLAG_ACTIVE)) return requested;
      }
      return 0;
  }
}

std::int16_t ResolveEquipmentSlot(oCNpc* npc, int instance) {
  if (!npc || !IsNetworkItemInstance(instance)) return EQUIP_SLOT_GENERIC;
  auto* item = npc->inventory2.IsIn(instance, 1);
  const bool temporary = item == nullptr;
  if (!item) item = CreateNetworkItem(instance);
  if (!item) return EQUIP_SLOT_GENERIC;
  std::int16_t slot = EQUIP_SLOT_GENERIC;
  if (IsArmorItem(item)) slot = EQUIP_SLOT_ARMOR;
  else if (IsHelmetItem(item)) slot = EQUIP_SLOT_HELMET;
  else if (IsShieldItem(item)) slot = EQUIP_SLOT_SHIELD;
  else if (IsMeleeWeaponItem(item)) slot = EQUIP_SLOT_MELEE_WEAPON;
  else if (IsRangedWeaponItem(item)) slot = EQUIP_SLOT_RANGED_WEAPON;
  else if (item->HasFlag(ITM_FLAG_AMULET)) slot = EQUIP_SLOT_AMULET;
  else if (item->HasFlag(ITM_FLAG_BELT)) slot = EQUIP_SLOT_BELT;
  else if (item->HasFlag(ITM_FLAG_RING)) {
    const auto rings = GetEquippedRingInstances(npc, {0, 0});
    slot = rings[0] == 0 ? EQUIP_SLOT_RING_LEFT : EQUIP_SLOT_RING_RIGHT;
  }
  if (temporary) item->Release();
  return slot;
}

struct WorldTimerTickValues {
  float ticks_per_hour;
  float ticks_per_minute;
  float ticks_per_day;
};

WorldTimerTickValues ReadWorldTimerTicks() {
  constexpr DWORD kTicksPerHourAddr = 0x0083E168;
  constexpr DWORD kTicksPerMinuteAddr = 0x00AB3764;
  constexpr DWORD kTicksPerDayAddr = 0x00AB371C;
  return {*reinterpret_cast<float*>(kTicksPerHourAddr), *reinterpret_cast<float*>(kTicksPerMinuteAddr), *reinterpret_cast<float*>(kTicksPerDayAddr)};
}

void WriteWorldTimerTicks(const WorldTimerTickValues& values) {
  constexpr DWORD kTicksPerHourAddr = 0x0083E168;
  constexpr DWORD kTicksPerMinuteAddr = 0x00AB3764;
  constexpr DWORD kTicksPerDayAddr = 0x00AB371C;
  MemoryPatch::WriteMemory(kTicksPerHourAddr, reinterpret_cast<PBYTE>(const_cast<float*>(&values.ticks_per_hour)), sizeof(float));
  MemoryPatch::WriteMemory(kTicksPerMinuteAddr, reinterpret_cast<PBYTE>(const_cast<float*>(&values.ticks_per_minute)), sizeof(float));
  MemoryPatch::WriteMemory(kTicksPerDayAddr, reinterpret_cast<PBYTE>(const_cast<float*>(&values.ticks_per_day)), sizeof(float));
}

float PositiveOrFallback(float value, float fallback) {
  return value > 0.0f ? value : fallback;
}

void ApplyGameTimeToEngine(const STime& time) {
  if (!ogame) {
    return;
  }

  const int day = static_cast<int>(time.day);
  const int hour = static_cast<int>(time.hour);
  const int min = static_cast<int>(time.min);

  auto* timer = ogame->GetWorldTimer();
  if (!timer) {
    ogame->SetTime(day, hour, min);
    return;
  }

  const auto ticks = ReadWorldTimerTicks();
  const float ticks_per_day = PositiveOrFallback(ticks.ticks_per_day, Gothic_II_Addon::WLD_TICKSPERDAY);
  const float ticks_per_hour = PositiveOrFallback(ticks.ticks_per_hour, Gothic_II_Addon::WLD_TICKSPERHOUR);
  const float ticks_per_minute = PositiveOrFallback(ticks.ticks_per_minute, Gothic_II_Addon::WLD_TICKSPERMIN);
  const float full_time =
      static_cast<float>(day) * ticks_per_day + static_cast<float>(hour) * ticks_per_hour + static_cast<float>(min) * ticks_per_minute;
  timer->SetFullTime(full_time);
}

void ApplyDayLengthToEngine(float day_length_ms) {
  static std::optional<WorldTimerTickValues> original_ticks;

  if (!original_ticks.has_value()) {
    original_ticks = ReadWorldTimerTicks();
  }

  WorldTimerTickValues current_ticks = ReadWorldTimerTicks();
  const float current_ticks_per_day = current_ticks.ticks_per_day <= 0.0f ? 1.0f : current_ticks.ticks_per_day;
  float percent = 0.0f;
  if (ogame && ogame->GetWorldTimer()) {
    percent = ogame->GetWorldTimer()->worldTime / current_ticks_per_day;
  }

  if (day_length_ms <= 0.0f) {
    if (original_ticks.has_value()) {
      WriteWorldTimerTicks(*original_ticks);
      if (ogame && ogame->GetWorldTimer()) {
        ogame->GetWorldTimer()->worldTime = percent * original_ticks->ticks_per_day;
      }
    }
    return;
  }

  WorldTimerTickValues new_ticks;
  new_ticks.ticks_per_day = day_length_ms;
  new_ticks.ticks_per_hour = day_length_ms / 24.0f;
  new_ticks.ticks_per_minute = new_ticks.ticks_per_hour / 60.0f;
  WriteWorldTimerTicks(new_ticks);

  if (ogame && ogame->GetWorldTimer()) {
    ogame->GetWorldTimer()->worldTime = percent * new_ticks.ticks_per_day;
  }
}

std::string BuildMultiplayerStatusMessage() {
  std::string tag = GIT_TAG;
  std::string commit = GIT_COMMIT;
  std::ostringstream message;
  message << "Gothic Multiplayer Classic";
  if (!tag.empty() || !commit.empty()) {
    message << " (";
    if (!tag.empty()) {
      message << tag;
    }
    if (!commit.empty()) {
      if (!tag.empty()) {
        message << " ";
      }
      message << commit;
    }
    message << ")";
  }
  return message.str();
}

}  // namespace

NetGame::NetGame() : task_scheduler(nullptr), game_client(nullptr), resource_runtime(nullptr), content_transition_manager(nullptr) {
  task_scheduler = std::make_unique<gmp::GothicTaskScheduler>();
  game_client = std::make_unique<gmp::client::GameClient>(*this, *task_scheduler);
  resource_runtime = std::make_unique<ClientResourceRuntime>();
  content_transition_manager = std::make_unique<gmp::gothic::ContentTransitionManager>();
  std::string content_error;
  if (!content_transition_manager->Initialize(content_error)) {
    SPDLOG_ERROR("Addon content initialization failed: {}", content_error);
  }
  resource_runtime->SetServerInfoProvider(*game_client);
  resource_runtime->SetResetCallback([]() {
    gmp::gothic::ResetGothicEvents();
    gmp::gothic::ResetCamera();
  });
  gmp::gothic::BindGothicEvents(resource_runtime->GetLuaState());
  gmp::gothic::BindGothicSpecific(resource_runtime->GetLuaState());
}

void NetGame::Shutdown() {
  CancelAllNpcActions();
  StopVoiceChat();
  EventManager::Instance().Reset();
  pending_local_spawn_position_.reset();
  if (game_client) {
    game_client->Disconnect();
  }
  for (auto* p : players) {
    if (p && !p->IsLocalPlayer()) p->LeaveGame();
    delete p;
  }
  players.clear();
}

void __stdcall NetGame::ProcessTaskScheduler() {
  NetGame& instance = NetGame::Instance();
  if (instance.task_scheduler) {
    instance.task_scheduler->ProcessTasks();
  }
  if (instance.resource_runtime) {
    instance.resource_runtime->ProcessTimers();
  }
  instance.ApplyPendingLocalSpawnPosition();
  instance.ProcessNpcActions();
  if (zinput) {
    gmp::gothic::ProcessInput(zinput);
  }
  CChat::GetInstance()->HandleInput();
  gmp::gothic::LuaSound3d::UpdateActiveSounds();
  gmp::gothic::LuaWorldLabel::RenderActiveLabels();
  instance.UpdateClientEventState();
  EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnRenderName, 0);
}

void NetGame::ApplyPendingLocalSpawnPosition() {
  if (!pending_local_spawn_position_.has_value()) {
    return;
  }

  auto& correction = *pending_local_spawn_position_;
  Gothic2APlayer* local_player = GetPlayerById(correction.player_id);
  if (!local_player || !local_player->GetNpc()) {
    return;
  }

  ApplyAuthoritativeNpcPosition(local_player->GetNpc(), correction.position);
  --correction.remaining_frames;
  if (correction.remaining_frames <= 0) {
    pending_local_spawn_position_.reset();
  }
}

void NetGame::SetDayLengthMs(float day_length_ms) {
  constexpr float kMinDayLengthMs = 10000.0f;
  const float clamped_day_length_ms = std::max(day_length_ms, kMinDayLengthMs);
  if (std::fabs(clamped_day_length_ms - day_length_ms_) > 0.01f) {
    ApplyDayLengthToEngine(clamped_day_length_ms);
    day_length_ms_ = clamped_day_length_ms;
  }
}

float NetGame::GetDayLengthMs() const {
  return day_length_ms_;
}

bool NetGame::IsVoiceChatAvailable() const {
  return server_voice_enabled_;
}

bool NetGame::IsVoiceChatEnabled() const {
  return IsVoiceChatAvailable() && Config::Instance().IsVoiceChatEnabled() && script_voice_enabled_;
}

void NetGame::SetVoiceChatScriptEnabled(bool enabled) {
  const bool was_enabled = IsVoiceChatEnabled();
  script_voice_enabled_ = enabled;
  const bool is_enabled = IsVoiceChatEnabled();

  if (is_enabled && IsInGame) {
    StartVoiceChat();
  } else if (!is_enabled) {
    StopVoiceChat();
  }

  if (was_enabled != is_enabled) {
    EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnVoiceChatStateChangeName,
                                          gmp::gothic::OnVoiceChatStateEvent{is_enabled, GetVoiceChatRange()});
  }
}

float NetGame::GetVoiceChatRange() const {
  if (!IsVoiceChatAvailable()) {
    return 0.0f;
  }

  if (!script_voice_range_.has_value()) {
    return voice_range_;
  }

  return std::min(voice_range_, *script_voice_range_);
}

bool NetGame::SetVoiceChatRange(float range) {
  if (!std::isfinite(range) || range < 0.0f) {
    return false;
  }

  const float previous_range = GetVoiceChatRange();
  script_voice_range_ = range;
  const float current_range = GetVoiceChatRange();
  if (previous_range != current_range) {
    EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnVoiceChatStateChangeName,
                                          gmp::gothic::OnVoiceChatStateEvent{IsVoiceChatEnabled(), current_range});
  }
  return true;
}

float NetGame::GetVoiceChatOutputVolume() const {
  return script_voice_output_volume_;
}

void NetGame::SetVoiceChatOutputVolume(float volume) {
  if (!std::isfinite(volume)) {
    return;
  }
  script_voice_output_volume_ = std::clamp(volume, 0.0f, 1.0f);
  voice_playback_.SetMasterVolume(Config::Instance().GetVoiceOutputVolume() * script_voice_output_volume_);
}

int NetGame::GetVoiceChatPushToTalkKey() const {
  return script_voice_push_to_talk_key_.value_or(Config::Instance().GetVoicePushToTalkKey());
}

bool NetGame::SetVoiceChatPushToTalkKey(int key) {
  if (key <= 0 || key > std::numeric_limits<std::uint8_t>::max()) {
    return false;
  }

  script_voice_push_to_talk_key_ = key;
  return true;
}

std::vector<std::string> NetGame::GetVoiceChatInputDevices() const {
  return voice_capture_.GetInputDevices();
}

std::string NetGame::GetVoiceChatInputDevice() const {
  return voice_capture_.GetInputDevice();
}

bool NetGame::SetVoiceChatInputDevice(const std::string& device_name) {
  if (device_name == voice_capture_.GetInputDevice()) {
    return true;
  }

  const std::string previous_device = voice_capture_.GetInputDevice();
  if (!voice_capture_.SetInputDevice(device_name)) {
    return false;
  }

  if (!voice_capture_.IsCapturing()) {
    return true;
  }

  SetVoiceTransmitting(false);
  voice_capture_.StopCapture();
  if (voice_capture_.StartCapture()) {
    return true;
  }

  voice_capture_.SetInputDevice(previous_device);
  voice_capture_.StartCapture();
  return false;
}

std::vector<std::string> NetGame::GetVoiceChatOutputDevices() const {
  return voice_playback_.GetOutputDevices();
}

std::string NetGame::GetVoiceChatOutputDevice() const {
  return voice_playback_.GetOutputDevice();
}

bool NetGame::SetVoiceChatOutputDevice(const std::string& device_name) {
  if (device_name == voice_playback_.GetOutputDevice()) {
    return true;
  }

  const std::string previous_device = voice_playback_.GetOutputDevice();
  if (!voice_playback_.SetOutputDevice(device_name)) {
    return false;
  }

  if (!voice_playback_.IsPlaying()) {
    return true;
  }

  voice_playback_.StopPlayback();
  if (voice_playback_.StartPlayback()) {
    return true;
  }

  voice_playback_.SetOutputDevice(previous_device);
  voice_playback_.StartPlayback();
  return false;
}

const std::string& NetGame::GetVoiceChatChannel() const {
  return voice_channel_;
}

bool NetGame::SetVoiceChatChannel(const std::string& channel) {
  if (!IsValidVoiceChannelName(channel)) {
    return false;
  }
  if (channel == voice_channel_) {
    return true;
  }
  if (IsInGame && (!game_client || !game_client->SendVoiceChannel(channel))) {
    return false;
  }

  const std::string old_channel = voice_channel_;
  voice_channel_ = channel;
  EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnVoiceChannelChangeName,
                                        gmp::gothic::OnVoiceChannelChangeEvent{old_channel, voice_channel_});
  return true;
}

bool NetGame::IsVoiceChatTransmitting() const {
  return voice_capture_.IsTransmitting();
}

bool NetGame::IsPlayerVoiceTalking(std::uint64_t player_id) const {
  if (player_id == 0 || player_id > std::numeric_limits<std::uint32_t>::max()) {
    return false;
  }
  return active_voice_players_.contains(static_cast<std::uint32_t>(player_id));
}

bool NetGame::IsPlayerVoiceMuted(std::uint64_t player_id) const {
  if (player_id == 0 || player_id > std::numeric_limits<std::uint32_t>::max()) {
    return false;
  }
  return muted_voice_players_.contains(static_cast<std::uint32_t>(player_id));
}

bool NetGame::SetPlayerVoiceMuted(std::uint64_t player_id, bool muted) {
  if (player_id == 0 || player_id > std::numeric_limits<std::uint32_t>::max()) {
    return false;
  }

  const auto voice_player_id = static_cast<std::uint32_t>(player_id);
  if (muted) {
    muted_voice_players_.insert(voice_player_id);
    voice_playback_.RemovePlayer(voice_player_id);
  } else {
    muted_voice_players_.erase(voice_player_id);
  }
  return true;
}

float NetGame::GetPlayerVoiceVolume(std::uint64_t player_id) const {
  if (player_id == 0 || player_id > std::numeric_limits<std::uint32_t>::max()) {
    return 1.0f;
  }

  auto volume = voice_player_volumes_.find(static_cast<std::uint32_t>(player_id));
  return volume == voice_player_volumes_.end() ? 1.0f : volume->second;
}

bool NetGame::SetPlayerVoiceVolume(std::uint64_t player_id, float volume) {
  if (player_id == 0 || player_id > std::numeric_limits<std::uint32_t>::max() || !std::isfinite(volume)) {
    return false;
  }

  const auto voice_player_id = static_cast<std::uint32_t>(player_id);
  const float clamped_volume = std::clamp(volume, 0.0f, 1.0f);
  if (clamped_volume == 1.0f) {
    voice_player_volumes_.erase(voice_player_id);
  } else {
    voice_player_volumes_[voice_player_id] = clamped_volume;
  }
  return true;
}

void NetGame::UpdateClientEventState() {
  if (ogame && ogame->GetWorldTimer()) {
    int hour = 0;
    int min = 0;
    auto* timer = ogame->GetWorldTimer();
    timer->GetTime(hour, min);
    const int day = timer->GetDay();
    GameTimeSnapshot snapshot{day, hour, min};
    if (!last_game_time_.has_value() || snapshot.day != last_game_time_->day || snapshot.hour != last_game_time_->hour ||
        snapshot.min != last_game_time_->min) {
      EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnTimeName, gmp::gothic::OnTimeEvent{day, hour, min});
      last_game_time_ = snapshot;
    }
  }

  if (ogame && ogame->GetGameWorld()) {
    std::string world_name = ogame->GetGameWorld()->GetWorldFilename().ToChar();
    if (!world_name.empty() && world_name != last_world_name_) {
      EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnWorldEnterName, gmp::gothic::OnWorldEnterEvent{world_name});
      SendPlayerWorldEnter(world_name);
      last_world_name_ = world_name;
    }

    gmp::gothic::ClientItemGroundManager::Instance().RefreshPending();
  }
}

bool NetGame::Connect(std::string_view full_address) {
  // Addon activation may replace GOTHIC.DAT/models before the gameplay world loads.
  CMainMenu::StopMenuScenes();
  if (!game_client || !content_transition_manager) {
    ShowConnectionProgressError("Connection could not be initialized");
    return false;
  }

  std::string content_error;
  if (!content_transition_manager->BeginConnection(content_error)) {
    SPDLOG_ERROR("Connection blocked by content lifecycle: {}", content_error);
    ShowConnectionProgressError("Connection failed: " + content_error);
    return false;
  }

  server_requested_map_.clear();
  map.Clear();

  const bool voice_was_enabled = IsVoiceChatEnabled();
  StopVoiceChat();
  server_voice_enabled_ = false;
  server_voice_transmit_enabled_ = false;
  voice_range_ = 0.0f;
  if (voice_was_enabled) {
    EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnVoiceChatStateChangeName,
                                          gmp::gothic::OnVoiceChatStateEvent{false, 0.0f});
  }
  ResetVoiceChatScriptState();
  game_client->ConnectAsync(full_address);
  // Return true to indicate the connection attempt was started.
  // Connection status is reported by transport callbacks pumped in HandleNetwork().
  return true;
}

void NetGame::RestoreHealth() {
  if (!mp_restore || !IsInGame || !player) {
    return;
  }
  if (last_mp_regen != time(NULL)) {
    last_mp_regen = time(NULL);
    if (player->attribute[NPC_ATR_MANAMAX] != player->attribute[NPC_ATR_MANA]) {
      player->attribute[NPC_ATR_MANA] = player->attribute[NPC_ATR_MANA] + mp_restore;
      if (player->attribute[NPC_ATR_MANA] > player->attribute[NPC_ATR_MANAMAX])
        player->attribute[NPC_ATR_MANA] = player->attribute[NPC_ATR_MANAMAX];
    }
  }
}

void NetGame::HandleNetwork() {
  if (game_client) {
    game_client->HandleNetwork();
  }
  UpdateVoiceChat();
}

void NetGame::StartVoiceChat() {
  if (!IsVoiceChatEnabled() || !IsInGame) {
    return;
  }

  voice_playback_.SetMasterVolume(Config::Instance().GetVoiceOutputVolume() * script_voice_output_volume_);
  if (!voice_playback_.IsPlaying()) {
    voice_playback_.StartPlayback();
  }
  if (server_voice_transmit_enabled_ && !voice_capture_.IsCapturing()) {
    voice_capture_.StartCapture();
  } else if (!server_voice_transmit_enabled_ && voice_capture_.IsCapturing()) {
    SetVoiceTransmitting(false);
    voice_capture_.StopCapture();
  }
}

void NetGame::StopVoiceChat() {
  SetVoiceTransmitting(false);
  voice_capture_.StopCapture();
  voice_playback_.StopPlayback();
  ClearPlayerVoiceActivity();
}

void NetGame::UpdateVoiceChat() {
  UpdatePlayerVoiceActivity();
  if (!voice_capture_.IsCapturing() || !game_client) {
    return;
  }

  const bool push_to_talk_pressed = IsVoiceChatEnabled() && server_voice_transmit_enabled_ && IsInGame && zinput &&
                                    !CChat::GetInstance()->IsInputActive() &&
                                    zinput->KeyPressed(GetVoiceChatPushToTalkKey());
  SetVoiceTransmitting(push_to_talk_pressed);
  if (!push_to_talk_pressed) {
    return;
  }

  for (auto& frame : voice_capture_.ConsumeEncodedFrames()) {
    game_client->SendVoiceFrame(frame.talkspurt_id, frame.sequence, frame.data);
  }
}

void NetGame::SetVoiceTransmitting(bool transmitting) {
  const bool was_transmitting = voice_capture_.IsTransmitting();
  voice_capture_.SetTransmitting(transmitting);
  const bool is_transmitting = voice_capture_.IsTransmitting();
  if (was_transmitting == is_transmitting) {
    return;
  }

  EventManager::Instance().TriggerEvent(is_transmitting ? gmp::gothic::kEventOnVoiceTransmitStartName
                                                        : gmp::gothic::kEventOnVoiceTransmitStopName,
                                        0);
}

void NetGame::MarkPlayerVoiceActive(std::uint32_t player_id) {
  const auto now = std::chrono::steady_clock::now();
  const bool started = active_voice_players_.insert_or_assign(player_id, now).second;
  if (started) {
    EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnPlayerVoiceStartName,
                                          gmp::gothic::OnPlayerVoiceEvent{player_id});
  }
}

void NetGame::EndPlayerVoiceActivity(std::uint32_t player_id) {
  if (active_voice_players_.erase(player_id) == 0) {
    return;
  }
  EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnPlayerVoiceStopName,
                                        gmp::gothic::OnPlayerVoiceEvent{player_id});
}

void NetGame::UpdatePlayerVoiceActivity() {
  const auto now = std::chrono::steady_clock::now();
  std::vector<std::uint32_t> stopped_players;
  for (auto it = active_voice_players_.begin(); it != active_voice_players_.end();) {
    if (now - it->second >= kVoiceActivityTimeout) {
      stopped_players.push_back(it->first);
      it = active_voice_players_.erase(it);
    } else {
      ++it;
    }
  }

  for (std::uint32_t player_id : stopped_players) {
    EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnPlayerVoiceStopName,
                                          gmp::gothic::OnPlayerVoiceEvent{player_id});
  }
}

void NetGame::ClearPlayerVoiceActivity() {
  std::vector<std::uint32_t> stopped_players;
  stopped_players.reserve(active_voice_players_.size());
  for (const auto& [player_id, last_frame] : active_voice_players_) {
    (void)last_frame;
    stopped_players.push_back(player_id);
  }
  active_voice_players_.clear();

  for (std::uint32_t player_id : stopped_players) {
    EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnPlayerVoiceStopName,
                                          gmp::gothic::OnPlayerVoiceEvent{player_id});
  }
}

void NetGame::ResetVoiceChatScriptState() {
  script_voice_enabled_ = true;
  script_voice_range_.reset();
  script_voice_push_to_talk_key_.reset();
  script_voice_output_volume_ = 1.0f;
  voice_capture_.SetInputDevice("");
  voice_playback_.SetOutputDevice("");
  voice_channel_ = "default";
  muted_voice_players_.clear();
  voice_player_volumes_.clear();
  voice_playback_.SetMasterVolume(Config::Instance().GetVoiceOutputVolume());
}

bool NetGame::IsConnected() {
  return game_client && game_client->IsConnected();
}

Gothic2APlayer* NetGame::GetPlayerById(std::uint64_t id) {
  const auto actor = actors_by_id_.find(id);
  return actor == actors_by_id_.end() ? nullptr : actor->second;
}

std::optional<std::uint64_t> NetGame::GetPlayerIdByNpc(oCNpc* npc) {
  const auto actor = actors_by_npc_.find(npc);
  if (actor == actors_by_npc_.end()) return std::nullopt;
  return actor->second->base_player().id();
}

void NetGame::ActorNpcChanged(Gothic2APlayer& actor, oCNpc* previous) {
  if (previous) {
    const auto previous_actor = actors_by_npc_.find(previous);
    if (previous_actor != actors_by_npc_.end() && previous_actor->second == &actor) actors_by_npc_.erase(previous_actor);
  }
  if (actor.GetNpc()) {
    if (previous && previous != actor.GetNpc()) {
      auto owned = proxy_presentation_items_.find(previous);
      if (owned != proxy_presentation_items_.end()) {
        auto items = std::move(owned->second);
        proxy_presentation_items_.erase(owned);
        auto& transferred = proxy_presentation_items_[actor.GetNpc()];
        transferred.insert(items.begin(), items.end());
      }
    }
    actors_by_id_[actor.base_player().id()] = &actor;
    actors_by_npc_[actor.GetNpc()] = &actor;
  } else {
    ReleaseProxyPresentationItems(previous);
    UnregisterActor(actor);
  }
}

void NetGame::UnregisterActor(Gothic2APlayer& actor) {
  ReleaseProxyPresentationItems(actor.GetNpc());
  const auto id = actors_by_id_.find(actor.base_player().id());
  if (id != actors_by_id_.end() && id->second == &actor) actors_by_id_.erase(id);
  const auto npc = actors_by_npc_.find(actor.GetNpc());
  if (npc != actors_by_npc_.end() && npc->second == &actor) actors_by_npc_.erase(npc);
}

void NetGame::TrackProxyPresentationItem(oCNpc* npc, oCItem* item) {
  const auto mapped = actors_by_npc_.find(npc);
  if (!item || mapped == actors_by_npc_.end() || mapped->second->IsLocalPlayer()) return;
  if (proxy_presentation_items_[npc].insert(item).second) item->AddRef();
}

void NetGame::ReleaseProxyPresentationItems(oCNpc* npc) {
  const auto owned = proxy_presentation_items_.find(npc);
  if (owned == proxy_presentation_items_.end()) return;
  for (auto* item : owned->second) item->Release();
  proxy_presentation_items_.erase(owned);
}

void NetGame::PruneProxyPresentationItems(Gothic2APlayer& actor, const PlayerState& state) {
  auto* npc = actor.GetNpc();
  if (!npc || actor.IsLocalPlayer()) return;
  auto owned = proxy_presentation_items_.find(npc);
  if (owned == proxy_presentation_items_.end()) return;
  const std::unordered_set<int> desired{LocalItemIndex(state.left_hand_item_instance), LocalItemIndex(state.right_hand_item_instance),
      LocalItemIndex(state.equipped_armor_instance), LocalItemIndex(state.equipped_helmet_instance), LocalItemIndex(state.equipped_shield_instance),
      LocalItemIndex(state.equipped_amulet_instance), LocalItemIndex(state.equipped_belt_instance), LocalItemIndex(state.equipped_ring_left_instance),
      LocalItemIndex(state.equipped_ring_right_instance), LocalItemIndex(state.melee_weapon_instance), LocalItemIndex(state.ranged_weapon_instance), LocalItemIndex(state.active_spell_instance)};
  std::unordered_set<oCItem*> inventory;
  for (auto* entry = npc->inventory2.inventory.GetNextInList(); entry; entry = entry->GetNextInList()) {
    if (auto* item = entry->GetData()) inventory.insert(item);
  }
  for (auto it = owned->second.begin(); it != owned->second.end();) {
    auto* item = *it;
    const bool held = npc->GetLeftHand() == item || npc->GetRightHand() == item;
    if (!held && inventory.contains(item) && !desired.contains(item->GetInstance()) &&
        (item->mainflag & (ITM_CAT_RUNE | ITM_CAT_MAGIC)) != 0) {
      // Multiple registered runes can remain ACTIVE although selection moved.
      // Retire only the old presentation-owned entry, never the current spell.
      npc->UnequipItem(item);
    }
    if (held || (inventory.contains(item) && (desired.contains(item->GetInstance()) || item->HasFlag(ITM_FLAG_ACTIVE)))) {
      ++it;
      continue;
    }
    if ((item->mainflag & (ITM_CAT_RUNE | ITM_CAT_MAGIC)) != 0) {
      UnregisterMagicItem(npc, item);
    }
    if (inventory.contains(item)) npc->inventory2.Remove(item);
    item->Release();
    it = owned->second.erase(it);
  }
  if (owned->second.empty()) proxy_presentation_items_.erase(owned);
}
void NetGame::ApplyPlayerLifeState(std::uint64_t player_id, std::uint8_t life_state, std::optional<std::uint64_t> actor_id, bool trigger_event) {
  Gothic2APlayer* cplayer = GetPlayerById(player_id);
  const auto normalized_life_state = NormalizeLifeState(life_state, cplayer ? cplayer->base_player().health() : 0);
  if (!cplayer || !cplayer->GetNpc()) {
    if (trigger_event && normalized_life_state == PLAYER_LIFE_DEAD) {
      EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnPlayerDeathName, gmp::gothic::PlayerLifecycleEvent{player_id});
    }
    return;
  }

  oCNpc* npc = cplayer->GetNpc();
  if (cplayer->base_player().is_npc()) {
    trigger_event = false;
    if (normalized_life_state != PLAYER_LIFE_ALIVE) {
      StopNpcPersistentAnimation(player_id);
      InterruptNpcAction(player_id);
    }
  }
  oCNpc* actor = nullptr;
  if (actor_id.has_value()) {
    if (Gothic2APlayer* actor_player = GetPlayerById(actor_id.value())) {
      actor = actor_player->GetNpc();
    }
  }

  if (normalized_life_state == PLAYER_LIFE_DEAD) {
    const bool was_dead = npc->IsDead() && npc->GetBodyState() == BS_DEAD;
    cplayer->StopPositionInterpolation();
    cplayer->ClearHandledAnimation();
    cplayer->base_player().set_health(0);
    cplayer->base_player().set_update_health_packet_counter(0);
    CloseSpellBook(npc);

    {
      ScopedLocalLifecycleEventSuppression suppress_lifecycle;
      ClearNpcHandsForLifecycle(npc);
      npc->SetWeaponMode(NPC_WEAPON_NONE);
      if (!was_dead) {
        npc->DoDie(actor);
      }
      SetNpcHealth(npc, 0);
    }

    if (trigger_event) {
      EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnPlayerDeathName, gmp::gothic::PlayerLifecycleEvent{player_id});
    }
    return;
  }

  if (normalized_life_state == PLAYER_LIFE_UNCONSCIOUS) {
    cplayer->StopPositionInterpolation();
    cplayer->ClearHandledAnimation();
    cplayer->base_player().set_health(1);
    cplayer->base_player().set_update_health_packet_counter(0);
    CloseSpellBook(npc);

    {
      ScopedLocalLifecycleEventSuppression suppress_lifecycle;
      if (npc->IsDead() || npc->GetBodyState() == BS_DEAD) {
        if (cplayer->IsLocalPlayer()) {
          npc->RefreshNpc();
        }
        SetNpcHealth(npc, 1);
        auto pos = npc->GetPositionWorld();
        npc->ResetPos(pos);
      }

      SetNpcHealth(npc, 1);
      npc->SetWeaponMode(NPC_WEAPON_NONE);
      if (!npc->IsUnconscious()) {
        npc->DropUnconscious(0.0f, actor);
      }
      ClearNpcHandsForLifecycle(npc);
    }
    return;
  }

  const int target_health = ClampAliveHealth(npc, cplayer->base_player().health());
  cplayer->base_player().set_health(target_health);
  cplayer->base_player().set_update_health_packet_counter(0);

  if (npc->IsDead() || npc->GetBodyState() == BS_DEAD) {
    cplayer->RespawnPlayer();
    SetNpcHealth(npc, target_health);
    cplayer->base_player().set_health(target_health);
    if (cplayer->base_player().is_npc()) {
      SuppressServerNpcAI(npc);
    }
    return;
  }

  if (npc->IsUnconscious() || npc->GetBodyState() == BS_UNCONSCIOUS) {
    cplayer->StopPositionInterpolation();
    cplayer->ClearHandledAnimation();
    CloseSpellBook(npc);
    {
      ScopedLocalLifecycleEventSuppression suppress_lifecycle;
      npc->ClearEM();
      npc->state.ClearAIState();
      SetNpcHealth(npc, target_health);
      npc->SetMovLock(0);
      npc->SetWeaponMode(NPC_WEAPON_NONE);
      npc->StandUp(0, 1);
      SetNpcHealth(npc, target_health);
    }
    if (cplayer->base_player().is_npc()) {
      SuppressServerNpcAI(npc);
    }
    return;
  }

  if (npc->GetAttribute(NPC_ATR_HITPOINTS) != target_health) {
    SetNpcHealth(npc, target_health);
  }
}

void NetGame::JoinGame() {
  if (IsReadyToJoin && game_client && player) {
    HooksManager::GetInstance()->AddHook(HT_RENDER, (DWORD)InterfaceLoop);

    auto sanitized_name = SanitizePlayerName(Config::Instance().Nickname.ToChar());
    player->name[0] = sanitized_name.c_str();

    if (!game_client->player_manager().HasLocalPlayer()) return;
    auto& local = game_client->player_manager().GetLocalPlayer();
    const auto position = player->GetPositionWorld();
    const auto heading = player->GetAtVectorWorld();
    local.set_position({position[VX], position[VY], position[VZ]});
    local.set_rotation({heading[VX], heading[VY], heading[VZ]});
    local.set_body_model(player->body_visualName.ToChar());
    local.set_head_model(player->head_visualName.ToChar());
    local.set_body_texture(static_cast<std::int16_t>(player->body_TexVarNr));
    local.set_head_texture(static_cast<std::int16_t>(player->head_TexVarNr));
    local.set_teeth_texture(static_cast<std::int16_t>(player->teeth_TexVarNr));
    local.set_skin_color(static_cast<std::int16_t>(player->body_TexColorNr));
    if (auto* item = dynamic_cast<oCItem*>(player->GetLeftHand())) local.set_left_hand_item(NetworkItemName(item->GetInstance()));
    if (auto* item = dynamic_cast<oCItem*>(player->GetRightHand())) local.set_right_hand_item(NetworkItemName(item->GetInstance()));
    local.set_equipped_armor(NetworkItemName(player->GetEquippedArmor() ? player->GetEquippedArmor()->GetInstance() : 0));
    local.set_equipped_helmet(NetworkItemName(GetSlotItemInstance(player, NPC_NODE_HELMET)));
    local.set_equipped_shield(NetworkItemName(GetSlotItemInstance(player, NPC_NODE_SHIELD)));
    local.set_equipped_amulet(NetworkItemName(GetEquippedInventoryItemInstance(player, ITM_FLAG_AMULET)));
    local.set_equipped_belt(NetworkItemName(GetEquippedInventoryItemInstance(player, ITM_FLAG_BELT)));
    const auto rings = GetEquippedRingInstances(player, {0, 0});
    local.set_equipped_ring_left(NetworkItemName(rings[0]));
    local.set_equipped_ring_right(NetworkItemName(rings[1]));
    const auto walk = player->GetAnictrl() ? std::clamp(player->GetAnictrl()->walkmode, 0, 2) : 0;
    game_client->JoinGame(sanitized_name, sanitized_name, player->body_visualName.ToChar(), player->body_TexVarNr,
        player->head_visualName.ToChar(), player->head_TexVarNr, walk, player->teeth_TexVarNr, player->body_TexColorNr,
        gmp::gothic::GetCharacterWalkingOverlay(*player));
    IsReadyToJoin = false;

    // Deferred world loading may have replaced the session since menu exit.
    if (ogame)
      ogame->SetShowPlayerStatus(TRUE);
    // Set up the local player now that we have the player ID from the server.
    CIngame* g = new CIngame();
    // LocalPlayer->id = game_client->player_manager().GetLocalPlayer().id();
    // LocalPlayer->enable = TRUE;
    // LocalPlayer->SetNpc(player);
    // LocalPlayer->hp = static_cast<short>(LocalPlayer->GetHealth());
    // LocalPlayer->update_hp_packet = 0;
    // LocalPlayer->npc->SetMovLock(0);
    // this->players.push_back(LocalPlayer);
    // this->HeroLastHp = player->attribute[NPC_ATR_HITPOINTS];
  }
}

void NetGame::SendMessage(const char* msg) {
  if (!game_client) {
    return;
  }

  if (IsInGame && player != nullptr && game_client && game_client->player_manager().HasLocalPlayer()) {
    UpdatePlayerStats(GetCurrentAnimationId());
  }
  game_client->SendChatMessage(msg);
}

void NetGame::SendCastSpell(oCNpc* Target, short SpellId, std::int32_t item_instance, std::uint32_t caster_id) {
  if (!game_client || SpellId < 0 || SpellId >= 100) {
    return;
  }

  const auto target = GetPlayerIdByNpc(Target);
  const auto target_id = target ? static_cast<std::uint32_t>(*target) : 0;
  auto* actor = caster_id ? GetPlayerById(caster_id) : nullptr;
  auto* caster = actor ? actor->GetNpc() : player;
  auto* book = caster ? caster->GetSpellBook() : nullptr;
  auto* spell = caster ? GetSelectedMagicSpell(caster) : nullptr;
  auto* item = book && spell ? book->GetSpellItem(spell) : nullptr;
  if (!item_instance && item) item_instance = item->GetInstance();
  if (caster && item_instance > 0) item = caster->inventory2.IsIn(item_instance, 1);
  const bool scroll = item && item->HasFlag(ITM_FLAG_MULTI);
  game_client->SendCastSpell(target_id, static_cast<std::uint16_t>(SpellId), NetworkItemName(item_instance), caster_id, -1, 1, false, scroll);
}

std::uint32_t NetGame::SendDropItem(std::int32_t instance, std::int32_t amount, const glm::vec3& position,
                                  const glm::vec3& rotation, bool physics_enabled) {
  if (!game_client) return 0;
  const auto request = game_client->SendDropItem(NetworkItemName(instance), amount, position, rotation, physics_enabled);
  if (request) pending_inventory_mutations_[request] = {instance, -amount};
  else if (player) ReconcileInventoryAmount(player, instance, static_cast<int>(std::min<std::int64_t>(
      std::numeric_limits<std::int32_t>::max(), static_cast<std::int64_t>(GetInventoryAmount(player, instance)) + amount)));
  return request;
}

std::uint32_t NetGame::SendTakeItem(std::int32_t instance, std::int32_t amount, std::optional<std::uint32_t> item_ground_id) {
  if (!game_client) return 0;
  const auto request = game_client->SendTakeItem(NetworkItemName(instance), amount, item_ground_id);
  if (request) pending_inventory_mutations_[request] = {instance, amount};
  else if (player) ReconcileInventoryAmount(player, instance, std::max(0, GetInventoryAmount(player, instance) - amount));
  return request;
}

void NetGame::SendPlayerWorldEnter(const std::string& world_name) {
  if (!game_client || !IsConnected() || !player) return;
  if (game_client->player_manager().HasLocalPlayer()) {
    auto& cached = game_client->player_manager().GetLocalPlayer();
    if (pending_inventory_generation_ != cached.actor_generation()) {
      pending_inventory_mutations_.clear();
      pending_inventory_generation_ = cached.actor_generation();
    }
    if (auto* local = GetPlayerById(cached.id())) {
      if (local->GetNpc() != player) {
        local->SetNpc(player);
        ResetLocalActionObservation();
      }
      ApplyServerStats(*local);
      ApplyServerInventory(*local, true);
      auto restored = cached.state();
      const auto position = player->GetPositionWorld();
      const auto heading = player->GetAtVectorWorld();
      restored.position = glm::vec3(position[VX], position[VY], position[VZ]);
      restored.nrot = glm::vec3(heading[VX], heading[VY], heading[VZ]);
      ApplyActorState(*local, restored, true);
    }
  }
  std::unordered_map<std::int32_t, std::int64_t> counts;
  for (auto* entry = player->inventory2.inventory.GetNextInList(); entry; entry = entry->GetNextInList()) {
    if (auto* item = entry->GetData(); item && IsNetworkItemInstance(item->GetInstance()) && item->amount > 0) {
      counts[item->GetInstance()] += item->amount;
    }
  }
  std::vector<InventoryEntry> inventory;
  inventory.reserve(counts.size());
  for (const auto& [instance, amount] : counts) {
    inventory.push_back({NetworkItemName(instance), static_cast<std::int32_t>(std::min<std::int64_t>(amount, std::numeric_limits<std::int32_t>::max()))});
  }
  std::size_t name_bytes = 0;
  for (const auto& entry : inventory) name_bytes += entry.item_instance.size();
  if (inventory.size() > kMaxInventoryEntries || name_bytes > kMaxInventoryNameBytes) {
    SPDLOG_ERROR("Native inventory exceeds protocol bootstrap limits");
    return;
  }
  game_client->SendPlayerWorldEnter(world_name, inventory);
}

void NetGame::SendAnimationAction(const std::string& name, bool started, std::uint64_t source_time_ms) {
  if (!game_client || !IsInGame || name.empty()) return;
  ActorActionPacket action;
  action.kind = started ? ActorActionKind::AnimationStart : ActorActionKind::AnimationStop;
  action.name = name;
  action.source_time_ms = source_time_ms;
  game_client->SendActorAction(std::move(action));
}

void NetGame::SendShotPresentation(std::uint8_t mode, const glm::vec3& origin, const glm::vec3& heading,
                                   std::optional<std::uint32_t> target, std::uint32_t actor_id) {
  if (!game_client || !IsInGame) return;
  ActorActionPacket action;
  action.kind = ActorActionKind::Shot;
  action.actor_id = actor_id;
  action.source_time_ms = ActorClockMilliseconds();
  action.position = origin;
  action.direction = heading;
  action.weapon_mode = mode;
  auto* actor = actor_id ? GetPlayerById(actor_id) : nullptr;
  if (auto* weapon = GetCurrentRangedWeapon(actor ? actor->GetNpc() : player)) action.item_instance = NetworkItemName(weapon->munition);
  action.target_id = target;
  game_client->SendActorAction(std::move(action));
}

void NetGame::SendItemUsePresentation(std::int32_t instance, const std::string& scheme, std::int32_t quantity,
                                     std::int32_t health_delta, std::int32_t mana_delta, std::uint32_t actor_id) {
  if (!game_client || !IsInGame) return;
  ActorActionPacket action;
  action.kind = ActorActionKind::ItemUse;
  action.actor_id = actor_id;
  action.source_time_ms = ActorClockMilliseconds();
  action.item_instance = NetworkItemName(instance);
  action.scheme = scheme;
  action.quantity = quantity;
  action.health_delta = health_delta;
  action.mana_delta = mana_delta;
  game_client->SendActorAction(std::move(action));
}

void NetGame::SendMobState(const std::string& name, const glm::vec3& position, std::int32_t target_state) {
  if (!game_client || !IsInGame) return;
  ActorActionPacket action;
  action.kind = ActorActionKind::MobState;
  action.source_time_ms = ActorClockMilliseconds();
  action.name = name;
  action.position = position;
  action.target_state = target_state;
  game_client->SendActorAction(std::move(action));
}

bool NetGame::CanReportActorDamage(oCNpc* attacker) {
  if (!attacker || ShouldSuppressLocalEquipEvents() || !game_client || !game_client->IsConnected() || !game_client->player_manager().HasLocalPlayer()) return false;
  const auto& local = game_client->player_manager().GetLocalPlayer();
  if (!local.has_spawned()) return false;
  if (attacker == player) return true;
  const auto id = GetPlayerIdByNpc(attacker);
  const auto* actor = id ? game_client->player_manager().GetPlayer(*id) : nullptr;
  return actor && actor->is_npc() && actor->has_spawned() && actor->npc_host_player_id() == local.id();
}

void NetGame::OnNativeTransformation(oCNpc* previous, oCNpc* current, std::uint16_t spell, bool entering) {
  const auto id = GetPlayerIdByNpc(previous);
  auto* actor = id ? GetPlayerById(*id) : nullptr;
  if (!actor || !actor->IsLocalPlayer() || !current || current != player)
    return;
  ResetWorldRenderState();
  if (entering) {
    actor->RememberTransformationInstance(previous);
    native_transformation_spell_ = spell;
    native_transformation_generation_ = actor->base_player().actor_generation();
    const auto position = previous->GetPositionWorld();
    native_transformation_origin_ = {position[VX], position[VY], position[VZ]};
  }
  actor->StopPositionInterpolation();
  actor->ClearHandledAnimation();
  actor->InvalidatePresentation(kSyncAll);
  actor->SetNpc(current);
  if (!actor->base_player().name().empty())
    current->name[0] = actor->base_player().name().c_str();
  if (entering) {
    // Finish the native spell/book call before selecting the creature's FIST
    // locomotion aliases. NONE selects human animations absent from this model.
    const bool movement_locked = previous->IsMovLock() != 0;
    const auto generation = actor->base_player().actor_generation();
    auto* world = current->GetHomeWorld();
    GMPCore::Instance().DeferToNextFrame([id = *id, generation, world, current, movement_locked, spell] {
      auto& game = NetGame::Instance();
      auto* actor = game.GetPlayerById(id);
      if (!actor || !actor->IsLocalPlayer() || actor->GetNpc() != current || current != player ||
          actor->base_player().actor_generation() != generation || game.native_transformation_spell_ != spell ||
          !ogame || ogame->GetGameWorld() != world || actor->base_player().life_state() != PLAYER_LIFE_ALIVE ||
          current->IsDead() || current->IsUnconscious())
        return;
      ScopedNetworkObservationSuppression suppress_observation;
      current->ClearEM();
      current->state.ClearAIState();
      current->SetSleeping(FALSE);
      current->SetMovLock(movement_locked);
      current->SetToFistMode();
      if (auto* controller = current->GetAnictrl()) {
        controller->SearchStandAni(TRUE);
      }
    });
  }
  ResetLocalActionObservation();
  if (!entering && game_client && native_transformation_generation_ == actor->base_player().actor_generation() && !ShouldSuppressLocalEquipEvents())
    game_client->SendTransformationReturn(native_transformation_cast_);
}

bool NetGame::ClearNativeTransformation() {
  if (!player)
    return true;
  oCSpell* active = nullptr;
  for (auto* node = player->activeSpells.GetNextInList(); node; node = node->GetNextInList()) {
    auto* spell = node->GetData();
    if (spell && gmp::GetTransformationForm(static_cast<std::uint16_t>(spell->GetSpellID())) && spell->saveNpc) {
      active = spell;
      break;
    }
  }
  if (!active)
    return true;
  auto* form = player;
  auto* human = active->saveNpc;
  form->AddRef();
  active->AddRef();
  {
    ScopedNetworkObservationSuppression suppress_observation;
    if (form->isInMovementMode != zCVob::zVOB_MOVE_MODE_NOTINBLOCK)
      form->EndMovement(TRUE);
    active->EndTimedEffect();
    if (player != human) {
      // The original body occupied this location before the native swap.
      // Retry rollback there when the animal no longer fits a human body.
      const auto position = native_transformation_spell_
                                ? zVEC3(native_transformation_origin_.x, native_transformation_origin_.y, native_transformation_origin_.z)
                                : human->GetPositionWorld();
      form->SetPositionWorld(position);
      active->EndTimedEffect();
    }
  }
  const bool returned = player == human;
  if (returned)
    form->RemoveActiveSpell(active);
  active->Release();
  form->Release();
  if (!returned)
    SPDLOG_ERROR("Cannot restore native transformation: no room for the human body");
  return returned;
}

bool NetGame::IsActorInstancePending(Gothic2APlayer& actor) const {
  if (actor.base_player().instance_change_revision()) return true;
  if (!actor.npc || (actor.IsLocalPlayer() && !native_transformation_spell_))
    return false;
  const auto expected = FindParserIndex(actor.base_player().instance().c_str());
  // Remote instance replacement runs next frame. Its new stats and pose must
  // not be applied to the old model while that replacement is pending.
  return expected ? actor.npc->GetInstance() != *expected : actor.IsLocalPlayer();
}

void NetGame::SendNativeTeleport(oCNpc* npc, const std::string& destination) {
  if (!CanReportActorDamage(npc) || destination.empty() || destination.size() > 255)
    return;
  const auto id = GetPlayerIdByNpc(npc);
  if (!id)
    return;
  const auto position = npc->GetPositionWorld();
  const auto normal = npc->GetAtVectorWorld();
  PlayerTeleportPacket packet;
  packet.player_id = static_cast<std::uint32_t>(*id);
  packet.position = {position[VX], position[VY], position[VZ]};
  packet.normal = {normal[VX], normal[VY], normal[VZ]};
  packet.destination = destination;
  game_client->SendNativeTeleport(std::move(packet));
}

void NetGame::SendActorHit(oCNpc* attacker, std::uint32_t victim, std::int32_t damage, std::uint32_t type, bool dont_kill,
                           bool finishing_move) {
  if (!game_client || !attacker) return;
  const auto attacker_id = GetPlayerIdByNpc(attacker);
  if (!attacker_id) return;
  auto* actor = GetPlayerById(*attacker_id);
  if (!actor) return;
  if (attacker == player) {
    game_client->SendPlayerHit(victim, damage, type, dont_kill, {}, 0, finishing_move);
  } else if (actor->base_player().is_npc() && game_client->player_manager().HasLocalPlayer() &&
      actor->base_player().npc_host_player_id() == game_client->player_manager().GetLocalPlayer().id()) {
    game_client->SendPlayerHit(victim, damage, type, dont_kill,
        static_cast<std::uint32_t>(*attacker_id), actor->base_player().npc_control_epoch(), finishing_move);
  }
}
bool NetGame::ReportFinishingMove(oCNpc* attacker, oCNpc* victim, bool send_report) {
  if (!attacker || !victim || !game_client || !game_client->IsConnected() ||
      (attacker != player && !GetPlayerIdByNpc(attacker))) return false;
  const auto victim_id = GetPlayerIdByNpc(victim);
  auto* actor = victim_id ? GetPlayerById(*victim_id) : nullptr;
  if (!actor || actor->base_player().life_state() != PLAYER_LIFE_UNCONSCIOUS) return false;
  const auto mode = attacker->GetWeaponMode();
  if (send_report && mode >= NPC_WEAPON_DAG && mode <= NPC_WEAPON_2HS && CanReportActorDamage(attacker))
    SendActorHit(attacker, static_cast<std::uint32_t>(*victim_id), 1, oEDamageType_Edge, false, true);
  // EV_AttackFinish has already set native HP to zero before calling DoDie.
  // Keep the victim down until the server accepts the execution.
  SetNpcHealth(victim, std::max(1, actor->base_player().health()));
  if (auto* model = victim->GetModel()) {
    const bool back = model->IsAnimationActive("T_WOUNDEDB_2_DEADB") || model->IsAnimationActive("S_WOUNDEDB");
    zSTRING wounded(back ? "S_WOUNDEDB" : "S_WOUNDED");
    if (!model->IsAnimationActive(wounded) && model->GetAniIDFromAniName(wounded) >= 0) model->StartAnimation(wounded);
  }
  return true;
}
void NetGame::SendPlayerHit(std::uint32_t victim_id, std::int32_t damage, std::uint32_t damage_type, bool dont_kill) {
  if (!game_client) {
    return;
  }

  game_client->SendPlayerHit(victim_id, damage, damage_type, dont_kill);
}

void NetGame::SendPlayerUnconscious(std::optional<std::uint32_t> attacker_id) {
  if (!game_client) {
    return;
  }

  game_client->SendPlayerUnconscious(attacker_id);
}

void NetGame::SendPlayerStandUp() {
  if (!game_client || !game_client->player_manager().HasLocalPlayer() || !player ||
      player->IsDead() || player->IsUnconscious() || player->GetBodyState() == BS_UNCONSCIOUS) return;
  const auto& local = game_client->player_manager().GetLocalPlayer();
  if (!local.has_spawned() || local.life_state() != PLAYER_LIFE_UNCONSCIOUS) return;
  const auto now_ms = ActorClockMilliseconds();
  if (stand_up_report_generation_ == local.actor_generation() && now_ms < stand_up_report_after_ms_) return;
  stand_up_report_generation_ = local.actor_generation();
  stand_up_report_after_ms_ = now_ms + 250;
  game_client->SendPlayerStandUp();
}

void NetGame::SendPlayerDeath(std::optional<std::uint32_t> killer_id) {
  if (!game_client) {
    return;
  }

  game_client->SendPlayerDeath(killer_id);
}

glm::vec3 Vec3ToGlmVec3(const zVEC3& vec) {
  return glm::vec3(vec[VX], vec[VY], vec[VZ]);
}

std::uint8_t GetHeadDirectionByte(oCNpc* Hero) {
  if (!Hero || !Hero->GetAnictrl()) {
    return static_cast<uint8_t>(Gothic2APlayer::HEAD_NONE);
  }

  zVEC2 HeadVar = zVEC2(Hero->GetAnictrl()->lookTargetx, Hero->GetAnictrl()->lookTargety);
  if (HeadVar[VX] == 0)
    return static_cast<uint8_t>(Gothic2APlayer::HEAD_LEFT);
  else if (HeadVar[VX] == 1)
    return static_cast<uint8_t>(Gothic2APlayer::HEAD_RIGHT);
  else if (HeadVar[VY] == 0)
    return static_cast<uint8_t>(Gothic2APlayer::HEAD_UP);
  else if (HeadVar[VY] == 1)
    return static_cast<uint8_t>(Gothic2APlayer::HEAD_DOWN);

  return 0;
}

void NetGame::UpdatePlayerStats(int anim) {
  if (!game_client || !player) {
    return;
  }
  if (game_client->player_manager().HasLocalPlayer()) {
    if (auto* local = GetPlayerById(game_client->player_manager().GetLocalPlayer().id())) {
      if (IsActorInstancePending(*local)) return;
      ApplyServerInventory(*local);
      ApplyServerStats(*local);
    }
  }

  // Gather player state from the Gothic engine
  PlayerState state;
  state.source_time_ms = ActorClockMilliseconds();
  state.position = Vec3ToGlmVec3(player->GetPositionWorld());
  state.nrot = Vec3ToGlmVec3(player->GetAtVectorWorld());
  oCItem* left_hand_item = dynamic_cast<oCItem*>(player->GetLeftHand());
  oCItem* right_hand_item = dynamic_cast<oCItem*>(player->GetRightHand());
  state.left_hand_item_instance = NetworkItemName(left_hand_item ? left_hand_item->GetInstance() : 0);
  state.right_hand_item_instance = NetworkItemName(right_hand_item ? right_hand_item->GetInstance() : 0);
  state.equipped_armor_instance = NetworkItemName(player->GetEquippedArmor() ? player->GetEquippedArmor()->GetInstance() : 0);
  state.equipped_helmet_instance = NetworkItemName(GetSlotItemInstance(player, NPC_NODE_HELMET));
  state.equipped_shield_instance = NetworkItemName(GetSlotItemInstance(player, NPC_NODE_SHIELD));
  state.equipped_amulet_instance = NetworkItemName(GetEquippedInventoryItemInstance(player, ITM_FLAG_AMULET));
  state.equipped_belt_instance = NetworkItemName(GetEquippedInventoryItemInstance(player, ITM_FLAG_BELT));
  std::array<std::int32_t, 2> previous_rings{0, 0};
  if (game_client && game_client->player_manager().HasLocalPlayer()) {
    auto& local_player = game_client->player_manager().GetLocalPlayer();
    previous_rings[0] = LocalItemIndex(local_player.equipped_ring_left());
    previous_rings[1] = LocalItemIndex(local_player.equipped_ring_right());
  }
  const auto equipped_rings = GetEquippedRingInstances(player, previous_rings);
  state.equipped_ring_left_instance = NetworkItemName(equipped_rings[0]);
  state.equipped_ring_right_instance = NetworkItemName(equipped_rings[1]);
  state.animation_name = GetAnimationNameById(player, anim);
  if (!gmp::IsValidAnimationName(state.animation_name)) state.animation_name.clear();
  if (player->IsDead()) {
    state.life_state = PLAYER_LIFE_DEAD;
  } else if (player->IsUnconscious()) {
    state.life_state = PLAYER_LIFE_UNCONSCIOUS;
  } else {
    state.life_state = PLAYER_LIFE_ALIVE;
  }
  state.health_points = std::max(0, player->attribute[NPC_ATR_HITPOINTS]);
  state.mana_points = std::max(0, player->attribute[NPC_ATR_MANA]);
  state.body_state = static_cast<std::uint32_t>(player->GetBodyState() & BS_ONLY_STATE);
  if (state.life_state == PLAYER_LIFE_ALIVE && state.health_points > 0) SendPlayerStandUp();
  state.invulnerable = (player->variousFlags & NPC_FLAG_IMMORTAL) != 0;
  if (auto* controller = player->GetAnictrl()) {
    state.walk_mode = static_cast<std::uint8_t>(std::clamp(controller->walkmode, 0, 2));
    state.look_yaw = std::clamp(controller->lookTargetx, 0.0f, 1.0f);
    state.look_pitch = std::clamp(controller->lookTargety, 0.0f, 1.0f);
  }
  if (auto focus = GetPlayerIdByNpc(zDYNAMIC_CAST<oCNpc>(player->GetFocusVob()))) {
    state.focus_target_id = static_cast<std::uint32_t>(*focus);
  }
  state.weapon_mode = static_cast<uint8_t>(player->GetWeaponMode());
  state.active_spell_instance = NetworkItemName(GetActiveSpellItemInstance(player));
  if (!state.active_spell_instance.empty()) {
    auto* book = player->GetSpellBook();
    auto* spell = GetSelectedMagicSpell(player);
    // The wire field is the 1-based hotkey, never the spell ID (Light = 18).
    const int hotkey = book->GetKeyByItem(book->GetSpellItem(spell));
    state.active_spell_nr = hotkey >= 1 && hotkey <= 8 ? static_cast<std::uint8_t>(hotkey) : 0;
    state.spell_investing = spell->manaInvested > 0;
    state.active_spell_level =
        static_cast<std::uint8_t>(std::clamp(spell->effect ? spell->effect->GetLevel() : 0, 0, static_cast<int>(kMaxSpellInvestLevel)));
  }
  state.head_direction = GetHeadDirectionByte(player);
  oCItem* melee_weapon = GetCurrentMeleeWeapon(player);
  oCItem* ranged_weapon = GetCurrentRangedWeapon(player);
  state.melee_weapon_instance = NetworkItemName(melee_weapon ? melee_weapon->GetInstance() : 0);
  state.ranged_weapon_instance = NetworkItemName(ranged_weapon ? ranged_weapon->GetInstance() : 0);

  game_client->UpdatePlayerStats(state);
}

void NetGame::SyncGameTime() {
  if (game_client) {
    game_client->SyncGameTime();
  }
}

void NetGame::Disconnect() {
  ClearNativeTransformation();
  native_transformation_spell_ = 0;
  native_transformation_cast_ = 0;
  ClearNativeLightSpells(player);
  CancelAllNpcActions();
  CMainMenu::StopMenuScenes();
  ++content_activation_generation_;
  const bool voice_was_enabled = IsVoiceChatEnabled();
  StopVoiceChat();
  server_voice_enabled_ = false;
  server_voice_transmit_enabled_ = false;
  voice_range_ = 0.0f;
  if (voice_was_enabled) {
    EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnVoiceChatStateChangeName,
                                          gmp::gothic::OnVoiceChatStateEvent{false, 0.0f});
  }
  ResetVoiceChatScriptState();
  pending_local_spawn_position_.reset();
  pending_resource_payloads_.clear();
  IsInGame = false;
  IsReadyToJoin = false;
  if (global_ingame) {
    global_ingame->IgnoreFirstSync = true;
  }

  if (resource_runtime) {
    gmp::gothic::CleanupGothicViews();
    resource_runtime->UnloadResources();
  }

  if (game_client) {
    game_client->Disconnect();
  }

  Gothic2APlayer::DeleteAllPlayers();
  CChat::GetInstance()->ClearChat();

  if (content_transition_manager) {
    const bool had_addon_archives = content_transition_manager->HasActiveAddonArchives();
    if (content_transition_manager->HasServerGameSessionStarted()) {
      base_world_reload_required_ = true;
      SPDLOG_INFO("Full base game-session restoration scheduled for addon disconnect");
    } else {
      std::string content_error;
      if (!content_transition_manager->DeactivateServerContent(content_error)) {
        SPDLOG_ERROR("Failed to deactivate pending server content during disconnect: {}", content_error);
      } else if (had_addon_archives) {
        SPDLOG_INFO("Unmounted addon content before a server game session was started");
      }
    }
  }
}

bool NetGame::ConsumeBaseWorldReloadRequest() {
  const bool required = base_world_reload_required_;
  base_world_reload_required_ = false;
  return required;
}

bool NetGame::FinalizeDownloadedContent(std::string& error_message) {
  if (!game_client || !game_client->IsConnected() || !IsReadyToJoin) {
    error_message = "Connection ended before client resources could be initialized";
    return false;
  }
  if (!resource_runtime) {
    error_message = "Client resource runtime unavailable";
    return false;
  }

  SPDLOG_INFO("Loading {} resource payload(s) into runtime", pending_resource_payloads_.size());
  if (game_client && game_client->player_manager().HasLocalPlayer()) {
    resource_runtime->GetLuaState()["heroId"] = static_cast<int>(game_client->player_manager().GetLocalPlayer().id());
  } else {
    resource_runtime->GetLuaState()["heroId"] = sol::lua_nil;
  }
  if (!resource_runtime->LoadResources(std::move(pending_resource_payloads_), error_message)) {
    pending_resource_payloads_.clear();
    if (error_message.empty()) {
      error_message = "Failed to initialize client resources";
    }
    return false;
  }

  // The new runtime must observe the already-loaded world and current clock,
  // even when their values match those seen by the previous/menu session.
  last_world_name_.clear();
  last_game_time_.reset();
  EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnVoiceChatStateChangeName,
                                        gmp::gothic::OnVoiceChatStateEvent{IsVoiceChatEnabled(), GetVoiceChatRange()});
  SPDLOG_INFO("All required client resources loaded into the fresh game session");
  error_message.clear();
  return true;
}

NetGame::ConnectionProgressDisplay NetGame::GetConnectionProgressDisplay() const {
  return {connection_progress_visible_, connection_progress_failed_,
          connection_progress_failed_ && std::chrono::steady_clock::now() >= connection_progress_error_until_,
          connection_progress_percent_, connection_progress_message_, connection_progress_banner_};
}

void NetGame::ClearConnectionProgressDisplay() {
  connection_progress_visible_ = false;
  connection_progress_failed_ = false;
  connection_progress_percent_ = 0;
  connection_progress_message_.clear();
  connection_progress_banner_.clear();
}

void NetGame::ShowConnectionProgressError(const std::string& message) {
  connection_progress_visible_ = true;
  connection_progress_failed_ = true;
  connection_progress_percent_ = 0;
  connection_progress_message_ = message;
  connection_progress_error_until_ = std::chrono::steady_clock::now() + std::chrono::seconds(5);
}

// ============================================================================
// EventObserver Implementation
// ============================================================================

void NetGame::OnConnectionStarted() {
  SPDLOG_INFO("Connection attempt started...");
  connection_progress_visible_ = true;
  connection_progress_failed_ = false;
  connection_progress_percent_ = 0;
  connection_progress_message_ = "Connecting to server...";
  connection_progress_banner_ = BuildMultiplayerStatusMessage();
  if (content_transition_manager) {
    content_transition_manager->MarkDownloading();
  }
}

void NetGame::OnConnected() {
  SPDLOG_INFO("Successfully connected to server");
  connection_progress_message_ = "Preparing resource downloads...";
}

void NetGame::OnConnectionFailed(const std::string& error) {
  SPDLOG_ERROR("Connection failed: {}", error);
  IsReadyToJoin = false;
  if (content_transition_manager) {
    content_transition_manager->AbortConnection();
  }
  ShowConnectionProgressError("Connection failed: " + error);
}

void NetGame::OnDisconnected() {
  ClearNativeTransformation();
  native_transformation_spell_ = 0;
  native_transformation_cast_ = 0;
  // Actor metadata may already be gone, but the native hero survives in the
  // menu session. Its timed lights must not leak into the next connection.
  ClearNativeLightSpells(player);
  // GameClient has already removed its actor metadata by this callback.
  npc_actions_.clear();
  SPDLOG_INFO("Disconnected from server");
  npc_animations_.clear();
  const bool voice_was_enabled = IsVoiceChatEnabled();
  StopVoiceChat();
  server_voice_enabled_ = false;
  server_voice_transmit_enabled_ = false;
  voice_range_ = 0.0f;
  if (voice_was_enabled) {
    EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnVoiceChatStateChangeName,
                                          gmp::gothic::OnVoiceChatStateEvent{false, 0.0f});
  }
  ResetVoiceChatScriptState();
  IsReadyToJoin = false;
}

void NetGame::OnConnectionLost() {
  OnConnectionClosed("Connection to the server was lost");
}

void NetGame::OnConnectionClosed(const std::string& reason) {
  SPDLOG_WARN("Connection closed: {}", reason);
  if (connection_progress_visible_ && !IsInGame) {
    ShowConnectionProgressError(reason);
    Disconnect();
    return;
  }
  const bool return_to_menu = IsInGame;
  Disconnect();
  if (player) {
    auto pos = player->GetPositionWorld();
    player->ResetPos(pos);
  }
  IsInGame = false;
  IsReadyToJoin = false;
  CChat::GetInstance()->WriteMessage(NORMAL, false, zCOLOR(255, 0, 0, 255), "%s: %s",
                                    Language::Instance()[Language::DISCONNECTED].ToChar(), reason.c_str());
  if (return_to_menu) {
    GMPCore::Instance().DeferToNextFrame([]() { ReturnToBigMainMenuAfterDisconnect(); });
  }
}

void NetGame::OnAdminAuthChanged(bool authenticated) {
  debug::DevTools::Instance().SetNoclipAllowed(authenticated);
}

void NetGame::OnResourceDownloadPhase(const std::string& phase) {
  connection_progress_visible_ = true;
  connection_progress_failed_ = false;
  connection_progress_message_ = phase;
}

void NetGame::OnResourceDownloadProgress(const std::string& /*resource_name*/, std::uint64_t downloaded_bytes, std::uint64_t total_bytes) {
  if (total_bytes == 0) {
    return;
  }
  const double percent = (static_cast<double>(downloaded_bytes) / static_cast<double>(total_bytes)) * 100.0;
  connection_progress_visible_ = true;
  connection_progress_failed_ = false;
  connection_progress_percent_ = std::clamp(static_cast<int>(percent), 0, 100);
  connection_progress_message_ = "Downloading resource packs...";
}

void NetGame::OnResourceDownloadFailed(const std::string& reason) {
  SPDLOG_ERROR("Resource download failed: {}", reason);
  IsReadyToJoin = false;
  ShowConnectionProgressError("Download failed: " + reason);
  Disconnect();
}

void NetGame::OnResourcesReady() {
  const auto generation = ++content_activation_generation_;
  connection_progress_percent_ = 100;
  connection_progress_message_ = "Preparing game content...";
  SPDLOG_INFO("Client signaled that resources are ready; deferring content activation to the next frame boundary");
  GMPCore::Instance().DeferToNextFrame([generation]() { NetGame::Instance().ActivateDownloadedContent(generation); });
}

void NetGame::ActivateDownloadedContent(std::uint64_t generation) {
  if (generation != content_activation_generation_) {
    SPDLOG_DEBUG("Ignoring stale deferred content activation");
    return;
  }
  if (!game_client || !game_client->IsConnected()) {
    SPDLOG_DEBUG("Ignoring deferred content activation after disconnect");
    return;
  }

  SPDLOG_INFO("Activating downloaded content at the frame boundary");
  connection_progress_message_ = "Mounting addons...";
  auto payloads = game_client->ConsumeDownloadedResources();
  auto addon_payloads = game_client->ConsumeDownloadedAddons();

  if (!resource_runtime) {
    SPDLOG_ERROR("Client resource runtime is unavailable; disconnecting");
    OnResourceDownloadFailed("Client resource runtime unavailable");
    return;
  }

  if (!content_transition_manager) {
    OnResourceDownloadFailed("Addon content transition manager unavailable");
    return;
  }

  std::vector<std::filesystem::path> addon_paths;
  addon_paths.reserve(addon_payloads.size());
  std::size_t gothic_dat_count = 0;
  for (const auto& addon : addon_payloads) {
    addon_paths.push_back(addon.cached_path);
    gothic_dat_count += addon.descriptor.contains_gothic_dat ? 1U : 0U;
  }
  if (gothic_dat_count > 1) {
    OnResourceDownloadFailed("Server announced more than one addon GOTHIC.DAT");
    return;
  }

  if (!addon_paths.empty()) {
    connection_progress_message_ = "Mounting addon VDFs...";
  }
  std::string error_message;
  if (gothic_dat_count == 1) {
    connection_progress_message_ = "Loading Gothic.dat...";
  }
  if (!content_transition_manager->ActivateServerContent(addon_paths, gothic_dat_count == 1, error_message)) {
    OnResourceDownloadFailed("Failed to activate server addon content: " + error_message);
    return;
  }

  if (!addon_paths.empty() && !server_requested_map_.empty()) {
    map = server_requested_map_.c_str();
    SPDLOG_INFO("Addon content active; server world '{}' will be reloaded even if it is already open", server_requested_map_);
  }

  pending_resource_payloads_ = std::move(payloads);
  SPDLOG_INFO("Downloaded content mounted; game-session transition may begin");
  IsReadyToJoin = true;
  ClearConnectionProgressDisplay();
}

void NetGame::OnMapChange(const std::string& map_name) {
  server_requested_map_ = map_name;
  if (!ogame || !ogame->GetGameWorld()) {
    return;
  }

  std::string currentMap = ogame->GetGameWorld()->GetWorldFilename().ToChar();

  if (map_name != currentMap) {
    this->map = map_name.c_str();
  } else if (!this->map.IsEmpty()) {
    this->map.Clear();
  }
}

void NetGame::OnGameInfoReceived(std::uint32_t raw_game_time, float day_length_ms, std::uint8_t flags) {
  STime t;
  t.time = raw_game_time;

  if (day_length_ms > 0.0f) {
    SetDayLengthMs(day_length_ms);
  }

  ApplyGameTimeToEngine(t);

  oCGame::s_bUsePotionKeys = flags & 0x01;
  this->ForceHideMap = flags & 0x04;
}

void NetGame::OnSkySettingsReceived(std::uint8_t flags, std::int32_t weather_type, std::int16_t rain_start_hour, std::int16_t rain_start_min,
                                    std::int16_t rain_stop_hour, std::int16_t rain_stop_min, float wind_scale, bool dont_rain, float rain_weight,
                                    bool render_lightning) {
  if (flags & SKY_SETTING_WEATHER) {
    gmp::gothic::ApplyWeatherType(weather_type);
  }
  if (flags & SKY_SETTING_RAIN_START) {
    gmp::gothic::SetRainStartTime(rain_start_hour, rain_start_min);
  }
  if (flags & SKY_SETTING_RAIN_STOP) {
    gmp::gothic::SetRainStopTime(rain_stop_hour, rain_stop_min);
  }
  if (flags & SKY_SETTING_WIND_SCALE) {
    gmp::gothic::SetWindScale(wind_scale);
  }
  if (flags & SKY_SETTING_RAIN_WEIGHT) {
    gmp::gothic::SetRainWeight(rain_weight, weather_type, render_lightning);
  }
  if (flags & SKY_SETTING_DONT_RAIN) {
    gmp::gothic::SetDontRain(dont_rain);
  }
}

void NetGame::OnVoiceConfiguration(bool enabled, bool transmit_enabled, std::uint32_t range) {
  const bool was_enabled = IsVoiceChatEnabled();
  const float previous_range = GetVoiceChatRange();
  server_voice_enabled_ = enabled;
  server_voice_transmit_enabled_ = enabled && transmit_enabled;
  voice_range_ = enabled ? static_cast<float>(range) : 0.0f;
  if (!server_voice_enabled_) {
    StopVoiceChat();
  } else if (IsInGame) {
    StartVoiceChat();
  }

  const bool is_enabled = IsVoiceChatEnabled();
  if (was_enabled != is_enabled || previous_range != GetVoiceChatRange()) {
    EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnVoiceChatStateChangeName,
                                          gmp::gothic::OnVoiceChatStateEvent{is_enabled, GetVoiceChatRange()});
  }
}

void NetGame::OnVoiceFrame(std::uint32_t player_id, std::uint32_t talkspurt_id, std::uint32_t sequence,
                           bool spatial, std::uint32_t range, const std::vector<std::uint8_t>& encoded_data) {
  if (!IsVoiceChatEnabled() || !::player) {
    return;
  }

  float spatial_gain = 1.0f;
  if (spatial) {
    const float local_range = GetVoiceChatRange();
    if (range == 0 || local_range <= 0.0f) {
      return;
    }
    const float effective_range = std::min(static_cast<float>(range), local_range);
    Gothic2APlayer* remote_player = GetPlayerById(player_id);
    if (!remote_player || !remote_player->GetNpc()) {
      return;
    }
    const zVEC3 delta = remote_player->GetNpc()->GetPositionWorld() - ::player->GetPositionWorld();
    const float distance = std::sqrt(delta[VX] * delta[VX] + delta[VY] * delta[VY] + delta[VZ] * delta[VZ]);
    spatial_gain = std::clamp(1.0f - distance / effective_range, 0.0f, 1.0f);
    if (spatial_gain <= 0.0f) {
      return;
    }
  }

  MarkPlayerVoiceActive(player_id);
  const float player_volume = GetPlayerVoiceVolume(player_id);
  if (IsPlayerVoiceMuted(player_id)) {
    return;
  }

  const float gain = spatial_gain * player_volume;
  voice_playback_.PlayVoice(player_id, talkspurt_id, sequence, encoded_data, gain);
}

void NetGame::OnLocalPlayerJoined(gmp::client::Player& player) {
  SPDLOG_INFO("Local player registered with id {}", player.id());
}

void NetGame::OnLocalPlayerSpawned(gmp::client::Player& player) {
  if (!::player) {
    SPDLOG_ERROR("Local player spawned by server before Gothic hero exists");
    return;
  }

  this->IsInGame = true;
  game_client->SendVoiceChannel(voice_channel_);
  StartVoiceChat();

  Gothic2APlayer* local_player = GetPlayerById(player.id());
  if (local_player == nullptr) {
    local_player = new Gothic2APlayer(player, true);
    players.insert(players.begin(), local_player);
    EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnPlayerCreateName, gmp::gothic::PlayerLifecycleEvent{player.id()});
  }

  local_player->SetNpc(::player);
  zVEC3 pos(player.position().x, player.position().y, player.position().z);

  SPDLOG_INFO("Local player spawned at position ({}, {}, {})", player.position().x, player.position().y, player.position().z);
  local_player->StopPositionInterpolation();
  ApplyAuthoritativeNpcPosition(local_player->GetNpc(), pos);
  // A completed spawn snapshot establishes one transform. Repeated per-frame
  // writes would undo script/server teleports and depend on the render rate.
  pending_local_spawn_position_.reset();
  local_player->base_player().set_enabled(true);

#ifndef NDEBUG
  // Spawn Quarhodron NPC near the player
  auto npcIndex = FindParserIndex("NONE_ADDON_111_QUARHODRON");
  if (npcIndex.has_value() && zfactory && ogame && ogame->GetGameWorld()) {
    oCNpc* quarhodron = zfactory->CreateNpc(*npcIndex);
    if (quarhodron) {
      zVEC3 npcPos(pos[VX] + 200.0f, pos[VY], pos[VZ] + 200.0f);
      quarhodron->SetPositionWorld(npcPos);
      ogame->GetGameWorld()->AddVob(quarhodron);
      SPDLOG_INFO("Spawned Quarhodron NPC near local player at ({}, {}, {})", npcPos[VX], npcPos[VY], npcPos[VZ]);
    }
  } else {
    SPDLOG_WARN("Could not find NPC instance NONE_ADDON_111_QUARHODRON");
  }
#endif
}

void NetGame::OnPlayerJoined(gmp::client::Player& new_player) {
  // Remote player metadata registered; actual spawn handled in OnPlayerSpawned.
  (void)new_player;
}

void NetGame::OnPlayerSpawned(gmp::client::Player& new_player) {
  SpawnRemotePlayer(new_player);
}

void NetGame::OnPlayersClearing() {
  pending_inventory_mutations_.clear();
  pending_inventory_generation_ = 0;
  stand_up_report_generation_ = stand_up_report_after_ms_ = 0;
  ResetLocalActionObservation();
  CancelAllNpcActions();
  Gothic2APlayer::DeleteAllPlayers();
}

void NetGame::OnPlayerSpawnSnapshotApplied(gmp::client::Player& player, bool new_spawn) {
  auto* actor = GetPlayerById(player.id());
  if (!actor || !actor->GetNpc()) {
    return;
  }
  if (actor->IsLocalPlayer() && pending_inventory_generation_ != player.actor_generation()) {
    pending_inventory_mutations_.clear();
    pending_inventory_generation_ = player.actor_generation();
  }
  ApplyActorState(*actor, player.state(), true);
  ApplyServerStats(*actor);
  auto* npc = actor->GetNpc();
  const auto& rotation = player.rotation();
  const zVEC3 normal(rotation.x, 0.0f, rotation.z);
  if (normal.Length_Sqr() > 0.000001f) {
    npc->SetHeadingYWorld(npc->GetPositionWorld() + normal);
  }
  if (player.is_npc()) {
    if (player.life_state() == PLAYER_LIFE_ALIVE) {
      SuppressServerNpcAI(npc);
    }
  } else if (new_spawn) {
    EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnPlayerSpawnName, gmp::gothic::PlayerLifecycleEvent{player.id()});
  }
  if (actor->IsLocalPlayer() && ogame && ogame->GetGameWorld()) {
    SendPlayerWorldEnter(ogame->GetGameWorld()->GetWorldFilename().ToChar());
  }
}

void NetGame::StopNpcPersistentAnimation(std::uint64_t npc_id) {
  const auto it = npc_animations_.find(static_cast<std::uint32_t>(npc_id));
  if (it == npc_animations_.end()) {
    return;
  }
  auto& animation = it->second;
  auto* actor = GetPlayerById(npc_id);
  if (actor && actor->GetNpc() && actor->GetNpc() == animation.npc) {
    auto* model = actor->GetNpc()->GetModel();
    if (model && model == animation.model && animation.animation &&
        model->GetAniFromAniID(animation.animation_id) == animation.animation) {
      model->StopAni(animation.animation);
    }
  }
  animation.npc = nullptr;
  animation.model = nullptr;
  animation.animation = nullptr;
  animation.applied = false;
  animation.looping = false;
}

void NetGame::OnNpcAnimation(const NpcAnimationPacket& state) {
  StopNpcPersistentAnimation(state.npc_id);
  npc_animations_[state.npc_id].state = state;
  if (auto* actor = GetPlayerById(state.npc_id)) {
    ProcessNpcPersistentAnimation(*actor);
  }
}

void NetGame::ProcessNpcPersistentAnimation(Gothic2APlayer& actor) {
  const auto id = static_cast<std::uint32_t>(actor.base_player().id());
  const auto it = npc_animations_.find(id);
  if (it == npc_animations_.end()) {
    return;
  }
  auto& animation = it->second;
  auto* npc = actor.GetNpc();
  const auto action = npc_actions_.find(id);
  if (!npc || actor.base_player().life_state() != PLAYER_LIFE_ALIVE || npc->IsDead() || npc->IsUnconscious() ||
      (action != npc_actions_.end() && !action->second.finished)) {
    if (animation.applied) {
      StopNpcPersistentAnimation(id);
    }
    return;
  }
  auto* model = npc->GetModel();
  if (!model) {
    return;
  }
  if (animation.applied && animation.npc == npc && animation.model == model &&
      (!animation.animation || model->GetAniFromAniID(animation.animation_id) == animation.animation)) {
    // Finite selections play once. A persistent loop must survive an engine
    // stand/pose correction that unexpectedly stops its active animation.
    if (!animation.looping || !animation.animation || model->IsAniActive(animation.animation)) {
      return;
    }
  }
  StopNpcPersistentAnimation(id);
  animation.npc = npc;
  animation.model = model;
  animation.applied = true;
  if (animation.state.animation.empty()) {
    return;
  }
  zSTRING name(animation.state.animation.c_str());
  animation.animation_id = model->GetAniIDFromAniName(name);
  animation.animation = model->GetAniFromAniID(animation.animation_id);
  if (!animation.animation ||
      (animation.animation->GetAniType() != zMDL_ANI_TYPE_NORMAL && animation.animation->GetAniType() != zMDL_ANI_TYPE_ALIAS)) {
    animation.animation = nullptr;
    SPDLOG_WARN("Cannot play persistent animation '{}' for NPC {}", animation.state.animation, id);
    return;
  }
  model->StartAni(animation.animation, zCModel::zMDL_STARTANI_FORCE);
  auto* active = model->GetActiveAni(animation.animation);
  animation.looping = animation.animation->nextAni == animation.animation || (active && active->nextAni == active->protoAni);
  if (active && animation.state.source_time_ms > 0) {
    const double elapsed_ms = actor.PresentationTime(ActorClockMilliseconds()) - animation.state.source_time_ms;
    const float frames = static_cast<float>(std::max(0.0, elapsed_ms) * animation.animation->fpsRate / 1000.0);
    const float span = static_cast<float>(std::max(1, static_cast<int>(animation.animation->numFrames) - 1));
    if (animation.looping) active->SetActFrame(std::fmod(frames, span));
    else if (frames < span) active->SetActFrame(frames);
    else model->StopAni(animation.animation);
  }
}

void NetGame::StopNpcActionAnimation(NpcActionRuntime& action) {
  auto* actor = GetPlayerById(action.control.npc_id);
  if (actor && actor->GetNpc() && actor->GetNpc() == action.npc) {
    auto* model = actor->GetNpc()->GetModel();
    // Overlay or instance replacement can retire the old prototype even when
    // the wrapper survived. Never pass an obsolete prototype to the engine.
    if (model && model == action.model && action.animation &&
        model->GetAniFromAniID(action.animation_id) == action.animation) {
      model->StopAni(action.animation);
    }
  }
  action.animation = nullptr;
  action.model = nullptr;
  action.npc = nullptr;
}

void NetGame::FinishNpcAction(NpcActionRuntime& action, bool success) {
  if (action.finished) {
    return;
  }
  action.finished = true;
  StopNpcActionAnimation(action);
  if (game_client && action.control.action_id != 0) {
    game_client->SendNpcActionResult(action.control.npc_id, action.control.control_epoch, action.control.action_id, success);
  }
}

void NetGame::CancelNpcAction(std::uint64_t npc_id) {
  const auto it = npc_actions_.find(static_cast<std::uint32_t>(npc_id));
  if (it != npc_actions_.end()) {
    StopNpcActionAnimation(it->second);
    npc_actions_.erase(it);
  }
}

void NetGame::CancelAllNpcActions() {
  for (auto& [id, action] : npc_actions_) {
    StopNpcActionAnimation(action);
  }
  npc_actions_.clear();
  for (const auto& [id, animation] : npc_animations_) {
    StopNpcPersistentAnimation(id);
  }
  npc_animations_.clear();
}

void NetGame::InterruptNpcAction(std::uint64_t npc_id) {
  const auto it = npc_actions_.find(static_cast<std::uint32_t>(npc_id));
  if (it != npc_actions_.end()) {
    FinishNpcAction(it->second, false);
  }
}

void NetGame::OnNpcControl(const NpcControlPacket& control) {
  // A host handover/queue change suspends presentation, without losing the
  // selected persistent state. Movement packets never enter this path.
  if (control.action_id != 0) {
    StopNpcPersistentAnimation(control.npc_id);
  }
  CancelNpcAction(control.npc_id);
  auto& action = npc_actions_[control.npc_id];
  action.control = control;
  action.deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(control.timeout_ms);
  action.finished = control.action_id == 0 || control.host_player_id == 0;

  auto* actor = GetPlayerById(control.npc_id);
  if (!actor || !actor->GetNpc()) {
    FinishNpcAction(action, false);
    return;
  }
  if (actor->base_player().instance_change_revision()) {
    ChangePlayerInstance(control.npc_id, actor->base_player().instance(), true);
    return;
  }
  actor->ClearHandledAnimation();
  auto* npc = actor->GetNpc();
  if (actor->base_player().life_state() == PLAYER_LIFE_ALIVE) {
    SuppressServerNpcAI(npc);
  }
  const zVEC3 position(control.position.x, control.position.y, control.position.z);
  const zVEC3 normal(control.normal.x, 0.0f, control.normal.z);
  actor->AnalyzePose(position, normal, control.source_time_ms);
  if (action.finished) {
    return;
  }

  action.npc = npc;
  action.model = npc->GetModel();
  if (!action.model || npc->IsDead() || npc->IsUnconscious()) {
    FinishNpcAction(action, false);
    return;
  }
  zSTRING animation_name(control.animation.c_str());
  action.animation_id = action.model->GetAniIDFromAniName(animation_name);
  action.animation = action.model->GetAniFromAniID(action.animation_id);
  if (!action.animation ||
      (action.animation->GetAniType() != zMDL_ANI_TYPE_NORMAL && action.animation->GetAniType() != zMDL_ANI_TYPE_ALIAS) ||
      action.animation->nextAni == action.animation) {
    FinishNpcAction(action, false);
    return;
  }
  action.model->StartAni(action.animation, zCModel::zMDL_STARTANI_FORCE);
  auto* active = action.model->GetActiveAni(action.animation);
  if (!active || active->nextAni == active->protoAni) {
    // A looping state has no completion boundary and cannot head a finite queue.
    FinishNpcAction(action, false);
    return;
  }
  const float fps = action.animation->fpsRate;
  const float frames = static_cast<float>(std::max(1, static_cast<int>(action.animation->numFrames) - 1));
  if (!std::isfinite(fps) || fps <= 0.0f) { FinishNpcAction(action, false); return; }
  const float elapsed_frames = static_cast<float>(control.elapsed_ms) * fps / 1000.0f;
  if (elapsed_frames >= frames) { FinishNpcAction(action, true); return; }
  active->SetActFrame(active->advanceDir >= 0 ? elapsed_frames : frames - elapsed_frames);
  action.last_progress = elapsed_frames / frames;
  action.expected_finish = std::chrono::steady_clock::now() +
      std::chrono::milliseconds(static_cast<std::int64_t>((frames - elapsed_frames) * 1000.0f / fps));
}

void NetGame::ProcessNpcActions() {
  const auto now = std::chrono::steady_clock::now();
  // One pass over actors avoids a linear GetPlayerById search for every NPC
  // on every frame. Stream-out removes the corresponding action separately.
  for (auto* actor : players) {
    if (!actor || !actor->base_player().is_npc()) {
      continue;
    }
    const auto it = npc_actions_.find(static_cast<std::uint32_t>(actor->base_player().id()));
    if (it == npc_actions_.end()) {
      continue;
    }
    auto& action = it->second;
    if (!actor->GetNpc()) {
      FinishNpcAction(action, false);
      continue;
    }
    auto* npc = actor->GetNpc();
    // Lua's logical transform and the delayed presentation pose are distinct.
    // The interpolation pass owns presentation; actions never pin its baseline.
    if (action.finished) {
      ProcessNpcPersistentAnimation(*actor);
      continue;
    }
    auto* model = npc->GetModel();
    if (npc != action.npc || !model || model != action.model ||
        model->GetAniFromAniID(action.animation_id) != action.animation || npc->IsDead() || npc->IsUnconscious()) {
      FinishNpcAction(action, false);
    } else if (now >= action.deadline) {
      FinishNpcAction(action, false);
    } else if (!model->IsAniActive(action.animation)) {
      FinishNpcAction(action, action.last_progress >= 0.8f &&
          now + std::chrono::milliseconds(50) >= action.expected_finish);
    } else if (auto* active = model->GetActiveAni(action.animation)) {
      const float frames = static_cast<float>(std::max(1, static_cast<int>(action.animation->numFrames) - 1));
      action.last_progress = std::clamp(active->advanceDir >= 0 ? active->actFrame / frames :
          (frames - active->actFrame) / frames, 0.0f, 1.0f);
    }
    ProcessNpcPersistentAnimation(*actor);
  }
}

void NetGame::SpawnRemotePlayer(gmp::client::Player& new_player) {
  if (Gothic2APlayer* existing_player = GetPlayerById(new_player.id())) {
    if (existing_player->GetNpc()) {
      zVEC3 pos(new_player.position().x, new_player.position().y, new_player.position().z);
      existing_player->StopPositionInterpolation();
      if (!existing_player->GetNpc()->GetHomeWorld()) {
        existing_player->GetNpc()->Enable(pos);
      }
      existing_player->SetPosition(pos);
      existing_player->base_player().set_enabled(true);
      existing_player->base_player().set_update_health_packet_counter(0);
    }
    return;
  }

  Gothic2APlayer* newhero = new Gothic2APlayer(new_player);
  zVEC3 pos(new_player.position().x, new_player.position().y, new_player.position().z);
  // An unspecified player instance means the default human, including while
  // the viewing client's own hero is transformed into a creature.
  int instance_id = FindParserIndex("PC_HERO").value_or(-1);
  if (!new_player.instance().empty()) {
    if (auto parsed_instance_id = FindParserIndex(new_player.instance().c_str()); parsed_instance_id.has_value() && *parsed_instance_id > 0) {
      instance_id = *parsed_instance_id;
    } else {
      if (new_player.is_npc()) {
        SPDLOG_WARN("Cannot spawn server NPC {}: missing instance '{}'", new_player.id(), new_player.instance());
        delete newhero;
        return;
      }
      SPDLOG_WARN("Remote player '{}' has unknown instance '{}'; using default player instance.", new_player.name(), new_player.instance());
    }
  }
  if (!zfactory || instance_id < 0) {
    SPDLOG_ERROR("Failed to resolve remote NPC instance for player '{}'.", new_player.name());
    delete newhero;
    return;
  }
  auto* parser = Gothic_II_Addon::parser;
  std::optional<ScriptInstanceScope> npc_instance_scope;
  if (parser) {
    if (!IsNpcInstance(parser, instance_id)) {
      SPDLOG_WARN("Cannot spawn server NPC {}: instance is not C_NPC", new_player.id());
      delete newhero;
      return;
    }
    npc_instance_scope.emplace(*parser, instance_id);
  }
  oCNpc* npc = zfactory->CreateNpc(instance_id);
  if (!npc) {
    SPDLOG_ERROR("Failed to create remote NPC for player '{}' with instance id {}.", new_player.name(), instance_id);
    delete newhero;
    return;
  }
  newhero->SetNpc(npc);
  // Enable chooses the initial stand/dead pose from native HP. Supply the
  // server values before that decision rather than the script constructor's.
  if (!new_player.instance_change_revision()) {
    npc->SetAttribute(NPC_ATR_HITPOINTSMAX, new_player.max_health());
    SetNpcHealth(npc, new_player.health());
    npc->SetAttribute(NPC_ATR_MANAMAX, new_player.max_mana());
    npc->SetAttribute(NPC_ATR_MANA, new_player.mana());
  }
  newhero->npc->startAIState = 0;
  if (new_player.is_npc()) {
    SuppressServerNpcAI(npc);
  }
  newhero->npc->Enable(pos);
  npc->Release();  // Enable/world owns the factory-created remote actor.
  if (new_player.is_npc()) {
    SuppressServerNpcAI(npc);
  }
  newhero->SetPosition(pos);
  newhero->SetName(new_player.name().c_str());
  (void)new_player;

  SPDLOG_DEBUG("{} '{}' (id {}) streamed in", new_player.is_npc() ? "NPC" : "Player", new_player.name(), new_player.id());
  newhero->base_player().set_enabled(true);
  newhero->base_player().set_update_health_packet_counter(0);
  this->players.push_back(newhero);
}

void NetGame::OnPlayerLeft(std::uint64_t player_id, const std::string& player_name) {
  StopNpcPersistentAnimation(player_id);
  npc_animations_.erase(static_cast<std::uint32_t>(player_id));
  CancelNpcAction(player_id);
  if (player_id <= std::numeric_limits<std::uint32_t>::max()) {
    const auto voice_player_id = static_cast<std::uint32_t>(player_id);
    EndPlayerVoiceActivity(voice_player_id);
    voice_playback_.RemovePlayer(voice_player_id);
  }
  for (size_t i = 0; i < this->players.size(); i++) {
    if (!this->players[i]) {
      continue;
    }

    if (this->players[i]->base_player().id() == player_id) {
      if (this->players[i]->IsLocalPlayer()) {
        SPDLOG_INFO("Local player '{}' (id {}) was unspawned", this->players[i]->GetName(), player_id);
        StopVoiceChat();
        pending_local_spawn_position_.reset();
        this->IsInGame = false;
        this->players[i]->base_player().set_has_spawned(false);
        this->players[i]->base_player().set_enabled(false);
        EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnPlayerDestroyName, gmp::gothic::PlayerLifecycleEvent{player_id});
        break;
      }

      SPDLOG_DEBUG("{} '{}' (id {}) streamed out", this->players[i]->base_player().is_npc() ? "NPC" : "Player",
                   this->players[i]->GetName(), player_id);
      this->players[i]->LeaveGame();
      if (!this->players[i]->base_player().is_npc()) {
        EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnPlayerDestroyName, gmp::gothic::PlayerLifecycleEvent{player_id});
      }
      delete this->players[i];
      this->players.erase(this->players.begin() + i);
      break;
    }
  }
}

void NetGame::OnPlayerNameUpdate(std::uint64_t player_id, const std::string& name) {
  Gothic2APlayer* cplayer = GetPlayerById(player_id);
  if (!cplayer) {
    return;
  }

  cplayer->base_player().set_name(name);
  if (!cplayer->GetNpc()) {
    return;
  }

  zSTRING new_name(name.c_str());
  cplayer->SetName(new_name);
}

PlayerSpawnPacket NetGame::CaptureInstanceDefaults(Gothic2APlayer& actor) {
  auto* npc = actor.GetNpc();
  const auto& base = actor.base_player();
  PlayerSpawnPacket packet;
  packet.packet_type = PT_PLAYER_SPAWN;
  packet.player_id = static_cast<std::uint32_t>(base.id());
  packet.actor_generation = base.actor_generation();
  packet.command_revision = base.command_revision();
  packet.state_sequence = base.state_sequence();
  packet.instance_change_revision = base.instance_change_revision();
  packet.source_time_ms = ActorClockMilliseconds();
  packet.instance = base.instance();
  packet.player_name = base.name();
  packet.name_color_r = base.name_color_r();
  packet.name_color_g = base.name_color_g();
  packet.name_color_b = base.name_color_b();
  packet.position = Vec3ToGlmVec3(npc->GetPositionWorld());
  packet.normal = Vec3ToGlmVec3(npc->GetAtVectorWorld());
  packet.body_model = npc->body_visualName.ToChar();
  packet.head_model = npc->head_visualName.ToChar();
  packet.body_texture = std::max(0, npc->body_TexVarNr);
  packet.head_texture = std::max(0, npc->head_TexVarNr);
  packet.teeth_texture = std::max(0, npc->teeth_TexVarNr);
  packet.skin_color = std::max(0, npc->body_TexColorNr);
  packet.fatness = npc->model_fatness;
  packet.scale = Vec3ToGlmVec3(npc->model_scale);
  packet.health = std::max(0, npc->GetAttribute(NPC_ATR_HITPOINTS));
  packet.max_health = std::max(0, npc->GetAttribute(NPC_ATR_HITPOINTSMAX));
  packet.mana = std::max(0, npc->GetAttribute(NPC_ATR_MANA));
  packet.max_mana = std::max(0, npc->GetAttribute(NPC_ATR_MANAMAX));
  packet.strength = std::max(0, npc->GetAttribute(NPC_ATR_STRENGTH));
  packet.dexterity = std::max(0, npc->GetAttribute(NPC_ATR_DEXTERITY));
  packet.level = std::max(0, npc->level);
  packet.exp = static_cast<std::int32_t>(std::min<unsigned long>(npc->experience_points, INT_MAX));
  packet.next_level_exp = static_cast<std::int32_t>(std::min<unsigned long>(npc->experience_points_next_level, INT_MAX));
  packet.learn_points = static_cast<std::int32_t>(std::min<unsigned long>(npc->learn_points, INT_MAX));
  packet.life_state = packet.health ? PLAYER_LIFE_ALIVE : PLAYER_LIFE_DEAD;
  packet.invulnerable = (npc->variousFlags & NPC_FLAG_IMMORTAL) != 0;
  packet.weapon_mode = static_cast<std::uint8_t>(npc->GetWeaponMode());
  packet.body_state = static_cast<std::uint32_t>(npc->GetBodyState());
  if (auto* controller = npc->GetAnictrl())
    packet.walk_mode = static_cast<std::uint8_t>(std::clamp(controller->walkmode, 0, 2));
  packet.equipped_armor_instance = NetworkItemName(npc->GetEquippedArmor() ? npc->GetEquippedArmor()->GetInstance() : 0);
  packet.equipped_helmet_instance = NetworkItemName(GetSlotItemInstance(npc, NPC_NODE_HELMET));
  packet.equipped_shield_instance = NetworkItemName(GetSlotItemInstance(npc, NPC_NODE_SHIELD));
  packet.equipped_amulet_instance = NetworkItemName(GetEquippedInventoryItemInstance(npc, ITM_FLAG_AMULET));
  packet.equipped_belt_instance = NetworkItemName(GetEquippedInventoryItemInstance(npc, ITM_FLAG_BELT));
  const auto rings = GetEquippedRingInstances(npc, {0, 0});
  packet.equipped_ring_left_instance = NetworkItemName(rings[0]);
  packet.equipped_ring_right_instance = NetworkItemName(rings[1]);
  packet.melee_weapon_instance = NetworkItemName(GetCurrentMeleeWeapon(npc) ? GetCurrentMeleeWeapon(npc)->GetInstance() : 0);
  packet.ranged_weapon_instance = NetworkItemName(GetCurrentRangedWeapon(npc) ? GetCurrentRangedWeapon(npc)->GetInstance() : 0);
  std::map<int, std::int64_t> inventory;
  for (auto* entry = npc->inventory2.inventory.GetNextInList(); entry; entry = entry->GetNextInList()) {
    if (auto* item = entry->GetData())
      inventory[item->GetInstance()] += std::max(0, item->amount);
  }
  for (const auto& [instance, count] : inventory)
    if (count > 0)
      packet.inventory.push_back({NetworkItemName(instance), static_cast<int>(std::min<std::int64_t>(count, INT_MAX))});
  for (int skill = 1; skill < NPC_HITCHANCE_MAX; ++skill) packet.weapon_skills.push_back({skill, npc->GetHitChance(skill)});
  for (int talent = 1; talent < oCNpcTalent::NPC_TAL_MAX; ++talent) packet.talents.push_back({talent, npc->GetTalentSkill(talent)});
  for (int i = 0; i < npc->activeOverlays.GetNumInList(); ++i) packet.overlays.emplace_back(npc->activeOverlays[i].ToChar());
  return packet;
}

void NetGame::OnPlayerInstanceUpdate(std::uint64_t player_id, const std::string& instance) {
  auto* actor = GetPlayerById(player_id);
  ChangePlayerInstance(player_id, instance, actor && actor->base_player().instance_change_revision() != 0);
}

void NetGame::ChangePlayerInstance(std::uint64_t player_id, const std::string& instance, bool reset_defaults) {
  if (instance.empty()) {
    return;
  }

  Gothic2APlayer* cplayer = GetPlayerById(player_id);
  if (!cplayer || !cplayer->GetNpc()) {
    return;
  }
  if (cplayer->IsLocalPlayer() && native_transformation_spell_ && !cplayer->base_player().transformation_spell_id()) {
    if (!ClearNativeTransformation())
      return;
    native_transformation_spell_ = 0;
    native_transformation_cast_ = 0;
    cplayer = GetPlayerById(player_id);
    if (!cplayer || !cplayer->GetNpc())
      return;
  }

  const auto instance_id = FindParserIndex(instance.c_str());
  if (!instance_id.has_value() || *instance_id <= 0) {
    SPDLOG_WARN("Unknown NPC instance '{}' for player {}", instance, player_id);
    if (cplayer->base_player().instance_change_revision())
      game_client->SendInstanceReady(CaptureInstanceDefaults(*cplayer), false);
    return;
  }

  if (!IsNpcInstance(Gothic_II_Addon::parser, *instance_id)) {
    SPDLOG_WARN("Cannot replace server NPC {}: instance is not C_NPC", player_id);
    if (cplayer->base_player().instance_change_revision())
      game_client->SendInstanceReady(CaptureInstanceDefaults(*cplayer), false);
    return;
  }
  cplayer->base_player().set_instance(instance);
  const auto request = cplayer->base_player().instance_change_revision();
  if (reset_defaults && request && cplayer->initialized_instance_revision == request) {
    game_client->SendInstanceReady(CaptureInstanceDefaults(*cplayer));
    return;
  }
  if (!reset_defaults && cplayer->GetNpc()->GetInstance() == *instance_id)
    return;
  const auto generation = cplayer->base_player().actor_generation();
  auto* world = ogame ? ogame->GetGameWorld() : nullptr;
  GMPCore::Instance().DeferToNextFrame(
      [player_id, generation, world, instance, reset_defaults, request, expected_npc = cplayer->GetNpc(), index = *instance_id] {
        auto& game = NetGame::Instance();
        auto* actor = game.GetPlayerById(player_id);
        if (!game.IsConnected() || !ogame || ogame->GetGameWorld() != world || !actor || actor->GetNpc() != expected_npc ||
            actor->base_player().actor_generation() != generation || actor->base_player().instance() != instance ||
            (request && actor->base_player().instance_change_revision() != request))
          return;
        if (!reset_defaults && actor->GetNpc()->GetInstance() == index)
          return;
        game.InterruptNpcAction(player_id);
        game.StopNpcPersistentAnimation(player_id);
        // ReplaceNpcInstance scopes construction only. A scope spanning old-NPC
        // destruction would restore SELF/the instance symbol to the retired actor.
        if (!actor->ReplaceNpcInstance(index)) {
          SPDLOG_WARN("Failed to replace NPC instance '{}' for player {}", instance, player_id);
          if (request)
            game.game_client->SendInstanceReady(game.CaptureInstanceDefaults(*actor), false);
          return;
        }
        if (reset_defaults) {
          auto defaults = game.CaptureInstanceDefaults(*actor);
          game.game_client->ApplyNativeInstanceDefaults(defaults);
          actor->initialized_instance_revision = request;
          if (request)
            game.game_client->SendInstanceReady(defaults);
        } else {
          const auto& base = actor->base_player();
          if (!base.transformation_spell_id() && actor->GetNpc()->IsHuman()) {
            // Transformation returns restore the server's human appearance;
            // constructor defaults belong only to an explicit instance change.
            game.OnPlayerVisualUpdate(player_id, base.body_model(), base.body_texture(), base.head_model(), base.head_texture(), base.teeth_texture(),
                                      base.skin_color());
            game.OnPlayerFatnessUpdate(player_id, base.fatness());
            game.OnPlayerScaleUpdate(player_id, base.scale());
            for (const auto& overlay : base.overlays()) game.OnPlayerOverlayUpdate(player_id, overlay, true);
          }
          game.ApplyServerStats(*actor);
          game.ApplyServerInventory(*actor, true);
          game.ApplyActorState(*actor, actor->base_player().state(), true);
        }
        if (actor->base_player().is_npc())
          SuppressServerNpcAI(actor->GetNpc());
      });
}

void NetGame::OnPlayerColorUpdate(std::uint64_t player_id, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
  Gothic2APlayer* cplayer = GetPlayerById(player_id);
  if (!cplayer || !cplayer->GetNpc()) {
    return;
  }

  cplayer->SetNameColor(zCOLOR(r, g, b, 255));
  cplayer->base_player().set_name_color(r, g, b);
}

void NetGame::OnPlayerSkillWeaponUpdate(std::uint64_t player_id, std::int32_t skill_id, std::int32_t percentage) {
  Gothic2APlayer* cplayer = GetPlayerById(player_id);
  if (!cplayer || !cplayer->GetNpc()) {
    return;
  }
  if (!gmp::IsWeaponSkill(skill_id)) return;
  cplayer->base_player().set_weapon_skill(skill_id, std::clamp(percentage, 0, 100));
  cplayer->base_player().set_talent(skill_id, gmp::WeaponSkillTier(percentage));
  if (cplayer->base_player().transformation_spell_id() || IsActorInstancePending(*cplayer)) return;

  ApplyNpcWeaponSkill(cplayer->GetNpc(), skill_id, percentage);
}

void NetGame::OnPlayerTalentUpdate(std::uint64_t player_id, std::int32_t talent_id, std::int32_t talent_value) {
  Gothic2APlayer* cplayer = GetPlayerById(player_id);
  if (!cplayer || !cplayer->GetNpc()) {
    return;
  }
  cplayer->base_player().set_talent(talent_id, talent_value);
  if (cplayer->base_player().transformation_spell_id() || IsActorInstancePending(*cplayer)) return;

  ApplyNpcTalent(cplayer->GetNpc(), talent_id, talent_value);
}

void NetGame::OnPlayerVisualUpdate(std::uint64_t player_id, const std::string& body_model, std::int16_t body_texture, const std::string& head_model,
                                   std::int16_t head_texture, std::int16_t teeth_texture, std::int16_t skin_color) {
  Gothic2APlayer* cplayer = GetPlayerById(player_id);
  if (!cplayer || !cplayer->GetNpc()) {
    return;
  }
  if (!cplayer->GetNpc()->IsHuman()) return;

  InterruptNpcAction(player_id);
  StopNpcPersistentAnimation(player_id);
  zSTRING body(body_model.c_str());
  zSTRING head(head_model.c_str());
  cplayer->GetNpc()->SetAdditionalVisuals(body, body_texture, skin_color, head, head_texture, teeth_texture, -1);
  cplayer->base_player().set_body_model(body_model);
  cplayer->base_player().set_body_texture(body_texture);
  cplayer->base_player().set_head_model(head_model);
  cplayer->base_player().set_head_texture(head_texture);
  cplayer->base_player().set_teeth_texture(teeth_texture);
  cplayer->base_player().set_skin_color(skin_color);
}

void NetGame::OnPlayerFatnessUpdate(std::uint64_t player_id, float fatness) {
  Gothic2APlayer* cplayer = GetPlayerById(player_id);
  if (!cplayer || !cplayer->GetNpc()) {
    return;
  }

  cplayer->GetNpc()->SetFatness(fatness);
  cplayer->base_player().set_fatness(fatness);
}

void NetGame::OnPlayerScaleUpdate(std::uint64_t player_id, const glm::vec3& scale) {
  Gothic2APlayer* cplayer = GetPlayerById(player_id);
  if (!cplayer || !cplayer->GetNpc()) {
    return;
  }

  cplayer->GetNpc()->SetModelScale(zVEC3{scale.x, scale.y, scale.z});
  cplayer->base_player().set_scale(scale);
}

void NetGame::OnPlayerOverlayUpdate(std::uint64_t player_id, const std::string& overlay, bool apply) {
  Gothic2APlayer* cplayer = GetPlayerById(player_id);
  if (!cplayer || !cplayer->GetNpc()) {
    return;
  }
  if (apply)
    cplayer->base_player().add_overlay(overlay);
  else
    cplayer->base_player().remove_overlay(overlay);
  if (cplayer->base_player().transformation_spell_id() || IsActorInstancePending(*cplayer)) return;

  InterruptNpcAction(player_id);
  StopNpcPersistentAnimation(player_id);
  zSTRING overlay_name(overlay.c_str());
  if (apply) {
    cplayer->GetNpc()->ApplyOverlay(overlay_name);
    return;
  }

  cplayer->GetNpc()->RemoveOverlay(overlay_name);
}

void NetGame::OnPlayerAnimationPlay(std::uint64_t player_id, const std::string& animation) {
  auto* actor = GetPlayerById(player_id);
  auto* model = actor && actor->GetNpc() ? actor->GetNpc()->GetModel() : nullptr;
  if (!model || !gmp::IsValidAnimationName(animation)) return;
  zSTRING name(animation.c_str());
  const int id = model->GetAniIDFromAniName(name);
  if (id >= 0 && model->GetAniFromAniID(id)) {
    actor->InvalidatePresentation(kSyncPose);
    model->StartAni(id, 0);
  }
}

void NetGame::OnPlayerAnimationStop(std::uint64_t player_id, const std::string& animation) {
  auto* actor = GetPlayerById(player_id);
  auto* model = actor && actor->GetNpc() ? actor->GetNpc()->GetModel() : nullptr;
  if (!model) return;
  actor->InvalidatePresentation(kSyncPose);
  if (animation.empty()) {
    for (int channel = model->numActiveAnis - 1; channel >= 0; --channel) {
      if (auto* active = model->GetActiveAni(channel)) model->StopAni(active);
    }
  } else if (gmp::IsValidScriptName(animation)) {
    zSTRING name(animation.c_str());
    const int id = model->GetAniIDFromAniName(name);
    if (id >= 0) model->StopAni(id);
  }
}

void NetGame::OnPlayerFaceAnimationPlay(std::uint64_t player_id, const std::string& animation) {
  if (animation.empty()) {
    return;
  }

  Gothic2APlayer* cplayer = GetPlayerById(player_id);
  if (!cplayer || !cplayer->GetNpc()) {
    return;
  }

  cplayer->GetNpc()->StartFaceAni(zSTRING(animation.c_str()), 1.0f, 1.0f);
}

void NetGame::OnPlayerFaceAnimationStop(std::uint64_t player_id, const std::string& animation) {
  Gothic2APlayer* cplayer = GetPlayerById(player_id);
  if (!cplayer || !cplayer->GetNpc()) {
    return;
  }

  cplayer->GetNpc()->StopFaceAni(zSTRING(animation.c_str()));
}

void NetGame::OnPlayerGesticulation(std::uint64_t player_id) {
  Gothic2APlayer* cplayer = GetPlayerById(player_id);
  if (!cplayer || !cplayer->GetNpc()) {
    return;
  }

  oCNpc* npc = cplayer->GetNpc();
  if (!npc->IsDead() && !npc->IsUnconscious()) {
    npc->StartDialogAni();
  }
}

void NetGame::OnPlayerAttributeUpdate(std::uint64_t player_id, PlayerAttributeId attribute_id, std::int32_t value) {
  Gothic2APlayer* cplayer = GetPlayerById(player_id);
  if (!cplayer || !cplayer->GetNpc()) {
    return;
  }
  if (IsActorInstancePending(*cplayer)) return;

  oCNpc* npc = cplayer->GetNpc();
  const bool is_local = cplayer->IsLocalPlayer();
  const int clamped_value = std::max(0, static_cast<int>(value));

  switch (attribute_id) {
    case ATTR_STRENGTH:
      npc->SetAttribute(NPC_ATR_STRENGTH, clamped_value);
      cplayer->base_player().set_strength(clamped_value);
      break;
    case ATTR_DEXTERITY:
      npc->SetAttribute(NPC_ATR_DEXTERITY, clamped_value);
      cplayer->base_player().set_dexterity(clamped_value);
      break;
    case ATTR_LEVEL:
      npc->level = clamped_value;
      cplayer->base_player().set_level(clamped_value);
      break;
    case ATTR_EXP:
      npc->experience_points = clamped_value;
      cplayer->base_player().set_exp(clamped_value);
      if (is_local) {
        if (auto* status_menu = GetStatusMenu()) {
          status_menu->SetExperience(npc->experience_points, 0, npc->experience_points_next_level);
        }
      }
      break;
    case ATTR_NEXT_LEVEL_EXP:
      npc->experience_points_next_level = clamped_value;
      cplayer->base_player().set_next_level_exp(clamped_value);
      if (is_local) {
        if (auto* status_menu = GetStatusMenu()) {
          status_menu->SetExperience(npc->experience_points, 0, npc->experience_points_next_level);
        }
      }
      break;
    case ATTR_LEARN_POINTS:
      npc->learn_points = static_cast<unsigned long>(clamped_value);
      cplayer->base_player().set_learn_points(clamped_value);
      if (is_local) {
        if (auto* status_menu = GetStatusMenu()) {
          status_menu->SetLearnPoints(npc->learn_points);
        }
      }
      break;
    case ATTR_MAX_HEALTH: {
      npc->SetAttribute(NPC_ATR_HITPOINTSMAX, clamped_value);
      cplayer->base_player().set_max_health(clamped_value);
      int current_health = npc->GetAttribute(NPC_ATR_HITPOINTS);
      if (current_health > clamped_value) {
        cplayer->base_player().set_health(clamped_value);
        cplayer->base_player().set_update_health_packet_counter(0);
        if (clamped_value <= 0) {
          ApplyPlayerLifeState(player_id, PLAYER_LIFE_DEAD, std::nullopt, false);
        } else {
          SetNpcHealth(npc, clamped_value);
        }
      }
      break;
    }
    case ATTR_HEALTH: {
      int max_health = npc->GetAttribute(NPC_ATR_HITPOINTSMAX);
      int clamped_health = std::min(clamped_value, std::max(0, max_health));
      cplayer->base_player().set_health(clamped_health);
      cplayer->base_player().set_update_health_packet_counter(0);
      if (clamped_health <= 0) {
        ApplyPlayerLifeState(player_id, PLAYER_LIFE_DEAD, std::nullopt, false);
        break;
      }

      const auto life_state = cplayer->base_player().life_state();
      if (life_state == PLAYER_LIFE_DEAD) {
        break;
      }
      if (life_state == PLAYER_LIFE_UNCONSCIOUS && clamped_health <= 1) {
        SetNpcHealth(npc, 1);
        break;
      }
      if (npc->IsDead() || npc->IsUnconscious()) {
        ApplyPlayerLifeState(player_id, PLAYER_LIFE_ALIVE, std::nullopt, false);
        break;
      }

      SetNpcHealth(npc, clamped_health);
      break;
    }
    case ATTR_MAX_MANA: {
      npc->SetAttribute(NPC_ATR_MANAMAX, clamped_value);
      cplayer->base_player().set_max_mana(clamped_value);
      int current_mana = npc->GetAttribute(NPC_ATR_MANA);
      if (current_mana > clamped_value) {
        npc->SetAttribute(NPC_ATR_MANA, clamped_value);
        cplayer->base_player().set_mana(clamped_value);
      }
      break;
    }
    case ATTR_MANA: {
      int max_mana = npc->GetAttribute(NPC_ATR_MANAMAX);
      int clamped_mana = std::min(clamped_value, max_mana);
      npc->SetAttribute(NPC_ATR_MANA, clamped_mana);
      cplayer->base_player().set_mana(clamped_mana);
      break;
    }
    default:
      break;
  }
}

void NetGame::OnPlayerWorldUpdate(std::uint64_t player_id, const std::string& world_name, const std::string& start_point) {
  Gothic2APlayer* cplayer = GetPlayerById(player_id);
  if (!cplayer || !cplayer->IsLocalPlayer() || !ogame) {
    return;
  }
  pending_inventory_mutations_.clear();
  pending_inventory_generation_ = cplayer->base_player().actor_generation();
  ResetLocalActionObservation();

  EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnWorldChangeName, gmp::gothic::OnWorldChangeEvent{world_name, start_point});

  GMPCore::Instance().DeferToNextFrame([player_id, world_name, start_point]() {
    NetGame& net_game = NetGame::Instance();
    auto* client = net_game.game_client.get();
    if (!client || !client->IsConnected() || !client->player_manager().HasLocalPlayer() ||
        client->player_manager().GetLocalPlayer().id() != player_id || !ogame) {
      return;
    }

    zSTRING z_world(world_name.c_str());
    zSTRING z_start_point(start_point.c_str());
    PrepareWorldChange();
    Patch::ChangeLevelEnabled(true);
    ogame->ChangeLevel(z_world, z_start_point);
    ResetWorldRenderState();
    Patch::ChangeLevelEnabled(false);
    // ChangeLevel is synchronous here. Rebind the loaded hero before deferred
    // corrections, item commands and new-world sampling touch engine state.
    net_game.SendPlayerWorldEnter(world_name);
    net_game.last_world_name_ = world_name;
  });
}

void NetGame::ApplyServerInventory(Gothic2APlayer& actor, bool restore_missing) {
  if (actor.base_player().instance_change_revision()) return;
  // The native spell retains the human's real inventory in saveNpc. Copying it
  // into the animal would duplicate it and run equipment scripts twice.
  if (actor.IsLocalPlayer() && native_transformation_spell_ && actor.GetNpc() == player &&
      (actor.base_player().transformation_spell_id() || IsActorInstancePending(actor))) return;
  auto* npc = actor.GetNpc();
  if (!npc || (!actor.IsLocalPlayer() && !actor.base_player().is_npc())) return;
  std::unordered_map<std::int32_t, std::int64_t> native_counts;
  for (auto* entry = npc->inventory2.inventory.GetNextInList(); entry; entry = entry->GetNextInList()) {
    if (auto* item = entry->GetData()) native_counts[item->GetInstance()] += std::max(0, item->amount);
  }
  if (auto* parser = Gothic_II_Addon::parser) {
    for (const auto* name : {"ITLSTORCH", "ITLSTORCHBURNING"}) {
      const int instance = parser->GetIndex(name);
      if (GetCarriedTorchAmount(npc, instance) > 0) native_counts.try_emplace(instance, 0);
    }
  }
  std::unordered_map<int, std::int32_t> allowed;
  for (const auto& [name, count] : actor.base_player().inventory()) {
    if (const auto index = LocalItemIndex(name); index > 0) allowed[index] = count;
  }
  if (actor.IsLocalPlayer()) {
    for (const auto& [_, pending] : pending_inventory_mutations_) {
      // A requested pickup is predicted until the server replies. Native use
      // and drop deficits are restored only by the corresponding correction.
      if (pending.delta > 0) {
        auto& amount = allowed[pending.instance];
        amount = static_cast<std::int32_t>(std::min<std::int64_t>(std::numeric_limits<std::int32_t>::max(),
            static_cast<std::int64_t>(amount) + pending.delta));
      }
    }
  }
  if (restore_missing) {
    for (const auto& [instance, _] : allowed) native_counts.try_emplace(instance, 0);
  }
  ScopedNetworkObservationSuppression suppress_observation;
  for (const auto& [instance, count] : native_counts) {
    const auto found = allowed.find(instance);
    const auto desired = found == allowed.end() ? 0 : found->second;
    const auto owned_count = count + GetCarriedTorchAmount(npc, instance);
    if (owned_count > desired || (restore_missing && owned_count < desired)) ReconcileInventoryAmount(npc, instance, desired);
  }
}

void NetGame::ApplyServerStats(Gothic2APlayer& actor) {
  if (IsActorInstancePending(actor)) return;
  auto* npc = actor.GetNpc();
  if (!npc) return;
  const auto& state = actor.base_player();
  const std::pair<int, int> attributes[] = {{NPC_ATR_HITPOINTSMAX, state.max_health()}, {NPC_ATR_MANAMAX, state.max_mana()},
      {NPC_ATR_STRENGTH, state.strength()}, {NPC_ATR_DEXTERITY, state.dexterity()}};
  for (const auto& [attribute, value] : attributes) {
    if (npc->GetAttribute(attribute) != value) npc->SetAttribute(attribute, value);
  }
  npc->level = state.level();
  npc->experience_points = state.exp();
  npc->experience_points_next_level = state.next_level_exp();
  npc->learn_points = state.learn_points();
  // Native transformation retains human talents on the saved human body.
  // SetTalentSkill can load human overlays (notably acrobatics); applying it
  // to the creature changes its animation controller and can stop movement.
  if (state.transformation_spell_id()) return;
  for (int skill = 0; skill < NPC_HITCHANCE_MAX; ++skill) {
    const auto found = state.weapon_skills().find(skill);
    const auto value = found == state.weapon_skills().end() ? 0 : std::clamp(found->second, 0, 100);
    npc->hitChance[skill] = value;
  }
  for (int talent = 1; talent < oCNpcTalent::NPC_TAL_MAX; ++talent) {
    const auto found = state.talents().find(talent);
    const auto value = found == state.talents().end() ? 0 : found->second;
    ApplyNpcTalent(npc, talent, value);
  }
}

void NetGame::ApplyActorState(Gothic2APlayer& actor, const PlayerState& state, bool snapshot, bool presentation_due, bool weapon_mode_command) {
  if (IsActorInstancePending(actor))
    return;
  auto* npc = actor.GetNpc();
  if (!npc) {
    return;
  }
  auto mask = snapshot ? kSyncAll : state.correction_mask;
  if (snapshot) actor.InvalidatePresentation(kSyncEquipment | kSyncCombat | kSyncPose);
  const auto id = actor.base_player().id();
  const bool local = actor.IsLocalPlayer();
  const bool server_npc = actor.base_player().is_npc();
  const bool terminal_update = ((mask & kSyncLife) != 0 && IsTerminalLifeState(state.life_state));
  if (!snapshot && !local && !presentation_due && !terminal_update) {
    const auto presentation_mask = mask & (kSyncEquipment | kSyncCombat | kSyncPose);
    if (presentation_mask) {
      PlayerState presentation = state;
      presentation.correction_mask = static_cast<std::uint16_t>(presentation_mask);
      const auto generation = actor.base_player().actor_generation();
      const auto corrections = actor.presentation_epochs();
      const auto command_revision = actor.base_player().command_revision();
      actor.QueuePresentation(state.source_time_ms, [this, id, generation, corrections, command_revision, presentation]() mutable {
        auto* current = GetPlayerById(id);
        if (current && current->GetNpc() && current->base_player().actor_generation() == generation &&
            current->base_player().accepts_life_action_revision(command_revision)) {
          presentation.correction_mask = current->presentation_epochs().CurrentComponents(presentation.correction_mask, corrections);
          if (!presentation.correction_mask) return;
          ApplyActorState(*current, presentation, false, true);
        }
      });
      mask &= ~presentation_mask;
    }
  }
  const bool previous_suppression = ShouldSuppressLocalEquipEvents();
  SetSuppressLocalEquipEvents(true);

  if ((mask & kSyncPosition) != 0) {
    zVEC3 position(state.position.x, state.position.y, state.position.z);
    if (snapshot || local || !actor.base_player().is_enabled() || !npc->GetHomeWorld()) {
      actor.StopPositionInterpolation();
      if (!npc->GetHomeWorld()) {
        npc->Enable(position);
      }
      actor.SetPosition(position);
      actor.base_player().set_enabled(true);
      if (!local) actor.AnalyzePose(position, zVEC3(state.nrot.x, 0.0f, state.nrot.z), state.source_time_ms, true);
    } else {
      actor.AnalyzePose(position, zVEC3(state.nrot.x, 0.0f, state.nrot.z), state.source_time_ms);
    }
    if (local) {
      pending_local_spawn_position_.reset();
    }
    if (server_npc) {
      const auto action = npc_actions_.find(static_cast<std::uint32_t>(id));
      if (action != npc_actions_.end()) {
        action->second.control.position = state.position;
        action->second.control.normal = state.nrot;
      }
    }
  }
  if ((mask & kSyncOrientation) != 0 && (snapshot || local || (mask & kSyncPosition) == 0)) {
    const zVEC3 normal(state.nrot.x, 0.0f, state.nrot.z);
    if (std::isfinite(normal[VX]) && std::isfinite(normal[VZ]) && normal.Length_Sqr() > 0.000001f) {
      npc->SetHeadingYWorld(npc->GetPositionWorld() + normal);
    }
  }

  // Reconcile owned inventory/back slots before selecting magic or draw mode.
  // A hand visual uses that same object; it is never an independent clone.
  if ((mask & kSyncEquipment) != 0) {
    SyncEquippedCombatItem(npc, LocalItemIndex(state.equipped_armor_instance), npc->GetEquippedArmor(), IsArmorItem);
    SyncEquippedSlotItem(npc, LocalItemIndex(state.equipped_helmet_instance), NPC_NODE_HELMET, IsHelmetItem);
    SyncEquippedSlotItem(npc, LocalItemIndex(state.equipped_shield_instance), NPC_NODE_SHIELD, IsShieldItem);
    SyncEquippedInventoryItem(npc, LocalItemIndex(state.equipped_amulet_instance), ITM_FLAG_AMULET);
    SyncEquippedInventoryItem(npc, LocalItemIndex(state.equipped_belt_instance), ITM_FLAG_BELT);
    SyncEquippedRingItems(npc, LocalItemIndex(state.equipped_ring_left_instance), LocalItemIndex(state.equipped_ring_right_instance));
    SyncEquippedCombatItem(npc, LocalItemIndex(state.ranged_weapon_instance), GetCurrentRangedWeapon(npc), IsRangedWeaponItem);
    SyncEquippedCombatItem(npc, LocalItemIndex(state.melee_weapon_instance), GetCurrentMeleeWeapon(npc), IsMeleeWeaponItem);
  }

  const auto life = (mask & kSyncLife) != 0 ? NormalizeLifeState(state.life_state, state.health_points)
                                         : actor.base_player().life_state();
  const bool terminal = IsTerminalLifeState(life);
  if ((mask & kSyncLife) != 0) {
    if (state.invulnerable) npc->variousFlags |= NPC_FLAG_IMMORTAL;
    else npc->variousFlags &= ~NPC_FLAG_IMMORTAL;
    ApplyPlayerLifeState(id, life, std::nullopt, false);
    if (terminal)
      actor.ClearRemoteLightEffect();
  }
  if ((mask & (kSyncCombat | kSyncEquipment)) != 0 && (snapshot || !local)) {
    if (terminal || LocalItemIndex(state.active_spell_instance) == 0 || state.weapon_mode != NPC_WEAPON_MAG) {
      CloseSpellBook(npc);
    } else if (IsNetworkItemInstance(LocalItemIndex(state.active_spell_instance))) {
      auto* spell_item = npc->inventory2.IsIn(LocalItemIndex(state.active_spell_instance), 1);
      if (!spell_item) {
        spell_item = CreateNetworkItem(LocalItemIndex(state.active_spell_instance));
        if (spell_item && (spell_item->mainflag & (ITM_CAT_RUNE | ITM_CAT_MAGIC)) == 0) {
          spell_item->Release();
          spell_item = nullptr;
        }
        spell_item = InsertOwnedItem(npc, spell_item);
      }
      if (spell_item && (spell_item->mainflag & (ITM_CAT_RUNE | ITM_CAT_MAGIC)) != 0) {
        npc->MakeSpellBook();
        auto* book = npc->GetSpellBook();
        if (book) {
          int index = -1;
          for (int i = 0; i < book->GetNoOfSpells(); ++i) {
            if (book->GetSpellItem(i) == spell_item) {
              index = i;
              break;
            }
          }
          if (index < 0) {
            // Equip toggles ACTIVE runes off. Registration is the source of
            // truth when a proxy's inventory was reconciled independently.
            spell_item->ClearFlag(ITM_FLAG_ACTIVE);
            npc->Equip(spell_item);
            for (int i = 0; i < book->GetNoOfSpells(); ++i) {
              if (book->GetSpellItem(i) == spell_item) {
                index = i;
                break;
              }
            }
          }
          if (index >= 0) {
            SelectMagicSpellForPresentation(npc, index);
            // Open(0) stops/recreates the hand effect even when already shown.
            // Reopening it for every pose drops PFX, audio and charge state.
            if (!book->show_handsymbol)
              book->Open(0);
            auto* spell = GetSelectedMagicSpell(npc);
            if (spell) {
              if (spell->effect && ((!state.spell_investing && spell->effect->fxState == zVFXSTATE_INVESTNEXT) ||
                                    state.active_spell_level < spell->effect->GetLevel()))
                book->StopSelectedSpell();
              spell = GetSelectedMagicSpell(npc);
            }
            if (spell) {
              auto* focus = state.focus_target_id ? GetPlayerById(*state.focus_target_id) : nullptr;
              spell->Setup(npc, focus ? focus->GetNpc() : nullptr, 0);
              if (spell->effect) {
                spell->effect->SetDamage(0.0f);
                spell->effect->SetDamageType(0);
                if (state.spell_investing && state.active_spell_level > spell->effect->GetLevel())
                  spell->effect->SetLevel(state.active_spell_level, 0);
              }
            }
          }
        }
      }
    }
  }

  if ((mask & kSyncCombat) != 0) {
    int mode = terminal ? NPC_WEAPON_NONE : std::clamp<int>(state.weapon_mode, NPC_WEAPON_NONE, NPC_WEAPON_MAG);
    if (!terminal && actor.base_player().transformation_spell_id())
      mode = NPC_WEAPON_FIST;
    if (npc->GetWeaponMode() != mode || weapon_mode_command) {
      SetNpcWeaponMode(npc, mode, weapon_mode_command);
    }
    if (mode == NPC_WEAPON_MAG && (snapshot || !local)) {
      if (auto* spell = GetSelectedMagicSpell(npc))
        ConfigureMagicAnimations(npc, spell->GetSpellID());
    }
  }
  if ((mask & kSyncEquipment) != 0 && (snapshot || !local)) {
    ScopedTorchUseSuppression suppress_torch_use;
    auto sync_hand = [&](std::int32_t instance, bool right) {
      auto* current = dynamic_cast<oCItem*>(right ? npc->GetRightHand() : npc->GetLeftHand());
      if (current && current->GetInstance() == instance && !terminal) {
        if (GetSheathedWeaponSlot(current)) {
          MoveInventoryWeaponToSlot(npc, current, right ? NPC_NODE_RIGHTHAND : NPC_NODE_LEFTHAND);
        }
        return;
      }
      const char* sheathed_slot = GetSheathedWeaponSlot(current);
      const bool remains_equipped = current && current->HasFlag(ITM_FLAG_ACTIVE) &&
                                    ((IsMeleeWeaponItem(current) && current->GetInstance() == LocalItemIndex(state.melee_weapon_instance)) ||
                                     (IsRangedWeaponItem(current) && current->GetInstance() == LocalItemIndex(state.ranged_weapon_instance)));
      if (!terminal && sheathed_slot && remains_equipped) {
        // Sheathing moves the existing equipped object back; it must not toggle
        // equipment, rerun equip scripts, or leave the weapon unattached.
        MoveInventoryWeaponToSlot(npc, current, sheathed_slot);
      } else {
        RemoveHandItem(npc, right);
      }
      if (terminal || instance == 0 || !IsNetworkItemInstance(instance)) {
        return;
      }
      auto* parser = Gothic_II_Addon::parser;
      if (parser && instance == parser->GetIndex("ITLSTORCHBURNING")) {
        // A torch's ZEN contains the flame/light subtree. Inventory attachment
        // substitutes only a mesh, so keep the burning item out of inventory2.
        auto* torch = CreateNetworkItem(instance);
        if (!torch)
          return;
        if (!IsBurningTorch(torch)) {
          torch->Release();
          return;
        }
        torch->dontWriteIntoArchive = true;
        if (right)
          npc->SetRightHand(torch);
        else
          npc->SetLeftHand(torch);
        if ((right ? npc->GetRightHand() : npc->GetLeftHand()) == torch && torch->GetHomeWorld()) {
          // Viewers need the prefab's light/particles, not torch gameplay AI
          // that could independently extinguish it and spawn burned items.
          if (local) {
            if (auto* ai = torch->GetAIVobMove()) {
              auto position = torch->GetPositionWorld();
              ai->Init(torch, npc, position, 0.0f, 0.0f, nullptr);
            }
          }
          TrackProxyPresentationItem(npc, torch);
          // Match native UseItem: release factory references to every child,
          // including the dynamic light, once the world owns the subtree.
          torch->ReleaseVobSubtree(nullptr);
        } else {
          torch->Release();
        }
        return;
      }
      oCItem* item = GetCurrentMeleeWeapon(npc);
      if (!item || item->GetInstance() != instance)
        item = GetCurrentRangedWeapon(npc);
      if (!item || item->GetInstance() != instance)
        item = npc->inventory2.IsIn(instance, 1);
      if (item && ((!right && npc->GetRightHand() == item) || (right && npc->GetLeftHand() == item))) {
        item = nullptr;
      }
      if (!item) {
        item = InsertOwnedItem(npc, CreateNetworkItem(instance), true);
      }
      if (item) {
        if (GetSheathedWeaponSlot(item)) {
          MoveInventoryWeaponToSlot(npc, item, right ? NPC_NODE_RIGHTHAND : NPC_NODE_LEFTHAND);
        } else if (right) {
          npc->SetRightHand(item);
        } else {
          npc->SetLeftHand(item);
        }
      }
    };
    sync_hand(LocalItemIndex(state.left_hand_item_instance), false);
    sync_hand(LocalItemIndex(state.right_hand_item_instance), true);
  }

  // Pose-only updates must also repair a torch overlay displaced by a later
  // walking style, including a walk/run toggle while standing still.
  if ((mask & (kSyncEquipment | kSyncPose)) != 0 && (snapshot || !local))
    UpdateTorchAnimation(npc);

  if ((mask & kSyncPose) != 0 && !terminal && (snapshot || !local)) {
    const bool reacting = IsPlayingDamageReaction(npc->GetModel());
    if (auto* controller = npc->GetAnictrl()) {
      controller->SetWalkMode(state.walk_mode);
      controller->SetLookAtTarget(std::clamp(state.look_yaw, 0.0f, 1.0f), std::clamp(state.look_pitch, 0.0f, 1.0f));
    }
    if (!reacting && state.body_state < BS_MAX && state.body_state != BS_DEAD && state.body_state != BS_UNCONSCIOUS) {
      npc->SetBodyState(static_cast<int>(state.body_state));
    }
    auto* focus = state.focus_target_id ? GetPlayerById(*state.focus_target_id) : nullptr;
    npc->SetFocusVob(focus ? focus->GetNpc() : nullptr);
    // Strafe cycles use T_* names, while finite attacks have separate action IDs.
    if (!server_npc && !local && !state.animation_name.empty()) {
      auto* model = npc->GetModel();
      zSTRING animation(state.animation_name.c_str());
      auto* prototype = model ? model->GetAniFromAniID(model->GetAniIDFromAniName(animation)) : nullptr;
      const bool recovering = model && (model->IsAnimationActive("T_WOUNDED_2_STAND") || model->IsAnimationActive("T_WOUNDEDB_2_STAND"));
      if (prototype && !recovering && !reacting && (!IsPlayingMagicAction(npc) || IsMagicActionAnimation(npc, prototype->GetAniID())) &&
          (prototype->GetAniType() == zMDL_ANI_TYPE_NORMAL || prototype->GetAniType() == zMDL_ANI_TYPE_ALIAS) &&
          gmp::IsSampledPlayerAnimation(state.animation_name, IsLoopingModelAnimation(model, prototype))) {
        auto* active = model->GetActiveAni(prototype);
        if (!active || active->isFadingOut) model->StartAni(prototype, zCModel::zMDL_STARTANI_FORCE);
      }
    }
  }
  SetSuppressLocalEquipEvents(previous_suppression);

  if ((mask & (kSyncEquipment | kSyncCombat)) != 0) PruneProxyPresentationItems(actor, state);

  // Native equipment and animation changes may alter attributes/pose. Finish
  // with authoritative resources and life, including server NPCs.
  if ((mask & kSyncHealth) != 0) {
    const int health = life == PLAYER_LIFE_DEAD ? 0 : life == PLAYER_LIFE_UNCONSCIOUS ? 1 : ClampAliveHealth(npc, state.health_points);
    actor.base_player().set_health(health);
    actor.base_player().set_update_health_packet_counter(0);
    SetNpcHealth(npc, health);
  }
  if ((mask & kSyncMana) != 0) {
    npc->SetAttribute(NPC_ATR_MANA, std::clamp<int>(state.mana_points, 0, std::max(0, npc->GetAttribute(NPC_ATR_MANAMAX))));
  }
  if ((mask & kSyncLife) != 0) {
    ApplyServerStats(actor);
  }
}

void NetGame::OnPlayerStateUpdate(std::uint64_t player_id, const PlayerState& state) {
  if (auto* actor = GetPlayerById(player_id); actor && actor->GetNpc()) {
    ApplyActorState(*actor, state, false);
  }
}

void NetGame::OnPlayerStateUpdate(std::uint64_t player_id, const PlayerState& state, bool authoritative_correction) {
  auto* actor = GetPlayerById(player_id);
  if (!actor || !actor->GetNpc())
    return;
  if (authoritative_correction) {
    // Old draw/sheath animation actions also mutate weapon mode through native
    // tags. They must not run after an explicit combat correction.
    const auto invalidated = state.correction_mask | ((state.correction_mask & kSyncCombat) ? kSyncPose : 0);
    actor->InvalidatePresentation(invalidated);
  }
  ApplyActorState(*actor, state, false, authoritative_correction, authoritative_correction);
}

void NetGame::OnPlayerPositionUpdate(std::uint64_t player_id, float x, float y, float z) {
  Gothic2APlayer* cplayer = GetPlayerById(player_id);
  if (cplayer && cplayer->GetNpc()) {
    if (cplayer->base_player().is_npc()) {
      const auto it = npc_actions_.find(static_cast<std::uint32_t>(player_id));
      if (it != npc_actions_.end()) {
        it->second.control.position = glm::vec3(x, y, z);
      }
    }
    if (cplayer->IsLocalPlayer()) pending_local_spawn_position_.reset();
    cplayer->StopPositionInterpolation();
    cplayer->SetPosition(x, y, z);
  }
}

void NetGame::OnPlayerSummon(const PlayerSummonPacket& packet) {
  auto* caster = GetPlayerById(packet.caster_id);
  auto* npc = GetPlayerById(packet.npc_id);
  if (!caster || !caster->GetNpc() || caster->base_player().actor_generation() != packet.caster_generation || !npc || !npc->GetNpc() ||
      npc->base_player().actor_generation() != packet.npc_generation)
    return;
  EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnPlayerSummonName,
                                        gmp::gothic::OnPlayerSummonEvent{packet.caster_id, packet.spell_id, packet.summon_instance, packet.npc_id});
}

void NetGame::OnPlayerTeleport(const PlayerTeleportPacket& packet, std::uint16_t correction_mask) {
  auto* actor = GetPlayerById(packet.player_id);
  if (!actor || !actor->GetNpc() || actor->base_player().actor_generation() != packet.actor_generation)
    return;
  {
    ScopedNetworkObservationSuppression suppress_observation;
    actor->InvalidatePresentation(correction_mask);
    if (correction_mask & kSyncPosition)
      OnPlayerPositionUpdate(packet.player_id, packet.position.x, packet.position.y, packet.position.z);
    if (correction_mask & kSyncOrientation) {
      actor->GetNpc()->SetHeadingAtWorld(zVEC3(packet.normal.x, packet.normal.y, packet.normal.z));
      if (auto action = npc_actions_.find(packet.player_id); action != npc_actions_.end())
        action->second.control.normal = packet.normal;
    }
  }
  EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnPlayerTeleportName,
                                        gmp::gothic::OnPlayerTeleportEvent{packet.player_id, packet.destination});
}

void NetGame::OnPlayerTransformation(const PlayerTransformationPacket& packet, std::uint16_t correction_mask) {
  auto* actor = GetPlayerById(packet.player_id);
  if (!actor || !actor->GetNpc() || actor->base_player().actor_generation() != packet.actor_generation)
    return;
  if (packet.spell_id && !actor->IsLocalPlayer() && actor->GetTransformationReturnInstance().empty() && actor->GetNpc()->IsHuman()) {
    const auto form = FindParserIndex(packet.instance.c_str());
    if (form && actor->GetNpc()->GetInstance() != *form)
      actor->RememberTransformationInstance(actor->GetNpc());
  }
  if (actor->IsLocalPlayer()) {
    if (packet.spell_id == 0) {
      if (!ClearNativeTransformation())
        return;
      native_transformation_spell_ = 0;
      native_transformation_cast_ = 0;
      actor = GetPlayerById(packet.player_id);
      if (!actor || !actor->GetNpc() || actor->base_player().actor_generation() != packet.actor_generation)
        return;
    } else if (IsActorInstancePending(*actor)) {
      // Native return can complete before the accepted entry reaches us.
      // Keep the real human until the ordered return acknowledgement arrives.
      return;
    }
  }
  actor->InvalidatePresentation(kSyncAll);
  auto instance = packet.instance;
  if (instance.empty()) {
    // The server uses an empty instance for a player with the default body.
    // Prefer the actual pre-transform prototype; late viewers use PC_HERO.
    instance = actor->GetTransformationReturnInstance();
    if (instance.empty())
      instance = "PC_HERO";
  }
  ChangePlayerInstance(packet.player_id, instance, false);
  if (IsActorInstancePending(*actor)) return;
  ApplyServerStats(*actor);
  ApplyActorState(*actor, actor->base_player().state(), true);
  if (actor->IsLocalPlayer() && !packet.spell_id)
    ApplyServerInventory(*actor, true);
}

void NetGame::OnActorDamage(const ActorDamagePacket& damage) {
  auto* victim = GetPlayerById(damage.victim_id);
  if (!victim || !victim->GetNpc())
    return;
  auto apply = [this, damage, expected_npc = victim->GetNpc()] {
    auto* current = GetPlayerById(damage.victim_id);
    if (!current || current->GetNpc() != expected_npc || current->base_player().actor_generation() != damage.victim_generation ||
        current->base_player().life_state() != PLAYER_LIFE_ALIVE || !current->base_player().accepts_life_action_revision(damage.command_revision))
      return;
    auto* attacker = damage.attacker_id ? GetPlayerById(*damage.attacker_id) : nullptr;
    auto* attacker_npc = attacker && attacker->base_player().actor_generation() == damage.attacker_generation ? attacker->GetNpc() : nullptr;
    PlayNetworkDamageAnimation(current->GetNpc(), attacker_npc, damage.damage, damage.damage_type);
  };
  if (victim->IsLocalPlayer() || CanReportActorDamage(victim->GetNpc()))
    apply();
  else
    victim->QueuePresentation(damage.source_time_ms, std::move(apply));
}

void NetGame::OnActorAction(const ActorActionPacket& action) {
  if (action.kind == ActorActionKind::MobState) {
    if (!ogame || !ogame->GetGameWorld() || action.name.empty()) return;
    zSTRING name(action.name.c_str());
    zCArray<zCVob*> candidates;
    ogame->GetGameWorld()->SearchVobListByName(name, candidates);
    oCMobInter* mob = nullptr;
    float closest = 50.0f;
    for (int i = 0; i < candidates.GetNumInList(); ++i) {
      auto* candidate = dynamic_cast<oCMobInter*>(candidates[i]);
      if (!candidate) continue;
      const auto position = candidate->GetPositionWorld();
      const float distance = glm::distance(action.position, glm::vec3(position[VX], position[VY], position[VZ]));
      if (distance < closest) { mob = candidate; closest = distance; }
    }
    if (!mob || action.target_state < 0 || action.target_state > mob->GetStateNum()) return;
    // Absolute target state is idempotent and also restores late-world viewers.
    // SetTempState/SetStateToTempState update native pose without replaying use
    // scripts or toggling a nearby trigger target.
    mob->SetTempState(action.target_state);
    mob->SetStateToTempState();
    return;
  }
  auto* actor = GetPlayerById(action.actor_id);
  if (!actor || !actor->GetNpc() || actor->IsLocalPlayer()) return;
  if ((action.kind == ActorActionKind::AnimationStart || action.kind == ActorActionKind::AnimationStop) &&
      gmp::IsPlayerLifecycleAnimation(action.name)) return;
  const auto generation = actor->base_player().actor_generation();
  const auto corrections = actor->presentation_epochs();
  actor->QueuePresentation(action.source_time_ms, [this, action, generation, corrections] {
    auto* current = GetPlayerById(action.actor_id);
    if (!current || !current->GetNpc() || current->base_player().actor_generation() != generation ||
        !current->base_player().accepts_life_action_revision(action.command_revision) ||
        (current->base_player().is_npc() && current->base_player().npc_control_epoch() != action.control_epoch)) return;
    if ((action.kind == ActorActionKind::AnimationStart || action.kind == ActorActionKind::AnimationStop) &&
        !current->presentation_epochs().CurrentComponents(kSyncPose, corrections)) return;
    ScopedNetworkObservationSuppression suppress_observation;
    auto* npc = current->GetNpc();
    auto* model = npc->GetModel();
    if (!model) return;
    if (action.kind == ActorActionKind::AnimationStop) {
      zSTRING name(action.name.c_str());
      const int id = model->GetAniIDFromAniName(name);
      if (id >= 0) model->StopAni(id);
    } else if (action.kind == ActorActionKind::AnimationStart && !npc->IsDead() && !npc->IsUnconscious()) {
      zSTRING name(action.name.c_str());
      const int id = model->GetAniIDFromAniName(name);
      if (auto* animation = model->GetAniFromAniID(id)) model->StartAni(animation, zCModel::zMDL_STARTANI_FORCE);
    } else if (action.kind == ActorActionKind::Shot && !npc->IsDead() && !npc->IsUnconscious()) {
      auto* weapon = GetCurrentRangedWeapon(npc);
      if (!weapon || !IsNetworkItemInstance(weapon->munition)) return;
      // Native shooting consumes owned ammunition. Supply one visual projectile
      // from the weapon's semantic ammunition identity, never a parser constant.
      if (!npc->inventory2.IsIn(weapon->munition, 1)) InsertOwnedItem(npc, CreateNetworkItem(weapon->munition));
      auto* target = action.target_id ? GetPlayerById(*action.target_id) : nullptr;
      npc->SetFocusVob(target ? target->GetNpc() : nullptr);
      const auto rendered_position = npc->GetPositionWorld();
      const auto rendered_heading = npc->GetAtVectorWorld();
      if (IsFiniteActorVector(action.position) && IsFiniteActorVector(action.direction)) {
        ApplyAuthoritativeNpcPosition(npc, zVEC3(action.position.x, action.position.y, action.position.z));
        npc->SetHeadingYWorld(npc->GetPositionWorld() + zVEC3(action.direction.x, action.direction.y, action.direction.z));
      }
      npc->DoShootArrow(target != nullptr);
      PruneProxyPresentationItems(*current, current->base_player().state());
      ApplyAuthoritativeNpcPosition(npc, rendered_position);
      npc->SetHeadingYWorld(rendered_position + rendered_heading);
    }
    // Item effects are separate authoritative overlays/attributes. Consumption
    // presentation has no permission to execute native gameplay scripts.
  });
}
void NetGame::OnPlayerDied(std::uint64_t player_id) {
  OnPlayerDied(player_id, true);
}

void NetGame::OnPlayerDied(std::uint64_t player_id, bool apply_state) {
  if (apply_state) ApplyPlayerLifeState(player_id, PLAYER_LIFE_DEAD, std::nullopt, false);
  EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnPlayerDeathName, gmp::gothic::PlayerLifecycleEvent{player_id});
}

void NetGame::OnPlayerRespawned(std::uint64_t player_id) {
  OnPlayerRespawned(player_id, true); }
void NetGame::OnPlayerRespawned(std::uint64_t player_id, bool apply_state) {
  if (apply_state) {
    if (auto* actor = GetPlayerById(player_id); actor && actor->GetNpc()) {
      actor->base_player().set_life_state(PLAYER_LIFE_ALIVE);
      actor->RespawnPlayer();
      ApplyActorState(*actor, actor->base_player().state(), true);
    }
  }
  EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnPlayerRespawnName, gmp::gothic::PlayerLifecycleEvent{player_id});
}

void NetGame::OnPlayerUnconscious(std::uint64_t player_id, std::optional<std::uint64_t> attacker_id) {
  OnPlayerUnconscious(player_id, attacker_id, true);
}
void NetGame::OnPlayerUnconscious(std::uint64_t player_id, std::optional<std::uint64_t> attacker_id, bool apply_state) {
  if (apply_state) ApplyPlayerLifeState(player_id, PLAYER_LIFE_UNCONSCIOUS, attacker_id, false);
}

void NetGame::OnPlayerStandUp(std::uint64_t player_id) {
  OnPlayerStandUp(player_id, true); }
void NetGame::OnPlayerStandUp(std::uint64_t player_id, bool apply_state) {
  if (apply_state) ApplyPlayerLifeState(player_id, PLAYER_LIFE_ALIVE, std::nullopt, false);
}
void NetGame::OnPlayerPingUpdate(std::uint64_t player_id, std::int32_t ping) {
  EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnPlayerChangePingName, gmp::gothic::OnPlayerPingEvent{player_id, ping});
}

void NetGame::OnItemDropped(std::uint64_t player_id, const std::string& network_instance, std::int32_t amount) {
  const auto item_instance = LocalItemIndex(network_instance);
  Gothic2APlayer* cplayer = GetPlayerById(player_id);
  if (cplayer && cplayer->GetNpc() && IsNetworkItemInstance(item_instance) && ogame && ogame->GetGameWorld()) {
    oCWorld* world = ogame->GetGameWorld();
    oCItem* NpcDrop = CreateNetworkItem(item_instance);
    if (!NpcDrop) {
      return;
    }
    NpcDrop->amount = amount;
    zVEC3 startPos = cplayer->npc->GetTrafoModelNodeToWorld("ZS_RIGHTHAND").GetTranslation();
    NpcDrop->trafoObjToWorld.SetTranslation(startPos);
    world->AddVob(NpcDrop);
    NpcDrop->SetSleeping(false);
    NpcDrop->SetStaticVob(false);
    NpcDrop->SetPhysicsEnabled(true);
    NpcDrop->Release();
  }
}

void NetGame::OnItemTaken(std::uint64_t player_id, const std::string& network_instance) {
  const auto item_instance = LocalItemIndex(network_instance);
  Gothic2APlayer* cplayer = GetPlayerById(player_id);
  if (cplayer && cplayer->GetNpc() && IsNetworkItemInstance(item_instance) && ogame && ogame->GetGameWorld()) {
    zCListSort<oCItem>* ItemList = ogame->GetGameWorld()->voblist_items;
    if (!ItemList) {
      return;
    }
    for (int i = 0; i < ItemList->GetNumInList(); i++) {
      ItemList = ItemList->GetNextInList();
      if (!ItemList) {
        break;
      }
      oCItem* ItemInList = ItemList->GetData();
      if (ItemInList && ItemInList->GetInstance() == item_instance) {
        if (cplayer->npc->GetDistanceToVob(*ItemInList) < 250.0f) {
          cplayer->npc->DoTakeVob(ItemInList);
          break;
        }
      }
    }
  }
}

void NetGame::OnItemGroundCreate(std::uint32_t item_ground_id, const std::string& instance, std::int32_t amount, bool physics_enabled,
                                 const glm::vec3& position, const glm::vec3& rotation) {
  if (!LocalItemIndex(instance)) {
    SPDLOG_WARN("Ground item '{}' is absent from the local scripts", instance);
    return;
  }
  gmp::gothic::ClientItemGroundManager::Instance().Upsert(item_ground_id, instance, amount, physics_enabled, position, rotation);
}

void NetGame::OnItemGroundDestroy(std::uint32_t item_ground_id) {
  gmp::gothic::ClientItemGroundManager::Instance().Destroy(item_ground_id, true);
}

void NetGame::OnItemsGroundDestroy() {
  gmp::gothic::ClientItemGroundManager::Instance().Clear(true);
}

void NetGame::OnItemGiven(std::uint64_t player_id, const std::string& network_instance, std::int32_t amount) {
  const auto item_instance = LocalItemIndex(network_instance);
  if (amount <= 0 || !IsNetworkItemInstance(item_instance)) {
    return;
  }

  Gothic2APlayer* cplayer = GetPlayerById(player_id);
  if (!cplayer || !cplayer->GetNpc()) {
    return;
  }

  OnActorItemTransactionResult(static_cast<std::uint32_t>(player_id), 0, network_instance,
      cplayer->base_player().inventory_count(network_instance), true, std::nullopt);
}

void NetGame::OnActorItemTransactionResult(std::uint32_t actor_id, std::uint32_t request_id, const std::string& network_instance, std::int32_t total,
                                     bool accepted, std::optional<std::uint32_t> ground) {
  const auto instance = LocalItemIndex(network_instance);
  (void)accepted;
  (void)ground;
  if (!game_client || !game_client->player_manager().HasLocalPlayer()) return;
  auto* actor = GetPlayerById(actor_id);
  if (!actor || !actor->GetNpc()) return;
  if (request_id != 0) {
    const auto pending = pending_inventory_mutations_.find(request_id);
    if (pending == pending_inventory_mutations_.end() || pending->second.instance != instance) return;
    pending_inventory_mutations_.erase(pending);
  }
  std::int64_t predicted_total = std::max(0, total);
  for (const auto& [id, pending] : pending_inventory_mutations_) {
    if (actor->IsLocalPlayer() && pending.instance == instance) predicted_total += pending.delta;
  }
  const bool previous = ShouldSuppressLocalEquipEvents();
  SetSuppressLocalEquipEvents(true);
  ReconcileInventoryAmount(actor->GetNpc(), instance, static_cast<int>(std::clamp<std::int64_t>(predicted_total, 0,
      std::numeric_limits<std::int32_t>::max())));
  SetSuppressLocalEquipEvents(previous);
}

void NetGame::OnActorInventorySnapshot(std::uint64_t actor_id, const std::vector<InventoryEntry>& inventory) {
  (void)inventory;
  auto* actor = GetPlayerById(actor_id);
  if (!actor) return;
  ApplyServerInventory(*actor, true);
}
void NetGame::OnItemEquipped(std::uint64_t id, const std::string& network_instance, std::int16_t slot) {
  auto* actor = GetPlayerById(id);
  OnItemEquipped(id, network_instance, slot, actor ? actor->base_player().command_revision() : 0, true);
}

void NetGame::OnItemEquipped(std::uint64_t id, const std::string& network_instance, std::int16_t slot,
                            std::uint32_t revision, bool apply_state) {
  auto* actor = GetPlayerById(id);
  if (!actor || !actor->GetNpc()) return;
  const auto instance = LocalItemIndex(network_instance);
  const bool previous = ShouldSuppressLocalEquipEvents();
  SetSuppressLocalEquipEvents(true);
  const auto observed_slot = slot == EQUIP_SLOT_GENERIC ? ResolveEquipmentSlot(actor->GetNpc(), instance) : slot;
  if (apply_state && instance > 0 && observed_slot != EQUIP_SLOT_GENERIC) {
    actor->InvalidatePresentation(kSyncEquipment | kSyncCombat);
    if (observed_slot == EQUIP_SLOT_RING_LEFT || observed_slot == EQUIP_SLOT_RING_RIGHT) {
      auto left = LocalItemIndex(actor->base_player().equipped_ring_left());
      auto right = LocalItemIndex(actor->base_player().equipped_ring_right());
      if (slot == EQUIP_SLOT_GENERIC) (observed_slot == EQUIP_SLOT_RING_LEFT ? left : right) = instance;
      SyncEquippedRingItems(actor->GetNpc(), left, right);
    } else {
      SyncEquippedPacketSlot(actor->GetNpc(), instance, observed_slot);
    }
  }
  const auto observed = ObserveEquipmentSlot(*actor, observed_slot, instance);
  ApplyServerStats(*actor);
  SetSuppressLocalEquipEvents(previous);
  if (game_client && (actor->IsLocalPlayer() || (actor->base_player().is_npc() &&
      game_client->player_manager().HasLocalPlayer() && actor->base_player().npc_host_player_id() ==
      game_client->player_manager().GetLocalPlayer().id()))) {
    game_client->SendEquipmentResult(slot, network_instance, instance > 0 && observed == instance && observed_slot != EQUIP_SLOT_GENERIC,
        NetworkItemName(observed), revision, static_cast<std::uint32_t>(id), actor->base_player().npc_control_epoch(), observed_slot);
  }
}

void NetGame::OnItemUnequipped(std::uint64_t id, const std::string& network_instance) {
  auto* actor = GetPlayerById(id);
  OnItemUnequipped(id, network_instance, EQUIP_SLOT_GENERIC, actor ? actor->base_player().command_revision() : 0, true);
}

void NetGame::OnItemUnequipped(std::uint64_t id, const std::string& network_instance, std::int16_t slot,
                              std::uint32_t revision, bool apply_state) {
  const auto instance = LocalItemIndex(network_instance);
  auto* actor = GetPlayerById(id);
  if (!actor || !actor->GetNpc()) return;
  const bool previous = ShouldSuppressLocalEquipEvents();
  SetSuppressLocalEquipEvents(true);
  if (apply_state) {
    actor->InvalidatePresentation(kSyncEquipment | kSyncCombat);
    if (slot == EQUIP_SLOT_RING_LEFT || slot == EQUIP_SLOT_RING_RIGHT) {
      SyncEquippedRingItems(actor->GetNpc(), LocalItemIndex(actor->base_player().equipped_ring_left()), LocalItemIndex(actor->base_player().equipped_ring_right()));
    } else if (!SyncEquippedPacketSlot(actor->GetNpc(), 0, slot)) {
      std::vector<oCItem*> matches;
      for (auto* entry = actor->GetNpc()->inventory2.inventory.GetNextInList(); entry; entry = entry->GetNextInList()) {
        if (auto* item = entry->GetData(); item && item->GetInstance() == instance && item->HasFlag(ITM_FLAG_ACTIVE)) matches.push_back(item);
      }
      for (auto* item : matches) {
        actor->GetNpc()->UnequipItem(item);
        RemoveItemFromHandSlots(actor->GetNpc(), item);
      }
    }
  }
  const auto observed = ObserveEquipmentSlot(*actor, slot, instance);
  ApplyServerStats(*actor);
  SetSuppressLocalEquipEvents(previous);
  if (game_client && (actor->IsLocalPlayer() || (actor->base_player().is_npc() &&
      game_client->player_manager().HasLocalPlayer() && actor->base_player().npc_host_player_id() ==
      game_client->player_manager().GetLocalPlayer().id()))) {
    game_client->SendEquipmentResult(slot, {}, observed == 0, NetworkItemName(observed), revision,
        static_cast<std::uint32_t>(id), actor->base_player().npc_control_epoch());
  }
}
void NetGame::OnItemRemoved(std::uint64_t player_id, const std::string& network_instance, std::int32_t amount) {
  const auto item_instance = LocalItemIndex(network_instance);
  if (amount <= 0 || !IsNetworkItemInstance(item_instance)) {
    return;
  }

  Gothic2APlayer* cplayer = GetPlayerById(player_id);
  if (!cplayer || !cplayer->GetNpc()) {
    return;
  }

  OnActorItemTransactionResult(static_cast<std::uint32_t>(player_id), 0, network_instance,
      cplayer->base_player().inventory_count(network_instance), true, std::nullopt);
}
namespace {
void PlaySpellPresentation(Gothic2APlayer& actor, oCNpc* target, std::uint16_t spell_id, std::uint8_t level) {
  auto* caster = actor.GetNpc();
  if (!caster || spell_id >= 100) return;
  auto* parser = Gothic_II_Addon::parser;
  std::optional<ScriptInstanceScope> scope;
  if (parser) {
    scope.emplace(*parser);
    parser->SetInstance("SELF", caster);
    parser->SetInstance("OTHER", target);
  }
  auto* selected = GetSelectedMagicSpell(caster);
  if (selected && selected->GetSpellID() == spell_id && selected->effect && selected->effect->GetLevel() > level) {
    caster->GetSpellBook()->StopSelectedSpell();
    selected = GetSelectedMagicSpell(caster);
  }
  const bool use_selected = selected && selected->GetSpellID() == spell_id;
  // Construct once: InitValues also creates an effect. Calling it again on a
  // default-constructed spell leaks the original effect (and its light vob).
  auto* spell = use_selected ? selected : new oCSpell(spell_id);
  if (!spell)
    return;
  spell->Setup(caster, target, 1);
  auto* effect = spell->effect;
  // Transfer the one owning reference before releasing/reusing the spell.
  // Its destructor calls Kill on any attached effect, including Light's vob.
  spell->effect = nullptr;
  if (effect) {
    effect->SetDamage(0.0f);
    effect->SetDamageType(0);
    if (level > effect->GetLevel())
      effect->SetLevel(level, 0);
    effect->Cast(1);
    if (spell_id == SPL_LIGHT || spell_id == SPL_PALLIGHT) {
      actor.SetRemoteLightEffect(effect);
    } else {
      // Init/Cast inserted it into the world, which now owns its lifetime.
      effect->Release();
    }
  }
  if (use_selected) {
    spell->manaTimer = 0.0f;
    spell->manaInvested = 0;
    spell->spellLevel = 1;
    spell->spellStatus = SPL_STATUS_CANINVEST;
    spell->spellInitDone = 0;
    spell->CreateEffect();
    if (spell->effect) {
      spell->effect->SetDamage(0.0f);
      spell->effect->SetDamageType(0);
    }
    if (auto* book = caster->GetSpellBook(); book && book->show_handsymbol)
      spell->Setup(caster, nullptr, 0);
  } else {
    spell->Release();
  }
}
}  // namespace

void NetGame::OnSpellCast(std::uint64_t caster_id, std::uint16_t spell_id) {
  auto* caster = GetPlayerById(caster_id);
  if (!caster) return;
  OnSpellCast(caster_id, spell_id, 0, caster->base_player().actor_generation(), caster->base_player().npc_control_epoch());
}

void NetGame::OnSpellCast(std::uint64_t caster_id, std::uint16_t spell_id, std::uint64_t source_time_ms,
                          std::uint64_t actor_generation, std::uint32_t control_epoch) {
  CastSpellPacket packet{};
  packet.caster_id = static_cast<std::uint32_t>(caster_id);
  packet.spell_id = spell_id;
  packet.source_time_ms = source_time_ms;
  packet.actor_generation = actor_generation;
  packet.control_epoch = control_epoch;
  if (auto* caster = GetPlayerById(caster_id))
    packet.command_revision = caster->base_player().command_revision();
  OnSpellCast(packet);
}

void NetGame::OnSpellCastOnTarget(std::uint64_t caster_id, std::uint64_t target_id, std::uint16_t spell_id) {
  auto* caster = GetPlayerById(caster_id);
  auto* target = GetPlayerById(target_id);
  if (!caster || !target) return;
  OnSpellCastOnTarget(caster_id, target_id, spell_id, 0, caster->base_player().actor_generation(),
                       target->base_player().actor_generation(), caster->base_player().npc_control_epoch());
}

void NetGame::OnSpellCastOnTarget(std::uint64_t caster_id, std::uint64_t target_id, std::uint16_t spell_id,
                                  std::uint64_t source_time_ms, std::uint64_t actor_generation,
                                  std::uint64_t target_generation, std::uint32_t control_epoch) {
  CastSpellPacket packet{};
  packet.caster_id = static_cast<std::uint32_t>(caster_id);
  packet.target_id = static_cast<std::uint32_t>(target_id);
  packet.spell_id = spell_id;
  packet.source_time_ms = source_time_ms;
  packet.actor_generation = actor_generation;
  packet.target_generation = target_generation;
  packet.control_epoch = control_epoch;
  if (auto* caster = GetPlayerById(caster_id))
    packet.command_revision = caster->base_player().command_revision();
  OnSpellCast(packet);
}

void NetGame::OnSpellCast(const CastSpellPacket& packet) {
  if (!packet.caster_id || packet.spell_id >= SPL_MAX_SPELL || packet.spell_level > kMaxSpellInvestLevel)
    return;
  auto* caster = GetPlayerById(*packet.caster_id);
  if (!caster || !caster->GetNpc() || caster->IsLocalPlayer())
    return;
  auto enqueue = [this, packet] {
    auto* caster = GetPlayerById(*packet.caster_id);
    if (!caster || !caster->GetNpc() || caster->base_player().actor_generation() != packet.actor_generation)
      return;
    caster->QueuePresentation(packet.source_time_ms, [this, packet] {
      auto* current = GetPlayerById(*packet.caster_id);
      auto* target = packet.target_id ? GetPlayerById(*packet.target_id) : nullptr;
      if (!current || !current->GetNpc() || current->IsLocalPlayer() || !current->base_player().has_spawned() ||
          current->base_player().actor_generation() != packet.actor_generation ||
          !current->base_player().accepts_life_action_revision(packet.command_revision) ||
          (current->base_player().is_npc() && current->base_player().npc_control_epoch() != packet.control_epoch) || current->GetNpc()->IsDead() ||
          current->GetNpc()->IsUnconscious())
        return;
      if (packet.target_id && (!target || !target->GetNpc() || !target->base_player().has_spawned() ||
                               target->base_player().actor_generation() != packet.target_generation))
        return;
      ScopedNetworkObservationSuppression suppress_observation;
      PlaySpellPresentation(*current, target ? target->GetNpc() : nullptr, packet.spell_id, packet.spell_level);
    });
  };
  if (gmp::GetTransformationForm(packet.spell_id)) {
    // Instance replacement is already queued. Enqueue its FX afterwards so
    // retiring the previous model cannot discard the accepted cast effect.
    GMPCore::Instance().DeferToNextFrame(std::move(enqueue));
  } else {
    enqueue();
  }
}
void NetGame::OnPlayerMessage(std::optional<std::uint64_t> sender_id, std::uint8_t r, std::uint8_t g, std::uint8_t b, const std::string& message) {
  if (sender_id) {
    Gothic2APlayer* sender = GetPlayerById(*sender_id);
    if (sender && sender->GetNpc()) {
      SPDLOG_INFO("Message from player {} ({}): {}", sender->base_player().name(), sender->GetNpc()->GetName().ToChar(), message);
    } else if (sender) {
      SPDLOG_INFO("Message from player {}: {}", sender->base_player().name(), message);
    }
  } else {
    SPDLOG_INFO("System message: {}", message);
  }

  EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnPlayerMessageName, gmp::gothic::OnPlayerMessageEvent{sender_id, r, g, b, message});
}

void NetGame::OnLuaEvent(const std::string& event_name, std::uint32_t source_element, const std::string& payload) {
  if (!resource_runtime) {
    SPDLOG_WARN("Ignoring Lua event '{}' because client resources are not loaded", event_name);
    return;
  }

  auto& lua = resource_runtime->GetLuaState();
  std::vector<sol::object> args;
  std::string error;
  if (!gmp::lua::DecodeLuaArgs(lua, payload, args, error)) {
    SPDLOG_ERROR("Failed to decode Lua event '{}' payload: {}", event_name, error);
    return;
  }

  if (!gmp::gothic::TriggerRemoteEvent(event_name, source_element, args)) {
    SPDLOG_WARN("Lua event '{}' rejected or cancelled", event_name);
  }
}
