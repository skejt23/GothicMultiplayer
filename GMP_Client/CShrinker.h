
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
**	File name:		CGmpClient/CShrinker.h		   							**
**																			**
**	Created by:		01/12/11	-	skejt23									**
**																			**
**	Description:	Effect for "Shrink" spell	 							**
**																			**
*****************************************************************************/

#pragma once

#include <vector>
#include "ZenGin/zGothicAPI.h"
#include <ctime>

using namespace std;

enum ShrinkStage
{
	FULLSIZE,
	SIZE90,
	SIZE80,
	SIZE70,
	SIZE60,
	SIZE50,
	SIZE40,
	SIZE30
};

struct Shrink
{
	oCNpc* Npc;
	ShrinkStage Stage;
	time_t TimeBetweenStages;
};

class CShrinker
{
private:
	vector<Shrink> ShrinkedNpcs;
public:
	~CShrinker();
	bool IsShrinked(oCNpc* Npc);
	void UnShrinkAll(void);
	int GetShrinkedNpcsNumber(void);
	void ShrinkNpc(oCNpc* Target);
	void UnShrinkNpc(oCNpc* Target);
	void Loop(void);
};