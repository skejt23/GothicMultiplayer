
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


/*
+#include <httplib.h>
 #include "ExtendedServerList.h"
+#include <winsock.h>
+#include <icmpapi.h>
+
 #pragma warning (disable : 4101)
-#include <windows.h>
-#include <httplib.h>
-#include <Iphlpapi.h>
-#include <Icmpapi.h>
-#include <winsock2.h>
+*/

#include <httplib.h>
#include "ExtendedServerList.h"
#include <windows.h>
#include <Iphlpapi.h>
#include <Icmpapi.h>
#include <winsock2.h>
#include <algorithm>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#pragma warning (disable : 4101)

#include "StandardFonts.h"
#include "InjectMage.h"
#include <stdio.h>
#include <spdlog/spdlog.h>
#include "CLanguage.h"

namespace {
constexpr char kDefaultFavoritesFile[] = "Multiplayer/Favorites.json";

std::filesystem::path ResolveFavoritesPath(const char* file) {
  if (file && *file) {
    return std::filesystem::path(file);
  }
  return std::filesystem::path("Multiplayer") / "Favorites.json";
}
}  // namespace


using namespace std;
using namespace G2W;

extern CLanguage* Lang;

ExtendedServerList::ExtendedServerList(CServerList* server_list)
{
	server_list_ = server_list;

	tab_all = new Button(0,0);
	tab_all->setHighlightTexture("LOG_PAPER.TGA");
	tab_all->setTexture("INV_BACK_PLUNDER.TGA");
	tab_all->setText((*Lang)[CLanguage::SRVLIST_ALL].ToChar());
	tab_all->setFont(FNT_WHITE_10);
	tab_all->setHighlightFont(FNT_GREEN_10);
	tab_all->highlight = true;

	tab_fav = new Button(1000,0);
	tab_fav->setHighlightTexture("LOG_PAPER.TGA");
	tab_fav->setTexture("INV_BACK_PLUNDER.TGA");
	tab_fav->setText((*Lang)[CLanguage::SRVLIST_FAVOURITES].ToChar());
	tab_fav->setFont(FNT_WHITE_10);
	tab_fav->setHighlightFont(FNT_GREEN_10);
	tab_fav->highlight = false;

	list_all = new Table(0,400, 8192, 7792);
	list_all->addColumn((*Lang)[CLanguage::SRVLIST_NAME].ToChar(), 2500);
	list_all->addColumn((*Lang)[CLanguage::SRVLIST_MAP].ToChar(), 3000);
	list_all->addColumn((*Lang)[CLanguage::SRVLIST_PLAYERNUMBER].ToChar(), 1500);
	list_all->addColumn("Ping", 600);
	list_all->setBackground("INV_BACK_PLUNDER.TGA");
	list_all->setHighlightFont(FNT_GREEN_10);
	list_all->setFont(FNT_WHITE_10);

	list_fav = new Table(0,400, 8192, 7792);
	list_fav->addColumn((*Lang)[CLanguage::SRVLIST_NAME].ToChar(), 2500);
	list_fav->addColumn((*Lang)[CLanguage::SRVLIST_MAP].ToChar(), 3000);
	list_fav->addColumn((*Lang)[CLanguage::SRVLIST_PLAYERNUMBER].ToChar(), 1500);
	list_fav->addColumn("Ping", 600);
	list_fav->setBackground("INV_BACK_PLUNDER.TGA");
	list_fav->setHighlightFont(FNT_GREEN_10);
	list_fav->setFont(FNT_WHITE_10);

	SelectedTab = TAB_ALL;
	SelectedServer = 0;
	srvList_access = CreateMutex(NULL, FALSE, NULL);    

	LangSetting = Lang;
    loadFav(kDefaultFavoritesFile);
}



