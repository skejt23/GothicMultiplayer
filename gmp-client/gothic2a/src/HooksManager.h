
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

#pragma once
#include "common.h"
#include "singleton.h"
#include "hooking/MemoryPatch.h"
#include <set>
#include <vector>

namespace Gothic_II_Addon {
class oCGame;
class zCAIPlayer;
class zCVob;
}

enum HOOK_TYPE
{
	HT_RENDER = 1,
	HT_CLOSELOADSCREEN,
	HT_DONE,
	HT_AIMOVING
};
class HooksManager : public TSingleton<HooksManager>
{
public:
	HooksManager(void);
	~HooksManager(void);
	void AddHook(HOOK_TYPE type, DWORD callback);
	void RemoveHook(HOOK_TYPE type, DWORD callback);
private:
	void InitAllPatches();
	typedef std::set<DWORD> HooksSet;
	std::vector<DWORD> RenderCallbacksToDelete;
	std::vector<DWORD> AiMovingCallbacksToDelete;
	HooksSet OnRenderHooks;
	HooksSet OnCloseLoadScreenHooks;
	HooksSet OnDoneHooks;
	HooksSet OnAiMovingHooks;
	CRITICAL_SECTION RenderCs, CloseLoadScreenCs, DoneCs, AiMovingCs;
private:
	static void __fastcall OnRender(Gothic_II_Addon::oCGame* gameInstance);
	static void __stdcall OnCloseLoadScreen();
	static void __stdcall OnDone();
	static void __fastcall OnAiMoving(Gothic_II_Addon::zCAIPlayer* aiPlayer, void* unusedEdx, Gothic_II_Addon::zCVob* targetVob);
};
