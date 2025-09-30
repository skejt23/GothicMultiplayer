/*
MIT License

Copyright (c) 2023 Gothic Multiplayer Team.

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

#include <string>

#include "CLanguage.h"
#include "CPlayer.h"
#include "CSyncFuncs.h"
#include "HooksManager.h"
#include "Network.h"
#include "VoiceCapture.h"
#include "VoicePlayback.h"
#include "world-builder\load.h"
#include "ZenGin/zGothicAPI.h"

enum FILE_REQ { WB_FILE = 1, NULL_SIZE = 255 };

struct MD5Sum {
  BYTE data[16];
};

union STime {
  int time;
  struct {
    unsigned short day;
    unsigned char hour, min;
  };
};

class GameClient : public CSyncFuncs {
public:
  GameClient(const char* ip, CLanguage* ptr);
  ~GameClient(void);

  void HandleNetwork(void);
  bool IsConnected(void);
  bool Connect(void);
  void JoinGame();
  zSTRING& GetLastError(void);
  void SendDropItem(short Instance, short amount);
  void SendTakeItem(short Instance);
  void SendCastSpell(oCNpc* Target, short SpellId);
  void SendMessage(const char* msg);
  void SendWhisper(const char* player_name, const char* msg);
  void SendCommand(const char* msg);
  void SendVoice();
  void UpdatePlayerStats(short anim);
  void SendHPDiff(size_t who, short diff);
  void SyncGameTime(void);
  void Disconnect(void);
  void DownloadWBFile(void);
  void RestoreHealth(void);

  VoiceCapture* voiceCapture;
  VoicePlayback* voicePlayback;
  std::vector<CPlayer*> players;
  std::vector<Info> VobsWorldBuilderMap;
  int HeroLastHp;
  zSTRING map;
  bool IsAdminOrModerator;
  bool IgnoreFirstTimeMessage;
  bool IsInGame;
  short mp_restore;
  int DropItemsAllowed;
  int ForceHideMap;
  CLanguage* lang;
  Network* network;
  bool IsReadyToJoin;

private:
  int clientPort;
  std::string clientHost;
  time_t last_mp_regen;

  std::string GetServerAddresForHTTPDownloader();
};
