// SPDX-License-Identifier: MIT
#include "torch_utils.h"
#include "item_instance.h"

#include <algorithm>
#include <cstring>
#include <initializer_list>
#include <string_view>

#include "animation_sync.h"
#include "net_game.h"

using namespace Gothic_II_Addon;

namespace {
thread_local bool suppress_torch_use = false;

bool HasOverlay(oCNpc* npc, const char* name) {
  for (int index = 0; index < npc->activeOverlays.GetNumInList(); ++index)
    if (_stricmp(npc->activeOverlays[index].ToChar(), name) == 0)
      return true;
  return false;
}

bool TorchLocomotionIsOverridden(zCModel* model, const char* overlay) {
  const auto stem_length = std::strlen(overlay) - 4;
  for (int index = 1; index < model->modelProtoList.GetNumInList(); ++index) {
    auto* prototype = model->modelProtoList[index];
    if (!prototype)
      continue;
    const char* name = prototype->modelProtoName.ToChar();
    if (_stricmp(name, overlay) != 0 && (std::strlen(name) != stem_length || _strnicmp(name, overlay, stem_length) != 0))
      continue;
    for (int ani_index = 0; ani_index < prototype->protoAnis.GetNumInList(); ++ani_index) {
      auto* animation = prototype->protoAnis[ani_index];
      if (animation && gmp::IsPlayerLocomotionAnimation(animation->GetAniName().ToChar()) &&
          model->GetAniFromAniID(animation->GetAniID()) != animation)
        return true;
    }
    return false;
  }
  // The NPC can retain an overlay name after its model has been replaced.
  return true;
}

int GetRefreshableTorchPose(zCModel* model) {
  int pose = -1;
  for (int index = 0; index < std::clamp(model->numActiveAnis, 0, zMDL_MAX_ANIS_PARALLEL); ++index) {
    auto* active = model->aniChannels[index];
    auto* animation = active ? active->protoAni : nullptr;
    if (!animation || active->isFadingOut)
      continue;
    if (animation->GetAniType() != zMDL_ANI_TYPE_NORMAL && animation->GetAniType() != zMDL_ANI_TYPE_ALIAS)
      continue;
    // Removing an overlay stops native channels. Defer priority repair during
    // attacks, item use, hits, casting and other non-locomotion animations.
    const std::string_view name = animation->GetAniName().ToChar();
    const auto* next = active->nextAniOverride ? active->nextAniOverride : active->nextAni;
    if (!gmp::IsPlayerLocomotionAnimation(name) || (!gmp::IsSampledPlayerAnimation(name, next == animation) && !gmp::IsPlayerTurningAnimation(name)))
      return -1;
    if (animation->layer == 1)
      pose = animation->GetAniID();
  }
  return pose;
}
}  // namespace

bool IsBurningTorch(oCItem* item) {
  return item && item->HasFlag(ITM_FLAG_TORCH) && item->GetInstanceName() == "ITLSTORCHBURNING";
}

int GetCarriedTorchAmount(oCNpc* npc, int inventory_instance) {
  auto* parser = Gothic_II_Addon::parser;
  if (!npc || !parser || inventory_instance <= 0)
    return 0;
  auto* symbol = parser->GetSymbol(inventory_instance);
  if (!symbol || (symbol->name != "ITLSTORCH" && symbol->name != "ITLSTORCHBURNING"))
    return 0;
  const int burning = symbol->name == "ITLSTORCHBURNING" ? inventory_instance : parser->GetIndex("ITLSTORCHBURNING");
  const int unlit = symbol->name == "ITLSTORCH" ? inventory_instance : parser->GetIndex("ITLSTORCH");
  const auto actor_id = NetGame::Instance().GetPlayerIdByNpc(npc);
  auto* actor = actor_id ? NetGame::Instance().FindEngineActor(*actor_id) : nullptr;
  // A directly granted burning torch takes precedence over the unlit backing
  // item. Never reserve the same held object against both inventory entries.
  const bool owns_burning = actor && actor->base_player().inventory_count(gmp::gothic::NetworkItemName(burning)) > 0;
  if (inventory_instance != (owns_burning ? burning : unlit))
    return 0;
  int count = 0;
  for (auto* hand : {npc->GetLeftHand(), npc->GetRightHand()}) {
    auto* item = dynamic_cast<oCItem*>(hand);
    if (!IsBurningTorch(item))
      continue;
    bool in_inventory = false;
    for (auto* entry = npc->inventory2.inventory.GetNextInList(); entry; entry = entry->GetNextInList())
      if (entry->GetData() == item)
        in_inventory = true;
    if (!in_inventory)
      ++count;
  }
  return count;
}

