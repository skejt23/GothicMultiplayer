
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
** 	File name:		Interface/CMainMenu.cpp
** 	Created by:		06/06/11	-	skejt23
** 	Description:	Multiplayer main menu functionallity
**
*****************************************************************************/

#include "CMainMenu.h"

#include <spdlog/spdlog.h>
#include <urlmon.h>

#include <fstream>
#include <nlohmann/json.hpp>

#include "CLanguage.h"
#include "CSyncFuncs.h"
#include "CWatch.h"
#include "ExtendedServerList.h"
#include "world-builder\CBuilder.h"
#include "config.h"
#include "game_client.h"
#include "interface.h"
#include "keyboard.h"
#include "localization_utils.h"
#include "mod.h"
#include "patch.h"
#include "version.h"

using namespace Gothic_II_Addon;

extern GameClient* client;
extern std::vector<zSTRING> vec_choose_lang;
extern std::vector<std::string> vec_lang_files;
extern const char* LANG_DIR;
extern float fWRatio, fHRatio;
zCOLOR Normal = zCOLOR(255, 255, 255);
zCOLOR Highlighted = zCOLOR(128, 180, 128);
zCOLOR Red = zCOLOR(0xFF, 0, 0);
extern zCOLOR Green;
zCOLOR FColor;
constexpr const char* FDefault = "FONT_DEFAULT.TGA";
constexpr const char* WalkAnim = "S_WALKL";
CLanguage* Lang;
CBuilder* Builder;
std::ifstream g2names;
std::ifstream g2particles;
constexpr const char* WRITE_MAPNAME = "Write ZEN world name ex. newworld.zen:";
constexpr const char* WRITE_SAVEDMAP = "Write saved map name:";
constexpr const char* MAPFILE_EMPTY = "Map file doesn't exist!";
#define RSS_FILE ""
#define RSS_URL_ADDON ""

int Language;
char x[2] = {0, 0};

namespace {
void DeleteAllNpcsBesidesHero() {
  zCListSort<oCNpc>* NpcList = ogame->GetGameWorld()->voblist_npcs;
  int size = NpcList->GetNumInList();
  for (int i = 0; i < size; i++) {
    NpcList = NpcList->next;
    oCNpc* NpcOnList = NpcList->GetData();
    if (NpcOnList->GetInstance() != 11471)
      NpcOnList->Disable();
  }
  ogame->GetSpawnManager()->SetSpawningEnabled(0);
}
}  // namespace

CMainMenu::CMainMenu() {
  string_tmp = "ItMw_1h_Mil_Sword";
  player->SetMovLock(1);
  Patch::PlayerInterfaceEnabled(false);
  LoadConfig();
  ScreenResolution.x = zoptions->ReadInt(zOPT_SEC_VIDEO, "zVidResFullscreenX", 320);
  ScreenResolution.y = zoptions->ReadInt(zOPT_SEC_VIDEO, "zVidResFullscreenY", 258);
  MenuItems = 4;
  hbX, hbY, ps = 0, MenuPos = 0, OptionPos = 0, WBMenuPos = 0;
  RECT wymiary;
  GetWindowRect(Patch::GetHWND(), &wymiary);
  fWRatio = 1280.0f / (float)wymiary.right;  // zalozenie jest takie ze szerokosc jest dopasowywana weddug szerokosci 1280
  fHRatio = 1024.0f / (float)wymiary.top;
  DisableHealthBar();
  GMPLogo = new zCView(0, 0, 8192, 8192, VIEW_ITEM);
  GMPLogo->SetPos(4000, 200);
  GMPLogo->InsertBack(zSTRING("GMP_LOGO_MENU.TGA"));
  screen->InsertItem(GMPLogo);
  GMPLogo->SetSize(5500, 2000);
  TitleWeaponEnabled = false;
  AppCamCreated = false;
  WritingNickname = false;
  AppWeapon = NULL;
  LangSetting = NULL;
  Options = NULL;
  esl = NULL;
  MState = CHOOSE_LANGUAGE;

  oCSpawnManager::SetRemoveRange(2097152.0f);
  ServerList = new CServerList();
  LaunchMenuScene();
  ogame->GetWorldTimer()->GetTime(Hour, Minute);
  HooksManager* hm = HooksManager::GetInstance();
  hm->AddHook(HT_RENDER, (DWORD)MainMenuLoop, false);
  hm->RemoveHook(HT_AIMOVING, (DWORD)Initialize);
  HeroPos = player->GetPositionWorld();
  Angle = player->trafoObjToWorld.GetAtVector();
  NAngle = player->trafoObjToWorld.GetRightVector();
  ClearNpcTalents(player);
  zCSoundFX* FXMusic;
  if (Christmas)
    FXMusic = zsound->LoadSoundFX("XMAS.WAV");
  else
    FXMusic = zsound->LoadSoundFX("K_KURKOWSKI_A_CERTAIN_PLACE.WAV");
  FXMusic->SetLooping(1);
  MusicId = zsound->PlaySound(FXMusic, 1);
};

CMainMenu::~CMainMenu() {
  delete esl;
  CleanUpMainMenu();
};

static void ReLaunchPart2() {
  HooksManager::GetInstance()->RemoveHook(HT_AIMOVING, (DWORD)ReLaunchPart2);
  CMainMenu* ReMenu = CMainMenu::GetInstance();
  ReMenu->ClearNpcTalents(player);
  ReMenu->ps = MAIN_MENU;
  ReMenu->MState = MENU_LOOP;
  player->SetMovLock(1);
  Patch::PlayerInterfaceEnabled(false);
  if (player->GetEquippedArmor())
    player->UnequipItem(player->GetEquippedArmor());
  if (player->GetEquippedRangedWeapon())
    player->UnequipItem(player->GetEquippedRangedWeapon());
  if (player->GetEquippedMeleeWeapon())
    player->UnequipItem(player->GetEquippedMeleeWeapon());
  if (player->GetRightHand()) {
    zCVob* Ptr = player->GetRightHand();
    zCVob* PtrLeft = player->GetLeftHand();
    player->DropAllInHand();
    if (PtrLeft)
      PtrLeft->RemoveVobFromWorld();
    Ptr->RemoveVobFromWorld();
  }
  player->GetModel()->StartAnimation("S_RUN");
  player->SetWeaponMode(NPC_WEAPON_NONE);
  player->inventory2.ClearInventory();
  ReMenu->DisableHealthBar();
  ReMenu->GMPLogo = new zCView(0, 0, 8192, 8192, VIEW_ITEM);
  ReMenu->GMPLogo->SetPos(4000, 200);
  ReMenu->GMPLogo->InsertBack(zSTRING("GMP_LOGO_MENU.TGA"));
  screen->InsertItem(ReMenu->GMPLogo);
  ReMenu->GMPLogo->SetSize(5500, 2000);
  HooksManager::GetInstance()->AddHook(HT_RENDER, (DWORD)CMainMenu::MainMenuLoop, false);
  ReMenu->TitleWeaponEnabled = false;
  ReMenu->AppCamCreated = false;
  ReMenu->WritingNickname = false;
  ReMenu->LaunchMenuScene();
};

