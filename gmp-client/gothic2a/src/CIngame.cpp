
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

#include <cstdint>
#include <cstring>
#include <time.h>

#include "CActiveAniID.h"
#include "CMenu.h"
#include "HooksManager.h"
#include "dev/dev_tools.h"
#include "config.h"
#include "main_menu.h"
#include "Mod.h"
#include "net_game.h"

CIngame* global_ingame = NULL;
void InterfaceLoop(void);
constexpr const char* DEADB = "S_DEADB";
constexpr const char* DEAD2 = "S_DEAD";
constexpr const char* TDEADB = "T_DEADB";
constexpr const char* TURN = "TURN";
extern zCOLOR Normal;
zCOLOR COLOR_RED = zCOLOR(255, 0, 0, 255);

namespace {
int GetCurrentAnimationId() {
  CActiveAniID* active_ani_id = CActiveAniID::GetInstance();
  if (!active_ani_id) {
    return -1;
  }

  return active_ani_id->GetAniID();
}

void TrackLocalActiveAnimations() {
  CActiveAniID* active_ani_id = CActiveAniID::GetInstance();
  if (!active_ani_id) {
    return;
  }

  active_ani_id->Observe();
  auto transitions = active_ani_id->TakeTransitions();
  if (NetGame::Instance().IsConnected()) {
    for (const auto& transition : transitions) {
      NetGame::Instance().SendAnimationAction(transition.name, transition.started, transition.source_time_ms);
    }
  }
}

void ApplyAimingDeathAnimationFix() {
  if (!player || !player->GetAnictrl() || !player->GetModel()) {
    return;
  }

  if (player->IsDead() && !player->GetAnictrl()->IsInWater()) {
    if (player->GetWeaponMode() == NPC_WEAPON_BOW || player->GetWeaponMode() == NPC_WEAPON_MAG || player->GetWeaponMode() == NPC_WEAPON_CBOW) {
      if (!player->GetModel()->IsAnimationActive(DEADB) && !player->GetModel()->IsAnimationActive(TDEADB) &&
          !player->GetModel()->IsAnimationActive(DEAD2)) {
        player->GetModel()->StartAnimation(TDEADB);
        player->GetAnictrl()->StopTurnAnis();
        zCVob* rightVob = player->GetRightHand();
        zCVob* leftVob = player->GetLeftHand();
        zSTRING rightSlot(NPC_NODE_RIGHTHAND);
        zSTRING leftSlot(NPC_NODE_LEFTHAND);
        oCItem* RHand = dynamic_cast<oCItem*>(player->RemoveFromSlot(rightSlot, 0, 1));
        oCItem* LHand = dynamic_cast<oCItem*>(player->RemoveFromSlot(leftSlot, 0, 1));
        if (!RHand) {
          RHand = dynamic_cast<oCItem*>(rightVob);
        }
        if (!LHand) {
          LHand = dynamic_cast<oCItem*>(leftVob);
        }
        if (RHand) {
          RHand->RemoveVobFromWorld();
        }
        if (LHand && LHand != RHand) {
          LHand->RemoveVobFromWorld();
        }
      }
    }
  }
}
}  // namespace

CIngame::CIngame() {
  this->last_player_update = std::chrono::steady_clock::now();
  this->chat_interface = CChat::GetInstance();
  this->NextTimeSync = time(NULL) + 1;
  this->Shrinker = new CShrinker();
  this->Inventory = player ? new CInventory(&player->inventory2) : nullptr;
  IgnoreFirstSync = true;
  SwampLightsOn = false;
  RecognizedMap = MAP_UNKNOWN;
  if (ogame && ogame->GetGameWorld()) {
    const char* world_filename = ogame->GetGameWorld()->GetWorldFilename().ToChar();
    if (!std::strncmp("OLDVALLEY.ZEN", world_filename, 13) ||
        !std::strncmp("COLONY.ZEN", world_filename, 10))
      RecognizedMap = MAP_COLONY;
    if (!std::strncmp("OLDWORLD\\OLDWORLD.ZEN", world_filename, 21))
      RecognizedMap = MAP_OLDWORLD;
    if (!std::strncmp("NEWWORLD\\NEWWORLD.ZEN", world_filename, 21))
      RecognizedMap = MAP_KHORINIS;
  }
  global_ingame = this;
  HooksManager::GetInstance()->AddHook(HT_RENDER, (DWORD)CIngame::Loop);
}

CIngame::~CIngame() {
  delete Shrinker;
  delete Inventory;
  this->chat_interface = NULL;
  this->Shrinker = NULL;
  this->Inventory = NULL;
  global_ingame = NULL;
  HooksManager::GetInstance()->RemoveHook(HT_RENDER, (DWORD)CIngame::Loop);
}

