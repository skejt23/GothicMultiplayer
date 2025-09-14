
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
** ** *	File name:		Interface/CChat.cpp		   								** *
*** *	Created by:		29/06/11	-	skejt23									** *
*** *	Description:	Multiplayer chat functionallity	 						** *
***
*****************************************************************************/

#pragma warning(disable : 4018)

#include "CChat.h"

#include <spdlog/spdlog.h>

#include "CLanguage.h"

using namespace Gothic_II_Addon;

extern zCOLOR Normal;
extern CLanguage* Lang;

CChat::CChat() {
  PrintMsgType = NORMAL;
  tmpanimname = "NULL";
  tmpnickname = CConfig::GetInstance()->Nickname;
  ShowHow = false;
  WriteMessage(WHISPER, false, zCOLOR(0, 255, 0), (*Lang)[CLanguage::CHAT_WHISPERTONOONE].ToChar());
};

CChat::~CChat() {
  ChatMessages.clear();
  WhisperMessages.clear();
  AdminMessages.clear();
  PrintMsgType = NORMAL;
};

void CChat::StartChatAnimation(int anim) {
  if (player->IsDead() || player->GetAnictrl()->IsRunning() || player->GetAnictrl()->IsInWater() || player->GetAnictrl()->IsFallen()) {
    return;
  }
  if (!player->GetModel()->IsAnimationActive(tmpanimname)) {
    tmpanimname = fmt::format("T_DIALOGGESTURE_{:02}", anim).c_str();
    player->GetModel()->StartAnimation(tmpanimname);
  }
}

void CChat::SetWhisperTo(std::string& whisperto) {
  sprintf(buffer, "%s : %s", (*Lang)[CLanguage::CHAT_WHISPERTO].ToChar(), whisperto.c_str());
  WhisperMessages[0].Message = buffer;
};

void CChat::WriteMessage(MsgType type, bool PrintTimed, const zCOLOR& rgb, const char* format, ...) {
  if (strlen(format) > 512)
    return;
  char text[512];
  va_list args;
  va_start(args, format);
  vsprintf(text, format, args);
  va_end(args);
  MsgStruct msg;
  msg.Message = text;
  msg.MsgColor = rgb;
  switch (type) {
    case NORMAL:
      if (PrintTimed) {
        ogame->array_view[oCGame::GAME_VIEW_SCREEN]->SetFont("FONT_DEFAULT.TGA");
        tmp = text;
        ogame->array_view[oCGame::GAME_VIEW_SCREEN]->PrintTimed(3700, 2800, tmp, 3000.0f, 0);
      }
      ChatMessages.push_back(msg);
      break;
    case WHISPER:
      WhisperMessages.push_back(msg);
      if (PrintTimed) {
        if (PrintMsgType != WHISPER) {
          tmp = text;
          tmpnickname = CConfig::GetInstance()->Nickname;
          if (tmp.Search(tmpnickname) < 2) {
            ogame->array_view[oCGame::GAME_VIEW_SCREEN]->SetFont("FONT_DEFAULT.TGA");
            ogame->array_view[oCGame::GAME_VIEW_SCREEN]->PrintTimed(3700, 2800, (*Lang)[CLanguage::WHISPERSTOYOU], 3000.0f, 0);
            ogame->array_view[oCGame::GAME_VIEW_SCREEN]->PrintTimed(3700, 3000, tmp, 3000.0f, 0);
            if (!ShowHow) {
              ogame->array_view[oCGame::GAME_VIEW_SCREEN]->PrintTimed(3700, 3200, (*Lang)[CLanguage::PRESSFORWHISPER], 3000.0f, 0);
              ShowHow = true;
            }
          }
        }
      }
      break;
    case ADMIN:
      AdminMessages.push_back(msg);
      break;
  };
  if (CConfig::GetInstance()->logchat) {
    SPDLOG_INFO("{}", text);
  }
};