void CMainMenu::ReLaunchMainMenu() {
  zinput->ClearKeyBuffer();
  if (!memcmp("NEWWORLD\\NEWWORLD.ZEN", ogame->GetGameWorld()->GetWorldFilename().ToChar(), 21)) {
  }  // jest mapa
  else {
    Patch::ChangeLevelEnabled(true);
    ogame->ChangeLevel("NEWWORLD\\NEWWORLD.ZEN", zSTRING("????"));
    Patch::ChangeLevelEnabled(false);
  }
  HooksManager::GetInstance()->AddHook(HT_AIMOVING, (DWORD)ReLaunchPart2, false);
};

void CMainMenu::EnableHealthBar() {
  ogame->hpBar->SetSize(hbX, hbY);
};
void CMainMenu::DisableHealthBar() {
  ogame->hpBar->GetSize(hbX, hbY);
  ogame->hpBar->SetSize(0, 0);
};

char* Convert(const wchar_t* wString) {
  size_t len = wcslen(wString);
  char* szString = new char[len + 1];
  memset(szString, 0, len + 1);
  for (size_t i = 0; i < len; i++) wctomb(szString + i, wString[i]);
  return szString;
}

void CMainMenu::LoadLangNames(void) {
  vec_lang_files.clear();
  vec_choose_lang.clear();
  std::string indexPath = std::string(LANG_DIR) + "index";
  std::ifstream ifs(indexPath, std::ifstream::in);
  if (!ifs.is_open()) {
    SPDLOG_ERROR("Couldn't find language index file {}!", indexPath);
    std::abort();
  }

  std::string line;
  while (std::getline(ifs, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty()) {
      continue;
    }
    vec_lang_files.push_back(line);
  }
  if (vec_lang_files.size() >= 2 && !vec_lang_files.back().compare(vec_lang_files[vec_lang_files.size() - 2].c_str())) {
    vec_lang_files.pop_back();
  }

  for (const auto& lang : vec_lang_files) {
    const std::string langPath = std::string(LANG_DIR) + lang;
    std::ifstream langFile(langPath, std::ifstream::in);
    if (!langFile.is_open()) {
      SPDLOG_ERROR("Couldn't open language file {}!", langPath);
      vec_choose_lang.push_back(zSTRING(lang.c_str()));
      continue;
    }

    try {
      nlohmann::json jsonData;
      langFile >> jsonData;
      auto rawLanguageName = jsonData.value("LANGUAGE", std::string{});
      if (rawLanguageName.empty()) {
        rawLanguageName = lang;
      }
      const auto encoding = localization::DetectLanguageEncoding(rawLanguageName, langPath);
      const auto localizedName = localization::ConvertFromUtf8(rawLanguageName, encoding);
      vec_choose_lang.push_back(zSTRING(localizedName.c_str()));
    } catch (const std::exception& ex) {
      SPDLOG_ERROR("Failed to parse language file {}: {}", langPath, ex.what());
      vec_choose_lang.push_back(zSTRING(lang.c_str()));
    }
  }
};

void CMainMenu::LaunchMenuScene() {
  CamWeapon = zfactory->CreateItem(zCParser::GetParser()->GetIndex(string_tmp));
  CamWeapon->name.Clear();
  CamWeapon->SetPositionWorld(zVEC3((float)13354.502930, 2040.0, (float)-1141.678467));
  CamWeapon->RotateWorldY(-150);
  ogame->CamInit(CamWeapon, zCCamera::activeCam);
  string_tmp = "ItMw_1H_Blessed_03";
  TitleWeapon = zfactory->CreateItem(zCParser::GetParser()->GetIndex(string_tmp));
  TitleWeapon->SetPositionWorld(zVEC3((float)13346.502930, 2006.0, (float)-1240.678467));
};

void CMainMenu::LoadConfig() {
  LoadLangNames();
  Language = Config::Instance().lang;
  if (!Config::Instance().IsDefault()) {
    headmodel_tmp = CPlayer::GetHeadModelNameFromByte(Config::Instance().headmodel);
    Walkstyle_tmp = CPlayer::GetWalkStyleFromByte(Config::Instance().walkstyle);
    string_tmp = "HUM_BODY_NAKED0";
    player->SetAdditionalVisuals(string_tmp, Config::Instance().skintexture, 0, headmodel_tmp, Config::Instance().facetexture, 0, -1);
    player->ApplyOverlay(Walkstyle_tmp);
  }
};

void CMainMenu::CleanUpMainMenu() {
  zsound->StopAllSounds();
  MState = MENU_CLEAN;
  ogame->zCSession::CamInit();
  TitleWeapon->RemoveVobFromWorld();
  CamWeapon->RemoveVobFromWorld();
  TitleWeapon = NULL;
  CamWeapon = NULL;
  TitleWeaponEnabled = false;
  MenuPos = 0;
  OptionPos = 0;
  player->SetMovLock(0);
  EnableHealthBar();
  screen->RemoveItem(GMPLogo);
  delete GMPLogo;
  if (AppWeapon)
    AppWeapon->RemoveVobFromWorld();
  ogame->GetWorldTimer()->SetDay(1);
  ogame->GetWorldTimer()->SetTime(12, 00);
  screen->SetFont(FDefault);
};

