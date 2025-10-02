
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

#include "CIngame.h"

#include <time.h>

#include <format>

#include "CActiveAniID.h"
#include "config.h"
#include "CLocalPlayer.h"
#include "CMainMenu.h"
#include "CMenu.h"
#include "CWatch.h"
#include "HooksManager.h"
#include "game_client.h"
#include "keyboard.h"
#include "patch.h"

CIngame* global_ingame = NULL;
extern GameClient* client;
extern CLocalPlayer* LocalPlayer;
void InterfaceLoop(void);
constexpr const char* arrow = "->";
constexpr const char* DEADB = "S_DEADB";
constexpr const char* DEAD2 = "S_DEAD";
constexpr const char* TDEADB = "T_DEADB";
constexpr const char* TURN = "TURN";
extern zCOLOR Normal;
zCOLOR COLOR_RED = zCOLOR(255, 0, 0, 255);
clock_t MuteTimer = 0;
clock_t MsgTimer = 0;
int SpamMessages = 0;
bool MuteCountdown = false;

CIngame::CIngame(CLanguage* pLang) {
  this->last_player_update = clock();
  this->lang = pLang;
  this->chat_interface = CChat::GetInstance();
  this->NextTimeSync = time(NULL) + 1;
  this->Shrinker = new CShrinker();
  this->Inventory = new CInventory(&player->inventory2);
  WritingOnChat = false;
  IgnoreFirstSync = true;
  SwampLightsOn = false;
  Movement = NULL;
  RecognizedMap = MAP_UNKNOWN;
  if (!memcmp("OLDVALLEY.ZEN", ogame->GetGameWorld()->GetWorldFilename().ToChar(), 13) ||
      !memcmp("COLONY.ZEN", ogame->GetGameWorld()->GetWorldFilename().ToChar(), 10))
    RecognizedMap = MAP_COLONY;
  if (!memcmp("OLDWORLD\\OLDWORLD.ZEN", ogame->GetGameWorld()->GetWorldFilename().ToChar(), 21))
    RecognizedMap = MAP_OLDWORLD;
  if (!memcmp("NEWWORLD\\NEWWORLD.ZEN", ogame->GetGameWorld()->GetWorldFilename().ToChar(), 21))
    RecognizedMap = MAP_KHORINIS;
  BuffTimer = 0, SecTimer = 0, ChatTimer = 0;
  PList = CPlayerList::GetInstance();
  MMap = CMap::GetInstance();
  AMenu = CAnimMenu::GetInstance();
  mapusable = false;
  if (MMap->CheckMap())
    mapusable = true;
  WhisperingTo = "";
  chat_interface->WriteMessage(NORMAL, false, "Gothic Multiplayer");
  global_ingame = this;
  HooksManager::GetInstance()->AddHook(HT_RENDER, (DWORD)CIngame::Loop, false);
}

CIngame::~CIngame() {
  delete Shrinker;
  delete Inventory;
  this->lang = NULL;
  this->chat_interface = NULL;
  this->MMap = NULL;
  this->PList = NULL;
  this->AMenu = NULL;
  this->Shrinker = NULL;
  this->Inventory = NULL;
  global_ingame = NULL;
  HooksManager::GetInstance()->RemoveHook(HT_RENDER, (DWORD)CIngame::Loop);
}

void CIngame::CheckSwampLights() {
  oCWorldTimer* Timer = ogame->GetWorldTimer();
  if (!SwampLightsOn) {
    if (Timer->IsTimeBetween(20, 00, 05, 00)) {
      SwampLightsOn = true;
      oCMobInter::SetAllMobsToState(ogame->GetGameWorld(), "PC", 1);
      int h;
      int m;
      Timer->GetTime(h, m);
      ogame->SetTime(Timer->GetDay(), h, m);
    }
  } else {
    if (Timer->IsTimeBetween(05, 00, 20, 00)) {
      SwampLightsOn = false;
      oCMobInter::SetAllMobsToState(ogame->GetGameWorld(), "PC", 0);
      int h;
      int m;
      Timer->GetTime(h, m);
      ogame->SetTime(Timer->GetDay(), h, m);
      oCMobInter::SetAllMobsToState(ogame->GetGameWorld(), "PC", 0);
    }
  }
};