void UpdateTorchAnimation(oCNpc* npc, bool refresh_stand) {
  if (!npc || !npc->GetModel())
    return;
  const bool held = IsBurningTorch(dynamic_cast<oCItem*>(npc->GetLeftHand()));
  const char* overlay = npc->GetTrueGuild() <= NPC_GIL_HUMANS ? "HUMANS_TORCH.MDS" : npc->GetTrueGuild() >= NPC_GIL_ORCS ? "ORC_TORCH.MDS" : nullptr;
  if (!overlay)
    return;
  const bool present = HasOverlay(npc, overlay);
  const bool changed = held != present;
  // The deferred native check waits for a locomotion state. Applying the torch
  // overlay immediately prevents needing a strafe to enter that state first.
  if (changed)
    npc->SetTorchAni(held, TRUE);
  else if (held && TorchLocomotionIsOverridden(npc->GetModel(), overlay)) {
    const int pose = GetRefreshableTorchPose(npc->GetModel());
    if (pose >= 0 && !npc->IsDead() && !npc->IsUnconscious()) {
      // Native ApplyOverlay is a no-op for an already registered name. Move the
      // torch overlay after conflicting walk styles, then restore locomotion.
      npc->RemoveOverlay(overlay);
      npc->SetTorchAni(TRUE, TRUE);
      npc->GetModel()->StartAni(pose, zCModel::zMDL_STARTANI_FORCE);
    }
  }
  if (refresh_stand && !npc->IsDead() && !npc->IsUnconscious()) {
    if (auto* controller = npc->GetAnictrl()) {
      const int walk_mode = controller->walkmode;
      controller->SearchStandAni(TRUE);
      controller->SetWalkMode(walk_mode);
      controller->_Stand();
    }
  }
}

bool IsTorchUseSuppressed() {
  return suppress_torch_use;
}

ScopedTorchUseSuppression::ScopedTorchUseSuppression() : previous_(suppress_torch_use) {
  suppress_torch_use = true;
}

ScopedTorchUseSuppression::~ScopedTorchUseSuppression() {
  suppress_torch_use = previous_;
}

bool RemoveTorchFromHand(oCNpc* npc, bool right) {
  if (!npc)
    return false;
  auto* item = dynamic_cast<oCItem*>(right ? npc->GetRightHand() : npc->GetLeftHand());
  if (!IsBurningTorch(item))
    return false;
  const auto actor_id = NetGame::Instance().GetPlayerIdByNpc(npc);
  auto* actor = actor_id ? NetGame::Instance().FindEngineActor(*actor_id) : nullptr;
  const bool owns_inventory = actor && (actor->IsLocalPlayer() || actor->base_player().is_npc());
  bool in_inventory = false;
  for (auto* entry = npc->inventory2.inventory.GetNextInList(); entry; entry = entry->GetNextInList())
    if (entry->GetData() == item)
      in_inventory = true;
  const bool standing = npc->GetAnictrl() && npc->GetAnictrl()->IsStanding();
  ScopedTorchUseSuppression suppress_use;
  item->AddRef();
  zSTRING slot(right ? NPC_NODE_RIGHTHAND : NPC_NODE_LEFTHAND);
  npc->RemoveFromSlot(slot, FALSE, TRUE);
  // Native put-away converts the burning vob back to an unlit inventory item.
  // A cancelled equip must return that reservation, not destroy an owned unit.
  if (owns_inventory && !in_inventory)
    npc->PutInInv(item);
  item->RemoveVobSubtreeFromWorld();
  item->Release();
  UpdateTorchAnimation(npc, standing);
  return true;
}

void ClearTorchHands(oCNpc* npc) {
  RemoveTorchFromHand(npc, true);
  RemoveTorchFromHand(npc, false);
}
