
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

namespace Patch
{	
	void NoStartupLogo();
	void SetWndName(const char * name);
	void SetRegProgram(const char *name);
	void InitNewSplash();
	void DisableStartupScript();
	void PlayerInterfaceEnabled(bool isit);
	void SetStatusScreenEnabled(bool isit);
	void SetOpenInventoryEnabled(bool isit);
	void SetLogScreenEnabled(bool isit);
	void SetFirstPersonCameraEnabled(bool isit);
	void DisablePause(void);
	void LaunchBarriere(void);
	void DisableAbnormalExit(void);
	void ErrorInCheckClassSizeEnabled(bool isit);
	void ChangeLevelEnabled(bool isit);
	void AlwaysNoMenu();
	void EraseCastSecurity();
	void FixApplyOverlay();
	void FixLights();
	void DisableGothicMainMenu(void);
	void DisablePlayBink(void);
	void DisableWriteSavegame(void);
	void ChangeDefaultIni();
	void DisableChangeSightKeys();
	void DropVobEnabled(bool arg);
	void CanNotUseMessageEnabled(bool isit);
	void ObjectDestructorPatched(bool arg);
	void FixSetTime();
	void DisableCheat();
	void DisableInjection();
	HWND GetHWND();
}