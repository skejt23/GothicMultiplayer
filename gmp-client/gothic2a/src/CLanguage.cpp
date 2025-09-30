
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

#pragma warning(disable : 4172)

#include "CLanguage.h"

#include <spdlog/spdlog.h>

#include <array>
#include <fstream>
#include <nlohmann/json.hpp>

#include "localization_utils.h"

using namespace Gothic_II_Addon;

namespace {
constexpr std::size_t kStringCount = static_cast<std::size_t>(CLanguage::SRVLIST_PLAYERNUMBER) + 1;

const std::array<const char*, kStringCount> kStringKeys = {"LANGUAGE",
                                                           "WRITE_NICKNAME",
                                                           "CHOOSE_APPERANCE",
                                                           "FACE_APPERANCE",
                                                           "HEAD_MODEL",
                                                           "SKIN_TEXTURE",
                                                           "CHOOSE_SERVER",
                                                           "WRITE_SERVER_ADDR",
                                                           "CHOOSE_SERVER_TIP",
                                                           "MANUAL_IP_TIP",
                                                           "MMENU_CHSERVER",
                                                           "MMENU_OPTIONS",
                                                           "MMENU_APPEARANCE",
                                                           "MMENU_LEAVEGAME",
                                                           "EMPTY_SERVER_LIST",
                                                           "APP_INFO1",
                                                           "ERR_CONN_NO_ERROR",
                                                           "ERR_CONN_FAIL",
                                                           "ERR_CONN_ALREADY_CONNECTED",
                                                           "ERR_CONN_SRV_FULL",
                                                           "ERR_CONN_BANNED",
                                                           "ERR_CONN_INCOMP_TECHNIC",
                                                           "MMENU_ONLINEOPTIONS",
                                                           "MMENU_BACK",
                                                           "SELECT_CONTROLS",
                                                           "MMENU_LOGCHATYES",
                                                           "MMENU_LOGCHATNO",
                                                           "MMENU_WATCHON",
                                                           "MMENU_WATCHOFF",
                                                           "MMENU_SETWATCHPOS",
                                                           "CWATCH_REALTIME",
                                                           "CWATCH_GAMETIME",
                                                           "MMENU_NICKNAME",
                                                           "MMENU_ANTIALIASINGYES",
                                                           "MMENU_ANTIAlIASINGNO",
                                                           "MMENU_JOYSTICKYES",
                                                           "MMENU_JOYSTICKNO",
                                                           "MMENU_POTIONKEYSYES",
                                                           "MMENU_POTIONKEYSNO",
                                                           "MMENU_CHATLINES",
                                                           "INGAMEM_HELP",
                                                           "INGAMEM_BACKTOGAME",
                                                           "HCONTROLS",
                                                           "HCHAT",
                                                           "HCHATMAIN",
                                                           "HCHATWHISPER",
                                                           "CHAT_WHISPERTONOONE",
                                                           "CHAT_WHISPERTO",
                                                           "CHAT_CANTWHISPERTOYOURSELF",
                                                           "SOMEONEDISCONNECT_FROM_SERVER",
                                                           "NOPLAYERS",
                                                           "EXITTOMAINMENU",
                                                           "SRV_IP",
                                                           "SRV_NAME",
                                                           "SRV_MAP",
                                                           "SRV_PLAYERS",
                                                           "DISCONNECTED",
                                                           "SOMEONE_JOIN_GAME",
                                                           "CHAT_PLAYER_DOES_NOT_EXIST",
                                                           "ANIMS_MENU",
                                                           "HPLAYERLIST",
                                                           "HMAP",
                                                           "HANIMSMENU",
                                                           "SHOWHOW",
                                                           "WHISPERSTOYOU",
                                                           "PRESSFORWHISPER",
                                                           "CHAT_WRONGWINDOW",
                                                           "WALK_STYLE",
                                                           "UNMUTE_TIME",
                                                           "PLIST_PM",
                                                           "KILLEDSOMEONE_MSG",
                                                           "WB_NEWMAP",
                                                           "WB_LOADMAP",
                                                           "WB_SAVEMAP",
                                                           "KILL_PLAYER",
                                                           "GOTO_PLAYER",
                                                           "ITEM_TOOFAR",
                                                           "KEYBOARD_POLISH",
                                                           "KEYBOARD_GERMAN",
                                                           "KEYBOARD_RUSSIAN",
                                                           "INTRO_YES",
                                                           "INTRO_NO",
                                                           "MERRY_CHRISTMAS",
                                                           "INV_HOWMUCH",
                                                           "CLASS_DESCRIPTION",
                                                           "START_OBSERVATION",
                                                           "END_OBSERVATION",
                                                           "SRVLIST_ALL",
                                                           "SRVLIST_FAVOURITES",
                                                           "SRVLIST_NAME",
                                                           "SRVLIST_MAP",
                                                           "SRVLIST_PLAYERNUMBER"};
}  // namespace