void ExtendedServerList::loadFav(const char* file) {
    favorite_servers_.clear();

    const std::filesystem::path favorites_path = ResolveFavoritesPath(file);

    std::ifstream favorites_file(favorites_path);
    if (!favorites_file.good()) {
    return;
    }

    try {
    nlohmann::json favorites_json;
    favorites_file >> favorites_json;

    if (!favorites_json.is_array()) {
      SPDLOG_WARN("Favorites file '{}' is not a JSON array. Ignoring its contents.", favorites_path.string());
      return;
    }

    for (const auto& entry : favorites_json) {
      const std::string ip = entry.value("ip", std::string{});
      const int port_value = entry.value("port", 0);

      if (ip.empty()) {
        SPDLOG_WARN("Encountered favorite entry without an IP address in '{}'.", favorites_path.string());
        continue;
      }

      if (port_value <= 0 || port_value > std::numeric_limits<std::uint16_t>::max()) {
        SPDLOG_WARN("Encountered favorite entry with invalid port '{}' for IP '{}' in '{}'.", port_value, ip, favorites_path.string());
        continue;
      }

      favorite_servers_.push_back({ip, static_cast<std::uint16_t>(port_value)});
    }
    } catch (const std::exception& ex) {
    SPDLOG_WARN("Failed to parse favorites file '{}': {}", favorites_path.string(), ex.what());
    favorite_servers_.clear();
    }
}


bool ExtendedServerList::getSelectedServer(void * buffer, int size){
	ServerInfo & si = srvList.at(SelectedServer);
	int ret =_snprintf((char *)buffer, size, "%s:%hu\0", si.ip.c_str(), si.port);
	if(ret > 0) return true;
	else return false;
}

void ExtendedServerList::addSelectedToFav(){
	DWORD wait_result = WaitForSingleObject(srvList_access, INFINITE);
	if (wait_result != WAIT_OBJECT_0) {
	        return;
	}

	if (SelectedServer < 0 || static_cast<size_t>(SelectedServer) >= srvList.size()) {
	        ReleaseMutex(srvList_access);
	        return;
	}

	const ServerInfo selected_server = srvList[SelectedServer];
	ReleaseMutex(srvList_access);

	if (selected_server.port <= 0 || selected_server.port > static_cast<int>(std::numeric_limits<std::uint16_t>::max())) {
	        SPDLOG_WARN("Selected server '{}' has an invalid port '{}'.", selected_server.ip, selected_server.port);
	        return;
	}

	loadFav(kDefaultFavoritesFile);

	const auto already_favorited = std::any_of(
	        favorite_servers_.begin(), favorite_servers_.end(),
	        [&selected_server](const FavoriteServerEndpoint& favorite) {
	                return favorite.ip == selected_server.ip &&
	                       favorite.port == static_cast<std::uint16_t>(selected_server.port);
	        });

	if (already_favorited) {
        favorite_servers_.erase(
            std::remove_if(favorite_servers_.begin(), favorite_servers_.end(),
                           [&](const FavoriteServerEndpoint& f) {
                               return f.ip == selected_server.ip &&
                                      f.port == static_cast<std::uint16_t>(selected_server.port);
                           }),
            favorite_servers_.end());
	} else {
		favorite_servers_.push_back({selected_server.ip, static_cast<std::uint16_t>(selected_server.port)});
	}

	saveFav(kDefaultFavoritesFile);

	wait_result = WaitForSingleObject(srvList_access, INFINITE);
	if (wait_result == WAIT_OBJECT_0) {
	        fillTables();
	        ReleaseMutex(srvList_access);
	}
}

void ExtendedServerList::saveFav(const char * file){
	const std::filesystem::path favorites_path = ResolveFavoritesPath(file);
	const std::filesystem::path parent_path = favorites_path.parent_path();

	if (!parent_path.empty()) {
		std::error_code ec;
		std::filesystem::create_directories(parent_path, ec);
		if (ec) {
			SPDLOG_ERROR("Failed to create directory '{}' for favorites: {}", parent_path.string(), ec.message());
			return;
		}
	}

	nlohmann::json favorites_json = nlohmann::json::array();
	for (const auto& favorite : favorite_servers_) {
		favorites_json.push_back({{"ip", favorite.ip}, {"port", favorite.port}});
	}

	std::ofstream favorites_file(favorites_path, std::ios::trunc);
	if (!favorites_file.is_open()) {
		SPDLOG_ERROR("Failed to open favorites file '{}' for writing.", favorites_path.string());
		return;
	}

	favorites_file << favorites_json.dump(2);
}

