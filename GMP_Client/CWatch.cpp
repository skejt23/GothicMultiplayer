
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
**	File name:		Interface/CWatch.cpp
**	Created by:		01/07/11	-	skejt23
**	Description:	Watch functionallity
**
*****************************************************************************/

#include "CWatch.h"

#include <time.h>

using namespace Gothic_II_Addon;

extern CLanguage* Lang;

CWatch::CWatch() {
  Con = CConfig::GetInstance();
};

void CWatch::PrintWatch() {
  time(&currtime);
  strftime(TimePrint, sizeof(TimePrint) - 1, "%H:%M:%S", localtime(&currtime));
  tmp = TimePrint;
  screen->SetFont("FONT_DEFAULT.TGA");
  screen->SetFontColor({255, 255, 255});
  screen->Print(Con->WatchPosX, Con->WatchPosY, (*Lang)[CLanguage::CWATCH_REALTIME]);
  screen->Print(Con->WatchPosX, Con->WatchPosY + 200, tmp);
  screen->Print(Con->WatchPosX, Con->WatchPosY + 400, (*Lang)[CLanguage::CWATCH_GAMETIME]);
  ogame->GetWorldTimer()->GetTime(H, M);
  sprintf(TimePrint, "%d:%.2d", H, M);
  tmp = TimePrint;
  screen->Print(Con->WatchPosX, Con->WatchPosY + 600, tmp);
};