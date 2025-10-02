
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

#include <string>
#include <vector>

#include "ZenGin/zGothicAPI.h"

class CLanguage {
public:
  enum STRING_ID {
    LANGUAGE = 0,
    WRITE_NICKNAME,
    CHOOSE_APPERANCE,
    FACE_APPERANCE,
    HEAD_MODEL,
    SKIN_TEXTURE,
    CHOOSE_SERVER,
    WRITE_SERVER_ADDR,
    CHOOSE_SERVER_TIP,
    MANUAL_IP_TIP,
    MMENU_CHSERVER,
    MMENU_OPTIONS,
    MMENU_APPEARANCE,
    MMENU_LEAVEGAME,
    EMPTY_SERVER_LIST,
    APP_INFO1,
    ERR_CONN_NO_ERROR,
    ERR_CONN_FAIL,
    ERR_CONN_ALREADY_CONNECTED,
    ERR_CONN_SRV_FULL,
    ERR_CONN_BANNED,
    ERR_CONN_INCOMP_TECHNIC,
    MMENU_ONLINEOPTIONS,
    MMENU_BACK,
    SELECT_CONTROLS,
    MMENU_LOGCHATYES,
    MMENU_LOGCHATNO,
    MMENU_WATCHON,
    MMENU_WATCHOFF,
    MMENU_SETWATCHPOS,
    CWATCH_REALTIME,
    CWATCH_GAMETIME,
    MMENU_NICKNAME,
    MMENU_ANTIALIASINGYES,
    MMENU_ANTIAlIASINGNO,
    MMENU_JOYSTICKYES,
    MMENU_JOYSTICKNO,
    MMENU_POTIONKEYSYES,
    MMENU_POTIONKEYSNO,
    MMENU_CHATLINES,
    INGAMEM_HELP,
    INGAMEM_BACKTOGAME,
    HCONTROLS,
    HCHAT,
    HCHATMAIN,
    HCHATWHISPER,
    CHAT_WHISPERTONOONE,
    CHAT_WHISPERTO,
    CHAT_CANTWHISPERTOYOURSELF,
    SOMEONEDISCONNECT_FROM_SERVER,
    NOPLAYERS,
    EXITTOMAINMENU,
    SRV_IP,
    SRV_NAME,
    SRV_MAP,
    SRV_PLAYERS,
    DISCONNECTED,
    SOMEONE_JOIN_GAME,
    CHAT_PLAYER_DOES_NOT_EXIST,
    ANIMS_MENU,
    HPLAYERLIST,
    HMAP,
    HANIMSMENU,
    SHOWHOW,
    WHISPERSTOYOU,
    PRESSFORWHISPER,
    CHAT_WRONGWINDOW,
    WALK_STYLE,
    UNMUTE_TIME,
    PLIST_PM,
    KILLEDSOMEONE_MSG,
    WB_NEWMAP,
    WB_LOADMAP,
    WB_SAVEMAP,
    KILL_PLAYER,
    GOTO_PLAYER,
    ITEM_TOOFAR,
    KEYBOARD_POLISH,
    KEYBOARD_GERMAN,
    KEYBOARD_RUSSIAN,
    INTRO_YES,
    INTRO_NO,
    MERRY_CHRISTMAS,
    INV_HOWMUCH,
    CLASS_DESCRIPTION,
    START_OBSERVATION,
    END_OBSERVATION,
    SRVLIST_ALL,
    SRVLIST_FAVOURITES,
    SRVLIST_NAME,
    SRVLIST_MAP,
    SRVLIST_PLAYERNUMBER
  };
  CLanguage(const char* file);
  ~CLanguage(void);
  Gothic_II_Addon::zSTRING& operator[](unsigned long);
  void RemovePolishCharactersFromWideString(std::wstring& txt);

private:
  std::vector<Gothic_II_Addon::zSTRING> data;
};