void ExtendedServerList::HandleInput(){
	if(zinput->KeyToggled(KEY_RIGHT)){
		this->nextTab();
	}
	if(zinput->KeyToggled(KEY_LEFT)){
		this->prevTab();
	}
	if(zinput->KeyToggled(KEY_R)){
		this->RefreshList();
	}
	if(zinput->KeyToggled(KEY_DOWN) && (srvList.size())) {
		list_all->scrollDown(1);
		if((srvList.size()-1)>(size_t)SelectedServer){
			SelectedServer++;
		}
	}
	if(zinput->KeyToggled(KEY_UP)) {
		list_all->scrollUp(1);
		if(SelectedServer>0){
			SelectedServer--;
		}
	}
	if(zinput->KeyToggled(KEY_A)){
		addSelectedToFav();
	}
}

bool ExtendedServerList::RefreshList(){
  if (!server_list_) {
    SPDLOG_ERROR("Failed to refresh server list: CServerList instance is null.");
    return false;
  }
  if (!server_list_->ReceiveListHttp()) {
    if (const char* error_message = server_list_->GetLastError()) {
                        SPDLOG_ERROR("Failed to refresh server list: {}", error_message);
    }
    DWORD wait_result = WaitForSingleObject(srvList_access, INFINITE);
    if (wait_result == WAIT_OBJECT_0) {
                        srvList.clear();
                        SelectedServer = 0;
                        fillTables();
                        ReleaseMutex(srvList_access);
    }
    return false;
  }

  DWORD wait_result = WaitForSingleObject(srvList_access, INFINITE);
  if (wait_result != WAIT_OBJECT_0) {
    return false;
  }

  srvList.clear();
  const size_t server_count = server_list_->GetListSize();
  srvList.reserve(server_count);

  for (size_t index = 0; index < server_count; ++index) {
    if (auto server_info = server_list_->At(index)) {
                        ServerInfo server{};
                        server.name = server_info->name.ToChar();
                        server.map = server_info->map.ToChar();
                        server.ip = server_info->ip.ToChar();
                        server.num_of_players = server_info->num_of_players;
                        server.max_players = server_info->max_players;
                        server.port = server_info->port;
                        server.ping = server_info->ping;
                        srvList.push_back(server);
    }
  }

  if (SelectedServer >= static_cast<int>(srvList.size())) {
    SelectedServer = srvList.empty() ? 0 : static_cast<int>(srvList.size()) - 1;
  }

  fillTables();
  ReleaseMutex(srvList_access);
  return true;
}

ExtendedServerList::~ExtendedServerList(void){
	delete tab_all;
	delete tab_fav;
	delete list_all;
}

void ExtendedServerList::fillTables() {
    list_all->clear();
	list_fav->clear();

	const auto build_row = [](const ServerInfo& server) {
	        std::vector<std::string> row;
	        row.push_back(server.name);
	        row.push_back(server.map);
	        char buff[128];
	        sprintf(buff, "%hu/%hu", server.num_of_players, server.max_players);
	        row.push_back(buff);

	        if(server.ping >= 0){
	                sprintf(buff, "%hu", server.ping);
	                row.push_back(buff);
            } else {
                    row.push_back("?");
			}

	        return row;
	};

	for (const auto& server : srvList) {
	        TableRow tr = {build_row(server), false};
	        list_all->addRow(tr);
	}

	for (const auto& server : srvList) {
	        const bool is_favorite = std::any_of(
	                favorite_servers_.begin(), favorite_servers_.end(),
	                [&server](const FavoriteServerEndpoint& favorite) {
	                        return favorite.ip == server.ip &&
	                               favorite.port == static_cast<std::uint16_t>(server.port);
	                });

	        if (!is_favorite) {
	                continue;
	        }

	        TableRow tr = {build_row(server), false};
	        list_fav->addRow(tr);
	}
}

