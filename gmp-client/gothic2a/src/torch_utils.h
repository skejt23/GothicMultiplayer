// SPDX-License-Identifier: MIT
#pragma once

#include "ZenGin/zGothicAPI.h"

bool IsBurningTorch(Gothic_II_Addon::oCItem* item);
// Inventory ownership includes the torch temporarily represented by a held vob.
int GetCarriedTorchAmount(Gothic_II_Addon::oCNpc* npc, int inventory_instance);
void UpdateTorchAnimation(Gothic_II_Addon::oCNpc* npc, bool refresh_stand = false);
bool RemoveTorchFromHand(Gothic_II_Addon::oCNpc* npc, bool right);
void ClearTorchHands(Gothic_II_Addon::oCNpc* npc);
bool IsTorchUseSuppressed();

// Native slot changes can automatically re-equip a stowed torch. Authoritative
// hand updates and removal must finish without executing that gameplay path.
class ScopedTorchUseSuppression {
public:
  ScopedTorchUseSuppression();
  ~ScopedTorchUseSuppression();
  ScopedTorchUseSuppression(const ScopedTorchUseSuppression&) = delete;
  ScopedTorchUseSuppression& operator=(const ScopedTorchUseSuppression&) = delete;

private:
  bool previous_;
};
