
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

#pragma warning (disable : 4996 4800)
#include "CConfig.h"

zSTRING Multiplayer = "MULTIPLAYER";
zSTRING Engine = "ENGINE";
zSTRING Game = "GAME";
DWORD KEYBOARDLAYOUT;

CConfig::CConfig()
{
	Opt = zCOption::GetOption();
	MultiSection = zCOption::GetOption()->GetSectionByName(Multiplayer, 1);
	LoadConfigFromFile();
};

CConfig::~CConfig()
{
	SaveConfigToFile();
};

void CConfig::LoadConfigFromFile()
{
	// Sprawdzanie czy ilosc wejsc w sekcji [MULTIPLAYER] sie zgadza, jesli nie ustawienie configu na default.
	if(Opt->ReadString(Multiplayer, "Nickname").IsEmpty() || Opt->GetNumEntries(MultiSection) != 12){
		if(Opt->SectionExists(Multiplayer)) Opt->RemoveSection(Multiplayer);
		d=TRUE;
		DefaultSettings();
	}
	else{
		Nickname = Opt->ReadString(Multiplayer, "Nickname");
		skintexture = Opt->ReadInt(Multiplayer, "Skintexture");
		facetexture = Opt->ReadInt(Multiplayer, "Facetexture");
		headmodel = Opt->ReadInt(Multiplayer, "Headmodel");
		walkstyle = Opt->ReadInt(Multiplayer, "Walkstyle");
		lang = Opt->ReadInt(Multiplayer, "Lang");
		logchat = Opt->ReadBool(Multiplayer, "Logchat");
		watch = Opt->ReadBool(Multiplayer, "Watch");
		WatchPosX = Opt->ReadInt(Multiplayer, "WatchPosX");
		WatchPosY = Opt->ReadInt(Multiplayer, "WatchPosY");
		ChatLines = Opt->ReadInt(Multiplayer, "ChatLines");
		keyboardlayout = Opt->ReadInt(Multiplayer, "KeyboardLayout");
		antialiasing = Opt->ReadBool(Engine, "zVidEnableAntiAliasing");
		joystick = Opt->ReadBool(Game, "enableJoystick");
		potionkeys = Opt->ReadBool(Game, "usePotionKeys");
		logovideos = Opt->ReadBool(Game, "playLogoVideos");
		d=FALSE;
	}
};

void CConfig::DefaultSettings()
{
	Nickname.Clear();
	skintexture = 9;
	facetexture = 18;
	headmodel = 3;
	walkstyle = 0;
	// 0 - polski, 1 - angielski
	lang = 0;
	logchat = false;
	watch = false;
	logovideos = true;
	antialiasing = false;
	joystick = false;
	potionkeys = false;
	keyboardlayout = 0;
	WatchPosX = 7000;
	WatchPosY = 2500;
	ChatLines = 6;
	KEYBOARDLAYOUT = 0x00000409;
};

void CConfig::SaveConfigToFile()
{
	// [MULTIPLAYER] Ini Section
	Opt->WriteString(Multiplayer, "Nickname", Nickname);
	Opt->WriteInt(Multiplayer, "Skintexture", skintexture);
	Opt->WriteInt(Multiplayer, "Facetexture", facetexture);
	Opt->WriteInt(Multiplayer, "Headmodel", headmodel);
	Opt->WriteInt(Multiplayer, "Walkstyle", walkstyle);
	Opt->WriteInt(Multiplayer, "Lang", lang);
	Opt->WriteBool(Multiplayer, "Logchat", logchat);
	Opt->WriteBool(Multiplayer, "Watch", watch);
	Opt->WriteInt(Multiplayer, "WatchPosX", WatchPosX);
	Opt->WriteInt(Multiplayer, "WatchPosY", WatchPosY);
	Opt->WriteInt(Multiplayer, "ChatLines", ChatLines);
	Opt->WriteInt(Multiplayer, "KeyboardLayout", keyboardlayout);
	// Other Sections
	Opt->WriteBool(Engine, "zVidEnableAntiAliasing", antialiasing);
	Opt->WriteBool(Game, "enableJoystick", joystick);
	Opt->WriteBool(Game, "usePotionKeys", potionkeys);
	Opt->WriteBool(Game, "playLogoVideos", logovideos);
	// Apply changes
	CGameManager::GetGameManager()->ApplySomeSettings();
}

BOOL CConfig::IsDefault(){ return (BOOL)d; }