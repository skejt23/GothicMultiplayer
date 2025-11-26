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

#include "net_game.h"

#include <bitsery/adapter/buffer.h>
#include <bitsery/bitsery.h>
#include <bitsery/ext/std_optional.h>
#include <bitsery/traits/array.h>
#include <bitsery/traits/string.h>
#include <bitsery/traits/vector.h>
#include <spdlog/spdlog.h>
#include <wincrypt.h>

#include <algorithm>
#include <array>
#include <ctime>
#include <filesystem>
#include <glm/glm.hpp>
#include <list>
#include <sstream>
#include <string>
#include <optional>
#include <vector>

#include "sol/sol.hpp"

#include "CChat.h"
#include "CIngame.h"
#include "Interface.h"
#include "ZenGin/zGothicAPI.h"
#include "config.h"
#include "discord_presence.h"
#include "language.h"
#include "net_enums.h"
#include "packets.h"
#include "patch.h"
#include "player_name_utils.hpp"
#include "scripting/gothic_bindings.h"
#include "client_resources/process_input.h"
#include "shared/event.h"
#include "client_resources/client_events.h"

const char* LANG_DIR = ".\\Multiplayer\\Localization\\";

float fWRatio, fHRatio;
extern CIngame* global_ingame;

using namespace Net;

NetGame::NetGame() : task_scheduler(nullptr), game_client(nullptr), resource_runtime(nullptr) {
  task_scheduler = std::make_unique<gmp::GothicTaskScheduler>();
  game_client = std::make_unique<gmp::client::GameClient>(*this, *task_scheduler);
  resource_runtime = std::make_unique<ClientResourceRuntime>();
  resource_runtime->SetServerInfoProvider(*game_client);
  gmp::gothic::BindGothicSpecific(resource_runtime->GetLuaState());
}

void __stdcall NetGame::ProcessTaskScheduler() {
  NetGame& instance = NetGame::Instance();
  if (instance.task_scheduler) {
    instance.task_scheduler->ProcessTasks();
  }
  if (instance.resource_runtime) {
    instance.resource_runtime->ProcessTimers();
  }
  if (zinput) {
    gmp::gothic::ProcessInput(zinput);
  }
  EventManager::Instance().TriggerEvent(gmp::client::kEventOnRenderName, 0);
}

bool NetGame::Connect(std::string_view full_address) {
  game_client->ConnectAsync(full_address);
  // Return true to indicate connection attempt started
  // Actual connection status will be reported via callbacks
  return true;
}

void NetGame::RestoreHealth() {
  if (!mp_restore || !IsInGame) {
    return;
  }
  if (last_mp_regen != time(NULL)) {
    last_mp_regen = time(NULL);
    if (player->attribute[NPC_ATR_MANAMAX] != player->attribute[NPC_ATR_MANA]) {
      player->attribute[NPC_ATR_MANA] = player->attribute[NPC_ATR_MANA] + mp_restore;
      if (player->attribute[NPC_ATR_MANA] > player->attribute[NPC_ATR_MANAMAX])
        player->attribute[NPC_ATR_MANA] = player->attribute[NPC_ATR_MANAMAX];
    }
  }
}

void NetGame::HandleNetwork() {
  if (IsConnected()) {
    game_client->HandleNetwork();
  }
}

bool NetGame::IsConnected() {
  return game_client->IsConnected();
}

Gothic2APlayer* NetGame::GetPlayerById(std::uint64_t player_id) {
  for (auto* player : players) {
    if (player->base_player().id() == player_id) {
      return player;
    }
  }
  return nullptr;
}

void NetGame::JoinGame() {
  if (IsReadyToJoin) {
    HooksManager::GetInstance()->AddHook(HT_RENDER, (DWORD)InterfaceLoop);

    auto sanitized_name = SanitizePlayerName(Config::Instance().Nickname.ToChar());
    player->name[0] = sanitized_name.c_str();

    // Call the new GameClient JoinGame method
    game_client->JoinGame(sanitized_name, sanitized_name, 0, 0, 0, 0);

    // Set up the local player now that we have the player ID from the server
    CIngame* g = new CIngame();
    // LocalPlayer->id = game_client->player_manager().GetLocalPlayer().id();
    // LocalPlayer->enable = TRUE;
    // LocalPlayer->SetNpc(player);
    // LocalPlayer->hp = static_cast<short>(LocalPlayer->GetHealth());
    // LocalPlayer->update_hp_packet = 0;
    // LocalPlayer->npc->SetMovLock(0);
    // this->players.push_back(LocalPlayer);
    // this->HeroLastHp = player->attribute[NPC_ATR_HITPOINTS];

    
    EventManager::Instance().TriggerEvent(gmp::client::kEventOnInitName, 0);
  }
}

void NetGame::SendMessage(const char* msg) {
  game_client->SendChatMessage(msg);
}

void NetGame::SendWhisper(const char* player_name, const char* msg) {
  // Find the player by name
  bool found = false;
  size_t length = strlen(player_name);
  std::uint64_t recipient_id = 0;

  for (size_t i = 0; i < this->players.size(); i++) {
    if (this->players[i]->GetNameLength() == length) {
      if (!memcmp(this->players[i]->GetName(), player_name, length)) {
        recipient_id = this->players[i]->base_player().id();
        found = true;
        break;
      }
    }
  }

  if (found) {
    game_client->SendWhisper(recipient_id, msg);
  }
}

