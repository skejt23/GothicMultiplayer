
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
#include <wincrypt.h>

#include <array>
#include <ctime>
#include <filesystem>
#include <glm/glm.hpp>
#include <list>
#include <sstream>
#include <string>
#include <vector>

#include "CChat.h"
#include "CIngame.h"
#include "CLocalPlayer.h"
#include "HTTPDownloader.h"
#include "Interface.h"
#include "ZenGin/zGothicAPI.h"
#include "config.h"
#include "net_enums.h"
#include "packets.h"
#include "patch.h"
#include "player_name_utils.hpp"

const char *LANG_DIR = ".\\Multiplayer\\Localization\\";

float fWRatio, fHRatio;
extern CIngame *global_ingame;
extern CLocalPlayer *LocalPlayer;

using namespace Net;

template <typename TContainer = std::vector<std::uint8_t>, typename Packet>
void SerializeAndSend(Network &network, const Packet &packet, Net::PacketPriority priority, Net::PacketReliability reliable) {
  TContainer buffer;
  auto written_size = bitsery::quickSerialization<bitsery::OutputBufferAdapter<TContainer>>(buffer, packet);
  network.Send(buffer.data(), written_size, priority, reliable);
}

bool NetGame::Connect(std::string_view full_address) {
  // Extract port number from IP address if present
  std::string host(full_address);
  int port = 0xDEAD;
  size_t pos = host.find_last_of(':');
  if (pos != std::string::npos) {
    std::string portStr = host.substr(pos + 1);
    std::istringstream iss(portStr);
    iss >> port;
    host.erase(pos);
  }
  if (network.Connect(host, port)) {
    return true;
  }
  return false;
}

string NetGame::GetServerAddresForHTTPDownloader() {
  auto address = network.GetServerIp() + ":" + std::to_string(network.GetServerPort() + 1);
  return address;
}

void NetGame::DownloadWBFile() {
  auto content = HTTPDownloader::GetWBFile(GetServerAddresForHTTPDownloader());
  static const std::filesystem::path path = ".\\Multiplayer\\Data\\";

  auto serverWbFile = path / (network.GetServerIp() + "_" + std::to_string(network.GetServerPort()));

  if (content == "EMPTY") {
    std::filesystem::remove(serverWbFile);
    return;
  }

  std::ofstream wbFile(serverWbFile, std::ios::binary);
  if (wbFile) {
    wbFile.write(content.data(), content.length());
  }
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
    network.Receive();
  }
}

bool NetGame::IsConnected() {
  return network.IsConnected();
}

void NetGame::JoinGame() {
  if (IsReadyToJoin) {
    HooksManager::GetInstance()->AddHook(HT_RENDER, (DWORD)InterfaceLoop, false);

    auto sanitized_name = SanitizePlayerName(Config::Instance().Nickname.ToChar());
    player->name[0] = sanitized_name.c_str();

    JoinGamePacket packet;
    packet.packet_type = PT_JOIN_GAME;

    auto pos = player->GetPositionWorld();
    packet.position.x = pos[VX];
    packet.position.y = pos[VY];
    packet.position.z = pos[VZ];

    auto right_vector = player->trafoObjToWorld.GetRightVector();
    packet.normal.x = right_vector[VX];
    packet.normal.y = right_vector[VY];
    packet.normal.z = right_vector[VZ];

    oCItem *itPtr = static_cast<oCItem *>(player->GetLeftHand());
    if (itPtr) {
      packet.left_hand_item_instance = itPtr->GetInstance();
    }
    itPtr = static_cast<oCItem *>(player->GetRightHand());
    if (itPtr) {
      packet.right_hand_item_instance = itPtr->GetInstance();
    }
    itPtr = static_cast<oCItem *>(player->GetEquippedArmor());
    if (itPtr) {
      packet.equipped_armor_instance = itPtr->GetInstance();
    }
    packet.animation = 265;  // TODO: get current animation (or remove it?)
    packet.head_model = Config::Instance().headmodel;
    packet.skin_texture = Config::Instance().skintexture;
    packet.face_texture = Config::Instance().facetexture;
    packet.walk_style = Config::Instance().walkstyle;
    packet.player_name = player->GetName().ToChar();

    SerializeAndSend(network, packet, IMMEDIATE_PRIORITY, RELIABLE_ORDERED);

    CIngame *g = new CIngame();
    if (!LocalPlayer)
      new CLocalPlayer();
    LocalPlayer->id = network.GetMyId();
    LocalPlayer->enable = TRUE;
    LocalPlayer->SetNpc(player);
    LocalPlayer->hp = static_cast<short>(LocalPlayer->GetHealth());
    LocalPlayer->update_hp_packet = 0;
    LocalPlayer->npc->SetMovLock(0);
    this->players.push_back(LocalPlayer);
    this->HeroLastHp = player->attribute[NPC_ATR_HITPOINTS];
  }
}

void NetGame::SendMessage(const char *msg) {
  MessagePacket packet;
  packet.packet_type = PT_MSG;
  packet.message = msg;
  SerializeAndSend(network, packet, MEDIUM_PRIORITY, RELIABLE);
}

void NetGame::SendWhisper(const char *player_name, const char *msg) {
  bool found = false;
  size_t i;
  size_t length = strlen(player_name);
  for (i = 0; i < this->players.size(); i++) {
    if (this->players[i]->GetNameLength() == length) {
      if (!memcmp(this->players[i]->GetName(), player_name, length)) {
        found = true;
        break;
      }
    }
  }

  if (found) {
    MessagePacket packet;
    packet.packet_type = PT_WHISPER;
    packet.message = msg;
    packet.recipient = players[i]->id;

    SerializeAndSend(network, packet, HIGH_PRIORITY, RELIABLE_ORDERED);
  }
}

