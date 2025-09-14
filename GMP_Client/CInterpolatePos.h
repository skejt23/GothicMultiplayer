
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
**	File name:		CGmpClient/CInterpolatePos.h		   					**
**																			**
**	Created by:		17/12/11	-	skejt23									**
**																			**
**	Description:	Position interpolation	(at least pretends to be :-)) 	**
**																			**
*****************************************************************************/
#pragma once

#include "ZenGin/zGothicAPI.h"
#include "CPlayer.h"

// U nas animacja służy za przewidywanie pozycji gracza, więc mamy tu takie gładkie przesuwanie gracza w kierunku prawdziwej pozycji(przesyłanej przez server). 
// Ewentualnie inne smieci dodamy w przyszłości.
class CInterpolatePos {
private:
  CPlayer* InterpolatingPlayer;
  zVEC3 InterpolatingTo;
  int InterCount;

public:
  bool IsInterpolating;

private:
  void Interpolate(float x, float y, float z, float value, bool NoCollideMode = false);

public:
  CInterpolatePos(CPlayer* Player);
  ~CInterpolatePos();
  void DoInterpolate();
  bool IsDistanceSmallerThanRadius(float radius, float bX, float bY, float bZ, float rX, float rY, float rZ);
  bool IsDistanceSmallerThanRadius(float radius, const zVEC3& Pos, const zVEC3& Pos1);
  void UpdateInterpolation(float x, float y, float z);
};