void NetGame::SendCommand(const char* msg) {
  game_client->SendCommand(msg);
}

void NetGame::SendCastSpell(oCNpc* Target, short SpellId) {
  std::uint64_t target_id = 0;
  if (Target) {
    for (int i = 0; i < (int)players.size(); i++) {
      if (players[i]->npc == Target) {
        target_id = players[i]->base_player().id();
        break;
      }
    }
  }
  game_client->SendCastSpell(target_id, static_cast<std::uint16_t>(SpellId));
}

void NetGame::SendDropItem(short instance, short amount) {
  game_client->SendDropItem(static_cast<std::uint16_t>(instance), static_cast<std::uint16_t>(amount));
}

void NetGame::SendTakeItem(short instance) {
  game_client->SendTakeItem(static_cast<std::uint16_t>(instance));
}

glm::vec3 Vec3ToGlmVec3(const zVEC3& vec) {
  return glm::vec3(vec[VX], vec[VY], vec[VZ]);
}

std::uint8_t GetHeadDirectionByte(oCNpc* Hero) {
  zVEC2 HeadVar = zVEC2(Hero->GetAnictrl()->lookTargetx, Hero->GetAnictrl()->lookTargety);
  if (HeadVar[VX] == 0)
    return static_cast<uint8_t>(Gothic2APlayer::HEAD_LEFT);
  else if (HeadVar[VX] == 1)
    return static_cast<uint8_t>(Gothic2APlayer::HEAD_RIGHT);
  else if (HeadVar[VY] == 0)
    return static_cast<uint8_t>(Gothic2APlayer::HEAD_UP);
  else if (HeadVar[VY] == 1)
    return static_cast<uint8_t>(Gothic2APlayer::HEAD_DOWN);

  return 0;
}

void NetGame::UpdatePlayerStats(short anim) {
  // Gather player state from the Gothic engine
  PlayerState state;
  state.position = Vec3ToGlmVec3(player->GetPositionWorld());
  state.nrot = Vec3ToGlmVec3(player->trafoObjToWorld.GetRightVector());
  state.left_hand_item_instance = player->GetLeftHand() ? static_cast<short>(player->GetLeftHand()->GetInstance()) : 0;
  state.right_hand_item_instance = player->GetRightHand() ? static_cast<short>(player->GetRightHand()->GetInstance()) : 0;
  state.equipped_armor_instance = player->GetEquippedArmor() ? static_cast<short>(player->GetEquippedArmor()->GetInstance()) : 0;
  state.animation = anim;
  state.health_points = static_cast<std::int16_t>(player->attribute[NPC_ATR_HITPOINTS]);
  state.mana_points = static_cast<std::int16_t>(player->attribute[NPC_ATR_MANA]);
  state.weapon_mode = static_cast<uint8_t>(player->GetWeaponMode());
  state.active_spell_nr = player->GetActiveSpellNr() > 0 ? static_cast<uint8_t>(player->GetActiveSpellNr()) : 0;
  state.head_direction = GetHeadDirectionByte(player);
  state.melee_weapon_instance = player->GetEquippedMeleeWeapon() ? static_cast<short>(player->GetEquippedMeleeWeapon()->GetInstance()) : 0;
  state.ranged_weapon_instance = player->GetEquippedRangedWeapon() ? static_cast<short>(player->GetEquippedRangedWeapon()->GetInstance()) : 0;

  game_client->UpdatePlayerStats(state);
}

void NetGame::SendHPDiff(size_t who, short diff) {
  if (who < this->players.size()) {
    game_client->SendHPDiff(this->players[who]->base_player().id(), diff);
  }
}

void NetGame::SyncGameTime() {
  game_client->SyncGameTime();
}

void NetGame::Disconnect() {
  if (game_client->IsConnected()) {
    IsInGame = false;
    IsReadyToJoin = false;
    if (global_ingame) {
      global_ingame->IgnoreFirstSync = true;
    }
    // if (LocalPlayer) {
    //   LocalPlayer->SetNpcType(Gothic2APlayer::NPC_HUMAN);
    // }
    game_client->Disconnect();
    Gothic2APlayer::DeleteAllPlayers();
    CChat::GetInstance()->ClearChat();
    if (global_ingame) {
      global_ingame->WhisperingTo.clear();
    }
  }

  if (resource_runtime) {
    EventManager::Instance().TriggerEvent(gmp::client::kEventOnExitName, 0);
    gmp::gothic::CleanupGothicViews();
    resource_runtime->UnloadResources();
  }
}

// ============================================================================
// EventObserver Implementation
// ============================================================================

void NetGame::OnConnectionStarted() {
  SPDLOG_INFO("Connection attempt started...");
}

void NetGame::OnConnected() {
  SPDLOG_INFO("Successfully connected to server");
}

void NetGame::OnConnectionFailed(const std::string& error) {
  SPDLOG_ERROR("Connection failed: {}", error);
  IsReadyToJoin = false;
  // Could show error message to user here
}