void CChat::WriteMessage(MsgType type, bool PrintTimed, const char* format, ...) {
  if (strlen(format) > 512)
    return;
  char text[512];
  va_list args;
  va_start(args, format);
  vsprintf(text, format, args);
  va_end(args);
  MsgStruct msg;
  msg.Message = text;
  msg.MsgColor = Normal;
  switch (type) {
    case NORMAL:
      if (PrintTimed) {
        ogame->array_view[oCGame::GAME_VIEW_SCREEN]->SetFont("FONT_DEFAULT.TGA");
        tmp = text;
        ogame->array_view[oCGame::GAME_VIEW_SCREEN]->PrintTimed(3700, 2800, tmp, 3000.0f, 0);
      }
      ChatMessages.push_back(msg);
      break;
    case WHISPER:
      WhisperMessages.push_back(msg);
      if (PrintTimed) {
        if (PrintMsgType != WHISPER) {
          tmp = text;
          tmpnickname = CConfig::GetInstance()->Nickname;
          if (tmp.Search(tmpnickname) < 2) {
            ogame->array_view[oCGame::GAME_VIEW_SCREEN]->SetFont("FONT_DEFAULT.TGA");
            ogame->array_view[oCGame::GAME_VIEW_SCREEN]->PrintTimed(3700, 2800, (*Lang)[CLanguage::WHISPERSTOYOU], 3000.0f, 0);
            ogame->array_view[oCGame::GAME_VIEW_SCREEN]->PrintTimed(3700, 3000, tmp, 3000.0f, 0);
            if (!ShowHow) {
              ogame->array_view[oCGame::GAME_VIEW_SCREEN]->PrintTimed(3700, 3200, (*Lang)[CLanguage::PRESSFORWHISPER], 3000.0f, 0);
              ShowHow = true;
            }
          }
        }
      }
      break;
    case ADMIN:
      // printf("Executed!\n");
      AdminMessages.push_back(msg);
      break;
  };
  if (CConfig::GetInstance()->logchat) {
    SPDLOG_INFO("{}", text);
  }
};

void CChat::ClearChat() {
  ChatMessages.clear();
  WhisperMessages.clear();
  AdminMessages.clear();
  WriteMessage(WHISPER, false, zCOLOR(0, 255, 0), (*Lang)[CLanguage::CHAT_WHISPERTONOONE].ToChar());
};

void CChat::PrintChat() {
  screen->SetFont("FONT_DEFAULT.TGA");
  if (zinput->KeyToggled(KEY_F5) && PrintMsgType != NORMAL)
    PrintMsgType = NORMAL;
  if (zinput->KeyToggled(KEY_F6) && PrintMsgType != WHISPER)
    PrintMsgType = WHISPER;
  if (zinput->KeyToggled(KEY_F7) && PrintMsgType != ADMIN)
    PrintMsgType = ADMIN;
  switch (PrintMsgType) {
    case NORMAL:
      if (ChatMessages.size() > CConfig::GetInstance()->ChatLines)
        ChatMessages.erase(ChatMessages.begin());
      if ((int)ChatMessages.size() > 0)
        for (int v = 0; v < (int)ChatMessages.size(); v++) {
          screen->SetFontColor(ChatMessages[v].MsgColor);
          screen->Print(0, v * 200, ChatMessages[v].Message);
          screen->SetFontColor(Normal);
        }
      break;
    case WHISPER:
      if (WhisperMessages.size() > CConfig::GetInstance()->ChatLines + 1)
        WhisperMessages.erase(WhisperMessages.begin() + 1);
      if ((int)WhisperMessages.size() > 0)
        for (int v = 0; v < (int)WhisperMessages.size(); v++) {
          screen->SetFontColor(WhisperMessages[v].MsgColor);
          screen->Print(0, v * 200, WhisperMessages[v].Message);
          screen->SetFontColor(Normal);
        }
      break;
    case ADMIN:
      if (AdminMessages.size() > CConfig::GetInstance()->ChatLines)
        AdminMessages.erase(AdminMessages.begin());
      if ((int)AdminMessages.size() > 0)
        for (int v = 0; v < (int)AdminMessages.size(); v++) {
          screen->SetFontColor(AdminMessages[v].MsgColor);
          screen->Print(0, v * 200, AdminMessages[v].Message);
          screen->SetFontColor(Normal);
        }
      break;
  };
};