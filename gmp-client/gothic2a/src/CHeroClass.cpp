
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

#include "CHeroClass.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <cstring>
#include <pugixml.hpp>

SHeroClass::~SHeroClass(void) {
  if (!this->class_name.IsEmpty())
    this->class_name.Clear();
  if (!this->team_name.IsEmpty())
    this->team_name.Clear();
  for (size_t i = 0; i < this->items.size(); i++) delete items[i];

  items.clear();
}

SHeroClass *CHeroClass::operator[](unsigned long i) {
  return (i < data.size()) ? data[i] : (SHeroClass *)NULL;
}
CHeroClass::CHeroClass(const char *szData, BYTE size) {
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_string(szData);
  if (!result) {
    SPDLOG_ERROR("Failed while parsing classes.");
    return;
  }

  size_t num_of_classes = std::distance(doc.child("character").children("class").begin(), doc.child("character").children("class").end());

  for (auto classNode : doc.child("character").children("class")) {
    SHeroClass *tmp = new SHeroClass;
    tmp->armor.count = 0;
    tmp->prim_wep.count = 0;
    tmp->sec_wep.count = 0;
    tmp->Type = CPlayer::NPC_HUMAN;
    tmp->mp = 0;
    tmp->hp = 100;
    tmp->strength = 10;
    tmp->dexterity = 10;
    tmp->class_description = "No description";
    for (short z = 0; z < SHeroClass::AB_MAX; z++) tmp->skill[z] = 0;

    tmp->class_name = classNode.child("name").text().as_string("");
    tmp->team_name = classNode.child("team").text().as_string("");
    tmp->class_description = classNode.child("description").text().as_string("");
    tmp->Type = (CPlayer::NpcType)classNode.child("npctype").text().as_int();
    tmp->strength = classNode.child("strength").text().as_int();
    tmp->dexterity = classNode.child("dexterity").text().as_int();
    tmp->mp = classNode.child("mana").text().as_int();
    tmp->hp = classNode.child("health").text().as_int();

    tmp->skill[SHeroClass::AB_1HWEP] = classNode.child("onehweapon").text().as_int();
    tmp->skill[SHeroClass::AB_2HWEP] = classNode.child("twohweapon").text().as_int();
    tmp->skill[SHeroClass::AB_BOW] = classNode.child("bow").text().as_int();
    tmp->skill[SHeroClass::AB_XBOW] = classNode.child("crossbow").text().as_int();
    tmp->skill[SHeroClass::AB_MAGIC_LVL] = classNode.child("magic_lvl").text().as_int();
    tmp->skill[SHeroClass::AB_SNEAK] = classNode.child("sneaking").text().as_int();
    tmp->skill[SHeroClass::AB_LOCKPICK] = classNode.child("lockpicking").text().as_int();
    tmp->skill[SHeroClass::AB_ACROBATICS] = classNode.child("acrobatics").text().as_int();
    tmp->skill[SHeroClass::AB_PICKPOCKETS] = classNode.child("pickpocket").text().as_int();

    tmp->armor.index = zCParser::GetParser()->GetIndex(classNode.child("armor").text().as_string(""));
    tmp->armor.count = 1;

    tmp->prim_wep.index = zCParser::GetParser()->GetIndex(classNode.child("prim_wep").text().as_string(""));
    tmp->prim_wep.count = 1;

    tmp->sec_wep.index = zCParser::GetParser()->GetIndex(classNode.child("sec_wep").text().as_string(""));
    tmp->sec_wep.count = 1;

    for (auto itemNode : classNode.child("items").children()) {
      SItem *tmp_it = new SItem;
      tmp_it->index = zCParser::GetParser()->GetIndex(itemNode.attribute("code").as_string(""));
      tmp_it->count = itemNode.attribute("count").as_int(1);
      tmp->items.push_back(tmp_it);
    }

    this->data.push_back(tmp);
  }
}

CHeroClass::~CHeroClass() {
  for (size_t i = 0; i < data.size(); i++) delete data[i];
  data.clear();
}

DWORD CHeroClass::GetSize() {
  return this->data.size();
}

enum {
  ONEHAND_WEAPON_SKILL = 1,
  TWOHAND_WEAPON_SKILL,
  BOW_SKILL,
  XBOW_SKILL,
  PICKLOCK_SKILL,
  MAGIC_LEVEL = 7,
  SNEAK_SKILL = 8,
  ACROBATIC_SKILL = 11,
  PICKPOCKET_SKILL = 12,
  SMITH_SKILL = 13,
  RUNE_SKILL = 14,
  ALCHEMY_SKILL = 15,
  TROPHY_SKILL = 16,
};