void NetGame::OnDisconnected() {
  SPDLOG_INFO("Disconnected from server");
  IsReadyToJoin = false;
  if (resource_runtime) {
    EventManager::Instance().TriggerEvent(gmp::client::kEventOnExitName, 0);
    gmp::gothic::CleanupGothicViews();
    resource_runtime->UnloadResources();
  }
}

void NetGame::OnConnectionLost() {
  SPDLOG_WARN("Connection lost");
  Disconnect();
  auto pos = player->GetPositionWorld();
  player->ResetPos(pos);
  IsInGame = false;
  IsReadyToJoin = false;
  CChat::GetInstance()->WriteMessage(NORMAL, false, zCOLOR(255, 0, 0, 255), "%s", Language::Instance()[Language::DISCONNECTED].ToChar());
}

bool NetGame::RequestResourceDownloadConsent(std::size_t resource_count, std::uint64_t total_bytes) {
  if (resource_count == 0 || total_bytes == 0) {
    return true;
  }

  const double total_mb = static_cast<double>(total_bytes) / (1024.0 * 1024.0);
  SPDLOG_INFO("Server requires downloading {} resource pack(s) ({:.2f} MiB)", resource_count, total_mb);
  CChat::GetInstance()->WriteMessage(NORMAL, false, zCOLOR(0, 200, 255, 255), "Server requires downloading %.2f MiB of client resources", total_mb);
  return true;
}

void NetGame::OnResourceDownloadProgress(const std::string& resource_name, std::uint64_t downloaded_bytes, std::uint64_t total_bytes) {
  if (total_bytes == 0) {
    return;
  }
  const double percent = (static_cast<double>(downloaded_bytes) / static_cast<double>(total_bytes)) * 100.0;
  SPDLOG_INFO("Downloading resources... {} ({:.2f}% complete)", resource_name, percent);
}

void NetGame::OnResourceDownloadFailed(const std::string& reason) {
  SPDLOG_ERROR("Resource download failed: {}", reason);
  IsReadyToJoin = false;
  CChat::GetInstance()->WriteMessage(NORMAL, false, zCOLOR(255, 0, 0, 255), "Resource download failed: %s", reason.c_str());
  Disconnect();
}

void NetGame::OnResourcesReady() {
  SPDLOG_INFO("Client signaled that resources are ready; consuming payloads");
  auto payloads = game_client->ConsumeDownloadedResources();

  if (!resource_runtime) {
    SPDLOG_ERROR("Client resource runtime is unavailable; disconnecting");
    OnResourceDownloadFailed("Client resource runtime unavailable");
    return;
  }

  SPDLOG_INFO("Loading {} resource payload(s) into runtime", payloads.size());
  std::string error_message;
  if (game_client->player_manager().HasLocalPlayer()) {
    resource_runtime->GetLuaState()["heroId"] =
        static_cast<int>(game_client->player_manager().GetLocalPlayer().id());
  } else {
    resource_runtime->GetLuaState()["heroId"] = sol::lua_nil;
  }
  if (!resource_runtime->LoadResources(std::move(payloads), error_message)) {
    if (error_message.empty()) {
      error_message = "Failed to initialize client resources";
    }
    OnResourceDownloadFailed(error_message);
    return;
  }

  SPDLOG_INFO("Client resources ready; player may join");
  SPDLOG_INFO("All required client resources downloaded and loaded");
  IsReadyToJoin = true;
  CChat::GetInstance()->WriteMessage(NORMAL, false, zCOLOR(0, 255, 0, 255), "Client resources ready. You may join the server.");
}

void NetGame::OnMapChange(const std::string& map_name) {
  std::string currentMap = ogame->GetGameWorld()->GetWorldFilename().ToChar();

  if (map_name != currentMap) {
    this->map = map_name.c_str();
  } else if (!this->map.IsEmpty()) {
    this->map.Clear();
  }
}

void NetGame::OnGameInfoReceived(std::uint32_t raw_game_time, std::uint8_t flags) {
  STime t;
  t.time = static_cast<int>(raw_game_time);
  ogame->SetTime(static_cast<int>(t.day), static_cast<int>(t.hour), static_cast<int>(t.min));

  oCGame::s_bUsePotionKeys = flags & 0x01;
  this->DropItemsAllowed = flags & 0x02;
  this->ForceHideMap = flags & 0x04;
}

void NetGame::OnLocalPlayerJoined(gmp::client::Player& player) {
  SPDLOG_INFO("Local player registered with id {}", player.id());
}

void NetGame::OnLocalPlayerSpawned(gmp::client::Player& player) {
  this->IsInGame = true;

  Gothic2APlayer* local_player = new Gothic2APlayer(player, true);
  local_player->SetNpc(::player);
  zVEC3 pos(player.position().x, player.position().y, player.position().z);

  SPDLOG_INFO("Local player spawned at position ({}, {}, {})", player.position().x, player.position().y, player.position().z);
  local_player->SetPosition(pos);
  players.insert(players.begin(), local_player);
  EventManager::Instance().TriggerEvent(gmp::client::kEventOnPlayerCreateName,
                                        gmp::client::PlayerLifecycleEvent{player.id()});
}

