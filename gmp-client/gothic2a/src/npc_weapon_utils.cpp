// SPDX-License-Identifier: MIT
#include "npc_weapon_utils.h"

#include <algorithm>

#include "Mod.h"
#include "torch_utils.h"

namespace {
int GetItemWeaponMode(oCItem* item) {
  if (!item)
    return NPC_WEAPON_NONE;
  if (item->HasFlag(ITM_FLAG_2HD_SWD) || item->HasFlag(ITM_FLAG_2HD_AXE))
    return NPC_WEAPON_2HS;
  if (item->HasFlag(ITM_FLAG_DAG))
    return NPC_WEAPON_DAG;
  if (item->HasFlag(ITM_FLAG_SWD) || item->HasFlag(ITM_FLAG_AXE))
    return NPC_WEAPON_1HS;
  if (item->HasFlag(ITM_FLAG_BOW))
    return NPC_WEAPON_BOW;
  if (item->HasFlag(ITM_FLAG_CROSSBOW))
    return NPC_WEAPON_CBOW;
  return NPC_WEAPON_NONE;
}

oCItem* FindEquippedWeapon(oCNpc* npc, int mode) {
  if (mode <= NPC_WEAPON_FIST || mode == NPC_WEAPON_MAG)
    return nullptr;
  const int flags[] = {0, 0, ITM_FLAG_DAG, ITM_FLAG_SWD | ITM_FLAG_AXE, ITM_FLAG_2HD_SWD | ITM_FLAG_2HD_AXE, ITM_FLAG_BOW, ITM_FLAG_CROSSBOW, 0};
  for (const auto* slot : {NPC_NODE_LEFTHAND, NPC_NODE_RIGHTHAND, NPC_NODE_SWORD, NPC_NODE_LONGSWORD, NPC_NODE_BOW, NPC_NODE_CROSSBOW}) {
    auto* item = npc->GetSlotItem(zSTRING(slot));
    if (item && item->HasFlag(ITM_FLAG_ACTIVE) && (item->flags & flags[mode]) != 0)
      return item;
  }
  return nullptr;
}

bool CancelWeaponTransition(oCNpc* npc, zCModel* model) {
  bool interrupted = false;
  auto* events = npc->GetEM();
  for (int i = 0; i < events->GetNumMessages(); ++i) {
    auto* message = zDYNAMIC_CAST<oCMsgWeapon>(events->GetEventMessage(i));
    if (message && message->subType <= oCMsgWeapon::EV_FORCEREMOVEWEAPON && !message->IsDeleted()) {
      // Mark jobs for native cleanup; deleting an executing job would invalidate its caller.
      message->Delete();
      interrupted = true;
    }
  }
  // A draw animation can otherwise apply DEF_FIGHTMODE after this command.
  for (int i = std::clamp(model->numActiveAnis, 0, zMDL_MAX_ANIS_PARALLEL) - 1; i >= 0; --i) {
    auto* active = model->aniChannels[i];
    auto* animation = active ? active->protoAni : nullptr;
    if (!animation || !animation->aniEvents)
      continue;
    const int event_count = animation->numAniEvents & 63;
    for (int event = 0; event < event_count; ++event) {
      const auto& tag = animation->aniEvents[event];
      if (tag.aniEventType == zMDL_EVENT_TAG && tag.tagString == zSTRING("DEF_FIGHTMODE")) {
        model->StopAni(active);
        interrupted = true;
        break;
      }
    }
  }
  return interrupted;
}
}  // namespace

const char* GetSheathedWeaponSlot(oCItem* item) {
  switch (GetItemWeaponMode(item)) {
    case NPC_WEAPON_DAG:
    case NPC_WEAPON_1HS:
      return NPC_NODE_SWORD;
    case NPC_WEAPON_2HS:
      return NPC_NODE_LONGSWORD;
    case NPC_WEAPON_BOW:
      return NPC_NODE_BOW;
    case NPC_WEAPON_CBOW:
      return NPC_NODE_CROSSBOW;
    default:
      return nullptr;
  }
}

