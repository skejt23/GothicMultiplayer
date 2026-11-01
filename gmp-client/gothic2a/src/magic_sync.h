#pragma once

#include <algorithm>

#include "ZenGin/zGothicAPI.h"

// GetActiveSpellNr is a spell ID, not a spellbook index or hotkey.
inline oCSpell* GetSelectedMagicSpell(oCNpc* npc) {
  auto* book = npc ? npc->GetSpellBook() : nullptr;
  return book && book->spellnr >= 0 && book->spellnr < book->GetNoOfSpells() ? book->GetSelectedSpell() : nullptr;
}

inline void SelectMagicSpellForPresentation(oCNpc* npc, int index) {
  auto* book = npc ? npc->GetSpellBook() : nullptr;
  if (!book || index < 0 || index >= book->GetNoOfSpells() || index == book->spellnr)
    return;
  if (GetSelectedMagicSpell(npc)) {
    book->SetFrontSpell(index);
  } else {
    // SetFrontSpell stops and removes spells[spellnr] without checking the
    // previous index. A new book, or removal of its last selection, has none.
    book->spellnr = index;
    book->in_movement = FALSE;
    book->action = 0;
  }
}

inline void ClearNativeLightSpells(oCNpc* npc) {
  if (!npc)
    return;
  // The local engine retains cast lights in activeSpells, independently of
  // the selected rune and GMPC's remote presentation cache. Remove only lights;
  // ending every active spell here could swap a transformed global hero.
  for (auto* node = npc->activeSpells.GetNextInList(); node;) {
    auto* spell = node->GetData();
    node = node->GetNextInList();
    if (spell && (spell->GetSpellID() == SPL_LIGHT || spell->GetSpellID() == SPL_PALLIGHT)) {
      // Transfer its owned FX reference so the spell destructor cannot kill
      // the same light again when the active-list reference is released.
      auto* effect = spell->effect;
      spell->effect = nullptr;
      if (effect) {
        effect->Kill();
        effect->Release();
      }
      npc->RemoveActiveSpell(spell);
    }
  }
}

inline int FindMagicItem(oCMag_Book* book, oCItem* item) {
  if (book && item) {
    for (int index = 0; index < book->GetNoOfSpells(); ++index) {
      if (book->GetSpellItem(index) == item)
        return index;
    }
  }
  return -1;
}

inline void UnregisterMagicItem(oCNpc* npc, oCItem* item) {
  auto* book = npc ? npc->GetSpellBook() : nullptr;
  const int index = FindMagicItem(book, item);
  if (index < 0)
    return;

  // DeRegister(item) probes slot zero even in an empty book. Repeated cleanup
  // can then release a stale spell pointer (refCtr == -1). Use a checked index
  // and repair the selected index after native removal shifts the arrays.
  const int selected = book->spellnr;
  if (selected == index)
    book->Close(1);
  book->DeRegister(index);
  const int count = book->GetNoOfSpells();
  book->spellnr = count == 0 ? -1 : std::clamp(selected - (index < selected ? 1 : 0), 0, count - 1);
}

inline void ConfigureMagicAnimations(oCNpc* npc, int spell_id) {
  auto* controller = npc ? npc->GetAnictrl() : nullptr;
  auto* parser = Gothic_II_Addon::parser;
  auto* symbol = parser ? parser->GetSymbol("spellFXAniLetters") : nullptr;
  if (!controller || !symbol || spell_id < 0 || spell_id >= SPL_MAX_SPELL)
    return;
  zSTRING prefix;
  symbol->GetValue(prefix, spell_id);
  controller->SetAnimations(NPC_WEAPON_MAG, prefix);
}

inline bool IsMagicActionAnimation(oCNpc* npc, int animation_id) {
  auto* controller = npc ? npc->GetAnictrl() : nullptr;
  if (!controller || npc->GetWeaponMode() != NPC_WEAPON_MAG || animation_id < 0)
    return false;
  return animation_id == controller->t_stand_2_cast || animation_id == controller->s_cast || animation_id == controller->t_cast_2_shoot ||
         animation_id == controller->t_cast_2_stand || animation_id == controller->s_shoot || animation_id == controller->t_shoot_2_stand;
}

inline bool IsPlayingMagicAction(oCNpc* npc) {
  auto* model = npc ? npc->GetModel() : nullptr;
  if (!model)
    return false;
  for (int channel = 0; channel < std::clamp(model->numActiveAnis, 0, zMDL_MAX_ANIS_PARALLEL); ++channel) {
    auto* active = model->aniChannels[channel];
    if (active && !active->isFadingOut && active->protoAni && IsMagicActionAnimation(npc, active->protoAni->GetAniID()))
      return true;
  }
  return false;
}