void NetGame::OnPlayerJoined(gmp::client::Player& new_player) {
  // Remote player metadata registered; actual spawn handled in OnPlayerSpawned.
  (void)new_player;
}

void NetGame::OnPlayerSpawned(gmp::client::Player& new_player) {
  SpawnRemotePlayer(new_player);
}

void NetGame::SpawnRemotePlayer(gmp::client::Player& new_player) {
  Gothic2APlayer* newhero = new Gothic2APlayer(new_player);
  zVEC3 pos(new_player.position().x, new_player.position().y, new_player.position().z);
  oCNpc* npc = zfactory->CreateNpc(player->GetInstance());
  newhero->SetNpc(npc);
  newhero->npc->SetGuild(9);
  newhero->base_player().set_hp(static_cast<short>(newhero->GetHealth()));
  newhero->SetPosition(pos);
  newhero->SetName(new_player.name().c_str());
  (void)new_player;

  CChat::GetInstance()->WriteMessage(NORMAL, false, zCOLOR(0, 255, 0, 255), "%s%s", new_player.name().c_str(),
                                     Language::Instance()[Language::SOMEONE_JOIN_GAME].ToChar());
  newhero->base_player().set_enabled(false);
  newhero->base_player().set_update_hp_packet_counter(0);
  this->players.push_back(newhero);
}

void NetGame::OnPlayerLeft(std::uint64_t player_id, const std::string& player_name) {
  for (size_t i = 1; i < this->players.size(); i++) {
    if (this->players[i]->base_player().id() == player_id) {
      CChat::GetInstance()->WriteMessage(NORMAL, false, zCOLOR(255, 0, 0, 255), "%s%s", this->players[i]->GetName(),
                                         Language::Instance()[Language::SOMEONEDISCONNECT_FROM_SERVER].ToChar());
      this->players[i]->LeaveGame();
      EventManager::Instance().TriggerEvent(gmp::client::kEventOnPlayerDestroyName,
                                            gmp::client::PlayerLifecycleEvent{player_id});
      delete this->players[i];
      this->players.erase(this->players.begin() + i);
      break;
    }
  }
}

