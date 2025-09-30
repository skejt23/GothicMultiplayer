
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

#include "ZenGin/zGothicAPI.h"
#include "CServerList.h"
#include "CLanguage.h"
#include "G2W.h"

#define TAB_ALL 0
#define TAB_FAV 1

class ServerInfo{
public:
	std::string name, map, ip;
	int max_players, num_of_players, port, ping;
	ServerInfo();
	void updatePing();
};

struct FavoriteServerEndpoint {
        std::string ip;
        std::uint16_t port;
};

class ExtendedServerList{
public:
	ExtendedServerList(CServerList* server_list);
	~ExtendedServerList(void);
	void SelectServer(int index);
	void Draw();
	void addServer(ServerInfo & si);
	void clearList();
	void setLanguage(CLanguage * lang);
	void selectTab(int index);
	void nextTab();
	void prevTab();
	void loadFav(const char * file);
	void saveFav(const char * file);
	void HandleInput();
	void addSelectedToFav();
	bool RefreshList();
	bool getSelectedServer(void * buffer, int size);
	void fillTables();
	void updatePings();
	
	HANDLE srvList_access; 
private:
	static DWORD WINAPI pingThreadProc(ExtendedServerList * esl);
	
	G2W::Button * tab_all, * tab_fav;
	G2W::Table * list_all, * list_fav;
    std::vector<FavoriteServerEndpoint> favorite_servers_;
	std::vector<ServerInfo> srvList;
	int SelectedTab;
	int SelectedServer;
	CLanguage* LangSetting;
    CServerList* server_list_;
};
