
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
#include "CIngame.h"

// Externs
extern CIngame* global_ingame;

// Enums
enum SpellsId
{
	SPL_SLEEP = 27, // Sen
	SPL_SHRINK = 46, // Zmniejszanie
	SPL_TRFLURKER = 56 // Przemiana w topielca
};

// -- zSTRINGs --
zSTRING SpellLogicTemporary;
zSTRING SpellCastTemp;
zSTRING SPRINT = "HUMANS_SPRINT.MDS";
  // Dzwieki wyciagania broni
zSTRING DrawSoundMe = "DRAWSOUND_ME.WAV";
zSTRING DrawSoundMe02 = "DRAWSOUND_ME_02.WAV";
zSTRING UnDrawSoundMe = "UNDRAWSOUND_ME.WAV";
zSTRING DrawSoundWo = "DRAWSOUND_WO.WAV";
zSTRING UnDrawSoundWo = "UNDRAWSOUND_WO.WAV";
 // Instancje NPC w skryptach
zSTRING SELF = "SELF";
zSTRING OTHER = "OTHER";

// Functions

void CSyncFuncs::RunSpellLogic(short SpellId, oCNpc* Caster, oCNpc* Target)
{
	switch (SpellId)
	{
		case SPL_SLEEP:
			if(!Target) return;
			zCParser::GetParser()->SetInstance(SELF, Caster);
			zCParser::GetParser()->SetInstance(OTHER, Target);
			SpellLogicTemporary = "SPELL_LOGIC_SLEEP";
			zCParser::GetParser()->CallFunc(SpellLogicTemporary);
		break;
		case SPL_SHRINK:
		{
			if(!Target) return;
			if(!global_ingame->Shrinker->IsShrinked(Target)) global_ingame->Shrinker->ShrinkNpc(Target);
		}
		break;
		default:
			// Unsupported SpellId
		break;
	};
};

// Urchamianie skryptu danego spella, np SPELL_CAST_FIREBALL
void CSyncFuncs::RunSpellScript(const char* SpellName, oCNpc* Caster, oCNpc* Target)
{
	if(!Caster || !SpellName) return;
	char buffer[128];
	zCParser::GetParser()->SetInstance(SELF, Caster);
	if(!Target) zCParser::GetParser()->SetInstance(OTHER, 0);
	else zCParser::GetParser()->SetInstance(OTHER, Target);
	sprintf(buffer, "SPELL_CAST_%s", SpellName);
	SpellCastTemp = buffer;
	SpellCastTemp.Upper();
	if(memcmp(buffer,"SPELL_CAST_TRANSFORM",19) != 0) zCParser::GetParser()->CallFunc(SpellCastTemp);
};

// Sprawdzanie czy wielkosc pozycji nie jest wieksza niz dopuszcza radius, risen chcial to koniecznie
bool CSyncFuncs::GetVectorSurfaceSphere(float radius, float bX, float bY, float bZ, float rX, float rY, float rZ)
{
   float vector[3];
   vector[0] = rX - bX;
   vector[1] = rY - bY;
   vector[2] = rZ - bZ;
   if((vector[0] < radius && vector[0] > -radius) && (vector[1] < radius && vector[1] > -radius) && (vector[2] < radius && vector[2] > -radius)){
      return true;
   }
   else return false;
}

// Sprawdzanie czy uzywany przedmiot daje jakies specjalne efekty
void CSyncFuncs::CheckForSpecialEffects(oCItem* Item, oCNpc* Npc)
{
	if(Item->GetInstance() == 6126) Npc->ApplyTimedOverlayMds(SPRINT, 120000);
	if(Item->GetInstance() == 7017) Npc->ApplyTimedOverlayMds(SPRINT, 15000);
	if(Item->GetInstance() == 7079) Npc->ApplyTimedOverlayMds(SPRINT, 300000);
};

// Odgrywanie dzwieku przy wyciaganiu broni
void CSyncFuncs::PlayDrawSound(oCItem* Item, oCNpc* Npc, bool Draw)
{
	if(oCItem::GetCategory(Item) != 1 || Item->mainflags != 2) return;
	zCSoundSystem* Sound = zCSoundSystem::GetSoundSystem();
	int RandomDraw = 0;
	switch (Item->GetSoundMaterial())
	{
		case MAT_WOOD:
			if(Draw) Sound->PlaySound3D(DrawSoundWo, Npc, 2);
			else Sound->PlaySound3D(UnDrawSoundWo, Npc, 2);
		break;
		case MAT_METAL:
			if(Draw){
				RandomDraw = rand() % 2 + 1;
				if(RandomDraw == 1) Sound->PlaySound3D(DrawSoundMe, Npc, 2);
				else Sound->PlaySound3D(DrawSoundMe02, Npc, 2);
			}
			else Sound->PlaySound3D(UnDrawSoundMe, Npc, 2);
		break;
		default:
			// Do nothing
		break;
	};
};