void NetGame::OnPlayerStateUpdate(std::uint64_t player_id, const PlayerState& state) {
  static const zSTRING Door = "DOOR";
  static const zSTRING BowSound = "BOWSHOOT";
  static const zSTRING CrossbowSound = "CROSSBOWSHOOT";
  static const zSTRING Lever = "LEVER";
  static const zSTRING Touchplate = "TOUCHPLATE";
  static const zSTRING VWheel = "VWHEEL";
  static const zSTRING Arrows = "ITRW_ARROW";
  static const zSTRING Bolt = "ITRW_BOLT";

  Gothic2APlayer* cplayer = GetPlayerById(player_id);
  if (!cplayer) {
    return;
  }

  // Update position
  zVEC3 pos(state.position.x, state.position.y, state.position.z);
  if (!cplayer->base_player().is_enabled()) {
    cplayer->npc->Enable(pos);
    cplayer->base_player().set_enabled(true);
  } else {
    cplayer->AnalyzePosition(pos);
  }

  // Update rotation
  if (!cplayer->npc->IsDead()) {
    const auto& right_vector = state.nrot;
    // Map current right vector components to legacy nrot indices
    float legacy_nx = right_vector.z;  // m[2][0]
    float legacy_ny = right_vector.x;  // m[0][0]
    float legacy_nz = right_vector.y;  // m[1][0]

    // Column 2 (position/orientation forward basis parts)
    cplayer->npc->trafoObjToWorld.v[0][2] = -legacy_nx;
    cplayer->npc->trafoObjToWorld.v[1][2] = 0.0f;
    cplayer->npc->trafoObjToWorld.v[2][2] = legacy_ny;

    // Column 0 (right/normal vector)
    cplayer->npc->trafoObjToWorld.v[0][0] = legacy_ny;
    cplayer->npc->trafoObjToWorld.v[2][0] = legacy_nx;
    cplayer->npc->trafoObjToWorld.v[1][0] = legacy_nz;
  }

  // Update left hand item
  if (state.left_hand_item_instance == 0) {
    if (cplayer->npc->GetLeftHand()) {
      oCItem* lptr = static_cast<oCItem*>(cplayer->npc->GetLeftHand());
      oCItem* ptr = static_cast<oCItem*>(cplayer->npc->GetRightHand());
      cplayer->npc->DropAllInHand();
      lptr->RemoveVobFromWorld();
      if (ptr)
        cplayer->npc->SetRightHand(ptr);
    }
  } else if (state.left_hand_item_instance > 5892 && state.left_hand_item_instance < 7850) {
    if (!cplayer->npc->GetLeftHand()) {
      oCItem* New = zfactory->CreateItem(static_cast<int>(state.left_hand_item_instance));
      cplayer->npc->SetLeftHand(New);
      this->CheckForSpecialEffects(static_cast<oCItem*>(cplayer->npc->GetLeftHand()), cplayer->npc);
    } else {
      if (cplayer->npc->GetLeftHand()->GetInstance() != static_cast<int>(state.left_hand_item_instance)) {
        oCItem* New = zfactory->CreateItem(static_cast<int>(state.left_hand_item_instance));
        oCItem* Old = static_cast<oCItem*>(cplayer->npc->GetLeftHand());
        cplayer->npc->SetLeftHand(New);
        Old->RemoveVobFromWorld();
        this->CheckForSpecialEffects(static_cast<oCItem*>(cplayer->npc->GetLeftHand()), cplayer->npc);
      }
    }
  }

  // Update right hand item
  if (state.right_hand_item_instance == 0) {
    if (cplayer->npc->GetRightHand()) {
      oCItem* rptr = static_cast<oCItem*>(cplayer->npc->GetRightHand());
      oCItem* ptr = static_cast<oCItem*>(cplayer->npc->GetLeftHand());
      cplayer->npc->DropAllInHand();
      if (ptr)
        cplayer->npc->SetLeftHand(ptr);
      this->PlayDrawSound(rptr, cplayer->npc, false);
      rptr->RemoveVobFromWorld();
    }
  } else if (state.right_hand_item_instance > 5892 && state.right_hand_item_instance < 7850) {
    if (!cplayer->npc->GetRightHand()) {
      oCItem* New = zfactory->CreateItem(static_cast<int>(state.right_hand_item_instance));
      cplayer->npc->SetRightHand(New);
      this->PlayDrawSound(static_cast<oCItem*>(cplayer->npc->GetRightHand()), cplayer->npc, true);
    } else {
      if (cplayer->npc->GetRightHand()->GetInstance() != static_cast<int>(state.right_hand_item_instance)) {
        oCItem* New = zfactory->CreateItem(static_cast<int>(state.right_hand_item_instance));
        oCItem* Old = static_cast<oCItem*>(cplayer->npc->GetRightHand());
        cplayer->npc->SetRightHand(New);
        Old->RemoveVobFromWorld();
        this->PlayDrawSound(static_cast<oCItem*>(cplayer->npc->GetRightHand()), cplayer->npc, true);
      }
    }
  }

  // Update equipped armor
  if (state.equipped_armor_instance == 0) {
    if (cplayer->npc->GetEquippedArmor()) {
      cplayer->npc->UnequipItem(cplayer->npc->GetEquippedArmor());
    }
  } else if (state.equipped_armor_instance > 5892 && state.equipped_armor_instance < 7850) {
    if (!cplayer->npc->GetEquippedArmor()) {
      if (!cplayer->npc->inventory2.IsIn(static_cast<int>(state.equipped_armor_instance), 1)) {
        oCItem* New = zfactory->CreateItem(static_cast<int>(state.equipped_armor_instance));
        cplayer->npc->inventory2.Insert(New);
        cplayer->npc->Equip(New);
      } else
        cplayer->npc->Equip(cplayer->npc->inventory2.IsIn(static_cast<int>(state.equipped_armor_instance), 1));
    } else {
      if (cplayer->npc->GetEquippedArmor()->GetInstance() != static_cast<int>(state.equipped_armor_instance)) {
        if (!cplayer->npc->inventory2.IsIn(static_cast<int>(state.equipped_armor_instance), 1)) {
          oCItem* New = zfactory->CreateItem(static_cast<int>(state.equipped_armor_instance));
          cplayer->npc->inventory2.Insert(New);
          cplayer->npc->Equip(New);
        } else
          cplayer->npc->Equip(cplayer->npc->inventory2.IsIn(static_cast<int>(state.equipped_armor_instance), 1));
      }
    }
  }

  // Update animation
  if (cplayer->npc && state.animation > 0 && state.animation < 1400) {
    zCModelAni* Ani = cplayer->npc->GetModel()->GetAniFromAniID(static_cast<int>(state.animation));
    if (Ani && !cplayer->npc->IsDead()) {
      if (!IsBadCodePtr((FARPROC)&Ani->GetAniName())) {
        if (!Ani->GetAniName().IsEmpty()) {
          if (Ani->GetAniName().Search(Door) == 2) {
            if (!cplayer->npc->GetInteractMob()) {
              oCMobLockable* ALocked = static_cast<oCMobLockable*>(cplayer->npc->FindMobInter(Door));
              if (ALocked) {
                ALocked->SetMobBodyState(cplayer->npc);
                ALocked->AI_UseMobToState(cplayer->npc, !ALocked->GetState());
              }
            } else {
              cplayer->npc->GetInteractMob()->SetMobBodyState(cplayer->npc);
              cplayer->npc->GetInteractMob()->AI_UseMobToState(cplayer->npc, !cplayer->npc->GetInteractMob()->GetState());
            }
          } else if (cplayer->npc->GetInteractMob()) {
            if (cplayer->npc->GetInteractMob()->GetState() == 0)
              cplayer->npc->GetInteractMob()->SendEndInteraction(cplayer->npc, 0, 1);
            else
              cplayer->npc->GetInteractMob()->SendEndInteraction(cplayer->npc, 1, 0);
          }
          if (!cplayer->npc->GetModel()->IsAnimationActive(Ani->GetAniName())) {
            cplayer->npc->GetModel()->StartAnimation(Ani->GetAniName());
            if (!memcmp("T_BOWRELOAD", Ani->GetAniName().ToChar(), 11)) {
              oCItem* Bullet = zfactory->CreateItem(zCParser::GetParser()->GetIndex(Arrows));
              zsound->PlaySound3D(BowSound, this->players[0]->npc, 2);
              cplayer->npc->SetRightHand(Bullet);
              oCItem* Arrowe = cplayer->npc->inventory2.IsIn(7083, 1);
              if (Arrowe)
                cplayer->npc->inventory2.Remove(7083, 1);
              cplayer->npc->DoShootArrow(1);
            }
            if (!memcmp("T_CBOWRELOAD", Ani->GetAniName().ToChar(), 12)) {
              oCItem* Bullet = zfactory->CreateItem(zCParser::GetParser()->GetIndex(Bolt));
              zsound->PlaySound3D(CrossbowSound, this->players[0]->npc, 2);
              cplayer->npc->SetLeftHand(Bullet);
              oCItem* Bolte = cplayer->npc->inventory2.IsIn(7084, 1);
              if (Bolte)
                cplayer->npc->inventory2.Remove(7084, 1);
              cplayer->npc->DoShootArrow(1);
            }
            if (!memcmp("T_LEVER_S0_2_S1", Ani->GetAniName().ToChar(), 15)) {
              oCMobInter* LeverSwitch = cplayer->npc->FindMobInter(Lever);
              if (LeverSwitch) {
                if (!LeverSwitch->triggerTarget.IsEmpty()) {
                  zCMover* Mover = static_cast<zCMover*>(ogame->GetGameWorld()->SearchVobByName(LeverSwitch->triggerTarget));
                  if (Mover)
                    Mover->TriggerMover(Mover);
                }
              }
            }
            if (!memcmp("T_TOUCHPLATE_S0_2_S1", Ani->GetAniName().ToChar(), 20) || !memcmp("T_TOUCHPLATE_S1_2_S0", Ani->GetAniName().ToChar(), 20)) {
              oCMobInter* LeverSwitch = cplayer->npc->FindMobInter(Touchplate);
              if (LeverSwitch) {
                if (!LeverSwitch->triggerTarget.IsEmpty()) {
                  zCMover* Mover = static_cast<zCMover*>(ogame->GetGameWorld()->SearchVobByName(LeverSwitch->triggerTarget));
                  if (Mover)
                    Mover->TriggerMover(Mover);
                }
              }
            }
            if (!memcmp("T_VWHEEL_S0_2_S1", Ani->GetAniName().ToChar(), 16)) {
              oCMobInter* LeverSwitch = cplayer->npc->FindMobInter(VWheel);
              if (LeverSwitch) {
                if (!LeverSwitch->triggerTarget.IsEmpty()) {
                  zCMover* Mover = static_cast<zCMover*>(ogame->GetGameWorld()->SearchVobByName(LeverSwitch->triggerTarget));
                  if (Mover)
                    Mover->TriggerMover(Mover);
                }
              }
            }
          }
        }
      }
    }
  }

  // Update health
  if (state.health_points > cplayer->npc->attribute[NPC_ATR_HITPOINTS]) {
    if (cplayer->npc->attribute[NPC_ATR_HITPOINTS] == 1) {
      cplayer->npc->RefreshNpc();
      cplayer->npc->attribute[NPC_ATR_HITPOINTS] = 1;
    }
  }
  if ((!cplayer->base_player().hp()) && (state.health_points == cplayer->npc->attribute[NPC_ATR_HITPOINTSMAX])) {
    cplayer->base_player().set_hp(state.health_points);
    auto pos = cplayer->npc->GetPositionWorld();
    cplayer->npc->ResetPos(pos);
  } else if ((cplayer->npc->attribute[NPC_ATR_HITPOINTS] > 0) && (state.health_points == 0)) {
    cplayer->base_player().set_hp(0);
  } else {
    if (cplayer->base_player().update_hp_packet_counter() >= 5) {
      cplayer->base_player().set_hp(state.health_points);
      cplayer->npc->attribute[NPC_ATR_HITPOINTS] = static_cast<int>(state.health_points);
      cplayer->base_player().set_update_hp_packet_counter(0);
    } else
      cplayer->base_player().set_update_hp_packet_counter(cplayer->base_player().update_hp_packet_counter() + 1);
  }

  // Update mana
  cplayer->npc->attribute[NPC_ATR_MANA] = static_cast<int>(state.mana_points);

  // Update active spell
  BYTE SpellNr = static_cast<BYTE>(state.active_spell_nr);
  if (SpellNr != cplayer->npc->GetActiveSpellNr() && SpellNr > 0 && SpellNr < 100) {
    for (int s = 0; s < cplayer->npc->GetSpellBook()->GetNoOfSpells(); s++) {
      cplayer->npc->Equip(cplayer->npc->GetSpellBook()->GetSpellItem(s));
    }
    oCItem* SpellItem = cplayer->npc->GetSpellItem((int)SpellNr);
    if (SpellItem) {
      cplayer->npc->Equip(SpellItem);
      cplayer->npc->GetSpellBook()->Open(0);
    }
  } else if (SpellNr == 0 && cplayer->npc->GetActiveSpellNr() > 0) {
    cplayer->npc->GetSpellBook()->Close(1);
  } else if (cplayer->npc->IsDead()) {
    cplayer->npc->GetSpellBook()->Close(1);
  }

  // Update weapon mode
  if ((BYTE)cplayer->npc->GetWeaponMode() != state.weapon_mode) {
    cplayer->npc->SetWeaponMode(state.weapon_mode);
  }

  // Update head direction
  switch ((Gothic2APlayer::HeadState)state.head_direction) {
    case Gothic2APlayer::HEAD_NONE:
      cplayer->npc->GetAnictrl()->SetLookAtTarget(0.5f, 0.5f);
      break;
    case Gothic2APlayer::HEAD_LEFT:
      cplayer->npc->GetAnictrl()->SetLookAtTarget(0.0f, 0.5f);
      break;
    case Gothic2APlayer::HEAD_RIGHT:
      cplayer->npc->GetAnictrl()->SetLookAtTarget(1.0f, 0.5f);
      break;
    case Gothic2APlayer::HEAD_UP:
      cplayer->npc->GetAnictrl()->SetLookAtTarget(0.5f, 0.0f);
      break;
    case Gothic2APlayer::HEAD_DOWN:
      cplayer->npc->GetAnictrl()->SetLookAtTarget(0.5f, 1.0f);
      break;
  }

  // Update ranged weapon
  if (state.ranged_weapon_instance == 0) {
    if (cplayer->npc->GetEquippedRangedWeapon()) {
      cplayer->npc->UnequipItem(cplayer->npc->GetEquippedRangedWeapon());
    }
  } else if (state.ranged_weapon_instance > 5892 && state.ranged_weapon_instance < 7850) {
    if (!cplayer->npc->GetEquippedRangedWeapon()) {
      if (!cplayer->npc->inventory2.IsIn(static_cast<int>(state.ranged_weapon_instance), 1)) {
        oCItem* New = zfactory->CreateItem(static_cast<int>(state.ranged_weapon_instance));
        cplayer->npc->inventory2.Insert(New);
        cplayer->npc->Equip(New);
      } else
        cplayer->npc->Equip(cplayer->npc->inventory2.IsIn(static_cast<int>(state.ranged_weapon_instance), 1));
    } else {
      if (cplayer->npc->GetEquippedRangedWeapon()->GetInstance() != static_cast<int>(state.ranged_weapon_instance)) {
        if (!cplayer->npc->inventory2.IsIn(static_cast<int>(state.ranged_weapon_instance), 1)) {
          oCItem* New = zfactory->CreateItem(static_cast<int>(state.ranged_weapon_instance));
          cplayer->npc->inventory2.Insert(New);
          cplayer->npc->Equip(New);
        } else
          cplayer->npc->Equip(cplayer->npc->inventory2.IsIn(static_cast<int>(state.ranged_weapon_instance), 1));
      }
    }
  }

  // Update melee weapon
  if (state.melee_weapon_instance == 0) {
    if (cplayer->npc->GetEquippedMeleeWeapon()) {
      cplayer->npc->UnequipItem(cplayer->npc->GetEquippedMeleeWeapon());
    }
  } else if (state.melee_weapon_instance > 5892 && state.melee_weapon_instance < 7850) {
    if (!cplayer->npc->GetEquippedMeleeWeapon()) {
      if (!cplayer->npc->inventory2.IsIn(static_cast<int>(state.melee_weapon_instance), 1)) {
        oCItem* New = zfactory->CreateItem(static_cast<int>(state.melee_weapon_instance));
        cplayer->npc->inventory2.Insert(New);
        cplayer->npc->Equip(New);
      } else
        cplayer->npc->Equip(cplayer->npc->inventory2.IsIn(static_cast<int>(state.melee_weapon_instance), 1));
    } else {
      if (cplayer->npc->GetEquippedMeleeWeapon()->GetInstance() != static_cast<int>(state.melee_weapon_instance)) {
        if (!cplayer->npc->inventory2.IsIn(static_cast<int>(state.melee_weapon_instance), 1)) {
          oCItem* New = zfactory->CreateItem(static_cast<int>(state.melee_weapon_instance));
          cplayer->npc->inventory2.Insert(New);
          cplayer->npc->Equip(New);
        } else
          cplayer->npc->Equip(cplayer->npc->inventory2.IsIn(static_cast<int>(state.melee_weapon_instance), 1));
      }
    }
  }
}

