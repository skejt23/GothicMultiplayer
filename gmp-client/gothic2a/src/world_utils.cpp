
/*
MIT License

Copyright (c) 2025 Gothic Multiplayer Team.

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

#include "world_utils.hpp"

#include <vector>

#include "CChat.h"
#include "net_game.h"
#include "renderer/d3d11/D3D11Renderer.h"
#include "renderer/d3d9/D3D9Renderer.h"

void ClearNpcFocusReferences(oCNpc* target) {
  if (!ogame || !ogame->GetGameWorld())
    return;
  // Releasing a focus can release another NPC. Retain the complete traversal.
  std::vector<oCNpc*> npcs;
  for (auto* entry = ogame->GetGameWorld()->voblist_npcs; entry; entry = entry->next) {
    if (auto* npc = entry->GetData()) {
      npc->AddRef();
      npcs.push_back(npc);
    }
  }
  for (auto* npc : npcs) {
    if (!target || npc == target || npc->GetFocusVob() == target)
      npc->ClearFocusVob();
  }
  for (auto* npc : npcs) npc->Release();
}

void PrepareWorldChange() {
  NetGame::Instance().ClearNativeTransformation();
  CChat::GetInstance()->CloseInput(true);
  ClearNpcFocusReferences();
  ResetWorldRenderState();
}

void ResetWorldRenderState() {
  // Loading can change the world's indoor/outdoor controller. Gothic runs AI
  // during precaching before zCWorld::Render normally activates that controller.
  if (ogame && ogame->GetGameWorld()) {
    zCSkyControler::s_activeSkyControler = ogame->GetGameWorld()->GetActiveSkyControler();
  }
  if (!zrenderer)
    return;
  // Complete queued geometry before releasing the textures it borrows. Unbind
  // every stage and use the public reset to clear the cached material pointer.
  zrenderer->FlushPolys();
  for (unsigned long stage = 0; stage < 4; ++stage) zrenderer->SetTexture(stage, nullptr);
  if (auto* renderer = dynamic_cast<zCRnd_D3D_DX11*>(zrenderer))
    renderer->RestoreOpaqueRenderState();
  if (auto* renderer = dynamic_cast<zCRnd_D3D_DX9*>(zrenderer))
    renderer->RestoreOpaqueRenderState();
}

namespace {
void UnlockLockable(oCMobLockable& lockable) {
  lockable.SetLocked(false);
  lockable.pickLockNr = 0;
  lockable.keyInstance.Clear();
  lockable.pickLockStr.Clear();
}

void EmptyAndUnlockContainer(oCMobContainer& container) {
  for (zCListSort<oCItem>* entry = container.containList.next; entry;) {
    zCListSort<oCItem>* next = entry->next;
    if (auto* item = entry->data) {
      item->RemoveVobFromWorld();
    }
    entry = next;
  }

  container.containList.DeleteListDatas();
  container.containList.data = nullptr;

  if (container.items) {
    container.items->DeleteContents();
  }

  container.contains.Clear();

  UnlockLockable(container);
}
}  // namespace

void CleanupWorldObjects(oCWorld* world) {
  if (!world) {
    return;
  }

  for (zCTree<zCVob>* node = world->globalVobTree.firstChild; node;) {
    zCTree<zCVob>* next = node->next;
    if (!node->data) {
      node = next;
      continue;
    }

    if (auto* item = zDYNAMIC_CAST<oCItem>(node->data)) {
      item->RemoveVobFromWorld();
      node = next;
      continue;
    }

    if (auto* container = zDYNAMIC_CAST<oCMobContainer>(node->data)) {
      EmptyAndUnlockContainer(*container);
      node = next;
      continue;
    }

    if (auto* door = zDYNAMIC_CAST<oCMobDoor>(node->data)) {
      UnlockLockable(*door);
    }
    node = next;
  }
}

void DeleteAllNpcsAndDisableSpawning() {
  if (!ogame || !ogame->GetGameWorld()) {
    return;
  }

  auto* npc_list = ogame->GetGameWorld()->voblist_npcs;
  const int size = npc_list ? npc_list->GetNumInList() : 0;
  for (int i = 0; npc_list && i < size; ++i) {
    npc_list = npc_list->next;
    if (!npc_list || !npc_list->GetData()) {
      continue;
    }

    // The parser index of PC_HERO is not stable after an addon GOTHIC.DAT is
    // loaded. Exclude the actual current hero rather than a magic instance id.
    if (npc_list->GetData() != Gothic_II_Addon::player) {
      npc_list->GetData()->Disable();
    }
  }
  if (ogame->GetSpawnManager()) {
    ogame->GetSpawnManager()->SetSpawningEnabled(0);
  }
}