void CIngame::CheckSwampLights() {
  if (!ogame || !ogame->GetWorldTimer() || !ogame->GetGameWorld()) {
    return;
  }

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
    RefreshLocalActionObservation();
    // Capture every render before the lower-frequency pose sample. One-shot
    // transitions use the reliable action stream independently of pose cadence.
    TrackLocalActiveAnimations();
    if (NetGame::Instance().IsConnected()) {
      if (global_ingame->NextTimeSync == time(NULL)) {
        if (!global_ingame->IgnoreFirstSync)
          NetGame::Instance().SyncGameTime();
        else
          global_ingame->IgnoreFirstSync = false;
        global_ingame->NextTimeSync += 1200;
      }
      NetGame::Instance().HandleNetwork();
      if (NetGame::Instance().IsConnected()) {
        NetGame::Instance().RestoreHealth();
        global_ingame->CheckForUpdate();
        global_ingame->CheckForHPDiff();
      }
    }
    // KINDA POSITION INTERPOLATION :C
    for (int i = 0; i < (int)global_ingame->Interpolation.size(); i++) {
      if (global_ingame->Interpolation[i] && global_ingame->Interpolation[i]->IsInterpolating)
        global_ingame->Interpolation[i]->DoInterpolate();
    }
    for (auto* actor : NetGame::Instance().players) {
      if (actor)
        actor->UpdateMagicPresentation();
    }
    // INVENTORY RENDER
    if (global_ingame->Inventory)
      global_ingame->Inventory->RenderInventory();
    // RUN SHRINKER
    if (global_ingame->Shrinker)
      global_ingame->Shrinker->Loop();
    // CHECK FOR SWAMP LIGHTS STATE
    if (global_ingame->RecognizedMap == MAP_COLONY)
      global_ingame->CheckSwampLights();
    // DEATH BUG WHEN AIMING FIX
    ApplyAimingDeathAnimationFix();
    // MAKING SURE THAT TEST MODE IS OFF FOREVER !
    if (ogame && *(int*)((DWORD)ogame + 0x0B0) != 0)
      *(int*)((DWORD)ogame + 0x0B0) = 0;
    global_ingame->HandleInput();
    global_ingame->Draw();
  }
}
bool CIngame::PlayerExists(const char* PlayerName) {
  if (!PlayerName) {
    return false;
  }

  if (NetGame::Instance().players.size() > 1) {
    const auto player_name_length = std::strlen(PlayerName);
    for (int i = 1; i < (int)NetGame::Instance().players.size(); i++) {
      if (NetGame::Instance().players[i] && !NetGame::Instance().players[i]->base_player().is_npc() && NetGame::Instance().players[i]->npc) {
        if (!std::strncmp(NetGame::Instance().players[i]->npc->GetName().ToChar(), PlayerName, player_name_length))
          return true;
      }
    }
  }
  return false;
}

void CIngame::HandleInput() {
  if (chat_interface->IsInputActive()) {
    return;
  }

  if ((zinput->KeyPressed(KEY_LCONTROL) || zinput->KeyPressed(KEY_RCONTROL)) && (zinput->KeyPressed(KEY_LALT) || zinput->KeyPressed(KEY_RALT)) &&
      zinput->KeyPressed(KEY_F8)) {
    if (NetGame::Instance().IsConnected()) {
      NetGame::Instance().Disconnect();
      CChat::GetInstance()->WriteMessage(NORMAL, false, zCOLOR(255, 0, 0, 255), "Disconnected!");
    }
  }
  // DEBUG TOOLS
  debug::DevTools::Instance().HandleInput(chat_interface->IsInputActive());
}

void CIngame::Draw() {
  this->chat_interface->PrintChat();
  debug::DevTools::Instance().Render();

  // Display DirectX version in bottom right
  const char* dx_version = "";
  switch (Config::Instance().GetRendererType()) {
    case Config::RendererType::D3D7:
      dx_version = "DirectX 7";
      break;
    case Config::RendererType::D3D9:
      dx_version = "DirectX 9";
      break;
    case Config::RendererType::D3D11:
      dx_version = "DirectX 11";
      break;
  }
  screen->PrintCX(8000, dx_version);

  /*if(client->IsConnected()){
                  char buffer[32];
                  sprintf(buffer, "Your ping: %d", client->GetPing());
                  szPing=buffer;
                  zCView::GetScreen()->Print(5000,0, szPing);
  }*/
}

void CIngame::CheckForUpdate() {
  const auto now = std::chrono::steady_clock::now();
  if (now - this->last_player_update > std::chrono::milliseconds(80)) {
    NetGame::Instance().UpdatePlayerStats(GetCurrentAnimationId());
    this->last_player_update = now;
  }
}

void CIngame::CheckForHPDiff() {
  for (size_t i = 0; i < NetGame::Instance().players.size(); i++) {
    if (!NetGame::Instance().players[i] || !NetGame::Instance().players[i]->npc) {
      continue;
    }

    if (NetGame::Instance().players[i]->base_player().health() !=
        NetGame::Instance().players[i]->npc->attribute[NPC_ATR_HITPOINTS]) {
      if (NetGame::Instance().players[i]->IsLocalPlayer()) {
        NetGame::Instance().players[i]->base_player().set_health(
            NetGame::Instance().players[i]->npc->attribute[NPC_ATR_HITPOINTS]);
        continue;
      }
      const auto life_state = NetGame::Instance().players[i]->base_player().life_state();
      if (life_state == PLAYER_LIFE_DEAD || life_state == PLAYER_LIFE_UNCONSCIOUS) {
        continue;
      }
      NetGame::Instance().players[i]->npc->attribute[NPC_ATR_HITPOINTS] =
          static_cast<int>(NetGame::Instance().players[i]->base_player().health());
    }
  }
}