void NetGame::OnPlayerPositionUpdate(std::uint64_t player_id, float x, float y, float z) {
  Gothic2APlayer* cplayer = GetPlayerById(player_id);
  if (cplayer) {
    cplayer->npc->trafoObjToWorld.SetTranslation(zVEC3(x, y, z));
    cplayer->DisablePlayer();
  }
}

void NetGame::OnPlayerDied(std::uint64_t player_id) {
  Gothic2APlayer* cplayer = GetPlayerById(player_id);
  if (cplayer) {
    cplayer->base_player().set_hp(0);
    cplayer->base_player().set_update_hp_packet_counter(0);
    cplayer->SetHealth(0);
  }
}

void NetGame::OnPlayerRespawned(std::uint64_t player_id) {
  Gothic2APlayer* cplayer = GetPlayerById(player_id);
  if (cplayer) {
    cplayer->RespawnPlayer();
  }
}

void NetGame::OnItemDropped(std::uint64_t player_id, std::uint16_t item_instance, std::uint16_t amount) {
  Gothic2APlayer* cplayer = GetPlayerById(player_id);
  if (cplayer && item_instance > 5892 && item_instance < 7850) {
    oCWorld* world = ogame->GetGameWorld();
    oCItem* NpcDrop = zfactory->CreateItem(item_instance);
    NpcDrop->amount = amount;
    zVEC3 startPos = cplayer->npc->GetTrafoModelNodeToWorld("ZS_RIGHTHAND").GetTranslation();
    NpcDrop->trafoObjToWorld.SetTranslation(startPos);
    world->AddVob(NpcDrop);
    NpcDrop->SetSleeping(false);
    NpcDrop->SetStaticVob(false);
    NpcDrop->SetPhysicsEnabled(true);
  }
}

