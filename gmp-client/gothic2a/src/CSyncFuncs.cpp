
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
**																			**
**	File name:		CGmpClient/CSyncFuncs.cpp		   						**
**																			**
**	Created by:		01/12/11	-	skejt23									**
**																			**
**	Description:	Synchronization functions	 							**
**																			**
*****************************************************************************/

// Includes
#include "CSyncFuncs.h"

#include <cstdio>
#include <cstring>

#include "CIngame.h"
#include "random_utils.h"
#include "script_instance_scope.h"

// Externs
extern CIngame* global_ingame;

constexpr const char* SPRINT = "HUMANS_SPRINT.MDS";
// Dzwieki wyciagania broni
constexpr const char* DrawSoundMe = "DRAWSOUND_ME.WAV";
constexpr const char* DrawSoundMe02 = "DRAWSOUND_ME_02.WAV";
constexpr const char* UnDrawSoundMe = "UNDRAWSOUND_ME.WAV";
constexpr const char* DrawSoundWo = "DRAWSOUND_WO.WAV";
constexpr const char* UnDrawSoundWo = "UNDRAWSOUND_WO.WAV";

void CSyncFuncs::RunSpellLogic(short SpellId, oCNpc* Caster, oCNpc* Target) {
  switch (SpellId) {
    case SPL_SLEEP:
      if (!Caster || !Target)
        return;
      if (auto* parser = Gothic_II_Addon::parser) {
        ScriptInstanceScope scope(*parser);
        parser->SetInstance("SELF", Caster);
        parser->SetInstance("OTHER", Target);
        parser->CallFunc(zSTRING("SPELL_LOGIC_SLEEP"));
      }
      break;
    case SPL_SHRINK: {
      if (!Target || !global_ingame || !global_ingame->Shrinker)
        return;
      if (!global_ingame->Shrinker->IsShrinked(Target))
        global_ingame->Shrinker->ShrinkNpc(Target);
    } break;
    default:
      // Unsupported SpellId
      break;
  };
};

// Urchamianie skryptu danego spella, np SPELL_CAST_FIREBALL
void CSyncFuncs::RunSpellScript(const char* SpellName, oCNpc* Caster, oCNpc* Target) {
  if (!Caster || !SpellName)
    return;
  zCParser* parser = Gothic_II_Addon::parser;
  if (!parser) {
    return;
  }

  char buffer[128];
  ScriptInstanceScope scope(*parser);
  parser->SetInstance("SELF", Caster);
  if (!Target)
    parser->SetInstance("OTHER", 0);
  else
    parser->SetInstance("OTHER", Target);
  std::snprintf(buffer, sizeof(buffer), "SPELL_CAST_%s", SpellName);
  zSTRING spellCastTemp = buffer;
  spellCastTemp.Upper();
  if (std::strncmp(buffer, "SPELL_CAST_TRANSFORM", 19) != 0)
    parser->CallFunc(spellCastTemp);
};

// Sprawdzanie czy wielkosc pozycji nie jest wieksza niz dopuszcza radius, risen chcial to koniecznie
bool CSyncFuncs::GetVectorSurfaceSphere(float radius, float bX, float bY, float bZ, float rX, float rY, float rZ) {
  float vector[3];
  vector[0] = rX - bX;
  vector[1] = rY - bY;
  vector[2] = rZ - bZ;
  if ((vector[0] < radius && vector[0] > -radius) && (vector[1] < radius && vector[1] > -radius) && (vector[2] < radius && vector[2] > -radius)) {
    return true;
  } else
    return false;
}

// Sprawdzanie czy uzywany przedmiot daje jakies specjalne efekty
void CSyncFuncs::CheckForSpecialEffects(oCItem* Item, oCNpc* Npc) {
  if (!Item || !Npc) {
    return;
  }

  // Holding an item is not consumption. Timed effects are delivered as
  // authoritative effect actions; parser indexes have no semantic meaning.
};

// Odgrywanie dzwieku przy wyciaganiu broni
void CSyncFuncs::PlayDrawSound(oCItem* Item, oCNpc* Npc, bool Draw) {
  if (!Item || !Npc || !zsound) {
    return;
  }

  if (oCItem::GetCategory(Item) != 1 || Item->mainflag != 2) {
    return;
  }
  
  int RandomDraw = 0;
  switch (Item->GetSoundMaterial()) {
    case SND_MAT_WOOD:
      if (Draw)
        zsound->PlaySound3D(DrawSoundWo, Npc, 2, nullptr);
      else
        zsound->PlaySound3D(UnDrawSoundWo, Npc, 2, nullptr);
      break;
    case SND_MAT_METAL:
      if (Draw) {
        RandomDraw = gmp::client::random::Int(1, 2);
        if (RandomDraw == 1)
          zsound->PlaySound3D(DrawSoundMe, Npc, 2, nullptr);
        else
          zsound->PlaySound3D(DrawSoundMe02, Npc, 2, nullptr);
      } else
        zsound->PlaySound3D(UnDrawSoundMe, Npc, 2, nullptr);
      break;
    default:
      // Do nothing
      break;
  };
};
