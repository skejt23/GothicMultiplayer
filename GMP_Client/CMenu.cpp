
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
**																			**
**	File name:		Interface/CMenu.cpp		   								**
**																			**
**	Created by:		02/04/11	-	skejt23									**
**																			**
**	Description:	Multiplayer menu functionallity	 						**
**																			**
*****************************************************************************/

//** Includes
#include "CMenu.h"
//**

//** Globals
constexpr const char* GlobalFont = "FONT_OLD_20_WHITE.TGA";
constexpr const char* GlobalBack = "MENU_INGAME.TGA";
constexpr const char* Arrow = "-->";
extern std::vector<CMenu*> MenuList;
//**

CMenu::CMenu(zSTRING MenuTitle, zCOLOR TitleTextColor, int sizex, int sizey) // Menu Constructor
{
	MainWindow = new zCView(0,0,8192,8192,VIEW_ITEM);
	MainWindow->SetFont(GlobalFont);
	MainWindow->SetPos(2500,2000);
	MainWindow->SetSize(sizex,sizey);
	MainWindow->InsertBack(GlobalBack);
	TitleText = new zCView(0,0,8192,8192,VIEW_ITEM);
	TitleText->SetFontColor(TitleTextColor);
	TitleText->SetFont(GlobalFont);
	TitleText->SetPos(2550,1700);
	TitleText->SetSize(4000,2000);
	MainWindow->Open();
	TitleText->Print(0,0,MenuTitle);
	Opened = true;
	WasClosed = true;
	time = clock() + 400;
	Counting = true;
	ArrowPos = 1;
	if(oCNpcInventory::IsHeroInventoryOpened()) oCNpc::GetHero()->GetInventory()->Close();
	Patch::PlayerInterfaceEnabled(false);
	oCNpc::GetHero()->SetMovLock(1);
	oCNpc::GetHero()->GetAnictrl()->StopTurnAnis();
	ArrowsInit();
	MenuList.push_back(this);
};

CMenu::~CMenu()
{
	MainWindow->Close();
	zCView::GetScreen()->RemoveItem(TitleText);
	zCView::GetScreen()->RemoveItem(Arrows);
	delete MainWindow;
	delete Arrows;
	delete TitleText;
	for (int i = 0; i < (int)MenuList.size(); i++){
		if(MenuList[i] == this) MenuList.erase(MenuList.begin()+i);
	}
};

void CMenu::AddMenuItem(zSTRING Text, DWORD Function)
{
	MenuItem list;
	list.Text = Text;
	list.Function = Function;
	list.Function2 = NULL;
	MenuItems.push_back(list);
	MainWindow->Print(1500, MenuItems.size() * 600, Text);
};

void CMenu::AddMenuItemValueChange(zSTRING Text, DWORD IncreaseFunction, DWORD DecreaseFunction, DWORD PrintValue)
{
	MenuItem list;
	list.Text = Text;
	list.Function = IncreaseFunction;
	list.Function2 = DecreaseFunction;
	ValuePrint.push_back(PrintValue);
	MenuItems.push_back(list);
	MainWindow->Print(1500, MenuItems.size() * 600, Text);
};

void CMenu::Open()
{
	for (int i = 0; i < (int)MenuList.size(); i++)
	{
		if(MenuList[i]){
			if(MenuList[i]->IsOpened()) MenuList[i]->Close();
		}
	}
	if(oCNpcInventory::IsHeroInventoryOpened()) oCNpc::GetHero()->GetInventory()->Close();
	Patch::SetOpenInventoryEnabled(false);
	Patch::SetStatusScreenEnabled(false);
	oCNpc::GetHero()->GetAnictrl()->StopTurnAnis();
	MainWindow->Open();
	for (int i = 0; i < (int)MenuItems.size(); i++)
	{
		MenuItem item = (MenuItem)MenuItems[i];
		int multiplier = i;
		multiplier++;
		MainWindow->Print(1500, multiplier * 600, item.Text);
	}
	Opened = true;
};