void NetGame::OnItemTaken(std::uint64_t player_id, std::uint16_t item_instance) {
  Gothic2APlayer* cplayer = GetPlayerById(player_id);
  if (cplayer && item_instance > 5892 && item_instance < 7850) {
    zCListSort<oCItem>* ItemList = ogame->GetGameWorld()->voblist_items;
    for (int i = 0; i < ItemList->GetNumInList(); i++) {
      ItemList = ItemList->GetNextInList();
      oCItem* ItemInList = ItemList->GetData();
      if (ItemInList->GetInstance() == item_instance) {
        if (cplayer->npc->GetDistanceToVob(*ItemInList) < 250.0f) {
          cplayer->npc->DoTakeVob(ItemInList);
          break;
        }
      }
    }
  }
}

void NetGame::OnSpellCast(std::uint64_t caster_id, std::uint16_t spell_id) {
  Gothic2APlayer* caster = GetPlayerById(caster_id);
  if (caster && spell_id >= 0 && spell_id < 100) {
    oCSpell* Spell = new oCSpell();
    Spell->InitValues(spell_id);
    Spell->Setup(caster->GetNpc(), 0, 0);
    this->RunSpellLogic(spell_id, caster->GetNpc(), 0);
    this->RunSpellScript(Spell->GetSpellInstanceName(spell_id).ToChar(), caster->GetNpc());
    Spell->Cast();
  }
}