void NetGame::SendCommand(const char *msg) {
  MessagePacket packet;
  packet.packet_type = PT_COMMAND;
  packet.message = msg;
  SerializeAndSend(network, packet, HIGH_PRIORITY, RELIABLE_ORDERED);
}

void NetGame::SendCastSpell(oCNpc *Target, short SpellId) {
  CastSpellPacket packet;
  packet.spell_id = static_cast<uint16_t>(SpellId);
  packet.packet_type = PT_CASTSPELL;

  if (Target) {
    for (int i = 0; i < (int)players.size(); i++) {
      if (players[i]->npc == Target) {
        packet.target_id = players[i]->id;
        packet.packet_type = PT_CASTSPELLONTARGET;
        break;
      }
    }
  }

  SerializeAndSend(network, packet, HIGH_PRIORITY, RELIABLE);
}

void NetGame::SendDropItem(short instance, short amount) {
  DropItemPacket packet;
  packet.packet_type = PT_DROPITEM;
  packet.item_instance = instance;
  packet.item_amount = amount;
  SerializeAndSend(network, packet, HIGH_PRIORITY, RELIABLE);
}

void NetGame::SendTakeItem(short instance) {
  TakeItemPacket packet;
  packet.packet_type = PT_TAKEITEM;
  packet.item_instance = instance;
  SerializeAndSend(network, packet, HIGH_PRIORITY, RELIABLE);
}

glm::vec3 Vec3ToGlmVec3(const zVEC3 &vec) {
  return glm::vec3(vec[VX], vec[VY], vec[VZ]);
}

std::uint8_t GetHeadDirectionByte(oCNpc *Hero) {
  zVEC2 HeadVar = zVEC2(Hero->GetAnictrl()->lookTargetx, Hero->GetAnictrl()->lookTargety);
  if (HeadVar[VX] == 0)
    return static_cast<uint8_t>(CPlayer::HEAD_LEFT);
  else if (HeadVar[VX] == 1)
    return static_cast<uint8_t>(CPlayer::HEAD_RIGHT);
  else if (HeadVar[VY] == 0)
    return static_cast<uint8_t>(CPlayer::HEAD_UP);
  else if (HeadVar[VY] == 1)
    return static_cast<uint8_t>(CPlayer::HEAD_DOWN);

  return 0;
}

void NetGame::UpdatePlayerStats(short anim) {
  PlayerStateUpdatePacket packet;
  packet.packet_type = PT_ACTUAL_STATISTICS;
  packet.state.position = Vec3ToGlmVec3(player->GetPositionWorld());
  packet.state.nrot = Vec3ToGlmVec3(player->trafoObjToWorld.GetRightVector());
  packet.state.left_hand_item_instance = player->GetLeftHand() ? static_cast<short>(player->GetLeftHand()->GetInstance()) : 0;
  packet.state.right_hand_item_instance = player->GetRightHand() ? static_cast<short>(player->GetRightHand()->GetInstance()) : 0;
  packet.state.equipped_armor_instance = player->GetEquippedArmor() ? static_cast<short>(player->GetEquippedArmor()->GetInstance()) : 0;
  packet.state.animation = anim;
  packet.state.health_points = static_cast<std::int16_t>(player->attribute[NPC_ATR_HITPOINTS]);
  packet.state.mana_points = static_cast<std::int16_t>(player->attribute[NPC_ATR_MANA]);
  packet.state.weapon_mode = static_cast<uint8_t>(player->GetWeaponMode());
  packet.state.active_spell_nr = player->GetActiveSpellNr() > 0 ? static_cast<uint8_t>(player->GetActiveSpellNr()) : 0;
  packet.state.head_direction = GetHeadDirectionByte(player);
  packet.state.melee_weapon_instance = player->GetEquippedMeleeWeapon() ? static_cast<short>(player->GetEquippedMeleeWeapon()->GetInstance()) : 0;
  packet.state.ranged_weapon_instance = player->GetEquippedRangedWeapon() ? static_cast<short>(player->GetEquippedRangedWeapon()->GetInstance()) : 0;

  SerializeAndSend(network, packet, IMMEDIATE_PRIORITY, RELIABLE_ORDERED);
}

void NetGame::SendHPDiff(size_t who, short diff) {
  if (who < this->players.size()) {
    HPDiffPacket packet;
    packet.packet_type = PT_HP_DIFF;
    packet.player_id = this->players[who]->id;
    packet.hp_difference = diff;

    SerializeAndSend(network, packet, IMMEDIATE_PRIORITY, RELIABLE);
  }
}

void NetGame::SyncGameTime() {
  BYTE data[2] = {PT_GAME_INFO, 0};
  network.Send((char *)data, 1, IMMEDIATE_PRIORITY, RELIABLE);
}

void NetGame::Disconnect() {
  if (network.IsConnected()) {
    IsInGame = false;
    global_ingame->IgnoreFirstSync = true;
    LocalPlayer->SetNpcType(CPlayer::NPC_HUMAN);
    network.Disconnect();
    delete LocalPlayer;
    CPlayer::DeleteAllPlayers();
    if (VobsWorldBuilderMap.size() > 0) {
      for (int i = 0; i < (int)VobsWorldBuilderMap.size(); i++) {
        VobsWorldBuilderMap[i].Vob->RemoveVobFromWorld();
      }
      VobsWorldBuilderMap.clear();
    }
    CChat::GetInstance()->ClearChat();
    global_ingame->WhisperingTo.clear();
    player->SetWeaponMode(NPC_WEAPON_NONE);
  }
}