void ExtendedServerList::SelectServer(int index){
	this->SelectedServer = index;	
}

void ExtendedServerList::setLanguage(CLanguage * lang){
	this->LangSetting = lang;
}

void ExtendedServerList::nextTab(){
	selectTab(TAB_FAV);
}
void ExtendedServerList::prevTab(){
	selectTab(TAB_ALL);
}
void ExtendedServerList::selectTab(int index){
	switch(index){
		case TAB_ALL:
			tab_all->highlight = true;
			tab_fav->highlight = false;
			SelectedTab = index;
			break;
		case TAB_FAV:
			tab_all->highlight = false;
			tab_fav->highlight = true;
			SelectedTab = index;
			break;
		default:
			//Nie ma takiej zakladki
			break;
	}
}
void ExtendedServerList::addServer(ServerInfo & si){
	//si.updatePing();
	srvList.push_back(si);
}

void ExtendedServerList::clearList(){
	srvList.clear();
}


void ExtendedServerList::Draw(){
	static DWORD tid;
	static HANDLE hThread=NULL;
	DWORD exitCode = 0;
	GetExitCodeThread(hThread, &exitCode);
	if(exitCode != STILL_ACTIVE){
		hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ExtendedServerList::pingThreadProc, this, 0, &tid);
	}


	tab_all->render();
	tab_fav->render();
	if(SelectedTab == TAB_ALL){
		list_all->unHighlightAll();
		if (!list_all->rows.empty())
		{
			list_all->rows[SelectedServer].highlight = true;
		}
		list_all->render();
	}
	else
	{
		list_fav->unHighlightAll();
		//list_fav->rows[SelectedServer].highlight = true;
		list_fav->render();
	}
	
	DWORD res = WaitForSingleObject(srvList_access, NULL);
	if(res == WAIT_OBJECT_0) {
		fillTables();
		ReleaseMutex(srvList_access);
	}
}
void ExtendedServerList::updatePings(){
	for(unsigned int i=0; i<srvList.size(); i++){
		srvList[i].updatePing();
	}
}

DWORD WINAPI ExtendedServerList::pingThreadProc(ExtendedServerList * esl){
	
	DWORD res = WaitForSingleObject(esl->srvList_access, INFINITE);
	if(res == WAIT_OBJECT_0) {
		esl->updatePings();
		ReleaseMutex(esl->srvList_access);
	}
	Sleep(1000);
	return 0;
}


ServerInfo::ServerInfo(){
	this->ping = 0;
}

void ServerInfo::updatePing(){
	int ret = 0;
	WSADATA wsaData;
	WSAStartup(0x0202, &wsaData);
	hostent* remoteHost;
	remoteHost = gethostbyname(ip.c_str());
	if (WSAGetLastError() == 0){
		HANDLE hIcmpFile = IcmpCreateFile();
		if (hIcmpFile != INVALID_HANDLE_VALUE){
			IPAddr* ipaddr = reinterpret_cast< IPAddr* >(remoteHost->h_addr_list[0]);
			LPVOID ReplyBuffer = (VOID*) malloc(sizeof(ICMP_ECHO_REPLY));
			if (IcmpSendEcho(hIcmpFile, *ipaddr,0,0,NULL, ReplyBuffer,sizeof(ICMP_ECHO_REPLY),1000)!=0){
				PICMP_ECHO_REPLY pEchoReply = (PICMP_ECHO_REPLY)ReplyBuffer;
				in_addr ipreplied;
				ipreplied.S_un.S_addr=pEchoReply->Address;
				ret = pEchoReply->RoundTripTime;
			}
			free(ReplyBuffer);
		}
		IcmpCloseHandle(hIcmpFile);
	}
	WSACleanup();
	ping = ret;
}