void CHeroClass::EquipNPC(size_t offset, CPlayer *Player, bool clear_inventory) {
  if (!Player)
    return;
  if (!Player->npc)
    return;
  Player->SetNpcType(data[offset]->Type);
  oCNpc *npc = Player->npc;
  if (clear_inventory) {
    oCItem *ptr = NULL;
    ptr = npc->GetEquippedArmor();
    if (ptr)
      npc->UnequipItem(ptr);
    ptr = npc->GetEquippedMeleeWeapon();
    if (ptr)
      npc->UnequipItem(ptr);
    ptr = npc->GetEquippedRangedWeapon();
    if (ptr)
      npc->UnequipItem(ptr);
    npc->inventory2.ClearInventory();
  }
  if (offset >= this->data.size())
    return;
  npc->attribute[NPC_ATR_STRENGTH] = data[offset]->strength;
  npc->attribute[NPC_ATR_DEXTERITY] = data[offset]->dexterity;
  npc->attribute[NPC_ATR_HITPOINTSMAX] = data[offset]->hp;
  npc->attribute[NPC_ATR_HITPOINTS] = data[offset]->hp;
  npc->attribute[NPC_ATR_MANA] = data[offset]->mp;
  npc->attribute[NPC_ATR_MANAMAX] = data[offset]->mp;
  npc->SetTalentSkill(ONEHAND_WEAPON_SKILL, data[offset]->skill[SHeroClass::AB_1HWEP] / 30);
  npc->SetTalentValue(ONEHAND_WEAPON_SKILL, data[offset]->skill[SHeroClass::AB_1HWEP] / 30);
  npc->hitChance[1] = data[offset]->skill[SHeroClass::AB_1HWEP];
  npc->SetTalentSkill(TWOHAND_WEAPON_SKILL, data[offset]->skill[SHeroClass::AB_2HWEP] / 30);
  npc->SetTalentValue(TWOHAND_WEAPON_SKILL, data[offset]->skill[SHeroClass::AB_2HWEP] / 30);
  npc->hitChance[2] = data[offset]->skill[SHeroClass::AB_2HWEP];
  npc->SetTalentSkill(BOW_SKILL, data[offset]->skill[SHeroClass::AB_BOW] / 30);
  npc->SetTalentValue(BOW_SKILL, data[offset]->skill[SHeroClass::AB_BOW] / 30);
  npc->hitChance[3] = data[offset]->skill[SHeroClass::AB_BOW];
  npc->SetTalentSkill(XBOW_SKILL, data[offset]->skill[SHeroClass::AB_XBOW] / 30);
  npc->SetTalentValue(XBOW_SKILL, data[offset]->skill[SHeroClass::AB_XBOW] / 30);
  npc->hitChance[4] = data[offset]->skill[SHeroClass::AB_XBOW];
  npc->SetTalentSkill(MAGIC_LEVEL, data[offset]->skill[SHeroClass::AB_MAGIC_LVL]);
  npc->SetTalentValue(MAGIC_LEVEL, data[offset]->skill[SHeroClass::AB_MAGIC_LVL]);
  npc->SetTalentSkill(SNEAK_SKILL, data[offset]->skill[SHeroClass::AB_SNEAK]);
  npc->SetTalentValue(SNEAK_SKILL, data[offset]->skill[SHeroClass::AB_SNEAK]);
  npc->SetTalentSkill(ACROBATIC_SKILL, data[offset]->skill[SHeroClass::AB_ACROBATICS]);
  npc->SetTalentValue(ACROBATIC_SKILL, data[offset]->skill[SHeroClass::AB_ACROBATICS]);
  npc->SetTalentSkill(PICKLOCK_SKILL, data[offset]->skill[SHeroClass::AB_LOCKPICK]);
  npc->SetTalentValue(PICKLOCK_SKILL, data[offset]->skill[SHeroClass::AB_LOCKPICK]);
  if (npc == player) {
    if (data[offset]->armor.index > 0)
      npc->Equip(npc->inventory2.Insert(zfactory->CreateItem(data[offset]->armor.index)));
  }
  if (npc == player) {
    if (data[offset]->sec_wep.index > 0)
      npc->EquipWeapon(npc->inventory2.Insert(zfactory->CreateItem(data[offset]->sec_wep.index)));
    if (data[offset]->prim_wep.index > 0)
      npc->EquipWeapon(npc->inventory2.Insert(zfactory->CreateItem(data[offset]->prim_wep.index)));
  } else {
    if (data[offset]->sec_wep.index > 0)
      npc->inventory2.Insert(zfactory->CreateItem(data[offset]->sec_wep.index));
    if (data[offset]->prim_wep.index > 0)
      npc->inventory2.Insert(zfactory->CreateItem(data[offset]->prim_wep.index));
  }
  printf("Number of items: %u\n", data[offset]->items.size());
  npc->DestroySpellBook();
  npc->MakeSpellBook();
  for (size_t i = 0; i < data[offset]->items.size(); i++) {
    oCItem *Item = zfactory->CreateItem(data[offset]->items[i]->index);
    if (Item) {
      Item->amount = data[offset]->items[i]->count;
      npc->inventory2.Insert(Item);
      if (npc == player) {
        if (Item->HasFlag(512)) {
          // Check for summon spells
          if (Item->GetInstanceName().Search("TRF") < 2) {
            if (npc->CanUse(Item))
              npc->Equip(Item);
          } else {
            npc->inventory2.Remove(Item);
            Item->RemoveVobFromWorld();
          }
        }
      }
    }
  }
}