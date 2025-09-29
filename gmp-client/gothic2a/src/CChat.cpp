
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

#include <algorithm>

#include "CLanguage.h"
#include "config.h"

using namespace Gothic_II_Addon;
namespace {
constexpr auto CHAT_FADE_DURATION = std::chrono::milliseconds(400);

void UpdateMessageAlpha(MsgStruct& message, const std::chrono::steady_clock::time_point& now) {
  const unsigned char target_alpha = message.MsgColor.alpha;
  if (!message.IsFadingIn) {
    message.CurrentAlpha = target_alpha;
    return;
  }

  if (target_alpha == 0) {
    message.CurrentAlpha = 0;
    message.IsFadingIn = false;
    return;
  }

  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - message.FadeStart);
  if (elapsed.count() >= CHAT_FADE_DURATION.count()) {
    message.CurrentAlpha = target_alpha;
    message.IsFadingIn = false;
    return;
  }

  const float progress = static_cast<float>(elapsed.count()) / static_cast<float>(CHAT_FADE_DURATION.count());
  const float alpha_value = std::min(progress, 1.0f) * static_cast<float>(target_alpha);
  message.CurrentAlpha = static_cast<unsigned char>(alpha_value);
}
}  // namespace

extern zCOLOR Normal;
extern CLanguage* Lang;

CChat::CChat() {
  PrintMsgType = NORMAL;
  tmpanimname = "NULL";
  tmpnickname = Config::Instance().Nickname;
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
  msg.FadeStart = std::chrono::steady_clock::now();
  msg.CurrentAlpha = 0;
  msg.IsFadingIn = true;
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
          tmpnickname = Config::Instance().Nickname;
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
  if (Config::Instance().logchat) {
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
  msg.FadeStart = std::chrono::steady_clock::now();
  msg.CurrentAlpha = 0;
  msg.IsFadingIn = true;
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
          tmpnickname = Config::Instance().Nickname;
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
  if (Config::Instance().logchat) {
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
  const auto now = std::chrono::steady_clock::now();
  if (zinput->KeyToggled(KEY_F5) && PrintMsgType != NORMAL)
    PrintMsgType = NORMAL;
  if (zinput->KeyToggled(KEY_F6) && PrintMsgType != WHISPER)
    PrintMsgType = WHISPER;
  if (zinput->KeyToggled(KEY_F7) && PrintMsgType != ADMIN)
    PrintMsgType = ADMIN;
  switch (PrintMsgType) {
    case NORMAL:
      if (ChatMessages.size() > Config::Instance().ChatLines)
        ChatMessages.erase(ChatMessages.begin());
      if (!ChatMessages.empty())
        for (size_t v = 0; v < ChatMessages.size(); v++) {
          UpdateMessageAlpha(ChatMessages[v], now);
          zCOLOR color = ChatMessages[v].MsgColor;
          color.alpha = ChatMessages[v].CurrentAlpha;
          screen->SetFontColor(color);
          screen->Print(0, static_cast<int>(v) * 200, ChatMessages[v].Message);
          screen->SetFontColor(Normal);
        }
      break;
    case WHISPER:
      if (WhisperMessages.size() > Config::Instance().ChatLines + 1)
        WhisperMessages.erase(WhisperMessages.begin() + 1);
      if (!WhisperMessages.empty())
        for (size_t v = 0; v < WhisperMessages.size(); v++) {
          UpdateMessageAlpha(WhisperMessages[v], now);
          zCOLOR color = WhisperMessages[v].MsgColor;
          color.alpha = WhisperMessages[v].CurrentAlpha;
          screen->SetFontColor(color);
          screen->Print(0, static_cast<int>(v) * 200, WhisperMessages[v].Message);
          screen->SetFontColor(Normal);
        }
      break;
    case ADMIN:
      if (AdminMessages.size() > Config::Instance().ChatLines)
        AdminMessages.erase(AdminMessages.begin());
      if (!AdminMessages.empty())
        for (size_t v = 0; v < AdminMessages.size(); v++) {
          UpdateMessageAlpha(AdminMessages[v], now);
          zCOLOR color = AdminMessages[v].MsgColor;
          color.alpha = AdminMessages[v].CurrentAlpha;
          screen->SetFontColor(color);
          screen->Print(0, static_cast<int>(v) * 200, AdminMessages[v].Message);
          screen->SetFontColor(Normal);
        }
      break;
  };
};