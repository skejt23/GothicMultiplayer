
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

#include "CObjectMenu.h"
#include "WorldBuilder\CBuilder.h"

extern zCOLOR Highlighted;
extern zCOLOR Normal;
extern zCOLOR FColors;
extern CBuilder* Builder;
bool Rotating = true;

CObjectMenu::CObjectMenu()
{
	MenuPos = 0, PrintFrom = 0, PrintTo = 0;
	CBuilder::SaveVobMatrix(Builder->SpawnedVobs[0].Vob, LastAngle);
	zCAICamera::GetCurrent()->SetTarget(Builder->SpawnedVobs[0].Vob);
};

CObjectMenu::~CObjectMenu()
{
	MenuPos = 0, PrintFrom = 0, PrintTo = 0;
	zCView::GetScreen()->SetFontColor(Normal);
};

void CObjectMenu::Draw()
{
	if(!oCNpc::GetHero()->IsMovLock()) oCNpc::GetHero()->SetMovLock(1);
	// INPUT
	zCInput* input = zCInput::GetInput();
	if(input->KeyToggled(KEY_DELETE)){
		Builder->SpawnedVobs[MenuPos].Vob->RemoveVobFromWorld();
		Builder->SpawnedVobs.erase(Builder->SpawnedVobs.begin()+MenuPos);
		if(MenuPos > 0){
			MenuPos--;
			zCAICamera::GetCurrent()->SetTarget(Builder->SpawnedVobs[MenuPos].Vob);
			CBuilder::SaveVobMatrix(Builder->SpawnedVobs[MenuPos].Vob, LastAngle);

		}
		if(PrintFrom > 0){
			if(MenuPos > PrintFrom) PrintFrom--;
		}

	}
	if(input->KeyToggled(KEY_SPACE)){
		Rotating = !Rotating;
		CBuilder::RestoreVobMatrix(Builder->SpawnedVobs[MenuPos].Vob, LastAngle);
	}
	if((int)Builder->SpawnedVobs.size() < 1){
		Builder->ClearAfterObjMenu();
		return;
	}
	if(input->KeyToggled(KEY_UP)){
		if(MenuPos > 0){
			CBuilder::RestoreVobMatrix(Builder->SpawnedVobs[MenuPos].Vob, LastAngle);
			MenuPos--;
			zCAICamera::GetCurrent()->SetTarget(Builder->SpawnedVobs[MenuPos].Vob);
			CBuilder::SaveVobMatrix(Builder->SpawnedVobs[MenuPos].Vob, LastAngle);
		}
		else{
			CBuilder::RestoreVobMatrix(Builder->SpawnedVobs[MenuPos].Vob, LastAngle);
			MenuPos = Builder->SpawnedVobs.size()-1;
			PrintFrom = Builder->SpawnedVobs.size()-9;
			zCAICamera::GetCurrent()->SetTarget(Builder->SpawnedVobs[MenuPos].Vob);
			CBuilder::SaveVobMatrix(Builder->SpawnedVobs[MenuPos].Vob, LastAngle);
		}
		if(PrintFrom > 0){
			if(MenuPos > PrintFrom) PrintFrom--;
		}
	}
	if(input->KeyToggled(KEY_DOWN)){
		if(MenuPos < (int)Builder->SpawnedVobs.size()-1){
			CBuilder::RestoreVobMatrix(Builder->SpawnedVobs[MenuPos].Vob, LastAngle);
			MenuPos++;
			zCAICamera::GetCurrent()->SetTarget(Builder->SpawnedVobs[MenuPos].Vob);
			CBuilder::SaveVobMatrix(Builder->SpawnedVobs[MenuPos].Vob, LastAngle);
			if(MenuPos > 9) PrintFrom++;
		}
		else{
			CBuilder::RestoreVobMatrix(Builder->SpawnedVobs[MenuPos].Vob, LastAngle);
			MenuPos = 0;
			PrintFrom = 0;
			zCAICamera::GetCurrent()->SetTarget(Builder->SpawnedVobs[MenuPos].Vob);
			CBuilder::SaveVobMatrix(Builder->SpawnedVobs[MenuPos].Vob, LastAngle);
		}
	}
	// PRINT
	zCView* Screen = zCView::GetScreen();
	Screen->SetFontColor(Normal);
	sprintf(buffer, "Spawned Objects (%d)", Builder->SpawnedVobs.size());
	MenuTitle = buffer;
	Screen->Print(5000, 3000, "KEY DELETE - REMOVE");
	Screen->Print(5000, 3200, MenuTitle);
	if((int)Builder->SpawnedVobs.size() > 9) PrintTo = 10;
	else PrintTo = (int)Builder->SpawnedVobs.size();
	int Size = 3400;
	for (int i = PrintFrom; i < PrintFrom+PrintTo; i++){
		FColors = (MenuPos == i) ? Highlighted : Normal;
		Screen->SetFontColor(FColors);
		TEMPSTRING = Builder->SpawnedVobs[i].VisualName.c_str();
		TEMPSTRING.DeleteRight(4);
		if(Builder->SpawnedVobs[i].MType == MOB_NORMAL) sprintf(buffer, "%s, NORMAL", TEMPSTRING.ToChar());
		else if(Builder->SpawnedVobs[i].MType == MOB_LADDER) sprintf(buffer, "%s, LADDER", TEMPSTRING.ToChar());
		else if(Builder->SpawnedVobs[i].MType == MOB_DOOR) sprintf(buffer, "%s, DOOR", TEMPSTRING.ToChar());
		else if(Builder->SpawnedVobs[i].MType == MOB_CONTAINER) sprintf(buffer, "%s, CONTAINER", TEMPSTRING.ToChar());
		else sprintf(buffer, "%s, NORMAL", TEMPSTRING.ToChar());
		TEMPSTRING = buffer;
		Screen->Print(5000, Size, TEMPSTRING);
		Size += 200;
	}
	zCView::GetScreen()->SetFontColor(Normal);
	if(Builder->SpawnedVobs[MenuPos].Type != TYPE_PARTICLE && Rotating) Builder->SpawnedVobs[MenuPos].Vob->RotateWorldY(0.5f);
};