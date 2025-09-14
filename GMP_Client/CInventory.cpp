
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

#include "CLanguage.h"
#include "keyboard.h"

extern CLanguage* Lang;
extern zCOLOR Normal;
char q[2] = {0, 0};

CInventory::CInventory(oCNpcInventory* HeroInventory) {
  DropInvoked = false;
  Inv = HeroInventory;
  Owner = HeroInventory->GetOwner();
  InvWindow = HeroInventory->viewItemInfo;
};

CInventory::~CInventory() {
  Inv = NULL;
  Owner = NULL;
  InvWindow = NULL;
};

void CInventory::DropAmount(oCItem* Item, int amount) {
  if (!Item)
    return;
  if (amount < 1)
    return;
  if (amount > Item->amount)
    return;
  if (amount == Item->amount) {
    Owner->DoDropVob(Item);
    return;
  }
  int AmountDec = Item->amount - amount;
  oCItem* ToDrop = zfactory->CreateItem(Item->GetInstance());
  ToDrop->amount = amount;
  Owner->DoDropVob(ToDrop);
};

oCItem* CInventory::GetSelectedItem() {
  if (IsEmpty())
    return NULL;
  int ItemNumber = Inv->selectedItem;
  return Inv->GetItem(ItemNumber);
};

void CInventory::InvokeAmountDrop() {
  if (!IsOpened())
    return;
  if (IsEmpty())
    return;
  oCItem* SelectedItem = GetSelectedItem();
  if (!SelectedItem)
    return;
  if (SelectedItem->amount < 2)
    return;
  DropInvoked = true;
};

bool CInventory::IsEmpty() {
  if (Inv->GetNumItemsInCategory() > 0)
    return false;
  return true;
};

bool CInventory::IsOpened() {
  return Inv->IsOpen();
};

void CInventory::RenderInventory() {
  if (!IsOpened()) {
    DropInvoked = false;
    return;
  }
  // INPUT
  if (!DropInvoked) {
    if (zinput->KeyToggled(KEY_RSHIFT) || zinput->KeyToggled(KEY_LSHIFT)) {
      InvokeAmountDrop();
    }
  } else {
    if (zinput->KeyToggled(KEY_RSHIFT) || zinput->KeyToggled(KEY_LSHIFT)) {
      DropInvoked = false;
    }
  }
  if (DropInvoked) {
    if (IsEmpty())
      DropInvoked = false;
    if (Owner->IsDead())
      DropInvoked = false;
    if (!Owner->IsMovLock())
      Owner->SetMovLock(1);
    screen->SetFontColor(Normal);
    screen->Print(2000, 2000, (*Lang)[CLanguage::INV_HOWMUCH]);
    screen->Print(2000, 2200, GetSelectedItem()->GetDescription());
    q[0] = GInput::GetNumberCharacterFromKeyboard();
    if ((q[0] == 0x08) && (AmountNum.Length() > 0))
      AmountNum.DeleteRight(1);
    if ((q[0] >= 0x20) && (AmountNum.Length() < 24))
      AmountNum += q;
    screen->Print(2000, 2400, AmountNum);
    if (zinput->KeyPressed(KEY_RETURN)) {
      zinput->ClearKeyBuffer();
      DropInvoked = false;
      Owner->SetMovLock(0);
      DropAmount(GetSelectedItem(), AmountNum.ToInt());
      AmountNum.Clear();
    }
  }
};