void CIngame::Loop() {
  if (global_ingame) {
    if (client->network->IsConnected()) {
      if (global_ingame->NextTimeSync == time(NULL)) {
        if (!global_ingame->IgnoreFirstSync)
          client->SyncGameTime();
        else
          global_ingame->IgnoreFirstSync = false;
        global_ingame->NextTimeSync += 1200;
      }
      client->HandleNetwork();
      if (client->network->IsConnected()) {
        client->RestoreHealth();
        global_ingame->CheckForUpdate();
        global_ingame->CheckForHPDiff();
        client->SendVoice();
      }
    }
    // SENDING MY ANIMATION
    zCModel* model = player->GetModel();
    if (model) {
      zCModelAni* AniUnusual = model->aniChannels[1] ? model->aniChannels[1]->protoAni : nullptr;  // TALK, TURNR ETC
      zCModelAni* Ani = model->aniChannels[0] ? model->aniChannels[0]->protoAni : nullptr;         // ZWYKLE
      if (Ani) {
        if (Ani->GetAniName().Search(TURN) < 2) {
          CActiveAniID::GetInstance()->AddAni(Ani->GetAniID());
        }
        if (AniUnusual && Ani->GetAniID() != AniUnusual->GetAniID()) {
          if (AniUnusual->GetAniName().Search(TURN) < 2) {
            CActiveAniID::GetInstance()->AddAni(AniUnusual->GetAniID());
          }
        }
      }
    }
    // KINDA POSITION INTERPOLATION :C
    for (int i = 0; i < (int)global_ingame->Interpolation.size(); i++) {
      if (global_ingame->Interpolation[i]->IsInterpolating)
        global_ingame->Interpolation[i]->DoInterpolate();
    }
    // INVENTORY RENDER
    if (global_ingame->Inventory)
      global_ingame->Inventory->RenderInventory();
    // START SNOW IF CHRISTMAS
    if (global_ingame->Christmas)
      ogame->GetWorld()->skyControlerOutdoor->SetWeatherType(zTWEATHER_SNOW);
    // RUN SHRINKER
    global_ingame->Shrinker->Loop();
    // CHECK FOR SWAMP LIGHTS STATE
    if (global_ingame->RecognizedMap == MAP_COLONY)
      global_ingame->CheckSwampLights();
    // MUTE
    if (MuteCountdown) {
      long secs_to_unmute = (MuteTimer - clock()) / 1000;
      char tmp_char[32];
      sprintf(tmp_char, "%s : %d", (*global_ingame->lang)[CLanguage::UNMUTE_TIME].ToChar(), secs_to_unmute);
      const std::string mute_msg = std::format("{} : {}", (*global_ingame->lang)[CLanguage::UNMUTE_TIME].ToChar(), secs_to_unmute);
      screen->PrintCXY(mute_msg.c_str());
      if (secs_to_unmute < 0) {
        MuteCountdown = false;
      }
    }
    // DEATH BUG WHEN AIMING FIX
    if (player->IsDead() && !player->GetAnictrl()->IsInWater()) {
      if (player->GetWeaponMode() == NPC_WEAPON_BOW || player->GetWeaponMode() == NPC_WEAPON_MAG || player->GetWeaponMode() == NPC_WEAPON_CBOW) {
        if (!player->GetModel()->IsAnimationActive(DEADB) && !player->GetModel()->IsAnimationActive(TDEADB) &&
            !player->GetModel()->IsAnimationActive(DEAD2)) {
          player->GetModel()->StartAnimation(TDEADB);
          player->GetAnictrl()->StopTurnAnis();
          if (player) {
            oCItem* RHand = nullptr;
            oCItem* LHand = nullptr;
            if (player->GetRightHand()) {
              RHand = dynamic_cast<oCItem*>(player->GetRightHand());
            }
            if (player->GetLeftHand()) {
              LHand = dynamic_cast<oCItem*>(player->GetLeftHand());
            }
            player->DropAllInHand();
            if (RHand)
              RHand->RemoveVobFromWorld();
            if (LHand)
              LHand->RemoveVobFromWorld();
          }
        }
      }
    }
    // MAKING SURE THAT TEST MODE IS OFF FOREVER !
    if (*(int*)((DWORD)ogame + 0x0B0) != 0)
      *(int*)((DWORD)ogame + 0x0B0) = 0;
    global_ingame->HandleInput();
    global_ingame->Draw();
  }
}

