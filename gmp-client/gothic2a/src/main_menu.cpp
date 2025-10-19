
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
** 	Created by:		06/06/11	-	skejt23
** 	Description:	Multiplayer main menu functionallity
**
*****************************************************************************/

#include "main_menu.h"

#include <spdlog/spdlog.h>
#include <urlmon.h>

#include <fstream>
#include <nlohmann/json.hpp>

#include "CSyncFuncs.h"
#include "CWatch.h"
#include "ExtendedServerList.h"
#include "config.h"
#include "interface.h"
#include "keyboard.h"
#include "language.h"
#include "localization_utils.h"
#include "mod.h"
#include "net_game.h"
#include "patch.h"
#include "version.h"
#include "world_utils.hpp"

using namespace Gothic_II_Addon;

std::vector<zSTRING> vec_choose_lang;
std::vector<std::string> vec_lang_files;
extern const char* LANG_DIR;
extern float fWRatio, fHRatio;
zCOLOR Normal = zCOLOR(255, 255, 255);
zCOLOR Highlighted = zCOLOR(128, 180, 128);
zCOLOR Red = zCOLOR(0xFF, 0, 0);
extern zCOLOR Green;
zCOLOR FColor;
constexpr const char* FDefault = "FONT_DEFAULT.TGA";
constexpr const char* WalkAnim = "S_WALKL";

int Language;
char x[2] = {0, 0};

namespace {
const zSTRING& GetVersionString() {
  static zSTRING version_string;
  if (version_string.IsEmpty()) {
    constexpr std::string_view version = GIT_TAG_LONG;
    version_string = zSTRING{version.empty() ? "Unknown build" : version.data()};
  }
  return version_string;
}

}  // namespace

CMainMenu::CMainMenu() {
  player->SetMovLock(1);
  Patch::PlayerInterfaceEnabled(false);
  LoadConfig();
  ScreenResolution.x = zoptions->ReadInt(zOPT_SEC_VIDEO, "zVidResFullscreenX", 320);
  ScreenResolution.y = zoptions->ReadInt(zOPT_SEC_VIDEO, "zVidResFullscreenY", 258);
  MenuItems = 3;
  hbX, hbY, ps = 0, MenuPos = 0, OptionPos = 0;
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
  Options = NULL;
  esl = NULL;
  MState = CHOOSE_LANGUAGE;

  oCSpawnManager::SetRemoveRange(2097152.0f);
  LaunchMenuScene();
  ogame->GetWorldTimer()->GetTime(Hour, Minute);
  HooksManager* hm = HooksManager::GetInstance();
  hm->AddHook(HT_RENDER, (DWORD)MainMenuLoop, false);
  hm->RemoveHook(HT_AIMOVING, (DWORD)Initialize);
  HeroPos = player->GetPositionWorld();
  Angle = player->trafoObjToWorld.GetAtVector();
  NAngle = player->trafoObjToWorld.GetRightVector();
  ClearNpcTalents(player);
  zCSoundFX* FXMusic = zsound->LoadSoundFX("K_KURKOWSKI_A_CERTAIN_PLACE.WAV");
  FXMusic->SetLooping(1);
  zsound->PlaySound(FXMusic, 1);
}

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
  // Just a placeholder weapon for the camera (the actual object is not relevant)
  CamWeapon = zfactory->CreateItem(zCParser::GetParser()->GetIndex("ItMw_1h_Mil_Sword"));
  CamWeapon->name.Clear();
  CamWeapon->SetPositionWorld(zVEC3((float)13354.502930, 2040.0, (float)-1141.678467));
  CamWeapon->RotateWorldY(-150);
  ogame->CamInit(CamWeapon, zCCamera::activeCam);
  TitleWeapon = zfactory->CreateItem(zCParser::GetParser()->GetIndex("ItMw_1H_Blessed_03"));
  TitleWeapon->SetPositionWorld(zVEC3((float)13346.502930, 2006.0, (float)-1240.678467));
};

