
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
**	File name:		CGmpClient/CObservation.h		   						**
**																			**
**	Created by:		23/12/11	-	skejt23									**
**																			**
**	Description:	Observation mode functionallity 						**
**																			**
*****************************************************************************/
// ONLY LOCAL PLAYER

#include "CObservation.h"
#include "CLocalPlayer.h"
#include "game_client.h"
#include "CIngame.h"

// Externs
extern GameClient* client;
extern CIngame* global_ingame;
extern CLocalPlayer* LocalPlayer;
extern CLanguage* Lang;
extern zCOLOR Normal;

CObservation::CObservation()
{
	ObservingPlayer = NULL;
	if(client->ObserveMode == NO_OBSERVATION){
		delete this;
		return;
	}
	LocalPlayer->npc->SetMovLock(1);
	zCAICamera::SetLookingOnNpcCamera(true);
	ObserveForward();
};

CObservation::~CObservation()
{
	zCAICamera::GetCurrent()->SetTarget(LocalPlayer->npc);
	LocalPlayer->npc->SetMovLock(0);
	zCAICamera::SetLookingOnNpcCamera(false);
	ObservingPlayer = NULL;
};

void CObservation::Loop()
{
	if(!CPlayer::IsPlayerValid(ObservingPlayer)){
		if(client->player.size() > 1) ObserveForward();
		else LocalPlayer->LeaveObserveMode();
	}
	zCInput* Input = zCInput::GetInput();
	zCView* Screen = zCView::GetScreen();
	if(Input->KeyToggled(KEY_LEFT)) ObserveBackward();
	if(Input->KeyToggled(KEY_RIGHT)) ObserveForward();
	char buffer[128];
	sprintf(buffer, "<- %s ->", ObservingPlayer->GetName());
	PrintName = buffer;
	Screen->SetFontColor(Normal);
	Screen->PrintCX(7000, PrintName);
};

void CObservation::ObserveBackward()
{
	if(!ObservingPlayer){
		for(size_t i=1; i<client->player.size(); i++){
			ObservingPlayer = client->player[i];
			zCAICamera::GetCurrent()->SetTarget(client->player[i]->npc);
			break;
		}
		return;
	}
	int playernum = 0;
	for(size_t i=1; i<client->player.size(); i++){
		if(ObservingPlayer == client->player[i]){
			playernum = i;
			break;
		}
	}
	playernum--;
	if(playernum < 1) playernum = client->player.size()-1;
	ObservingPlayer = client->player[playernum];
	zCAICamera::GetCurrent()->SetTarget(client->player[playernum]->npc);
};

void CObservation::ObserveForward()
{
	if(!ObservingPlayer){
		for(size_t i=1; i<client->player.size(); i++){
			ObservingPlayer = client->player[i];
			zCAICamera::GetCurrent()->SetTarget(client->player[i]->npc);
			break;
		}
		return;
	}
	int playernum = 0;
	for(size_t i=1; i<client->player.size(); i++){
		if(ObservingPlayer == client->player[i]){
			playernum = i;
			break;
		}
	}
	playernum++;
	if(playernum > (int)client->player.size()-1) playernum = 1;
	ObservingPlayer = client->player[playernum];
	zCAICamera::GetCurrent()->SetTarget(client->player[playernum]->npc);
};