void CIngame::ClearAfterWrite() {
  if (player->IsMovLock())
    player->SetMovLock(0);
  Patch::PlayerInterfaceEnabled(true);
  if (WritingOnChat)
    WritingOnChat = false;
}
void CIngame::PrepareForWrite() {
  player->GetAnictrl()->StopTurnAnis();
  Patch::PlayerInterfaceEnabled(false);
}
bool CIngame::PlayerExists(const char* PlayerName) {
  if (client->players.size() > 1) {
    for (int i = 1; i < (int)client->players.size(); i++) {
      if (client->players[i]->npc) {
        if (!memcmp(client->players[i]->npc->GetName().ToChar(), PlayerName, strlen(PlayerName)))
          return true;
      }
    }
  }
  return false;
}

void CIngame::HandleInput() {
  if ((zinput->KeyPressed(KEY_LCONTROL) || zinput->KeyPressed(KEY_RCONTROL)) && (zinput->KeyPressed(KEY_LALT) || zinput->KeyPressed(KEY_RALT)) &&
      zinput->KeyPressed(KEY_F8)) {
    if (client->network->IsConnected()) {
      client->Disconnect();
      CChat::GetInstance()->WriteMessage(NORMAL, false, zCOLOR(255, 0, 0, 255), "%s", "Disconnected!");
    }
  }
  // PLAYER LIST
  if (zinput->KeyToggled(KEY_F1)) {
    if (!PList->IsPlayerListOpen())
      PList->OpenPlayerList();
    else
      PList->ClosePlayerList();
  }
  if (PList->IsPlayerListOpen()) {
    if (zinput->KeyToggled(KEY_ESCAPE))
      PList->ClosePlayerList();
    PList->UpdatePlayerList();
  }
  // MAP
  if (mapusable) {
    if (zinput->KeyToggled(KEY_F2) && (!client->ForceHideMap)) {
      if (!MMap->Opened)
        MMap->Open();
      else
        MMap->Close();
    }
    if (MMap->Opened) {
      if (zinput->KeyToggled(KEY_ESCAPE) || (client->ForceHideMap))
        MMap->Close();
      MMap->PrintMap();
    }
  }
  // ANIM MENU
  if (zinput->KeyToggled(KEY_F3) && !player->IsDead()) {
    if (!AMenu->Opened)
      AMenu->Open();
    else
      AMenu->Close();
  }
  if (AMenu->Opened) {
    if (zinput->KeyToggled(KEY_ESCAPE))
      AMenu->Close();
    AMenu->PrintMenu();
  }
  // CHAT
  if (zinput->KeyPressed(KEY_T) && !WritingOnChat && !PList->IsPlayerListOpen()) {
    zinput->ClearKeyBuffer();
    WritingOnChat = true;
    PrepareForWrite();
  }
  if (WritingOnChat) {
    // CHAT ANIM
    if (!player->IsMovLock())
      player->SetMovLock(1);
    if (chat_interface->PrintMsgType == NORMAL) {
      int RandomAnim = rand() % 10 + 1;
      chat_interface->StartChatAnimation(RandomAnim);
    }
    // INPUT
    if (zinput->KeyToggled(KEY_ESCAPE))
      ClearAfterWrite();
    if (zinput->KeyPressed(KEY_BACKSPACE)) {
      if (ChatTimer < clock()) {
        if (chatbuffer.length() > 0)
          chatbuffer.erase(chatbuffer.end() - 1);
        ChatTimer = clock() + 150;
      }
    }
    char key = GInput::GetCharacterFormKeyboard();
    screen->SetFontColor(Normal);
    if (chat_interface->PrintMsgType != WHISPER)
      screen->Print(0, 200 * Config::Instance().ChatLines, arrow);
    else
      screen->Print(0, 200 * (Config::Instance().ChatLines + 1), arrow);
    if (key == 0x0D) {
      if (chatbuffer.length() != 0) {
        switch (chat_interface->PrintMsgType) {
          case NORMAL:
            if (!memcmp("passwd", chatbuffer.c_str(), 6) || !memcmp("login", chatbuffer.c_str(), 5))
              CChat::GetInstance()->WriteMessage(NORMAL, false, zCOLOR(255, 0, 0), (*lang)[CLanguage::CHAT_WRONGWINDOW].ToChar());
            else {
              if (MuteTimer < clock()) {
                if (SpamMessages < 3) {
                  client->SendMessage(chatbuffer.c_str());
                  if (ChatTimer > clock()) {
                    SpamMessages++;
                  } else {
                    SpamMessages = 0;
                  }
                  ChatTimer = clock() + 3000;
                } else {
                  MuteTimer = clock() + 60000;
                  SpamMessages = 0;
                  MuteCountdown = true;
                }
              }
            }
            break;
          case WHISPER:
            if (chatbuffer[0] == '/') {
              if (!memcmp(player->GetName().ToChar(), chatbuffer.c_str() + 1, strlen(chatbuffer.c_str() + 1)))
                chat_interface->WriteMessage(WHISPER, false, zCOLOR(255, 0, 0), (*lang)[CLanguage::CHAT_CANTWHISPERTOYOURSELF].ToChar());
              else {
                if (PlayerExists(chatbuffer.c_str() + 1)) {
                  WhisperingTo = chatbuffer.c_str() + 1;
                  chat_interface->SetWhisperTo(WhisperingTo);
                } else
                  chat_interface->WriteMessage(WHISPER, false, zCOLOR(255, 0, 0), (*lang)[CLanguage::CHAT_PLAYER_DOES_NOT_EXIST].ToChar());
              }
            } else if (WhisperingTo.length() > 0) {
              client->SendWhisper(WhisperingTo.c_str(), chatbuffer.c_str());
            }
            break;
          case ADMIN:
            client->SendCommand(chatbuffer.c_str());
            CChat::GetInstance()->WriteMessage(ADMIN, false, COLOR_RED, "%s", chatbuffer.c_str());
            break;
        }
        chatbuffer.clear();
      }
      ClearAfterWrite();
    } else if ((key >= 0x20) || ((key & 0x80) && (Config::Instance().keyboardlayout == 1))) {
      if (chatbuffer.length() < 84)
        chatbuffer += (char)key;
    }
    if (clock() > BuffTimer && clock() > SecTimer) {
      BuffTimer = clock() + 750;
      SecTimer = clock() + 1500;
    }
    if (BuffTimer > clock()) {
      sprintf(buffer, "%s_", chatbuffer.c_str());
      ChatTmp = buffer;
    } else
      ChatTmp = chatbuffer.c_str();
    if (chat_interface->PrintMsgType != WHISPER)
      screen->Print(200, 200 * Config::Instance().ChatLines, ChatTmp);
    else
      screen->Print(200, 200 * (Config::Instance().ChatLines + 1), ChatTmp);
  }
}

