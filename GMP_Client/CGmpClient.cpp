
/*
MIT License

Copyright (c) 2022 Gothic Multiplayer Team.

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

#include "CGmpClient.h"
#include "CConfig.h"
#include "CIngame.h"
#include "patch.h"
#include "g2Api.h"
#include "Interface.h"
#include "CSelectClass.h"
#include "CChat.h"
#include <vector>
#include <list>
#include <string>
#include <ctime>
#include <cstdlib>
#include "CLocalPlayer.h"
#include "HTTPDownloader.h"
#include "net_enums.h"
#include <wincrypt.h>

CConfig *user_config=NULL;
zSTRING CYL="Choose your language:";
const char *LANG_DIR=".\\Multiplayer\\Localization\\";
//lista uzywalnych itemow(chyba nic nie pominalem)
const short GOTHIC_DAT_ITEMS[866]={5892,5895,5902,5909,5916,5923,5930,5937,5944,5951,5967,5968,5969,5970,5971,5972,5973,5974,5975,5976,5977,5978,5980,5981,5982,5983,5984,5985,5986,5987,5988,5989,5990,5991,5992,5993,5994,5995,5996,5997,5998,5999,6001,6002,6003,6004,6005,6006,6007,6008,6009,6010,6011,6012,6013,6014,6015,6016,6017,6018,6019,6020,6021,6022,6053,6056,6059,6062,6065,6068,6071,6074,6077,6080,6083,6086,6089,6092,6095,6098,6115,6117,6119,6122,6124,6126,6129,6130,6132,6135,6136,6137,6138,6139,6140,6141,6142,6143,6147,6148,6149,6152,6153,6154,6155,6157,6158,6160,6161,6163,6165,6166,6168,6169,6171,6173,6175,6177,6179,6181,6183,6199,6200,6201,6202,6203,6204,6205,6206,6207,6208,6209,6210,6217,6218,6219,6220,6221,6239,6240,6242,6244,6246,6247,6249,6251,6253,6260,6262,6306,6308,6310,6312,6314,6316,6318,6320,6322,6324,6326,6328,6330,6332,6334,6336,6338,6340,6342,6344,6346,6348,6350,6352,6355,6356,6357,6360,6361,6362,6363,6364,6365,6366,6367,6368,6369,6370,6371,6372,6373,6374,6375,6376,6377,6378,6379,6380,6381,6382,6383,6386,6389,6392,6395,6398,6401,6404,6407,6410,6413,6417,6420,6422,6425,6426,6427,6428,6429,6430,6431,6432,6433,6434,6435,6436,6464,6467,6470,6473,6476,6479,6482,6485,6488,6491,6494,6534,6535,6537,6538,6539,6540,6541,6542,6543,6545,6546,6548,6549,6550,6551,6552,6553,6554,6555,6556,6557,6558,6559,6560,6561,6562,6563,6564,6565,6566,6567,6568,6569,6570,6571,6572,6573,6613,6614,6615,6616,6617,6618,6619,6620,6621,6622,6623,6626,6629,6630,6631,6632,6633,6634,6635,6638,6641,6644,6647,6648,6649,6650,6651,6654,6657,6660,6663,6664,6665,6666,6667,6668,6669,6670,6671,6709,6711,6713,6715,6717,6719,6721,6723,6725,6727,6730,6732,6734,6736,6738,6740,6742,6744,6750,6751,6752,6753,6754,6755,6756,6757,6758,6759,6760,6761,6762,6763,6764,6765,6766,6767,6768,6769,6770,6771,6772,6773,6774,6775,6776,6777,6778,6779,6780,6781,6782,6783,6784,6785,6786,6787,6788,6789,6790,6791,6792,6793,6794,6795,6796,6797,6798,6799,6800,6801,6802,6803,6804,6805,6806,6807,6808,6809,6810,6811,6812,6813,6814,6815,6816,6817,6818,6819,6820,6821,6822,6823,6824,6825,6826,6827,6828,6829,6830,6831,6832,6833,6834,6835,6836,6837,6838,6839,6840,6841,6842,6843,6844,6845,6846,6847,6848,6849,6850,6851,6852,6853,6903,6904,6905,6906,6907,6908,6909,6910,6911,6912,6913,6914,6915,6916,6918,6920,6922,6924,6926,6928,6929,6930,6931,6932,6933,6934,6935,6936,6937,6938,6939,6940,6941,6942,6943,6944,6945,6946,6947,6948,6949,6950,6951,6952,6953,6954,6955,6956,6957,6958,6959,6960,6996,6997,6999,7001,7003,7005,7007,7009,7011,7013,7015,7017,7019,7021,7023,7025,7027,7029,7031,7059,7061,7063,7065,7067,7069,7071,7073,7075,7077,7079,7081,7083,7084,7085,7086,7087,7088,7089,7090,7091,7092,7093,7094,7095,7096,7097,7098,7099,7100,7101,7102,7103,7104,7153,7156,7159,7162,7165,7168,7171,7174,7177,7180,7183,7186,7189,7192,7195,7198,7201,7204,7207,7210,7262,7263,7264,7265,7266,7267,7268,7269,7270,7271,7272,7273,7274,7275,7276,7277,7278,7279,7280,7281,7282,7283,7284,7285,7286,7287,7288,7289,7290,7291,7292,7293,7294,7295,7296,7297,7298,7299,7300,7301,7302,7303,7304,7305,7306,7307,7308,7309,7310,7311,7312,7363,7364,7365,7366,7367,7368,7369,7370,7371,7372,7373,7374,7375,7376,7377,7378,7379,7380,7381,7382,7383,7384,7385,7386,7387,7388,7389,7390,7391,7392,7393,7394,7395,7396,7397,7398,7399,7400,7401,7402,7403,7404,7405,7406,7407,7408,7409,7410,7411,7412,7414,7416,7418,7420,7422,7424,7426,7429,7430,7431,7432,7439,7442,7445,7450,7453,7456,7457,7460,7461,7462,7464,7465,7468,7469,7472,7475,7478,7479,7481,7483,7485,7488,7491,7494,7497,7498,7499,7500,7501,7502,7503,7504,7505,7507,7510,7511,7514,7517,7520,7521,7522,7525,7526,7527,7528,7530,7531,7532,7533,7534,7535,7536,7537,7538,7541,7542,7543,7544,7545,7548,7551,7552,7553,7554,7555,7557,7560,7561,7563,7564,7565,7566,7567,7570,7573,7576,7579,7582,7585,7592,7595,7597,7599,7602,7604,7605,7608,7611,7614,7616,7617,7619,7620,7623,7626,7628,7629,7634,7636,7638,7641,7644,7646,7647,7648,7651,7652,7653,7656,7659,7662,7667,7670,7671,7673,7674,7675,7676,7677,7681,7683,7687,7690,7693,7694,7696,7699,7702,7705,7708,7711,7712,7713,7715,7716,7719,7721,7724,7727,7730,7731,7732,7733,7734,7737,7738,7741,7744,7747,7748,7753,7754,7755,7756,7759,7765,7766,7767,7768,7769,7772,7776,7779,7780,7784,7785,7786,7787,7788,7790,7791,7795,7799,7800,7806,7807,7810,7811,7812,7815,7818,7821,7824,7827,7830,7831,7833,7836,7839,7840,7841,7844,7845,7848,7850};
std::vector<zSTRING> vec_choose_lang;
std::vector<std::string> vec_lang_files;
const BYTE GDI_MD5[16]={0x34,0x16,0x98,0x62,0x43,0xB0,0x3D,0x36,0xD6,0xB0,0x1A,0x4D,0x2B,0xCB,0xDB,0x8B};

float fWRatio, fHRatio;
extern CConfig* user_config;
extern CIngame* global_ingame;
extern CLocalPlayer* LocalPlayer;
extern zCOLOR AQUA, RED;

using namespace Net;

CGmpClient::CGmpClient(const char *ip, CLanguage *ptr)
{
	srand(static_cast<unsigned int>(time(NULL)));
	this->network = new Network(this);
    this->voiceCapture = nullptr;
    this->voicePlayback = nullptr;
	this->IsReadyToJoin = false;
	this->lang=ptr;
	this->classmgr=NULL;
	this->spawnpoint=NULL;
	this->IsAdminOrModerator = false;
	this->IgnoreFirstTimeMessage = true;
	this->DropItemsAllowed = false;
	this->IsInGame = false;
	this->ObserveMode = CObservation::NO_OBSERVATION;
	this->clientPort=0xDEAD;
	this->clientHost=ip;
	size_t pos=this->clientHost.find_last_of(':');
	if(pos!=std::string::npos){
		sscanf(this->clientHost.c_str()+pos+1, "%d", &this->clientPort);
		this->clientHost.erase(pos);
	}
}

CGmpClient::~CGmpClient(void)
{
	if(this->classmgr){
		delete this->classmgr;
		this->classmgr=NULL;
	}
	if(this->spawnpoint){
		delete this->spawnpoint;
		this->spawnpoint=NULL;
	}
    if (this->voiceCapture) {
      delete this->voiceCapture;
      this->voiceCapture = nullptr;
	}
    if (this->voicePlayback) {
      delete this->voicePlayback;
	  this->voicePlayback = nullptr;
	}
	delete this->network;
	this->network = NULL;
	this->clientHost.clear();
	this->IsInGame = false;
}

bool CGmpClient::Connect()
{
	if (network->Connect(clientHost, clientPort)) {
		this->voiceCapture = new VoiceCapture();
		this->voiceCapture->StartCapture();
		this->voicePlayback = new VoicePlayback();
		this->voicePlayback->StartPlayback();
		return true;
	}
    return false;
}

void CGmpClient::PrepareToJoin()
{
  std::uint8_t val[2] = {};
  val[0] = PT_WHOAMI;
  network->Send(val, 1, IMMEDIATE_PRIORITY, RELIABLE);
  val[0] = PT_MAP_NAME;
  network->Send(val, 1, IMMEDIATE_PRIORITY, RELIABLE);
  IsReadyToJoin = true;
}

string CGmpClient::GetServerAddresForHTTPDownloader() {
  auto address = network->GetServerIp() + ":" + std::to_string(network->GetServerPort() + 1);
  return address;
}

void CGmpClient::DownloadWBFile() {
  string content = HTTPDownloader::GetWBFile(GetServerAddresForHTTPDownloader());
  static const string path = ".\\Multiplayer\\Data\\";
  string serverWbFile = path + network->GetServerIp() + "_" + std::to_string(network->GetServerPort());
  if (content.compare("EMPTY") == 0) {
    remove(serverWbFile.c_str());
    return;
  }
  FILE *wbFile = fopen(serverWbFile.c_str(), "wb");
  if (wbFile) {
    fwrite(content.c_str(), content.length(), 1, wbFile);
    fclose(wbFile);
  }
}

void CGmpClient::DownloadClassFile()
{
	string content = HTTPDownloader::GetClassFile(GetServerAddresForHTTPDownloader());
	if (content.compare("EMPTY") == 0) {
		return;
	}
	this->classmgr = new CHeroClass(content.c_str(), content.length());
}

void CGmpClient::DownloadSpawnpointsFile()
{
	string content = HTTPDownloader::GetSpawnpointsFile(GetServerAddresForHTTPDownloader());
	if (content.compare("EMPTY") == 0) {
		return;
	}
	this->spawnpoint = new CSpawnPoint(content.c_str());
	size_t randomSpawnpoint = rand() % spawnpoint->GetSize();
	oCNpc::GetHero()->SetPosition((*spawnpoint)[randomSpawnpoint]->x, (*spawnpoint)[randomSpawnpoint]->y, (*spawnpoint)[randomSpawnpoint]->z);
}

void CGmpClient::RestoreHealth()
{
	if (!mp_restore || !IsInGame) {
		return;
	}
	if (last_mp_regen != time(NULL)) {
		last_mp_regen = time(NULL);
		if (oCNpc::GetHero()->GetMaxMana() != oCNpc::GetHero()->GetMana()) {
			oCNpc::GetHero()->SetMana(oCNpc::GetHero()->GetMana() + mp_restore);
			if (oCNpc::GetHero()->GetMana() > oCNpc::GetHero()->GetMaxMana()) oCNpc::GetHero()->SetMana(oCNpc::GetHero()->GetMaxMana());
		}
	}
}

void CGmpClient::HandleNetwork() {
  if (IsConnected()) {
    network->Receive();
  }
}

bool CGmpClient::IsConnected() {
  return network->IsConnected();
}

zSTRING& CGmpClient::GetLastError(){
	switch(network->error){
		case ID_CONNECTION_ATTEMPT_FAILED:
			return (*this->lang)[CLanguage::ERR_CONN_FAIL];
		case ID_ALREADY_CONNECTED:
			return (*this->lang)[CLanguage::ERR_CONN_ALREADY_CONNECTED];
		case ID_NO_FREE_INCOMING_CONNECTIONS:
			return (*this->lang)[CLanguage::ERR_CONN_SRV_FULL];
		case ID_CONNECTION_BANNED:
			return (*this->lang)[CLanguage::ERR_CONN_BANNED];
		case ID_INVALID_PASSWORD:
		case ID_INCOMPATIBLE_PROTOCOL_VERSION:
			return (*this->lang)[CLanguage::ERR_CONN_INCOMP_TECHNIC];
		default:
			return (*this->lang)[CLanguage::ERR_CONN_NO_ERROR];
	}
}

void CGmpClient::JoinGame(BYTE selected_class){
	if(IsReadyToJoin){
	HooksManager::GetInstance()->AddHook(HT_RENDER, (DWORD)InterfaceLoop, false);
	HooksManager::GetInstance()->RemoveHook(HT_RENDER, (DWORD)CSelectClass::Loop);
	BYTE *buffer=new BYTE[512];
	memset(buffer, 0, 512);
	buffer[0]=PT_JOIN_GAME;
	buffer[1]=selected_class;
	memcpy(buffer+2, &oCNpc::GetHero()->GetPosition(), 12);
	zVEC3 nr(oCNpc::GetHero()->GetAngleNX(), oCNpc::GetHero()->GetAngleNY(), oCNpc::GetHero()->GetAngleNZ());
	memcpy(buffer+14, &nr, 12);
	short word;
	oCItem *itPtr=oCNpc::GetHero()->GetLeftHand();
	if(itPtr){
		word=static_cast<short>(itPtr->GetInstance());
		memcpy(buffer+26, &word, 2);
	}
	itPtr=oCNpc::GetHero()->GetRightHand();
	if(itPtr){
		word=static_cast<short>(itPtr->GetInstance());
		memcpy(buffer+28, &word, 2);
	}
	itPtr=oCNpc::GetHero()->GetEquippedArmor();
	if(itPtr){
		word=static_cast<short>(itPtr->GetInstance());
		memcpy(buffer+30, &word, 2);
	}
	word=265;
	memcpy(buffer+32, &word, 2);
	buffer[34]=static_cast<BYTE>(user_config->headmodel);
	buffer[35]=static_cast<BYTE>(user_config->skintexture);
	buffer[36]=static_cast<BYTE>(user_config->facetexture);
	buffer[37]=static_cast<BYTE>(user_config->walkstyle);
	memcpy(buffer+38, oCNpc::GetHero()->GetName().ToChar(), oCNpc::GetHero()->GetName().Length()+1);
	network->Send((const char*)buffer, 40 + oCNpc::GetHero()->GetName().Length(), IMMEDIATE_PRIORITY, RELIABLE_ORDERED);
	memset(buffer, 0, 512);
	delete[] buffer;
	CIngame *g=new CIngame(lang);
	if(!LocalPlayer) new CLocalPlayer();
	LocalPlayer->id=network->GetMyId();
	LocalPlayer->enable=TRUE;
	LocalPlayer->SetNpc(oCNpc::GetHero());
	LocalPlayer->SetName(oCNpc::GetHero()->GetName());
	LocalPlayer->hp=static_cast<short>(LocalPlayer->GetHealth());
	LocalPlayer->update_hp_packet=0;
	LocalPlayer->npc->SetMovLock(0);
	LocalPlayer->char_class=selected_class;
	this->player.push_back(LocalPlayer);
	this->HeroLastHp = oCNpc::GetHero()->GetHealth();
	}
}

void CGmpClient::SendMessage(const char* msg){
	std::string szData;
	szData.reserve(1024);
	*((unsigned char*)szData.data())=PT_MSG;
	memcpy((char*)szData.data()+1, msg, strlen(msg)+1);
	network->Send(szData.data(), strlen(msg) + 2, HIGH_PRIORITY, RELIABLE_ORDERED);
	szData.clear();
}

void CGmpClient::SendWhisper(const char *player_name, const char *msg){
	bool found=false;
	size_t i;
	size_t length=strlen(player_name);
	for(i=0; i<this->player.size(); i++){
		if(this->player[i]->GetNameLength()==length){
			if(!memcmp(this->player[i]->GetName(), player_name, length)) {found=true; break;}
		}
	}
	if(found){
		std::string szData;
		szData.reserve(1024);
		*((unsigned char*)szData.data())=PT_WHISPER;
		memcpy((char*)szData.data()+1, &this->player[i]->id, sizeof(uint64_t));
		memcpy((char*)szData.data()+1+sizeof(uint64_t), msg, strlen(msg)+1);
		network->Send(szData.data(), 2 + sizeof(uint64_t) + strlen(msg), HIGH_PRIORITY, RELIABLE_ORDERED);
		szData.clear();
	}
}

void CGmpClient::SendRemoteMessage(const char *msg){
	std::string szData;
	szData.reserve(1024);
	*((unsigned char*)szData.data())=PT_RCON;
	memcpy((char*)szData.data()+1, msg, strlen(msg)+1);
	network->Send(szData.data(), 2 + strlen(msg), HIGH_PRIORITY, RELIABLE);
	szData.clear();
}

void CGmpClient::SendCastSpell(oCNpc* Target, short SpellId){
	std::string szData;
	szData.reserve(1024);
	uint64_t PlayerId = 0;
	if(Target){
		for(int i=0; i<(int)player.size(); i++){
			if(player[i]->npc == Target){
				PlayerId = player[i]->id;
				break;
			}
		}
	}
	if(!PlayerId){
		*((unsigned char*)szData.data())=PT_CASTSPELL;
		*((unsigned char*)szData.data()+1)=(BYTE)SpellId;
		network->Send(szData.data(), 2, HIGH_PRIORITY, RELIABLE);
		szData.clear();
	}
	else{
		*((unsigned char*)szData.data())=PT_CASTSPELLONTARGET;
		memcpy((char*)szData.data()+1, &PlayerId, 8);
		*((unsigned char*)szData.data()+9)=(BYTE)SpellId;
		network->Send(szData.data(), 10, HIGH_PRIORITY, RELIABLE);
		szData.clear();
	}
}

void CGmpClient::SendDropItem(short Instance, short amount){
	std::string szData;
	szData.reserve(1024);
	*((unsigned char*)szData.data())=PT_DROPITEM;
	memcpy((char*)szData.data()+1, &Instance, 2);
	memcpy((char*)szData.data()+3, &amount, 2);
	network->Send(szData.data(), 5, HIGH_PRIORITY, RELIABLE);
	szData.clear();
}

void CGmpClient::SendTakeItem(short Instance){
	std::string szData;
	szData.reserve(1024);
	*((unsigned char*)szData.data())=PT_TAKEITEM;
	memcpy((char*)szData.data()+1, &Instance, 2);
	network->Send(szData.data(), 3, HIGH_PRIORITY, RELIABLE);
	szData.clear();
}

void CGmpClient::UpdatePlayerStats(short anim){
	oCNpc* Hero = oCNpc::GetHero();
	std::string szData;
	szData.reserve(1024);
	*((unsigned char*)szData.data())=PT_ACTUAL_STATISTICS;
	zVEC3 pos=Hero->GetPosition();
	memcpy((char*)szData.data()+1, &pos, 12);
	float nrot[3]={Hero->GetAngleNX(),Hero->GetAngleNY(),Hero->GetAngleNZ()};
	memcpy((char*)szData.data()+13, nrot, 12);
	memset((char*)szData.data()+25, 0, 6);
	if(Hero->GetLeftHand()){
		short word=static_cast<short>(Hero->GetLeftHand()->GetInstance());
		memcpy((char*)szData.data()+25, &word, 2);
	}
	if(Hero->GetRightHand()){
		short word=static_cast<short>(Hero->GetRightHand()->GetInstance());
		memcpy((char*)szData.data()+27, &word, 2);
	}
	if(Hero->GetEquippedArmor()){
		short word=static_cast<short>(Hero->GetEquippedArmor()->GetInstance());
		memcpy((char*)szData.data()+29, &word, 2);
	}
	memcpy((char*)szData.data()+31, &anim, 2);
	short mp=static_cast<short>(Hero->GetMana());
	memcpy((char*)szData.data()+33, &mp, 2);
	*((unsigned char*)szData.data()+35)=(BYTE)Hero->GetWeaponMode();
	memset((char*)szData.data()+36, 0, 6);
	if(Hero->GetActiveSpellNr() > 0){
		*((unsigned char*)szData.data()+36)=(BYTE)Hero->GetActiveSpellNr();
	}
	zVEC2 HeadVar = Hero->GetAnictrl()->GetLookAtPos();
	if(HeadVar.x == 0) *((unsigned char*)szData.data()+37)=(BYTE)CPlayer::HEAD_LEFT;
	else if(HeadVar.x == 1) *((unsigned char*)szData.data()+37)=(BYTE)CPlayer::HEAD_RIGHT;
	else if(HeadVar.y == 0) *((unsigned char*)szData.data()+37)=(BYTE)CPlayer::HEAD_UP;
	else if(HeadVar.y == 1) *((unsigned char*)szData.data()+37)=(BYTE)CPlayer::HEAD_DOWN;
	if(Hero->GetEquippedRangedWeapon()){
		short word=static_cast<short>(Hero->GetEquippedRangedWeapon()->GetInstance());
		memcpy((char*)szData.data()+38, &word, 2);
	}
	if(Hero->GetEquippedMeleeWeapon()){
		short word=static_cast<short>(Hero->GetEquippedMeleeWeapon()->GetInstance());
		memcpy((char*)szData.data()+40, &word, 2);
	}
	network->Send(szData.data(), 42, IMMEDIATE_PRIORITY, UNRELIABLE);
}

void CGmpClient::SendHPDiff(size_t who, short diff){
	if(who<this->player.size()){
		std::string szData;
		szData.reserve(32);
		*((unsigned char*)szData.data())=PT_HP_DIFF;
		memcpy((char*)szData.data()+1, &this->player[who]->id, sizeof(uint64_t));
		memcpy((char*)szData.data()+1+sizeof(uint64_t), &diff, 2);
		network->Send(szData.data(), 3 + sizeof(uint64_t), IMMEDIATE_PRIORITY, RELIABLE);
		szData.clear();
	}
}

void CGmpClient::SendVoice()
{
  int audioChannels = voiceCapture->GetNumberOfChannels();
  char *voiceBuffer = new char[480 * sizeof(float) * audioChannels * 4096];  // TODO: correct size
  ZeroMemory(voiceBuffer, 480 * sizeof(float) * audioChannels * 4096);
  int size;
  if (voiceCapture->GetAndFlushVoiceBuffer(voiceBuffer, size) && zCInput::GetInput()->KeyPressed(KEY_K)) {
    std::string data;
    data.reserve(size + 5);
    *((unsigned char *)data.data()) = PT_VOICE;
    memcpy((char*)data.data()+1, &size, 4);
    memcpy((unsigned char *)data.data() + 5, voiceBuffer, size);
    network->Send(data.data(), size + 5, IMMEDIATE_PRIORITY, UNRELIABLE);
  }
  delete[] voiceBuffer;
}

void CGmpClient::SyncGameTime(){
	BYTE data[2]={PT_GAME_INFO, 0};
	network->Send((char*)data, 1, IMMEDIATE_PRIORITY, RELIABLE);
}

void CGmpClient::Disconnect(){
	if(network->IsConnected()){
		IsInGame = false;
		IgnoreFirstTimeMessage = true;
		global_ingame->IgnoreFirstSync = true;
		LocalPlayer->SetNpcType(CPlayer::NPC_HUMAN);
		network->Disconnect();
		delete LocalPlayer;
		CPlayer::DeleteAllPlayers();
		if(VobsWorldBuilderMap.size() > 0){
			for(int i = 0; i < (int)VobsWorldBuilderMap.size(); i++)
			{
				VobsWorldBuilderMap[i].Vob->RemoveVobFromWorld();
			}
			VobsWorldBuilderMap.clear();
		}
		CChat::GetInstance()->ClearChat();
		global_ingame->WhisperingTo.clear();
		oCNpc::GetHero()->SetWeaponMode(NPC_WEAPON_NONE);
	}
	if(this->classmgr){
		delete this->classmgr;
		this->classmgr=NULL;
	}
	if(this->spawnpoint){
		delete this->spawnpoint;
		this->spawnpoint=NULL;
	}
    if (this->voiceCapture) {
        delete this->voiceCapture;
        this->voiceCapture = nullptr;
    }
    if (this->voicePlayback) {
        delete this->voicePlayback;
        this->voicePlayback = nullptr;
    }
}

MD5Sum *CGmpClient::GetMD5(LPBYTE data, DWORD size){
	HCRYPTPROV hCryptProv;
	HCRYPTHASH hHash;
	DWORD dwHashLen;
	DWORD len;
	MD5Sum *result=NULL;

	if(CryptAcquireContextA(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)){
		if(CryptCreateHash(hCryptProv, CALG_MD5, 0, 0, &hHash)){
			if(CryptHashData(hHash, data, size, 0)){
				CryptGetHashParam(hHash, HP_HASHSIZE, (BYTE*)&dwHashLen, &len, 0);
				if(dwHashLen==16){
					result=new MD5Sum;
					CryptGetHashParam(hHash, HP_HASHVAL, (BYTE*)result, &dwHashLen, 0);
					CryptDestroyHash(hHash);
					CryptReleaseContext(hCryptProv, 0);
				}
			}
		}
	}
	return result;
}

BYTE CGmpClient::AreDefaultItems(){
	zSTRING greatStringOfPunishment="\0";
	char numBuff[16];
	Patch::ObjectDestructorPatched(true);
	for(size_t i=0; i<866; i++){
		oCItem *item=oCObjectFactory::GetFactory()->CreateItem(GOTHIC_DAT_ITEMS[i]);
		greatStringOfPunishment+=item->GetInstanceName().ToChar();
		sprintf(numBuff, "%d\0", item->hp);
		greatStringOfPunishment+=numBuff;
		sprintf(numBuff, "%d\0", item->hp_max);
		greatStringOfPunishment+=numBuff;
		sprintf(numBuff, "%d\0", item->mainflags);
		greatStringOfPunishment+=numBuff;
		sprintf(numBuff, "%d\0", item->flags);
		greatStringOfPunishment+=numBuff;
		sprintf(numBuff, "%d\0", item->weight);
		greatStringOfPunishment+=numBuff;
		sprintf(numBuff, "%d\0", item->value);
		greatStringOfPunishment+=numBuff;
		sprintf(numBuff, "%d\0", item->damageType);
		greatStringOfPunishment+=numBuff;
		sprintf(numBuff, "%d\0", item->damageTotal);
		greatStringOfPunishment+=numBuff;
		for(int zxy=0; zxy<8; zxy++){
			sprintf(numBuff, "%d\0", item->damage[zxy]);
			greatStringOfPunishment+=numBuff;
		}
		sprintf(numBuff, "%d\0", item->wear);
		greatStringOfPunishment+=numBuff;
		for(int zx=0; zx<8; zx++){
			sprintf(numBuff, "%d\0", item->protection[zx]);
			greatStringOfPunishment+=numBuff;
		}
		sprintf(numBuff, "%d\0", item->nutrition);
		greatStringOfPunishment+=numBuff;
		for(int z=0; z<3; z++){
			sprintf(numBuff, "%d\0", item->cond_atr[z]);
			greatStringOfPunishment+=numBuff;
			sprintf(numBuff, "%d\0", item->cond_value[z]);
			greatStringOfPunishment+=numBuff;
			sprintf(numBuff, "%d\0", item->change_atr[z]);
			greatStringOfPunishment+=numBuff;
			sprintf(numBuff, "%d\0", item->change_value[z]);
			greatStringOfPunishment+=numBuff;
		}
		sprintf(numBuff, "%d\0", item->material);
		greatStringOfPunishment+=numBuff;
		sprintf(numBuff, "%d\0", item->munition);
		greatStringOfPunishment+=numBuff;
		sprintf(numBuff, "%d\0", item->spell);
		greatStringOfPunishment+=numBuff;
		sprintf(numBuff, "%d\0", item->range);
		greatStringOfPunishment+=numBuff;
		sprintf(numBuff, "%d\0", item->mag_circle);
		greatStringOfPunishment+=numBuff;
		oCGame::GetGame()->GetGameWorld()->RemoveVob(item);
		item->Delete(1);
	}
	MD5Sum *sum=GetMD5((BYTE*)greatStringOfPunishment.ToChar(), greatStringOfPunishment.Length());
	greatStringOfPunishment.Clear();
	Patch::ObjectDestructorPatched(false);
	BYTE ret_val=0;
	for(size_t lol=0; lol<16; lol++) if(static_cast<BYTE>(sum->data[lol]^0xFF)!=GDI_MD5[lol]) ret_val++;
	memset(sum, 0, 16);
	delete sum;
	return ret_val;
}
