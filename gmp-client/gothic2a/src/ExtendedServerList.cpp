
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

#pragma warning (disable : 4101)

#include "StandardFonts.h"
#include "InjectMage.h"
#include <stdio.h>
#include "CLanguage.h"

using namespace std;
using namespace G2W;

extern CLanguage* Lang;

ExtendedServerList::ExtendedServerList()
{

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

}



void ExtendedServerList::loadFav(const char * file){
	/*FILE * f = fopen(file, "a+");
	if(f){
		int ret;
		do {
			ServerInfo si;
			ret = fscanf(f,"%s%s%s%hu%hu",&si.name, &si.map, &si.ip, &si.num_of_players, &si.max_players);
			favList.push_back(si);
		}while(ret != EOF);

	}*/
}


bool ExtendedServerList::getSelectedServer(void * buffer, int size){

	ServerInfo & si = srvList.at(SelectedServer);
	int ret =_snprintf((char *)buffer, size, "%s:%hu\0", si.ip.c_str(), si.port);
	if(ret > 0) return true;
	else return false;
}
void ExtendedServerList::addSelectedToFav(){
	for(unsigned int i=0; i<favList.size(); i++){
		if(favList[i] == SelectedServer) return;
	}
	favList.push_back(SelectedServer);
}


void ExtendedServerList::saveFav(const char * file){
	//FILE * f = fopen(file, "w");
	//if(f){
	//	for(int i=0; i<favList.size(); i++){
	//		ServerInfo & si = favList[i];
	//		fprintf(f, "%s%s%s%hu%hu", si.name, si.map, si.ip, si.num_of_players, si.max_players);
	//	}
	//}
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

bool ExtendedServerList::RefreshList()
{  // Potem bedzie tutaj polaczenie z master serwerem
  std::string data;
  const char* HTTP_DOMAIN = "127.0.0.1";
  const char* HTTP_LIST_FILE = "/ls2.php";
  const char* LIST_CONST_PREFIX = "gmp_list";
#define PREFIX_SIZE 8
  httplib::Client cli(HTTP_DOMAIN);
  auto res = cli.Get(HTTP_LIST_FILE);
  if (!res)
  {
    return false;
  }
  data = res.value().body;

  char work_buff[256];
  int count = 0;
  size_t pos = 0;

  sscanf(data.c_str(), "%s%d", work_buff, &count);
  if (memcmp(work_buff, LIST_CONST_PREFIX, strlen(LIST_CONST_PREFIX)) || (count == 0))
  {
    data.clear();
    return false;
  }
  pos = data.find_first_of(' ', PREFIX_SIZE + 1) + 1;
  ServerInfo si;
  srvList.clear();
  char chBuffer[3][128];
  do
  {
    memset(chBuffer[0], 0, 3 * 128);

    sscanf(data.c_str() + pos, "%s%s%hu%hu%s%hu", chBuffer[0], chBuffer[1], &si.num_of_players, &si.max_players,
           chBuffer[2], &si.port);
    for (int i = 0; i < 128; i++)
    {
      if (chBuffer[0][i] == 7)
        chBuffer[0][i] = 0x20;
      if (chBuffer[1][i] == 7)
        chBuffer[1][i] = 0x20;
      if (chBuffer[2][i] == 7)
        chBuffer[2][i] = 0x20;
    }
    si.ip = chBuffer[0];
    si.name = chBuffer[1];
    si.map = chBuffer[2];
    addServer(si);
    for (int i = 0; i < 6; i++) pos = data.find_first_of(' ', pos) + 1;
  } while (--count);

  fillTables();
  return true;
}

ExtendedServerList::~ExtendedServerList(void)
{
	delete tab_all;
	delete tab_fav;
	delete list_all;
	
}

void ExtendedServerList::fillTables(){
	list_all->clear();
	for(size_t i=0; i<srvList.size(); i++){
		std::vector<std::string> row;
		row.push_back(srvList[i].name);
		row.push_back(srvList[i].map);
		char buff[128];
		sprintf(buff, "%hu/%hu", srvList[i].num_of_players, srvList[i].max_players);
		row.push_back(buff);
		
		if(srvList[i].ping == 0)row.push_back("?");
		else {
			
			sprintf(buff, "%hu", srvList[i].ping);
			row.push_back(buff);
		}
		TableRow tr = {row, false};

		list_all->addRow(tr);
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