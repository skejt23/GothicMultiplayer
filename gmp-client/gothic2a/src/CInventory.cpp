
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

/*****************************************************************************
** ** *	File name:		CGmpClient/CInventory.cpp		   						** *
*** *	Created by:		08/12/11	-	skejt23									** *
*** *	Description:	Multiplayer inventory functionallity	 				** *
***
*****************************************************************************/

#include "CInventory.h"

#include "scripting/gothic_events.h"
#include "shared/event.h"

CInventory::CInventory(oCNpcInventory* HeroInventory) {
  BindInventory(HeroInventory);
};

void CInventory::BindInventory(oCNpcInventory* inventory) {
  Inv = inventory;
  was_open_ = Inv ? Inv->IsOpen() : false;
  last_selected_slot_ = Inv ? Inv->selectedItem : -1;
}

CInventory::~CInventory() {
  Inv = NULL;
};

bool CInventory::IsOpened() {
  return Inv && Inv->IsOpen();
};

void CInventory::RenderInventory() {
  const bool is_open = IsOpened();
  if (is_open != was_open_) {
    if (is_open) {
      EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnOpenInventoryName, 0);
      last_selected_slot_ = Inv ? Inv->selectedItem : -1;
    } else {
      EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnCloseInventoryName, 0);
      last_selected_slot_ = -1;
    }
    was_open_ = is_open;
  }

  if (!is_open) {
    return;
  }

  if (Inv) {
    const int current_slot = Inv->selectedItem;
    if (last_selected_slot_ != -1 && current_slot != last_selected_slot_) {
      EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnInventorySlotChangeName,
                                            gmp::gothic::OnInventorySlotChangeEvent{last_selected_slot_, current_slot});
    }
    last_selected_slot_ = current_slot;
  }
};