void CMainMenu::PrintMenu() {
  if (!LangSetting) {
    ApplyLanguage(Config::Instance().lang, false);
  }
  switch (ps) {
    default:
    case MAIN_MENU:
      screen->SetFont("FONT_OLD_20_WHITE.TGA");
      FColor = (MenuPos == 0) ? Highlighted : Normal;
      screen->SetFontColor(FColor);
      screen->Print(200, 3200, (*LangSetting)[CLanguage::MMENU_CHSERVER]);
      FColor = (MenuPos == 1) ? Highlighted : Normal;
      screen->SetFontColor(FColor);
      screen->Print(200, 3600, (*LangSetting)[CLanguage::MMENU_APPEARANCE]);
      FColor = (MenuPos == 2) ? Highlighted : Normal;
      screen->SetFontColor(FColor);
      screen->Print(200, 4000, (*LangSetting)[CLanguage::MMENU_OPTIONS]);
      FColor = (MenuPos == 3) ? Highlighted : Normal;
      screen->SetFontColor(FColor);
      screen->Print(200, 4400, (*LangSetting)[CLanguage::MMENU_ONLINEOPTIONS]);
      FColor = (MenuPos == 4) ? Highlighted : Normal;
      screen->SetFontColor(FColor);
      screen->Print(200, 4800, (*LangSetting)[CLanguage::MMENU_LEAVEGAME]);
      break;
    case SERVER_LIST: {
      esl->setLanguage(LangSetting);
      esl->HandleInput();
      esl->Draw();
      break;
    }
    case SETTINGS_MENU: {
      screen->SetFont("FONT_OLD_20_WHITE.TGA");
      FColor = (OptionPos == 0) ? Highlighted : Normal;
      if (WritingNickname)
        FColor = Red;
      screen->SetFontColor(FColor);
      screen->Print(200, 3200, (*LangSetting)[CLanguage::MMENU_NICKNAME]);
      screen->SetFontColor(Normal);
      if (ScreenResolution.x < 1024)
        screen->Print(1800, 3200, Config::Instance().Nickname);
      else
        screen->Print(1500, 3200, Config::Instance().Nickname);
      FColor = (OptionPos == 1) ? Highlighted : Normal;
      screen->SetFontColor(FColor);
      screen->Print(200, 3600,
                    (Config::Instance().logchat) ? (*LangSetting)[CLanguage::MMENU_LOGCHATYES] : (*LangSetting)[CLanguage::MMENU_LOGCHATNO]);
      FColor = (OptionPos == 2) ? Highlighted : Normal;
      screen->SetFontColor(FColor);
      screen->Print(200, 4000, (Config::Instance().watch) ? (*LangSetting)[CLanguage::MMENU_WATCHON] : (*LangSetting)[CLanguage::MMENU_WATCHOFF]);
      FColor = (OptionPos == 3) ? Highlighted : Normal;
      screen->SetFontColor(FColor);
      screen->Print(200, 4400, (*LangSetting)[CLanguage::MMENU_SETWATCHPOS]);
      FColor = (OptionPos == 4) ? Highlighted : Normal;
      screen->SetFontColor(FColor);
      screen->Print(200, 4800,
                    (zoptions->ReadInt("ENGINE", "zVidEnableAntiAliasing", 0) == 1) ? (*LangSetting)[CLanguage::MMENU_ANTIALIASINGYES]
                                                                                    : (*LangSetting)[CLanguage::MMENU_ANTIAlIASINGNO]);
      FColor = (OptionPos == 5) ? Highlighted : Normal;
      screen->SetFontColor(FColor);
      screen->Print(200, 5200,
                    (zoptions->ReadBool(zOPT_SEC_GAME, "joystick", 0) == 1) ? (*LangSetting)[CLanguage::MMENU_JOYSTICKYES]
                                                                            : (*LangSetting)[CLanguage::MMENU_JOYSTICKNO]);
      FColor = (OptionPos == 6) ? Highlighted : Normal;
      screen->SetFontColor(FColor);
      sprintf(tmpbuff, "%s %d", (*LangSetting)[CLanguage::MMENU_CHATLINES].ToChar(), Config::Instance().ChatLines);
      ChatLinesTMP = tmpbuff;
      screen->Print(200, 5600, ChatLinesTMP);
      FColor = (OptionPos == 7) ? Highlighted : Normal;
      screen->SetFontColor(FColor);
      int printx = 200;
      int printy = 6000;
      switch (Config::Instance().keyboardlayout) {
        case Config::KEYBOARD_POLISH:
          screen->Print(printx, printy, (*LangSetting)[CLanguage::KEYBOARD_POLISH]);
          break;
        case Config::KEYBOARD_GERMAN:
          screen->Print(printx, printy, (*LangSetting)[CLanguage::KEYBOARD_GERMAN]);
          break;
        case Config::KEYBOARD_CYRYLLIC:
          screen->Print(printx, printy, (*LangSetting)[CLanguage::KEYBOARD_RUSSIAN]);
          break;
      };
      FColor = (OptionPos == 8) ? Highlighted : Normal;
      screen->SetFontColor(FColor);
      if (vec_choose_lang.empty() || vec_lang_files.empty()) {
        LoadLangNames();
      }
      zSTRING languageOptionText = "Language: ";
      if (Config::Instance().lang >= 0 && static_cast<size_t>(Config::Instance().lang) < vec_choose_lang.size())
        languageOptionText += vec_choose_lang[Config::Instance().lang];
      else
        languageOptionText += (*LangSetting)[CLanguage::LANGUAGE];
      screen->Print(200, 6400, languageOptionText);
      FColor = (OptionPos == 9) ? Highlighted : Normal;
      screen->SetFontColor(FColor);
      screen->Print(
          200, 6800,
          (zoptions->ReadBool(zOPT_SEC_GAME, "playLogoVideos", 1) == 1) ? (*LangSetting)[CLanguage::INTRO_YES] : (*LangSetting)[CLanguage::INTRO_NO]);
      FColor = (OptionPos == 10) ? Highlighted : Normal;
      screen->SetFontColor(FColor);
      screen->Print(200, 7200, (*LangSetting)[CLanguage::MMENU_BACK]);
    } break;
    case WORLDBUILDER_MENU:
      screen->SetFont("FONT_OLD_20_WHITE.TGA");
      FColor = (WBMenuPos == 0) ? Highlighted : Normal;
      screen->SetFontColor(FColor);
      screen->Print(200, 3200, (*LangSetting)[CLanguage::WB_NEWMAP]);
      FColor = (WBMenuPos == 1) ? Highlighted : Normal;
      screen->SetFontColor(FColor);
      screen->Print(200, 3600, (*LangSetting)[CLanguage::WB_LOADMAP]);
      FColor = (WBMenuPos == 2) ? Highlighted : Normal;
      screen->SetFontColor(FColor);
      screen->Print(200, 4000, (*LangSetting)[CLanguage::MMENU_BACK]);
      break;
  }
};

