
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

namespace {
void UnlockLockable(oCMobLockable& lockable) {
  lockable.SetLocked(false);
  lockable.pickLockNr = 0;
  lockable.keyInstance.Clear();
  lockable.pickLockStr.Clear();
}

void EmptyAndUnlockContainer(oCMobContainer& container) {
  for (zCListSort<oCItem>* entry = &container.containList; entry; entry = entry->next) {
    if (auto* item = entry->data) {
      item->RemoveVobFromWorld();
    }
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

  for (zCTree<zCVob>* node = world->globalVobTree.firstChild; node; node = node->next) {
    if (!node->data) {
      continue;
    }

    if (auto* item = zDYNAMIC_CAST<oCItem>(node->data)) {
      item->RemoveVobFromWorld();
      continue;
    }

    if (auto* container = zDYNAMIC_CAST<oCMobContainer>(node->data)) {
      EmptyAndUnlockContainer(*container);
      continue;
    }

    if (auto* door = zDYNAMIC_CAST<oCMobDoor>(node->data)) {
      UnlockLockable(*door);
    }
  }
}