CLanguage::CLanguage(const char* file) {
  std::ifstream ifs(file);
  if (!ifs.good()) {
    MessageBoxA(NULL, file, "Can not open file!", MB_ICONERROR);
    return;
  }
  nlohmann::json json_data;
  try {
    ifs >> json_data;
  } catch (const std::exception& ex) {
    SPDLOG_ERROR("Failed to parse language file {}: {}", file, ex.what());
    MessageBoxA(NULL, file, "Invalid language file!", MB_ICONERROR);
    return;
  }

  const std::string language_field = json_data.value("LANGUAGE", std::string{});
  const auto encoding = localization::DetectLanguageEncoding(language_field, file);

  data.resize(kStringCount);
  for (std::size_t i = 0; i < kStringKeys.size(); ++i) {
    const auto key = kStringKeys[i];
    std::string value;
    try {
      if (!json_data.contains(key)) {
        SPDLOG_WARN("Missing language key '{}' in file {}", key, file);
      }
      value = json_data.value(key, std::string{});
    } catch (const std::exception& ex) {
      SPDLOG_WARN("Language key '{}' in file {} has incompatible type: {}", key, file, ex.what());
    }
    value = localization::ConvertFromUtf8(value, encoding);
    data[i] = value.c_str();
  }
}

void CLanguage::RemovePolishCharactersFromWideString(std::wstring& txt) {
  wchar_t letter[1] = {0x22};
  size_t found;
  found = 0;
  while (found != std::string::npos) {
    found = txt.find(L"Ż");
    if (found == std::string::npos)
      break;
    else
      txt.replace(found, 1, L"Z");
  }
  found = 0;
  while (found != std::string::npos) {
    found = txt.find(L"&#8221;");
    if (found == std::string::npos)
      break;
    else
      txt.replace(found, 7, letter);
  }
  found = 0;
  while (found != std::string::npos) {
    found = txt.find(L"&#8211;");
    if (found == std::string::npos)
      break;
    else
      txt.replace(found, 7, L"-");
  }
  found = 0;
  while (found != std::string::npos) {
    found = txt.find(L"ż");
    if (found == std::string::npos)
      break;
    else
      txt.replace(found, 1, L"z");
  }
  found = 0;
  while (found != std::string::npos) {
    found = txt.find(L"ł");
    if (found == std::string::npos)
      break;
    else
      txt.replace(found, 1, L"l");
  }
  found = 0;
  while (found != std::string::npos) {
    found = txt.find(L"Ł");
    if (found == std::string::npos)
      break;
    else
      txt.replace(found, 1, L"L");
  }
  found = 0;
  while (found != std::string::npos) {
    found = txt.find(L"ą");
    if (found == std::string::npos)
      break;
    else
      txt.replace(found, 1, L"a");
  }
  found = 0;
  while (found != std::string::npos) {
    found = txt.find(L"ń");
    if (found == std::string::npos)
      break;
    else
      txt.replace(found, 1, L"n");
  }
  found = 0;
  while (found != std::string::npos) {
    found = txt.find(L"ę");
    if (found == std::string::npos)
      break;
    else
      txt.replace(found, 1, L"e");
  }
  found = 0;
  while (found != std::string::npos) {
    found = txt.find(L"ś");
    if (found == std::string::npos)
      break;
    else
      txt.replace(found, 1, L"s");
  }
  found = 0;
  while (found != std::string::npos) {
    found = txt.find(L"ć");
    if (found == std::string::npos)
      break;
    else
      txt.replace(found, 1, L"c");
  }
  found = 0;
  while (found != std::string::npos) {
    found = txt.find(L"ź");
    if (found == std::string::npos)
      break;
    else
      txt.replace(found, 1, L"z");
  }
};

CLanguage::~CLanguage(void) {
  if (!data.empty())
    data.clear();
}

zSTRING& CLanguage::operator[](unsigned long i) {
  static zSTRING Empty = "EMPTY";
  return (i < data.size()) ? data[i] : Empty;
}