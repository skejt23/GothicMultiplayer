// SPDX-License-Identifier: MIT
#pragma once

#include "ZenGin/zGothicAPI.h"

const char* GetSheathedWeaponSlot(oCItem* item);
bool MoveInventoryWeaponToSlot(oCNpc* npc, oCItem* item, const char* destination);

// Complete an immediate scripted draw/sheath, including slots and animations.
// Sampled remote state uses cancel_transition=false to preserve its animation timeline.
bool SetNpcWeaponMode(oCNpc* npc, int mode, bool cancel_transition = true);
