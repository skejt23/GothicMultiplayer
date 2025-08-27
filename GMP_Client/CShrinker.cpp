
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
**	File name:		CGmpClient/CShrinker.cpp		   						**
**																			**
**	Created by:		01/12/11	-	skejt23									**
**																			**
**	Description:	Effect for "Shrink" spell	 							**
**																			**
*****************************************************************************/

#include "CShrinker.h"

// Stage Sizes 
zVEC3 OrginalSize = zVEC3(1.0f, 1.0f, 1.0f);
zVEC3 Size90 = zVEC3(0.90f, 0.90f, 0.90f);
zVEC3 Size80 = zVEC3(0.80f, 0.80f, 0.80f);
zVEC3 Size70 = zVEC3(0.70f, 0.70f, 0.70f);
zVEC3 Size60 = zVEC3(0.60f, 0.60f, 0.60f);
zVEC3 Size50 = zVEC3(0.50f, 0.50f, 0.50f);
zVEC3 Size40 = zVEC3(0.40f, 0.40f, 0.40f);
zVEC3 Size30 = zVEC3(0.30f, 0.30f, 0.30f);
// Shrink Spell 
constexpr const char* OutEffect = "MFX_SHRINK_TARGET";

CShrinker::~CShrinker()
{
	for(int i = 0; i < (int)ShrinkedNpcs.size(); i++)
	{
		if(ShrinkedNpcs[i].Npc) ShrinkedNpcs[i].Npc->SetModelScale(OrginalSize);
	
	}
	ShrinkedNpcs.clear();
};

bool CShrinker::IsShrinked(oCNpc* Npc)
{
	for(int i = 0; i < (int)ShrinkedNpcs.size(); i++)
	{
		if(ShrinkedNpcs[i].Npc == Npc) return true;
	
	}
	return false;
};


void CShrinker::UnShrinkAll(void)
{
	for(int i = 0; i < (int)ShrinkedNpcs.size(); i++)
	{
		if(ShrinkedNpcs[i].Npc) ShrinkedNpcs[i].Npc->SetModelScale(OrginalSize);
	
	}
	ShrinkedNpcs.clear();
};

void CShrinker::UnShrinkNpc(oCNpc* Target)
{
	for(int i = 0; i < (int)ShrinkedNpcs.size(); i++)
	{
		if(ShrinkedNpcs[i].Npc == Target){
			ShrinkedNpcs[i].Npc->SetModelScale(OrginalSize);
			ShrinkedNpcs.erase(ShrinkedNpcs.begin()+i);
		}
	
	}
};

void CShrinker::ShrinkNpc(oCNpc* Target)
{
	if(Target){
		Shrink DoShrink;
		DoShrink.Npc = Target;
		DoShrink.Stage = FULLSIZE;
		DoShrink.TimeBetweenStages = 0;
		ShrinkedNpcs.push_back(DoShrink);
	}
};

int CShrinker::GetShrinkedNpcsNumber(void)
{
	return ShrinkedNpcs.size();
};

void CShrinker::Loop(void)
{
	if(ShrinkedNpcs.size() < 1) return;
	for(int i = 0; i < (int)ShrinkedNpcs.size(); i++)
	{
		if(ShrinkedNpcs[i].Npc){
			if(ShrinkedNpcs[i].TimeBetweenStages < clock()){
				switch (ShrinkedNpcs[i].Stage)
				{
					case FULLSIZE:
						ShrinkedNpcs[i].Npc->SetModelScale(Size90);
						ShrinkedNpcs[i].Stage = SIZE90;
						ShrinkedNpcs[i].TimeBetweenStages = clock() + 1000;
					break;
					case SIZE90:
						ShrinkedNpcs[i].Npc->SetModelScale(Size80);
						ShrinkedNpcs[i].Stage = SIZE80;
						ShrinkedNpcs[i].TimeBetweenStages = clock() + 1000;
					break;
					case SIZE80:
						ShrinkedNpcs[i].Npc->SetModelScale(Size70);
						ShrinkedNpcs[i].Stage = SIZE70;
						ShrinkedNpcs[i].TimeBetweenStages = clock() + 1000;
					break;
					case SIZE70:
						ShrinkedNpcs[i].Npc->SetModelScale(Size60);
						ShrinkedNpcs[i].Stage = SIZE60;
						ShrinkedNpcs[i].TimeBetweenStages = clock() + 1000;
					break;
					case SIZE60:
						ShrinkedNpcs[i].Npc->SetModelScale(Size50);
						ShrinkedNpcs[i].Stage = SIZE50;
						ShrinkedNpcs[i].TimeBetweenStages = clock() + 1000;
					break;
					case SIZE50:
						ShrinkedNpcs[i].Npc->SetModelScale(Size40);
						ShrinkedNpcs[i].Stage = SIZE40;
						ShrinkedNpcs[i].TimeBetweenStages = clock() + 1000;
					break;
					case SIZE40:
						ShrinkedNpcs[i].Npc->SetModelScale(Size30);
						ShrinkedNpcs[i].Stage = SIZE30;
						ShrinkedNpcs[i].TimeBetweenStages = clock() + 60000;
					break;
					case SIZE30:
					{
						ShrinkedNpcs[i].Npc->SetModelScale(OrginalSize);
						oCVisualFX* Out = oCVisualFX::_CreateNewInstance();
						zCParticleFX* ParticleVisual =  zCParticleFX::Load(OutEffect);
						Out->SetVisual(ParticleVisual);
						Out->SetPositionWorld(ShrinkedNpcs[i].Npc->GetPositionWorld());
						oCGame::GetGame()->GetWorld()->AddVob(Out);
						ShrinkedNpcs[i].Stage = FULLSIZE;
						ShrinkedNpcs.erase(ShrinkedNpcs.begin()+i);
					}
					break;
				};
			}
		}
		else{
			ShrinkedNpcs.erase(ShrinkedNpcs.begin()+i);
		}
	}
};