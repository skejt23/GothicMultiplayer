
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

#include "CSelectClass.h"

#include <list>

#include "CLocalPlayer.h"
#include "Interface.h"
#include "ZenGin/zGothicAPI.h"
#include "patch.h"

extern float fWRatio;
CSelectClass *selectmgr = NULL;
extern CLocalPlayer *LocalPlayer;

CSelectClass::CSelectClass(CLanguage *ptr, GameClient *cl) {
  // INITIALIZING LOCAL PLAYER HERE INSTEAD IN JOINGAME
  if (!LocalPlayer) {
    new CLocalPlayer();
    LocalPlayer->SetNpc(player);
  }
  this->lang = ptr;
  this->client = cl;
  selectmgr = this;
  client->classmgr->EquipNPC(0, LocalPlayer, true);
  CameraPrepared = false;
  Patch::SetLookingOnNpcCamera(true);
  Patch::DropVobEnabled(false);
  this->selected = 0;
  ChangeSpawnPointByClass();
}

CSelectClass::~CSelectClass() {
  this->lang = NULL;
  this->client = NULL;
  Patch::SetLookingOnNpcCamera(false);
  Patch::DropVobEnabled(true);
  selectmgr = NULL;
}

size_t CSelectClass::GetSelected() {
  return this->selected;
}

void CSelectClass::Loop() {
  if (selectmgr) {
    screen->SetFont(zfontman->GetFont(0));
    player->SetMovLock(1);
    screen->Print(0, 0, (*selectmgr->lang)[CLanguage::SELECT_CONTROLS]);
    screen->Print(0, 150, (*selectmgr->lang)[CLanguage::CLASS_NAME]);
    screen->Print(0, 300, (*selectmgr->lang)[CLanguage::CLASS_DESCRIPTION]);
    screen->Print(0, 450, (*selectmgr->lang)[CLanguage::TEAM_NAME]);
    CHeroClass *chc = selectmgr->client->classmgr;
    screen->Print(120 + static_cast<zINT>(static_cast<float>(60 * (*selectmgr->lang)[CLanguage::CLASS_NAME].Length()) * fWRatio), 150,
                  (*chc)[selectmgr->GetSelected()]->class_name);
    screen->Print(120 + static_cast<zINT>(static_cast<float>(60 * (*selectmgr->lang)[CLanguage::CLASS_DESCRIPTION].Length()) * fWRatio), 300,
                  (*chc)[selectmgr->GetSelected()]->class_description);
    screen->Print(120 + static_cast<zINT>(static_cast<float>(60 * (*selectmgr->lang)[CLanguage::TEAM_NAME].Length()) * fWRatio), 450,
                  (*chc)[selectmgr->GetSelected()]->team_name);
    selectmgr->HandleInput();  // wyrzucilem na koniec bo za szybko robil delete this;
  }
}

void CSelectClass::CleanUpBeforeNext() {
  player->SetWeaponMode(NPC_WEAPON_NONE);
  if (player->GetRightHand()) {
    zCVob *Ptr = player->GetRightHand();
    zCVob *PtrLeft = player->GetLeftHand();
    player->DropAllInHand();
    if (PtrLeft)
      PtrLeft->RemoveVobFromWorld();
    Ptr->RemoveVobFromWorld();
  }
};

void CSelectClass::ChangeSpawnPointByClass() {
  std::list<const char *> team_list;
  team_list.push_back((*client->classmgr)[0]->team_name.ToChar());
  for (size_t x = 1; x < client->classmgr->GetSize(); x++) {
    bool found = false;
    std::list<const char *>::iterator y;
    for (y = team_list.begin(); y != team_list.end(); y++) {
      if (!memcmp((*y), (*client->classmgr)[x]->team_name.ToChar(), strlen((*y)))) {
        found = true;
        break;
      }
    }
    if (!found)
      team_list.push_back((*client->classmgr)[x]->team_name.ToChar());
  }
  size_t who_am_i = 0;
  std::list<const char *>::iterator z;
  for (z = team_list.begin(); z != team_list.end(); z++) {
    if (!memcmp((*z), (*client->classmgr)[this->selected]->team_name.ToChar(), strlen((*z)) + 1)) {
      zVEC3 Pos = (*(*client->spawnpoint)[(rand() % (client->spawnpoint->GetSize() / team_list.size())) * team_list.size() + who_am_i]);
      player->trafoObjToWorld.SetTranslation(zVEC3(Pos[VX], Pos[VY], Pos[VZ]));
    } else
      who_am_i++;
  }
  team_list.clear();
};

void CSelectClass::HandleInput() {
  if (!CameraPrepared) {
    zCAICamera::GetCurrent()->bestRotY = 180.0f;
    CameraPrepared = true;
  }
  if (zinput->KeyToggled(KEY_ESCAPE)) {
    client->JoinGame(selected);
    ExitToBigMainMenu();
    delete this;
    return;
  }
  if ((this->selected > 0) && (zinput->KeyToggled(KEY_A))) {
    CleanUpBeforeNext();
    client->classmgr->EquipNPC(--this->selected, LocalPlayer, true);
    ChangeSpawnPointByClass();
  }
  if ((this->selected < client->classmgr->GetSize() - 1) && (zinput->KeyToggled(KEY_D))) {
    CleanUpBeforeNext();
    client->classmgr->EquipNPC(++this->selected, LocalPlayer, true);
    ChangeSpawnPointByClass();
  }
  if (zinput->KeyPressed(KEY_RETURN)) {
    zinput->ClearKeyBuffer();
    auto pos = player->trafoObjToWorld.GetTranslation();
    player->ResetPos(pos);
    client->JoinGame(this->selected);
    // dodac przejscie do zarzadzania gameplayem
    delete this;
  }
}
