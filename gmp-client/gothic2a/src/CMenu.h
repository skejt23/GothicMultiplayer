
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
** ** *	File name:		Interface/CMenu.h		  								** *
*** *	Created by:		02/04/11	-	skejt23									** *
*** *	Description:	Multiplayer menu functionallity	 					** *
***
*****************************************************************************/

//** Includes
#pragma once
#include <ctime>
#include <vector>

#include "ZenGin/zGothicAPI.h"
#include "patch.h"
//**

class CMenu {
private:
  Gothic_II_Addon::zCView* MainWindow;
  Gothic_II_Addon::zCView* TitleText;
  Gothic_II_Addon::zCView* Arrows;
  bool Opened;
  bool WasClosed;
  bool Counting;
  void RenderArrows();
  void ArrowsInit();
  int ArrowPos;
  struct MenuItem {
    Gothic_II_Addon::zSTRING Text;
    DWORD Function;
    DWORD Function2;
  };
  std::vector<MenuItem> MenuItems;
  std::vector<DWORD> ValuePrint;
  clock_t time;

public:
  CMenu(Gothic_II_Addon::zSTRING MenuTitle, Gothic_II_Addon::zCOLOR TitleTextColor, int sizex, int sizey);
  ~CMenu();
  void AddMenuItem(Gothic_II_Addon::zSTRING Text, DWORD Function);
  void AddMenuItemValueChange(Gothic_II_Addon::zSTRING Text, DWORD IncreaseFunction, DWORD DecreaseFunction, DWORD GetFunction);
  void Open();
  void Close();
  bool IsOpened();
  Gothic_II_Addon::zVEC2 GetPosition();
  void RenderMenu();
};