void CMenu::Close()
{
	if(!Counting){
	oCNpc::GetHero()->SetMovLock(0);
	Patch::SetOpenInventoryEnabled(true);
	Patch::SetStatusScreenEnabled(true);
	MainWindow->Close();
	zCView::GetScreen()->RemoveItem(TitleText);
	zCView::GetScreen()->RemoveItem(Arrows);
	Opened = false;
	WasClosed = true;
	}
};

bool CMenu::IsOpened()
{
	if(Opened) return true;
	else return false;
};

zVEC2 CMenu::GetPosition()
{
	int x,y;
	MainWindow->GetPos(x,y);
	return zVEC2((float)x,(float)y);
};

void CMenu::ArrowsInit()
{
	Arrows = new zCView(0,0,8192,8192,VIEW_ITEM);
	Arrows->SetPos(2500,2000);
	Arrows->SetSize(3500,4000);
	Arrows->SetFontColor(zCOLOR(128,0,0));
	Arrows->SetFont(GlobalFont);
	Arrows->Print(500, ArrowPos * 600, Arrow);
};

void CMenu::RenderArrows()
{
	if(Arrows){
	zCView::GetScreen()->RemoveItem(Arrows);
	delete Arrows;
	Arrows = NULL;}
	Arrows = new zCView(0,0,8192,8192,VIEW_ITEM);
	Arrows->SetPos(2500,2000);
	Arrows->SetSize(3500,4000);
	Arrows->SetFontColor(zCOLOR(128,0,0));
	Arrows->SetFont(GlobalFont);
	Arrows->Print(500, ArrowPos * 600, Arrow);
	zCView::GetScreen()->InsertItem(Arrows);
};

void CMenu::RenderMenu()
{
	if(!oCNpc::GetHero()->IsMovLock()) oCNpc::GetHero()->SetMovLock(1);
	oCNpc::GetHero()->GetAnictrl()->StopTurnAnis();
	if(!WasClosed){
	if(ValuePrint.size() > 0){
		for (int i = 0; i < (int)ValuePrint.size(); i++)
		{
			DWORD function = ValuePrint[i];
			__asm call function;
		}
	}
	if(zCInput::GetInput()->KeyToggled( KEY_UP )){
		ArrowPos--;
		if(ArrowPos < 1) ArrowPos = (int)MenuItems.size();
		RenderArrows();
	}
	if(zCInput::GetInput()->KeyToggled( KEY_DOWN )){
		ArrowPos++;
		if(ArrowPos > (int)MenuItems.size()) ArrowPos = 1;
		RenderArrows();
	}
	if(zCInput::GetInput()->KeyPressed( KEY_LEFT )){
		MenuItem item = (MenuItem)MenuItems[ArrowPos-1];
		if(item.Function2 != NULL){
			DWORD function = (DWORD)item.Function2;
			__asm call function;
		}
	}
	if(zCInput::GetInput()->KeyPressed( KEY_RIGHT )){
		MenuItem item = (MenuItem)MenuItems[ArrowPos-1];
		if(item.Function2 != NULL){
			DWORD function = (DWORD)item.Function;
			__asm call function;
		}
	}
	if(zCInput::GetInput()->KeyPressed(KEY_RETURN))
	{
		MenuItem item = (MenuItem)MenuItems[ArrowPos-1];
		if(item.Function2 == NULL){
		Opened = false;
		WasClosed = true;
		Close();
		DWORD function = (DWORD)item.Function;
		__asm call function;
		}
	}}
	else{
		if(!Counting){
			time = clock() + 300;
			Counting = true;
		}
		if(clock() > time){
			if(Opened){
			zCView::GetScreen()->InsertItem(TitleText);
			zCView::GetScreen()->InsertItem(Arrows);}
			WasClosed = false;
			Counting = false;
		}
	}
};