void NetGame::OnSpellCastOnTarget(std::uint64_t caster_id, std::uint64_t target_id, std::uint16_t spell_id) {
  Gothic2APlayer* caster = GetPlayerById(caster_id);
  Gothic2APlayer* target = GetPlayerById(target_id);

  if (caster && target && spell_id >= 0 && spell_id < 100) {
    oCSpell* Spell = new oCSpell;
    Spell->InitValues(spell_id);
    Spell->Setup(caster->GetNpc(), target->GetNpc(), 0);
    this->RunSpellLogic(spell_id, caster->GetNpc(), target->GetNpc());
    this->RunSpellScript(Spell->GetSpellInstanceName(spell_id).ToChar(), caster->GetNpc());
    Spell->Cast();
  }
}

void NetGame::OnPlayerMessage(std::optional<std::uint64_t> sender_id, std::uint8_t r, std::uint8_t g, std::uint8_t b,
                              const std::string& message) {
  zCOLOR color(r, g, b, 255);

  if (sender_id) {
    Gothic2APlayer* sender = GetPlayerById(*sender_id);
    if (sender) {
      SPDLOG_INFO("Message from player: {} ({}): {}", sender->npc->GetName().ToChar(), sender->GetName(), message);
      CChat::GetInstance()->WriteMessage(NORMAL, false, color, "%s", message.c_str());
    }
  } else {
    CChat::GetInstance()->WriteMessage(NORMAL, false, color, "%s", message.c_str());
  }

  EventManager::Instance().TriggerEvent(gmp::client::kEventOnPlayerMessageName,
                                        gmp::client::OnPlayerMessageEvent{sender_id, r, g, b, message});
}

void NetGame::OnWhisperReceived(std::uint64_t sender_id, const std::string& sender_name, const std::string& message) {
  Gothic2APlayer* sender = GetPlayerById(sender_id);
  if (sender) {
    CChat::GetInstance()->WriteMessage(WHISPER, true, zCOLOR(0, 255, 255, 255), "%s -> %s", sender->npc->GetName().ToChar(), message.c_str());
  }
}

void NetGame::OnRconResponse(const std::string& response, bool is_admin) {
  if (is_admin) {
    this->IsAdminOrModerator = true;
  }
  CChat::GetInstance()->WriteMessage(ADMIN, false, RED, "%s", response.c_str());
}

void NetGame::OnDiscordActivityUpdate(const std::string& state, const std::string& details, const std::string& large_image_key,
                                      const std::string& large_image_text, const std::string& small_image_key, const std::string& small_image_text) {
  SPDLOG_DEBUG("Discord activity update: {} - {}", state, details);
  DiscordRichPresence::Instance().UpdateActivity(state, details, 0, 0, large_image_key, large_image_text, small_image_key, small_image_text);
}

void NetGame::OnPacket(const gmp::client::Packet& packet) {
  EventManager::Instance().TriggerEvent(gmp::client::kEventOnPacketName, gmp::client::OnPacketEvent{packet});
}