void CMainMenu::ChangeLanguage(int direction) {
  if (vec_lang_files.empty() || vec_choose_lang.empty()) {
    LoadLangNames();
  }
  if (vec_lang_files.empty())
    return;
  int languageCount = static_cast<int>(vec_lang_files.size());
  int newIndex = Config::Instance().lang + direction;
  if (newIndex < 0)
    newIndex = languageCount - 1;
  if (newIndex >= languageCount)
    newIndex = 0;
  if (newIndex == Config::Instance().lang)
    return;
  ApplyLanguage(newIndex);
}

void CMainMenu::ApplyLanguage(int newLangIndex, bool persist) {
  if (vec_lang_files.empty() || vec_choose_lang.empty()) {
    LoadLangNames();
  }
  if (vec_lang_files.empty())
    return;
  if (newLangIndex < 0 || newLangIndex >= static_cast<int>(vec_lang_files.size()))
    newLangIndex = 0;

  zSTRING path = LANG_DIR;
  path += vec_lang_files[newLangIndex].c_str();
  CLanguage* newLanguage = new CLanguage(path.ToChar());
  path.Clear();

  if (LangSetting)
    delete LangSetting;
  LangSetting = newLanguage;
  Lang = LangSetting;

  if (esl) {
    delete esl;
  }
  esl = new ExtendedServerList(ServerList);
  SelectedServer = 0;

  if (client)
    client->lang = LangSetting;

  Config::Instance().lang = newLangIndex;
  if (persist)
    Config::Instance().SaveConfigToFile();

  Language = newLangIndex;
}

void CMainMenu::SpeedUpTime() {
  ogame->GetWorldTimer()->GetTime(Hour, Minute);
  if (Minute >= 59)
    Hour++;
  Minute++;
  ogame->GetWorldTimer()->SetTime(Hour, Minute);
};
void CMainMenu::ClearNpcTalents(oCNpc* Npc) {
  Npc->inventory2.ClearInventory();
  Npc->DestroySpellBook();
  Npc->MakeSpellBook();
  for (int i = 0; i < 21; i++) {
    Npc->SetTalentSkill(i, 0);
    Npc->SetTalentValue(i, 0);
  }
}

void CMainMenu::LeaveOptionsMenu() {
  zinput->ClearKeyBuffer();
  CMainMenu::GetInstance()->Options->Leave();
  gameMan->ApplySomeSettings();
  player->SetMovLock(1);
  CMainMenu::GetInstance()->MState = MENU_LOOP;
  ScreenResolution.x = zoptions->ReadInt(zOPT_SEC_VIDEO, "zVidResFullscreenX", 320);
};

void CMainMenu::RunMenuItem() {
  switch (MenuPos) {
    case 0:
      // WYBIERZ SERWER
      screen->RemoveItem(GMPLogo);
      ps = SERVER_LIST;
      MState = CHOOSE_SRV_LOOP;
      SelectedServer = 0;
      esl->RefreshList();
      if (!ServerIP.IsEmpty())
        ServerIP.Clear();
      break;
    case 1:
      // WYBIERZ WYGLAD
      if (AppCamCreated) {
        ChoosingApperance = ApperancePart::FACE;
        LastApperance = ApperancePart::FACE;
        AppWeapon->ResetRotationsWorld();
        AppWeapon->SetPositionWorld(
            zVEC3(player->GetPositionWorld()[VX] - 78, player->GetPositionWorld()[VY] + 50, player->GetPositionWorld()[VZ] - 119));
        AppWeapon->RotateWorldY(30);
        ogame->CamInit(CMainMenu::GetInstance()->AppWeapon, zCCamera::activeCam);
      }
      MState = MENU_APPEARANCE;
      break;
    case 2:
      // OPCJE
      MState = MENU_OPTIONS;
      if (!Options)
        Options = zCMenu::Create(zSTRING("MENU_OPTIONS"));
      Options->Run();
      break;
    case 3:
      // OPCJE DODATKOWE
      MState = MENU_OPTONLINE;
      ps = SETTINGS_MENU;
      break;
    case 4:
      // WYJDZ Z GRY
      gameMan->Done();
      break;
  };
};

void CMainMenu::RunOptionsItem() {
  switch (OptionPos) {
    case 0:
      // NICKNAME
      WritingNickname = true;
      break;
    case 1:
      // LOGOWANIE CZATU
      Config::Instance().logchat = !Config::Instance().logchat;
      Config::Instance().SaveConfigToFile();
      break;
    case 2:
      // WLACZENIE ZEGARA
      Config::Instance().watch = !Config::Instance().watch;
      Config::Instance().SaveConfigToFile();
      break;
    case 3:
      // POZYCJA ZEGARA
      Config::Instance().watch = true;
      screen->RemoveItem(GMPLogo);
      MState = MENU_SETWATCHPOS;
      break;
    case 4: {
      // ANTIALIASING
      int current_value = zoptions->ReadInt("ENGINE", "zVidEnableAntiAliasing", 0);
      zoptions->WriteInt("ENGINE", "zVidEnableAntiAliasing", (current_value == 0) ? 1 : 0);
      gameMan->ApplySomeSettings();
      break;
    }
    case 5: {
      // JOYSTICK
      int current_value = zoptions->ReadBool(zOPT_SEC_GAME, "enableJoystick", 0);
      zoptions->WriteBool(zOPT_SEC_GAME, "enableJoystick", !current_value);
      gameMan->ApplySomeSettings();
      break;
    }
    case 6:
      // Chat Lines
      break;
    case 7:
      // KEYBOARD LAYOUT
      break;
    case 8:
      // LANGUAGE
      break;
    case 9: {
      // INTROS
      int current_value = zoptions->ReadBool(zOPT_SEC_GAME, "playLogoVideos", 1);
      zoptions->WriteBool(zOPT_SEC_GAME, "playLogoVideos", !current_value);
      gameMan->ApplySomeSettings();
      break;
    }
    case 10:
      // POWROT
      ps = MAIN_MENU;
      MState = MENU_LOOP;
      break;
  };
};
void CMainMenu::RunWbMenuItem() {
  switch (WBMenuPos) {
    case 0:
      // NOWA MAPA
      MState = WRITE_WORLDNAME;
      WBMapName.Clear();
      break;
    case 1:
      // WCZYTYWANIE MAPY
      MState = LOAD_WBMAP;
      WBMapName.Clear();
      break;
    case 2:
      // POWROT DO MENU
      ps = MAIN_MENU;
      MState = MENU_LOOP;
      break;
  };
};

