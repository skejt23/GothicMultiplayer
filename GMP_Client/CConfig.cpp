
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

#pragma warning(disable : 4996 4800)
#include "CConfig.h"

using namespace Gothic_II_Addon;

CConfig::CConfig() {
  MultiSection = zoptions->GetSectionByName("MULTIPLAYER", 1);
  LoadConfigFromFile();
};

CConfig::~CConfig() {
  SaveConfigToFile();
};

void CConfig::LoadConfigFromFile() {
  zSTRING Multiplayer = "MULTIPLAYER";
  zSTRING Engine = "ENGINE";
  zSTRING Game = "GAME";

  // Sprawdzanie czy ilosc wejsc w sekcji [MULTIPLAYER] sie zgadza, jesli nie ustawienie configu na default.
  if (zoptions->ReadString(Multiplayer, "Nickname").IsEmpty() || zoptions->GetNumEntries(MultiSection) != 12) {
    if (zoptions->SectionExists(Multiplayer))
      zoptions->RemoveSection(Multiplayer);
    d = TRUE;
    DefaultSettings();
  } else {
    Nickname = zoptions->ReadString(Multiplayer, "Nickname");
    skintexture = zoptions->ReadInt(Multiplayer, "Skintexture");
    facetexture = zoptions->ReadInt(Multiplayer, "Facetexture");
    headmodel = zoptions->ReadInt(Multiplayer, "Headmodel");
    walkstyle = zoptions->ReadInt(Multiplayer, "Walkstyle");
    lang = zoptions->ReadInt(Multiplayer, "Lang");
    logchat = zoptions->ReadBool(Multiplayer, "Logchat");
    watch = zoptions->ReadBool(Multiplayer, "Watch");
    WatchPosX = zoptions->ReadInt(Multiplayer, "WatchPosX");
    WatchPosY = zoptions->ReadInt(Multiplayer, "WatchPosY");
    ChatLines = zoptions->ReadInt(Multiplayer, "ChatLines");
    keyboardlayout = zoptions->ReadInt(Multiplayer, "KeyboardLayout");
    antialiasing = zoptions->ReadBool(Engine, "zVidEnableAntiAliasing");
    joystick = zoptions->ReadBool(Game, "enableJoystick");
    potionkeys = zoptions->ReadBool(Game, "usePotionKeys");
    logovideos = zoptions->ReadBool(Game, "playLogoVideos");
    d = FALSE;
  }
};

void CConfig::DefaultSettings() {
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
};

void CConfig::SaveConfigToFile() {
  zSTRING Multiplayer = "MULTIPLAYER";
  zSTRING Engine = "ENGINE";
  zSTRING Game = "GAME";

  // [MULTIPLAYER] Ini Section
  zoptions->WriteString(Multiplayer, "Nickname", Nickname);
  zoptions->WriteInt(Multiplayer, "Skintexture", skintexture);
  zoptions->WriteInt(Multiplayer, "Facetexture", facetexture);
  zoptions->WriteInt(Multiplayer, "Headmodel", headmodel);
  zoptions->WriteInt(Multiplayer, "Walkstyle", walkstyle);
  zoptions->WriteInt(Multiplayer, "Lang", lang);
  zoptions->WriteBool(Multiplayer, "Logchat", logchat);
  zoptions->WriteBool(Multiplayer, "Watch", watch);
  zoptions->WriteInt(Multiplayer, "WatchPosX", WatchPosX);
  zoptions->WriteInt(Multiplayer, "WatchPosY", WatchPosY);
  zoptions->WriteInt(Multiplayer, "ChatLines", ChatLines);
  zoptions->WriteInt(Multiplayer, "KeyboardLayout", keyboardlayout);
  // Other Sections
  zoptions->WriteBool(Engine, "zVidEnableAntiAliasing", antialiasing);
  zoptions->WriteBool(Game, "enableJoystick", joystick);
  zoptions->WriteBool(Game, "usePotionKeys", potionkeys);
  zoptions->WriteBool(Game, "playLogoVideos", logovideos);
  // Apply changes
  gameMan->ApplySomeSettings();
}

bool CConfig::IsDefault() {
  return d;
}