bool MoveInventoryWeaponToSlot(oCNpc* npc, oCItem* item, const char* destination) {
  auto* target = npc && destination ? npc->GetInvSlot(zSTRING(destination)) : nullptr;
  if (!target || !item)
    return false;
  // An equipped inventory object may have a stale mesh on more than one node.
  item->AddRef();
  bool detached = false;
  for (const char* name : {NPC_NODE_SWORD, NPC_NODE_LONGSWORD, NPC_NODE_BOW, NPC_NODE_CROSSBOW, NPC_NODE_LEFTHAND, NPC_NODE_RIGHTHAND}) {
    auto* source = npc->GetInvSlot(zSTRING(name));
    if (source && source != target && source->vob == item) {
      npc->RemoveFromSlot(source, FALSE, FALSE);
      detached = true;
    }
  }
  const bool changed = target->vob != item || detached;
  if (changed)
    npc->PutInSlot(target, item, npc->IsInInv(item, 1) != nullptr);
  item->Release();
  return changed;
}

bool SetNpcWeaponMode(oCNpc* npc, int mode, bool cancel_transition) {
  if (!npc || mode < NPC_WEAPON_NONE || mode > NPC_WEAPON_MAG)
    return false;
  auto* model = npc->GetModel();
  auto* controller = npc->GetAnictrl();
  if (!model || !controller || !npc->human_ai)
    return false;

  ScopedNetworkObservationSuppression suppress_observation;
  ScopedTorchUseSuppression suppress_torch_use;
  const int previous = npc->GetWeaponMode();
  const bool was_standing = controller->IsStanding();
  const bool interrupted = cancel_transition && CancelWeaponTransition(npc, model);

  if (previous == NPC_WEAPON_MAG || mode == NPC_WEAPON_MAG) {
    npc->MakeSpellBook();
    if (mode != NPC_WEAPON_MAG)
      npc->GetSpellBook()->Close(TRUE);
  }

  // SetWeaponMode only sets fmode/focus. SetWeaponMode2 also swaps the actual
  // inventory slots and selects fight/walk animations, but cannot switch
  // directly between two non-NONE modes.
  if (previous != NPC_WEAPON_NONE && previous != mode)
    npc->SetWeaponMode2(NPC_WEAPON_NONE);
  npc->SetWeaponMode2(mode);

  auto* selected = FindEquippedWeapon(npc, mode);
  const char* hand = mode == NPC_WEAPON_BOW ? NPC_NODE_LEFTHAND : NPC_NODE_RIGHTHAND;
  bool repaired = false;
  for (const auto* slot : {NPC_NODE_LEFTHAND, NPC_NODE_RIGHTHAND}) {
    auto* item = npc->GetSlotItem(zSTRING(slot));
    if (const auto* sheathed = GetSheathedWeaponSlot(item)) {
      repaired |= MoveInventoryWeaponToSlot(npc, item, item == selected ? hand : sheathed);
    }
  }
  if (selected)
    repaired |= MoveInventoryWeaponToSlot(npc, selected, hand);
  // Repair the already-inconsistent case as well: NONE with a weapon still
  // held makes native SetWeaponMode2 return before touching slots or aliases.
  if (repaired || interrupted) {
    controller->SetFightAnis(mode);
    controller->SetWalkMode(controller->walkmode);
    if ((was_standing || interrupted) && !npc->IsDead() && !npc->IsUnconscious())
      npc->human_ai->StartStandAni();
  }
  if (cancel_transition) {
    controller->wmode = mode == NPC_WEAPON_NONE ? WMODE_NONE : WMODE_STAND;
    controller->wmode_last = controller->wmode_selected = mode;
    controller->changeweapon = FALSE;
    if (controller->GetActionMode() == ANI_ACTION_CHOOSEWEAPON)
      controller->SetActionMode(ANI_ACTION_STAND);
  }
  return npc->GetWeaponMode() == mode;
}