void __stdcall CMainMenu::MainMenuLoop() {
  CMainMenu::GetInstance()->RenderMenu();
};

void CMainMenu::RenderMenu() {
  if (Christmas) {
    ogame->GetWorld()->skyControlerOutdoor->SetWeatherType(zTWEATHER_SNOW);
  }
  if (player->GetPositionWorld()[VX] != HeroPos[VX]) {
    player->trafoObjToWorld.SetTranslation(HeroPos);
    player->trafoObjToWorld.SetAtVector(Angle);
    player->trafoObjToWorld.SetRightVector(NAngle);
  }
  static bool fast_localhost_join = false;
  switch (MState) {
    case CHOOSE_LANGUAGE:
      if (Config::Instance().IsDefault()) {
        string_tmp = "Choose your language:";
        screen->Print(200, 200, string_tmp);
        screen->Print(200, 350, vec_choose_lang[Language]);
        if (zinput->KeyToggled(KEY_LEFT))
          Language = (Language == 0) ? (vec_choose_lang.size() - 1) : Language - 1;
        if (zinput->KeyToggled(KEY_RIGHT))
          Language = (++Language == vec_choose_lang.size()) ? 0 : Language;
        if (zinput->KeyPressed(KEY_RETURN)) {
          ApplyLanguage(Language);
          zinput->ClearKeyBuffer();
          MState = CHOOSE_NICKNAME;
        }
      } else {
        MState = CHOOSE_NICKNAME;
      }
      break;
    case CHOOSE_NICKNAME:
      if (Config::Instance().IsDefault() || Config::Instance().Nickname.IsEmpty()) {
        screen->Print(200, 200, (*LangSetting)[CLanguage::WRITE_NICKNAME]);
        screen->Print(200 + static_cast<zINT>(static_cast<float>((*LangSetting)[CLanguage::WRITE_NICKNAME].Length() * 70) * fWRatio), 200,
                      Config::Instance().Nickname);
        x[0] = GInput::GetCharacterFormKeyboard();
        if ((x[0] == 8) && (Config::Instance().Nickname.Length() > 0))
          Config::Instance().Nickname.DeleteRight(1);
        if ((x[0] >= 0x20) && (Config::Instance().Nickname.Length() < 24))
          Config::Instance().Nickname += x;
        if ((x[0] == 0x0D) && (!Config::Instance().Nickname.IsEmpty())) {
          Config::Instance().SaveConfigToFile();
          MState = MENU_LOOP;
        }
      } else {
        MState = MENU_LOOP;
      }
      break;
    case MENU_LOOP: {
      if (player->GetModel()->IsAnimationActive(WalkAnim))
        player->GetModel()->StopAnimation(WalkAnim);
      if (!TitleWeaponEnabled && TitleWeapon) {
        TitleWeaponEnabled = true;
        ogame->GetWorld()->AddVob(TitleWeapon);
      }
      if (!Christmas)
        SpeedUpTime();
      screen->SetFont(FDefault);
      screen->SetFontColor(Normal);
      std::string version = GIT_TAG_LONG;
      VersionString = (!version.empty()) ? version.c_str() : "Unknown build";
      screen->Print(8192 - screen->FontSize(VersionString), 8192 - screen->FontY(), VersionString);
      static zSTRING HOWTOWB = "F1 - World Builder";
      screen->Print(100, 8192 - screen->FontY(), HOWTOWB);
      static zSTRING fast_localhost_join_text = "F5 - Fast join localhost server";
      screen->Print(100 + screen->FontSize(HOWTOWB) + 50, 8192 - screen->FontY(), fast_localhost_join_text);
      if (TitleWeapon)
        TitleWeapon->RotateWorldX(0.6f);
      if (zinput->KeyToggled(KEY_F1)) {
        g2names.open(".\\Multiplayer\\world-builder\\g2mobs.wb");
        g2particles.open(".\\Multiplayer\\world-builder\\g2particles.wb");
        if (g2names.good() && g2particles.good()) {
          ps = WORLDBUILDER_MENU;
          MState = MENU_WORLDBUILDER;
        } else
          ogame->array_view[oCGame::GAME_VIEW_SCREEN]->PrintTimedCXY(
              "Important World Builder files missing. Couldn't launch. Download full installer from GMP site.", 5000.0f, 0);
        g2names.close();
        g2particles.close();
        g2names.clear();
        g2particles.clear();
      }
      if (zinput->KeyToggled(KEY_F5)) {
        SelectedServer = -1;
        ServerIP = "127.0.0.1";
        MState = CHOOSE_SRV_LOOP;
        fast_localhost_join = true;
      }
      if (zinput->KeyToggled(KEY_UP)) {
        MenuPos == 0 ? MenuPos = MenuItems : MenuPos--;
      }
      if (zinput->KeyToggled(KEY_DOWN)) {
        MenuPos == MenuItems ? MenuPos = 0 : MenuPos++;
      }
      if (zinput->KeyPressed(KEY_RETURN)) {
        zinput->ClearKeyBuffer();
        RunMenuItem();
      }
      PrintMenu();
    } break;
    case CHOOSE_SRV_LOOP:
      if (!TitleWeaponEnabled && TitleWeapon) {
        TitleWeaponEnabled = true;
        ogame->GetWorld()->AddVob(TitleWeapon);
      }
      if (zinput->KeyPressed(KEY_RETURN) || fast_localhost_join) {
        if (SelectedServer != -1) {
          ServerIP.Clear();
          char buforek[256];
          esl->getSelectedServer(buforek, sizeof(buforek));
          ServerIP += buforek;
        }
        fast_localhost_join = false;
        zinput->ClearKeyBuffer();
        if (client) {
          delete client;
          client = NULL;
        }
        client = new GameClient(ServerIP.ToChar(), LangSetting);
        if (!client->Connect()) {
          ogame->array_view[oCGame::GAME_VIEW_SCREEN]->PrintTimedCXY(client->GetLastError(), 5000.0f, &Red);
        }
        if (client->IsConnected()) {
          client->HandleNetwork();
          client->SyncGameTime();
          zVEC3 SpawnpointPos = player->GetPositionWorld();
          CleanUpMainMenu();
          Patch::PlayerInterfaceEnabled(true);
          HooksManager::GetInstance()->RemoveHook(HT_RENDER, (DWORD)MainMenuLoop);
          if (!client->map.IsEmpty()) {
            Patch::ChangeLevelEnabled(true);
            ogame->ChangeLevel(client->map, zSTRING("????"));
            Patch::ChangeLevelEnabled(false);
          }
          DeleteAllNpcsBesidesHero();
          player->trafoObjToWorld.SetTranslation(SpawnpointPos);
          std::string WordBuilderMapFileName = ".\\Multiplayer\\Data\\";
          WordBuilderMapFileName += client->network->GetServerIp() + "_" + std::to_string(client->network->GetServerPort());
          std::ifstream WbMap(WordBuilderMapFileName.c_str());
          if (WbMap.good()) {
            WbMap.close();
            LoadWorld::LoadWorld(WordBuilderMapFileName.c_str(), client->VobsWorldBuilderMap);
          }
          if (WbMap.is_open())
            WbMap.close();
          WordBuilderMapFileName.clear();
          if (client->VobsWorldBuilderMap.size() > 0) {
            for (int i = 0; i < (int)client->VobsWorldBuilderMap.size(); i++) {
              if (client->VobsWorldBuilderMap[i].Type == TYPE_MOB)
                client->VobsWorldBuilderMap[i].Vob->SetCollDet(1);
            }
          }
          client->JoinGame();
        } else {
          SPDLOG_ERROR("ERROR: {}", client->GetLastError().ToChar());
        }
      }
      if (zinput->KeyPressed(KEY_W)) {
        SelectedServer = -1;
        zinput->ClearKeyBuffer();
      }
      if (SelectedServer == -1) {
        if (zinput->KeyToggled(KEY_SLASH)) {
          SelectedServer = 0;
        }
        if (zinput->KeyToggled(KEY_F1)) {
          ServerIP = "127.0.0.1";
        }
        char x[2] = {0, 0};
        x[0] = GInput::GetCharacterFormKeyboard(true);
        if (x[0] > 0x20) {
          ServerIP += x;
        }
        if ((x[0] == 0x08) && (ServerIP.Length()))
          ServerIP.DeleteRight(1);
        screen->PrintCXY(ServerIP);
      } else
        PrintMenu();
      if (TitleWeapon)
        TitleWeapon->RotateWorldX(0.6f);
      if (zinput->KeyPressed(KEY_ESCAPE)) {
        zinput->ClearKeyBuffer();
        screen->InsertItem(GMPLogo);
        ps = MAIN_MENU;
        MState = MENU_LOOP;
      }
      break;
    case MENU_APPEARANCE:
      screen->SetFont(FDefault);
      screen->Print(100, 200, (*LangSetting)[CLanguage::APP_INFO1]);
      if (!AppCamCreated) {
        string_tmp = "ItMw_1h_Mil_Sword";
        AppWeapon = zfactory->CreateItem(zCParser::GetParser()->GetIndex(string_tmp));
        AppWeapon->SetPositionWorld(
            zVEC3(player->GetPositionWorld()[VX] - 78, player->GetPositionWorld()[VY] + 50, player->GetPositionWorld()[VZ] - 119));
        AppWeapon->RotateWorldY(30);
        AppWeapon->name.Clear();
        string_tmp = "HUM_BODY_NAKED0";
        headmodel_tmp = CPlayer::GetHeadModelNameFromByte(Config::Instance().headmodel);
        player->SetAdditionalVisuals(string_tmp, Config::Instance().skintexture, 0, headmodel_tmp, Config::Instance().facetexture, 0, -1);
        player->GetModel()->meshLibList[0]->texAniState.actAniFrames[0][0] = Config::Instance().skintexture;
        ChoosingApperance = ApperancePart::FACE;
        LastApperance = ApperancePart::FACE;
        ogame->CamInit(AppWeapon, zCCamera::activeCam);
        AppCamCreated = true;
      }
      if ((zinput->KeyToggled(KEY_UP)) && (ChoosingApperance > ApperancePart::HEAD))
        --ChoosingApperance;
      if ((zinput->KeyToggled(KEY_DOWN)) && (ChoosingApperance < ApperancePart::WALKSTYLE))
        ++ChoosingApperance;
      if ((zinput->KeyPressed(KEY_ESCAPE))) {
        string_tmp.Clear();
        zinput->ClearKeyBuffer();
        MState = MENU_LOOP;
        Config::Instance().SaveConfigToFile();
        ogame->CamInit(CamWeapon, zCCamera::activeCam);
      }
      switch (ChoosingApperance) {
        default:
        case ApperancePart::HEAD:
          screen->Print(500, 2000, (*LangSetting)[CLanguage::HEAD_MODEL]);
          if ((zinput->KeyToggled(KEY_LEFT))) {
            if (Config::Instance().headmodel > 0) {
              Config::Instance().headmodel--;
              headmodel_tmp = CPlayer::GetHeadModelNameFromByte(Config::Instance().headmodel);
              player->SetAdditionalVisuals(string_tmp, Config::Instance().skintexture, 0, headmodel_tmp, Config::Instance().facetexture, 0, -1);
            }
          }
          if ((zinput->KeyToggled(KEY_RIGHT))) {
            if (Config::Instance().headmodel < 5) {
              Config::Instance().headmodel++;
              headmodel_tmp = CPlayer::GetHeadModelNameFromByte(Config::Instance().headmodel);
              player->SetAdditionalVisuals(string_tmp, Config::Instance().skintexture, 0, headmodel_tmp, Config::Instance().facetexture, 0, -1);
            }
          }
          if (LastApperance != ChoosingApperance) {
            AppWeapon->SetPositionWorld(
                zVEC3(player->GetPositionWorld()[VX] + 55, player->GetPositionWorld()[VY] + 70, player->GetPositionWorld()[VZ] - 37));
            AppWeapon->RotateWorldY(-90);
            LastApperance = ChoosingApperance;
          }
          break;
        case ApperancePart::FACE:
          screen->Print(500, 2000, (*LangSetting)[CLanguage::FACE_APPERANCE]);
          if ((zinput->KeyToggled(KEY_LEFT))) {
            if (Config::Instance().facetexture > 0) {
              Config::Instance().facetexture--;
              player->SetAdditionalVisuals(string_tmp, Config::Instance().skintexture, 0, headmodel_tmp, Config::Instance().facetexture, 0, -1);
            }
          }
          if ((zinput->KeyToggled(KEY_RIGHT))) {
            if (Config::Instance().facetexture < 162) {
              Config::Instance().facetexture++;
              player->SetAdditionalVisuals(string_tmp, Config::Instance().skintexture, 0, headmodel_tmp, Config::Instance().facetexture, 0, -1);
            }
          }
          if (LastApperance != ChoosingApperance) {
            AppWeapon->SetPositionWorld(
                zVEC3(player->GetPositionWorld()[VX] - 78, player->GetPositionWorld()[VY] + 50, player->GetPositionWorld()[VZ] - 119));
            AppWeapon->RotateWorldY(90);
            LastApperance = ChoosingApperance;
          }
          break;
        case ApperancePart::SKIN:
          screen->Print(500, 2000, (*LangSetting)[CLanguage::SKIN_TEXTURE]);
          if (player->GetModel()->IsAnimationActive(WalkAnim))
            player->GetModel()->StopAnimation(WalkAnim);
          if ((zinput->KeyToggled(KEY_LEFT))) {
            if (Config::Instance().skintexture > 0) {
              Config::Instance().skintexture--;
              player->SetAdditionalVisuals(string_tmp, Config::Instance().skintexture, 0, headmodel_tmp, Config::Instance().facetexture, 0, -1);
              player->GetModel()->meshLibList[0]->texAniState.actAniFrames[0][0] = Config::Instance().skintexture;
            }
          }
          if ((zinput->KeyToggled(KEY_RIGHT))) {
            if (Config::Instance().skintexture < 12) {
              Config::Instance().skintexture++;
              player->SetAdditionalVisuals(string_tmp, Config::Instance().skintexture, 0, headmodel_tmp, Config::Instance().facetexture, 0, -1);
              player->GetModel()->meshLibList[0]->texAniState.actAniFrames[0][0] = Config::Instance().skintexture;
            }
          }
          break;
        case ApperancePart::WALKSTYLE:
          screen->Print(500, 2000, (*LangSetting)[CLanguage::WALK_STYLE]);
          if ((zinput->KeyPressed(KEY_LEFT))) {
            zinput->ClearKeyBuffer();
            if (Config::Instance().walkstyle > 0) {
              if (player->GetModel()->IsAnimationActive(WalkAnim))
                player->GetModel()->StopAnimation(WalkAnim);
              player->RemoveOverlay(Walkstyle_tmp);
              Config::Instance().walkstyle--;
              Walkstyle_tmp = CPlayer::GetWalkStyleFromByte(Config::Instance().walkstyle);
              player->ApplyOverlay(Walkstyle_tmp);
            }
          }
          if ((zinput->KeyPressed(KEY_RIGHT))) {
            zinput->ClearKeyBuffer();
            if (Config::Instance().walkstyle < 6) {
              if (player->GetModel()->IsAnimationActive(WalkAnim))
                player->GetModel()->StopAnimation(WalkAnim);
              player->RemoveOverlay(Walkstyle_tmp);
              Config::Instance().walkstyle++;
              Walkstyle_tmp = CPlayer::GetWalkStyleFromByte(Config::Instance().walkstyle);
              player->ApplyOverlay(Walkstyle_tmp);
            }
          }
          if (!player->GetModel()->IsAnimationActive(WalkAnim))
            player->GetModel()->StartAnimation(WalkAnim);
          player->trafoObjToWorld.SetTranslation(HeroPos);
          break;
      }
      break;
    case MENU_OPTIONS:
      if (!player->IsMovLock())
        player->SetMovLock(1);
      if (TitleWeapon)
        TitleWeapon->RotateWorldX(0.6f);
      if (memcmp("MENU_OPTIONS", zCMenu::GetActive()->GetName().ToChar(), 12) == 0 && zinput->KeyPressed(KEY_ESCAPE))
        LeaveOptionsMenu();
      if (memcmp("MENUITEM_OPT_BACK", Options->GetActiveItem()->GetName().ToChar(), 17) == 0 && zinput->KeyPressed(KEY_RETURN))
        LeaveOptionsMenu();
      break;
    case MENU_OPTONLINE:
      if (TitleWeapon)
        TitleWeapon->RotateWorldX(0.6f);
      if (Config::Instance().watch)
        CWatch::GetInstance()->PrintWatch();
      if (WritingNickname) {
        x[0] = GInput::GetCharacterFormKeyboard();
        if ((x[0] == 8) && (Config::Instance().Nickname.Length() > 0))
          Config::Instance().Nickname.DeleteRight(1);
        if ((x[0] >= 0x20) && (Config::Instance().Nickname.Length() < 24))
          Config::Instance().Nickname += x;
        if ((x[0] == 0x0D) && (!Config::Instance().Nickname.IsEmpty())) {
          Config::Instance().SaveConfigToFile();
          WritingNickname = false;
        }
      }
      // Wybor opcji przez enter
      if (!WritingNickname) {
        if (zinput->KeyToggled(KEY_UP)) {
          OptionPos == 0 ? OptionPos = 10 : OptionPos--;
        }
        if (zinput->KeyToggled(KEY_DOWN)) {
          OptionPos == 10 ? OptionPos = 0 : OptionPos++;
        }
        if (zinput->KeyPressed(KEY_RETURN)) {
          zinput->ClearKeyBuffer();
          RunOptionsItem();
        }
      }
      // Opcja od ilosci lini czatu
      if (OptionPos == 6) {
        if (zinput->KeyToggled(KEY_LEFT)) {
          if (Config::Instance().ChatLines > 0) {
            if (Config::Instance().ChatLines <= 5)
              Config::Instance().ChatLines = 0;
            else
              Config::Instance().ChatLines--;
            Config::Instance().SaveConfigToFile();
          }
        }
        if (zinput->KeyToggled(KEY_RIGHT)) {
          if (Config::Instance().ChatLines < 30) {
            if (Config::Instance().ChatLines < 5)
              Config::Instance().ChatLines = 5;
            else
              Config::Instance().ChatLines++;
            Config::Instance().SaveConfigToFile();
          }
        }
      }
      if (OptionPos == 7) {
        if (zinput->KeyToggled(KEY_LEFT)) {
          if (Config::Instance().keyboardlayout > Config::KEYBOARD_POLISH) {
            Config::Instance().keyboardlayout--;
            Config::Instance().SaveConfigToFile();
          }
        }
        if (zinput->KeyToggled(KEY_RIGHT)) {
          if (Config::Instance().keyboardlayout < Config::KEYBOARD_CYRYLLIC) {
            Config::Instance().keyboardlayout++;
            Config::Instance().SaveConfigToFile();
          }
        }
      }
      if (OptionPos == 8) {
        if (zinput->KeyToggled(KEY_LEFT)) {
          ChangeLanguage(-1);
        }
        if (zinput->KeyToggled(KEY_RIGHT)) {
          ChangeLanguage(1);
        }
      }
      PrintMenu();
      break;
    case MENU_WORLDBUILDER:
      if (TitleWeapon)
        TitleWeapon->RotateWorldX(0.6f);
      // Wybor opcji przez enter
      if (zinput->KeyToggled(KEY_UP)) {
        WBMenuPos == 0 ? WBMenuPos = 2 : WBMenuPos--;
      }
      if (zinput->KeyToggled(KEY_DOWN)) {
        WBMenuPos == 2 ? WBMenuPos = 0 : WBMenuPos++;
      }
      if (zinput->KeyPressed(KEY_RETURN)) {
        zinput->ClearKeyBuffer();
        RunWbMenuItem();
      }
      PrintMenu();
      break;
    case WRITE_WORLDNAME:
      if (TitleWeapon)
        TitleWeapon->RotateWorldX(0.6f);
      if (zinput->KeyToggled(KEY_ESCAPE)) {
        ps = WORLDBUILDER_MENU;
        MState = MENU_WORLDBUILDER;
      }
      x[0] = GInput::GetCharacterFormKeyboard(true);
      if ((x[0] == 8) && (WBMapName.Length() > 0))
        WBMapName.DeleteRight(1);
      if ((x[0] >= 0x20) && (WBMapName.Length() < 24))
        WBMapName += x;
      if ((x[0] == 0x0D) && (!WBMapName.IsEmpty())) {
        CleanUpMainMenu();
        HooksManager::GetInstance()->RemoveHook(HT_RENDER, (DWORD)MainMenuLoop);
        WBMapName.Upper();
        if (!strstr(WBMapName.ToChar(), ".ZEN"))
          WBMapName += ".ZEN";
        if (!memcmp("NEWWORLD.ZEN", WBMapName.ToChar(), 12)) {
          WBMapName = "NEWWORLD\\NEWWORLD.ZEN";
          goto ALLDONE;
        };
        if (!memcmp("OLDWORLD.ZEN", WBMapName.ToChar(), 12)) {
          WBMapName = "OLDWORLD\\OLDWORLD.ZEN";
        };
        if (!memcmp("ADDONWORLD.ZEN", WBMapName.ToChar(), 13)) {
          WBMapName = "ADDON\\ADDONWORLD.ZEN";
        };
        Patch::ChangeLevelEnabled(true);
        ogame->ChangeLevel(WBMapName, zSTRING("????"));
        Patch::ChangeLevelEnabled(false);
      ALLDONE:
        DeleteAllNpcsBesidesHero();
        Builder = new CBuilder();
        HooksManager::GetInstance()->AddHook(HT_RENDER, (DWORD)WorldBuilderInterface, false);
      }
      screen->PrintCX(3600, WRITE_MAPNAME);
      screen->PrintCX(4000, WBMapName);
      break;
    case LOAD_WBMAP:
      if (TitleWeapon)
        TitleWeapon->RotateWorldX(0.6f);
      if (zinput->KeyToggled(KEY_ESCAPE)) {
        ps = WORLDBUILDER_MENU;
        MState = MENU_WORLDBUILDER;
      }
      x[0] = GInput::GetCharacterFormKeyboard();
      if ((x[0] == 8) && (WBMapName.Length() > 0))
        WBMapName.DeleteRight(1);
      if ((x[0] >= 0x20) && (WBMapName.Length() < 24))
        WBMapName += x;
      if ((x[0] == 0x0D) && (!WBMapName.IsEmpty())) {
        WBMapName.Upper();
        if (!strstr(WBMapName.ToChar(), ".WBM"))
          WBMapName += ".WBM";
        char buffer[64];
        sprintf(buffer, ".\\Multiplayer\\world-builder\\Maps\\%s", WBMapName.ToChar());
        std::string Map = LoadWorld::GetZenName(buffer);
        if (Map.size() > 0) {
          CleanUpMainMenu();
          HooksManager::GetInstance()->RemoveHook(HT_RENDER, (DWORD)MainMenuLoop);
          if (!memcmp("NEWWORLD\\NEWWORLD.ZEN", Map.c_str(), 22)) {
            goto ALLDONE2;
          };
          WBMapName = Map.c_str();
          Patch::ChangeLevelEnabled(true);
          ogame->ChangeLevel(WBMapName, zSTRING("????"));
          Patch::ChangeLevelEnabled(false);
        ALLDONE2:
          DeleteAllNpcsBesidesHero();
          Builder = new CBuilder();
          LoadWorld::LoadWorld(buffer, Builder->SpawnedVobs);
          HooksManager::GetInstance()->AddHook(HT_RENDER, (DWORD)WorldBuilderInterface, false);
        } else
          ogame->array_view[oCGame::GAME_VIEW_SCREEN]->PrintTimedCX(2000, MAPFILE_EMPTY, 5000.0f, 0);
      }
      screen->PrintCX(3600, WRITE_SAVEDMAP);
      screen->PrintCX(4000, WBMapName);
      break;
    case MENU_SETWATCHPOS:
      if (Config::Instance().watch)
        CWatch::GetInstance()->PrintWatch();
      if (zinput->KeyPressed(KEY_ESCAPE)) {
        zinput->ClearKeyBuffer();
        MState = MENU_OPTONLINE;
        screen->InsertItem(GMPLogo, false);
        Config::Instance().SaveConfigToFile();
      }
      if (zinput->KeyPressed(KEY_UP))
        Config::Instance().WatchPosY -= 16;
      if (zinput->KeyPressed(KEY_DOWN))
        Config::Instance().WatchPosY += 16;
      if (zinput->KeyPressed(KEY_LEFT))
        Config::Instance().WatchPosX -= 16;
      if (zinput->KeyPressed(KEY_RIGHT))
        Config::Instance().WatchPosX += 16;
      break;
  };
};
void CMainMenu::SetServerIP(int selected) {
  if (selected >= 0) {
    if (!ServerIP.IsEmpty())
      ServerIP.Clear();
    char buffer[128];
    sprintf(buffer, "%s:%hu\0", ServerList->At((size_t)selected)->ip.ToChar(), ServerList->At((size_t)selected)->port);
    ServerIP = buffer;
  }
}