void CMainMenu::LoadConfig() {
  LoadLangNames();
  Language = Config::Instance().lang;
  if (!Config::Instance().IsDefault()) {
    auto head_model = Gothic2APlayer::GetHeadModelNameFromByte(Config::Instance().headmodel);
    auto walkstyle = Gothic2APlayer::GetWalkStyleFromByte(Config::Instance().walkstyle);
    player->SetAdditionalVisuals("HUM_BODY_NAKED0", Config::Instance().skintexture, 0, head_model, Config::Instance().facetexture, 0, -1);
    player->ApplyOverlay(walkstyle);
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
  if (Language::Instance().IsLoaded() == false) {
    ApplyLanguage(Config::Instance().lang, false);
  }
  switch (ps) {
    default:
    case MAIN_MENU:
      screen->SetFont("FONT_OLD_20_WHITE.TGA");
      FColor = (MenuPos == 0) ? Highlighted : Normal;
      screen->SetFontColor(FColor);
      screen->Print(200, 3200, Language::Instance()[Language::MMENU_CHSERVER]);
      FColor = (MenuPos == 1) ? Highlighted : Normal;
      screen->SetFontColor(FColor);
      screen->Print(200, 3600, Language::Instance()[Language::MMENU_APPEARANCE]);
      FColor = (MenuPos == 2) ? Highlighted : Normal;
      screen->SetFontColor(FColor);
      screen->Print(200, 4000, Language::Instance()[Language::MMENU_OPTIONS]);
      FColor = (MenuPos == 3) ? Highlighted : Normal;
      screen->SetFontColor(FColor);
      screen->Print(200, 4400, Language::Instance()[Language::MMENU_LEAVEGAME]);
      break;
    case SERVER_LIST: {
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
      screen->Print(200, 3200, Language::Instance()[Language::MMENU_NICKNAME]);
      screen->SetFontColor(Normal);
      if (ScreenResolution.x < 1024)
        screen->Print(1800, 3200, Config::Instance().Nickname);
      else
        screen->Print(1500, 3200, Config::Instance().Nickname);
      FColor = (OptionPos == 1) ? Highlighted : Normal;
      screen->SetFontColor(FColor);
      screen->Print(
          200, 3600,
          (Config::Instance().logchat) ? Language::Instance()[Language::MMENU_LOGCHATYES] : Language::Instance()[Language::MMENU_LOGCHATNO]);
      FColor = (OptionPos == 2) ? Highlighted : Normal;
      screen->SetFontColor(FColor);
      screen->Print(200, 4000,
                    (Config::Instance().watch) ? Language::Instance()[Language::MMENU_WATCHON] : Language::Instance()[Language::MMENU_WATCHOFF]);
      FColor = (OptionPos == 3) ? Highlighted : Normal;
      screen->SetFontColor(FColor);
      screen->Print(200, 4400, Language::Instance()[Language::MMENU_SETWATCHPOS]);
      FColor = (OptionPos == 4) ? Highlighted : Normal;
      screen->SetFontColor(FColor);
      screen->Print(200, 4800,
                    (zoptions->ReadInt("ENGINE", "zVidEnableAntiAliasing", 0) == 1) ? Language::Instance()[Language::MMENU_ANTIALIASINGYES]
                                                                                    : Language::Instance()[Language::MMENU_ANTIAlIASINGNO]);
      FColor = (OptionPos == 5) ? Highlighted : Normal;
      screen->SetFontColor(FColor);
      screen->Print(200, 5200,
                    (zoptions->ReadBool(zOPT_SEC_GAME, "joystick", 0) == 1) ? Language::Instance()[Language::MMENU_JOYSTICKYES]
                                                                            : Language::Instance()[Language::MMENU_JOYSTICKNO]);
      FColor = (OptionPos == 6) ? Highlighted : Normal;
      screen->SetFontColor(FColor);

      const auto chatlines_str = std::format("{} {}", Language::Instance()[Language::MMENU_CHATLINES].ToChar(), Config::Instance().ChatLines);
      screen->Print(200, 5600, chatlines_str.c_str());
      FColor = (OptionPos == 7) ? Highlighted : Normal;
      screen->SetFontColor(FColor);
      int printx = 200;
      int printy = 6000;
      switch (Config::Instance().keyboardlayout) {
        case Config::KEYBOARD_POLISH:
          screen->Print(printx, printy, Language::Instance()[Language::KEYBOARD_POLISH]);
          break;
        case Config::KEYBOARD_GERMAN:
          screen->Print(printx, printy, Language::Instance()[Language::KEYBOARD_GERMAN]);
          break;
        case Config::KEYBOARD_CYRYLLIC:
          screen->Print(printx, printy, Language::Instance()[Language::KEYBOARD_RUSSIAN]);
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
        languageOptionText += Language::Instance()[Language::LANGUAGE];
      screen->Print(200, 6400, languageOptionText);
      FColor = (OptionPos == 9) ? Highlighted : Normal;
      screen->SetFontColor(FColor);
      screen->Print(200, 6800,
                    (zoptions->ReadBool(zOPT_SEC_GAME, "playLogoVideos", 1) == 1) ? Language::Instance()[Language::INTRO_YES]
                                                                                  : Language::Instance()[Language::INTRO_NO]);
      FColor = (OptionPos == 10) ? Highlighted : Normal;
      screen->SetFontColor(FColor);
      screen->Print(200, 7200, Language::Instance()[Language::MMENU_BACK]);
    } break;
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
  Language::Instance().LoadFromJsonFile(path.ToChar());
  path.Clear();

  if (esl) {
    delete esl;
  }
  esl = new ExtendedServerList(server_list_);
  SelectedServer = 0;

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
}

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

void __stdcall CMainMenu::MainMenuLoop() {
  CMainMenu::GetInstance()->RenderMenu();
};

void CMainMenu::RenderMenu() {
  if (player->GetPositionWorld()[VX] != HeroPos[VX]) {
    player->trafoObjToWorld.SetTranslation(HeroPos);
    player->trafoObjToWorld.SetAtVector(Angle);
    player->trafoObjToWorld.SetRightVector(NAngle);
  }
  static bool fast_localhost_join = false;
  switch (MState) {
    case CHOOSE_LANGUAGE:
      if (Config::Instance().IsDefault()) {
        screen->Print(200, 200, "Choose your language:");
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
        screen->Print(200, 200, Language::Instance()[Language::WRITE_NICKNAME]);
        screen->Print(200 + static_cast<zINT>(static_cast<float>(Language::Instance()[Language::WRITE_NICKNAME].Length() * 70) * fWRatio), 200,
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
      SpeedUpTime();
      screen->SetFont(FDefault);
      screen->SetFontColor(Normal);
      screen->Print(8192 - screen->FontSize(GetVersionString()), 8192 - screen->FontY(), GetVersionString());
      static zSTRING fast_localhost_join_text = "F5 - Fast join localhost server";
      screen->Print(100, 8192 - screen->FontY(), fast_localhost_join_text);
      if (TitleWeapon)
        TitleWeapon->RotateWorldX(0.6f);
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
        if (!NetGame::Instance().Connect(ServerIP.ToChar())) {
          SPDLOG_ERROR("Unable to connect to the server.");
        }
        if (NetGame::Instance().IsConnected()) {
          NetGame::Instance().HandleNetwork();
          NetGame::Instance().SyncGameTime();
          zVEC3 SpawnpointPos = player->GetPositionWorld();
          CleanUpMainMenu();
          Patch::PlayerInterfaceEnabled(true);
          HooksManager::GetInstance()->RemoveHook(HT_RENDER, (DWORD)MainMenuLoop);
          if (!NetGame::Instance().map.IsEmpty()) {
            Patch::ChangeLevelEnabled(true);
            ogame->ChangeLevel(NetGame::Instance().map, zSTRING("????"));
            Patch::ChangeLevelEnabled(false);
          }
          DeleteAllNpcsAndDisableSpawning();
          player->trafoObjToWorld.SetTranslation(SpawnpointPos);
          CleanupWorldObjects(ogame->GetGameWorld());
          NetGame::Instance().JoinGame();
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
      screen->Print(100, 200, Language::Instance()[Language::APP_INFO1]);
      if (!AppCamCreated) {
        AppWeapon = zfactory->CreateItem(zCParser::GetParser()->GetIndex("ItMw_1h_Mil_Sword"));
        AppWeapon->SetPositionWorld(
            zVEC3(player->GetPositionWorld()[VX] - 78, player->GetPositionWorld()[VY] + 50, player->GetPositionWorld()[VZ] - 119));
        AppWeapon->RotateWorldY(30);
        AppWeapon->name.Clear();
        auto head_model = Gothic2APlayer::GetHeadModelNameFromByte(Config::Instance().headmodel);
        player->SetAdditionalVisuals("HUM_BODY_NAKED0", Config::Instance().skintexture, 0, head_model, Config::Instance().facetexture, 0, -1);
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
        zinput->ClearKeyBuffer();
        MState = MENU_LOOP;
        Config::Instance().SaveConfigToFile();
        ogame->CamInit(CamWeapon, zCCamera::activeCam);
      }
      switch (ChoosingApperance) {
        default:
        case ApperancePart::HEAD:
          screen->Print(500, 2000, Language::Instance()[Language::HEAD_MODEL]);
          if ((zinput->KeyToggled(KEY_LEFT))) {
            if (Config::Instance().headmodel > 0) {
              Config::Instance().headmodel--;
              auto head_model = Gothic2APlayer::GetHeadModelNameFromByte(Config::Instance().headmodel);
              player->SetAdditionalVisuals("HUM_BODY_NAKED0", Config::Instance().skintexture, 0, head_model, Config::Instance().facetexture, 0, -1);
            }
          }
          if ((zinput->KeyToggled(KEY_RIGHT))) {
            if (Config::Instance().headmodel < 5) {
              Config::Instance().headmodel++;
              auto head_model = Gothic2APlayer::GetHeadModelNameFromByte(Config::Instance().headmodel);
              player->SetAdditionalVisuals("HUM_BODY_NAKED0", Config::Instance().skintexture, 0, head_model, Config::Instance().facetexture, 0, -1);
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
          screen->Print(500, 2000, Language::Instance()[Language::FACE_APPERANCE]);
          if ((zinput->KeyToggled(KEY_LEFT))) {
            if (Config::Instance().facetexture > 0) {
              Config::Instance().facetexture--;
              auto head_model = Gothic2APlayer::GetHeadModelNameFromByte(Config::Instance().headmodel);
              player->SetAdditionalVisuals("HUM_BODY_NAKED0", Config::Instance().skintexture, 0, head_model, Config::Instance().facetexture, 0, -1);
            }
          }
          if ((zinput->KeyToggled(KEY_RIGHT))) {
            if (Config::Instance().facetexture < 162) {
              Config::Instance().facetexture++;
              auto head_model = Gothic2APlayer::GetHeadModelNameFromByte(Config::Instance().headmodel);
              player->SetAdditionalVisuals("HUM_BODY_NAKED0", Config::Instance().skintexture, 0, head_model, Config::Instance().facetexture, 0, -1);
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
          screen->Print(500, 2000, Language::Instance()[Language::SKIN_TEXTURE]);
          if (player->GetModel()->IsAnimationActive(WalkAnim))
            player->GetModel()->StopAnimation(WalkAnim);
          if ((zinput->KeyToggled(KEY_LEFT))) {
            if (Config::Instance().skintexture > 0) {
              Config::Instance().skintexture--;
              auto head_model = Gothic2APlayer::GetHeadModelNameFromByte(Config::Instance().headmodel);
              player->SetAdditionalVisuals("HUM_BODY_NAKED0", Config::Instance().skintexture, 0, head_model, Config::Instance().facetexture, 0, -1);
              player->GetModel()->meshLibList[0]->texAniState.actAniFrames[0][0] = Config::Instance().skintexture;
            }
          }
          if ((zinput->KeyToggled(KEY_RIGHT))) {
            if (Config::Instance().skintexture < 12) {
              Config::Instance().skintexture++;
              auto head_model = Gothic2APlayer::GetHeadModelNameFromByte(Config::Instance().headmodel);
              player->SetAdditionalVisuals("HUM_BODY_NAKED0", Config::Instance().skintexture, 0, head_model, Config::Instance().facetexture, 0, -1);
              player->GetModel()->meshLibList[0]->texAniState.actAniFrames[0][0] = Config::Instance().skintexture;
            }
          }
          break;
        case ApperancePart::WALKSTYLE:
          screen->Print(500, 2000, Language::Instance()[Language::WALK_STYLE]);
          if ((zinput->KeyPressed(KEY_LEFT))) {
            zinput->ClearKeyBuffer();
            if (Config::Instance().walkstyle > 0) {
              if (player->GetModel()->IsAnimationActive(WalkAnim))
                player->GetModel()->StopAnimation(WalkAnim);
              auto walkstyle_tmp = Gothic2APlayer::GetWalkStyleFromByte(Config::Instance().walkstyle);
              player->RemoveOverlay(walkstyle_tmp);
              Config::Instance().walkstyle--;
              walkstyle_tmp = Gothic2APlayer::GetWalkStyleFromByte(Config::Instance().walkstyle);
              player->ApplyOverlay(walkstyle_tmp);
            }
          }
          if ((zinput->KeyPressed(KEY_RIGHT))) {
            zinput->ClearKeyBuffer();
            if (Config::Instance().walkstyle < 6) {
              if (player->GetModel()->IsAnimationActive(WalkAnim))
                player->GetModel()->StopAnimation(WalkAnim);
              auto walkstyle_tmp = Gothic2APlayer::GetWalkStyleFromByte(Config::Instance().walkstyle);
              player->RemoveOverlay(walkstyle_tmp);
              Config::Instance().walkstyle++;
              walkstyle_tmp = Gothic2APlayer::GetWalkStyleFromByte(Config::Instance().walkstyle);
              player->ApplyOverlay(walkstyle_tmp);
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
}

void CMainMenu::SetServerIP(int selected) {
  if (selected >= 0) {
    if (!ServerIP.IsEmpty())
      ServerIP.Clear();
    char buffer[128];
    sprintf(buffer, "%s:%hu\0", server_list_.At((size_t)selected)->ip.ToChar(), server_list_.At((size_t)selected)->port);
    ServerIP = buffer;
  }
}