void CIngame::Draw() {
  this->chat_interface->PrintChat();
  if (Config::Instance().watch)
    CWatch::GetInstance()->PrintWatch();
  /*if(client->IsConnected()){
                  char buffer[32];
                  sprintf(buffer, "Your ping: %d", client->GetPing());
                  szPing=buffer;
                  zCView::GetScreen()->Print(5000,0, szPing);
  }*/
}

void CIngame::CheckForUpdate() {
  if (clock() - this->last_player_update > 80) {
    client->UpdatePlayerStats(static_cast<short>(CActiveAniID::GetInstance()->GetAniID()));
    this->last_player_update = clock();
  }
}

void CIngame::CheckForHPDiff() {
  for (size_t i = 0; i < client->players.size(); i++) {
    if (client->players[i]->hp != static_cast<short>(client->players[i]->npc->attribute[NPC_ATR_HITPOINTS])) {
      if (!ValidatePlayerForHPDiff(client->players[i])) {
        if (client->players[i]->npc->attribute[NPC_ATR_HITPOINTS] <= 0) {
          client->players[i]->RespawnPlayer();
        }
        client->players[i]->npc->attribute[NPC_ATR_HITPOINTS] = static_cast<int>(client->players[i]->hp);
      }
      client->SendHPDiff(i, static_cast<short>(static_cast<short>(client->players[i]->npc->attribute[NPC_ATR_HITPOINTS]) - client->players[i]->hp));
      client->players[i]->hp = static_cast<short>(client->players[i]->npc->attribute[NPC_ATR_HITPOINTS]);
    }
  }
}

bool CIngame::ValidatePlayerForHPDiff(CPlayer* player) {
  if (oCNpc::player == player->npc) {
    return true;
  }
  if (oCNpc::player->GetFocusNpc() == player->npc && oCNpc::player->GetWeaponMode() > NPC_WEAPON_NONE &&
      oCNpc::player->GetWeaponMode() < NPC_WEAPON_MAG) {
    return true;
  }
  if (oCNpc::player->GetWeaponMode() == NPC_WEAPON_BOW || oCNpc::player->GetWeaponMode() == NPC_WEAPON_CBOW ||
      oCNpc::player->GetWeaponMode() == NPC_WEAPON_MAG) {
    return oCNpc::player->GetDistanceToVob(*player->npc) < 5000.0f;
  }
  return false;
}
