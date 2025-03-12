
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
**	File name:		CGmpClient/CInterpolatePos.cpp		   					**
**																			**
**	Created by:		17/12/11	-	skejt23									**
**																			**
**	Description:	Position interpolation	(at least pretends to be :-)) 								**
**																			**
*****************************************************************************/
#include "CInterpolatePos.h"
#include "CIngame.h"

// Externs
extern CIngame* global_ingame;

CInterpolatePos::CInterpolatePos(CPlayer* Player)
{
	InterpolatingPlayer = Player;
	IsInterpolating = false;
	global_ingame->Interpolation.push_back(this);
	InterCount = 0;
};

CInterpolatePos::~CInterpolatePos()
{
	IsInterpolating = false;
	InterCount = 0;
	InterpolatingPlayer = NULL;
	for(int i = 0; i < (int)global_ingame->Interpolation.size(); i++)
	{
		if(global_ingame->Interpolation[i] == this){
			global_ingame->Interpolation.erase(global_ingame->Interpolation.begin()+i);
		}
	
	}
};

void CInterpolatePos::DoInterpolate()
{
	if(!IsInterpolating) return;
	// trzeba bedzie wymyslic jakis lepszy pomysl na szybkosc interpolacji
	if(IsDistanceSmallerThanRadius(70.0f, InterpolatingPlayer->npc->GetPosition(), InterpolatingTo)) Interpolate(InterpolatingTo.x, InterpolatingTo.y, InterpolatingTo.z, 0.5f);
	else if(IsDistanceSmallerThanRadius(100.0f, InterpolatingPlayer->npc->GetPosition(), InterpolatingTo)) Interpolate(InterpolatingTo.x, InterpolatingTo.y, InterpolatingTo.z, 1);
	else if(IsDistanceSmallerThanRadius(200.0f, InterpolatingPlayer->npc->GetPosition(), InterpolatingTo)) Interpolate(InterpolatingTo.x, InterpolatingTo.y, InterpolatingTo.z, 2, true);
	else if(IsDistanceSmallerThanRadius(300.0f, InterpolatingPlayer->npc->GetPosition(), InterpolatingTo)) Interpolate(InterpolatingTo.x, InterpolatingTo.y, InterpolatingTo.z, 3, true);
	else if(IsDistanceSmallerThanRadius(400.0f, InterpolatingPlayer->npc->GetPosition(), InterpolatingTo)) Interpolate(InterpolatingTo.x, InterpolatingTo.y, InterpolatingTo.z, 4, true);
};

void CInterpolatePos::Interpolate(float x, float y, float z, float value, bool NoCollideMode)
{
	if(!IsInterpolating) return;
	zVEC3 Pos = InterpolatingPlayer->npc->GetPosition();
	float PosX = Pos.x;
	float PosY = Pos.y;
	float PosZ = Pos.z;
	if(PosX != x)
	{
		if(x > PosX) PosX+=value;
		if(x < PosX) PosX-=value;
	}
	if(PosY != y)
	{
		if(y > PosY) PosY+=value;
		if(y < PosY) PosY-=value;
	}
	if(PosZ != z)
	{
		if(z > PosZ) PosZ+=value;
		if(z < PosZ) PosZ-=value;
	}
	Pos.x = PosX;
	Pos.y = PosY;
	Pos.z = PosZ;
	if(!NoCollideMode) InterpolatingPlayer->npc->SetPositionWorld(Pos);
	else InterpolatingPlayer->SetPosition(Pos);
	if(IsDistanceSmallerThanRadius(50.0f,PosX, PosY, PosZ, x, y, z)){
		IsInterpolating = false;
		InterCount = 0;
	}
	InterCount++;
	if(InterCount > 3000){
		InterpolatingPlayer->npc->SetPosition(x, y, z);
		InterCount = 0;
		IsInterpolating = false;
	}
};

bool CInterpolatePos::IsDistanceSmallerThanRadius(float radius, float bX, float bY, float bZ, float rX, float rY, float rZ)
{
   float vector[3];
   vector[0] = rX - bX;
   vector[1] = rY - bY;
   vector[2] = rZ - bZ;
   if((vector[0] < radius && vector[0] > -radius) && (vector[1] < radius && vector[1] > -radius) && (vector[2] < radius && vector[2] > -radius)){
      return true;
   }
   else return false;
};

bool CInterpolatePos::IsDistanceSmallerThanRadius(float radius, const zVEC3& Pos, const zVEC3& Pos1)
{
   float vector[3];
   vector[0] = Pos1.x - Pos.x;
   vector[1] = Pos1.y - Pos.y;
   vector[2] = Pos1.z - Pos.z;
   if((vector[0] < radius && vector[0] > -radius) && (vector[1] < radius && vector[1] > -radius) && (vector[2] < radius && vector[2] > -radius)){
      return true;
   }
   else return false;
};

void CInterpolatePos::UpdateInterpolation(float x, float y, float z)
{
	if(!IsInterpolating) IsInterpolating = true;
	InterpolatingTo.x = x;
	InterpolatingTo.y = y;
	InterpolatingTo.z = z;
};