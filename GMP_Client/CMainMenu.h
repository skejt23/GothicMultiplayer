
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

#include "g2Api.h"
#include "CSelectClass.h"
#include "CServerList.h"
#include <vector>
#include "ExtendedServerList.h"

struct Resolution
{
	int x,y;
};
enum MenuState
{
	CHOOSE_LANGUAGE,
	CHOOSE_NICKNAME,
	MENU_LOOP,
	MENU_CLEAN,
	MENU_APPEARANCE,
	CHOOSE_SRV_LOOP,
	MENU_OPTIONS,
	MENU_OPTONLINE,
	MENU_SETWATCHPOS,
	MENU_WORLDBUILDER,
	WRITE_WORLDNAME,
	LOAD_WBMAP
};
enum PrintState{
	MAIN_MENU	= 0,
	SERVER_LIST,
	SETTINGS_MENU,
	WORLDBUILDER_MENU
};
namespace ApperancePart{
	enum{
		HEAD,
		FACE,
		SKIN,
		WALKSTYLE
	};
};

class CMainMenu : public TSingleton<CMainMenu>
{
private:
	Resolution ScreenResolution;
	oCItem* CamWeapon;
	CServerList *ServerList;
	int hbX, hbY;
	int Hour, Minute;
	char tmpbuff[32];
	zSTRING ChatLinesTMP;
	bool Christmas;
	int MusicId;
public:
	char versionbuff[32];
	zSTRING VersionString;
	zVEC3 HeroPos;
	zVEC3 Angle;
	zVEC3 NAngle;
	zCView* GMPLogo;
	std::vector<zSTRING> RssNews;
	int ps;
	zCMenu* Options;
	int SelectedServer;
	zSTRING ServerIP;
	oCItem* TitleWeapon;
	oCItem* AppWeapon;
	zSTRING WBMapName;
	short MenuItems;
	short MenuPos;
	short WBMenuPos;
	short OptionPos;
	MenuState MState;
	bool TitleWeaponEnabled;
	bool WritingNickname;
	zSTRING headmodel_tmp;
	zSTRING Walkstyle_tmp;
	CLanguage* LangSetting;
	CSelectClass *ClassSelect;
	bool AppCamCreated;
	unsigned char ChoosingApperance;
	unsigned char LastApperance;
	ExtendedServerList * esl;
public:
	CMainMenu();
	~CMainMenu();
	void RenderMenu();
	void ReLaunchMainMenu();
	void EnableHealthBar();
	void DisableHealthBar();
	void LoadLangNames(void);
	void LaunchMenuScene();
	void LoadConfig();
	void CleanUpMainMenu();
	void PrintMenu();
	void PrintNews();
	void SpeedUpTime();
	void LeaveOptionsMenu();
	void RunMenuItem();
	void RunWbMenuItem();
	void RunOptionsItem();
	void ClearNpcTalents(oCNpc* Npc);
	void EraseSpacesInNickname();
	void static __stdcall MainMenuLoop();
	void SetServerIP(int selected);
};