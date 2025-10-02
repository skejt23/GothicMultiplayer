
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

#include "Network.h"

#include <spdlog/spdlog.h>

#include <cassert>
#include <dylib.hpp>
#include <exception>

#include "packet-handlers/Connection.hpp"
#include "packet-handlers/Game.hpp"
#include "znet_client.h"

static Net::NetClient* g_netclient = nullptr;

Network::Network(GameClient* client) {
  assert(g_netclient != nullptr);
  client_ = client;
  playerID = -1;
  error = 0;

  AddPacketHandlers();
  g_netclient->AddPacketHandler(*this);
}

Network::~Network()
{
  g_netclient->RemovePacketHandler(*this); 
}

void Network::AddPacketHandlers() {
  using namespace Net;
  packetHandlers[PT_INITIAL_INFO] = Game::OnInitialInfo;
  packetHandlers[WL_INGAME] = Game::OnInGame;
  packetHandlers[PT_ACTUAL_STATISTICS] = Game::OnActualStatistics;
  packetHandlers[PT_MAP_ONLY] = Game::OnMapOnly;
  packetHandlers[PT_DODIE] = Game::OnDoDie;
  packetHandlers[PT_RESPAWN] = Game::OnRespawn;
  packetHandlers[PT_CASTSPELL] = Game::OnCastSpell;
  packetHandlers[PT_CASTSPELLONTARGET] = Game::OnCastSpellOnTarget;
  packetHandlers[PT_DROPITEM] = Game::OnDropItem;
  packetHandlers[PT_TAKEITEM] = Game::OnTakeItem;
  packetHandlers[PT_WHISPER] = Game::OnWhisper;
  packetHandlers[PT_MSG] = Game::OnMessage;
  packetHandlers[PT_SRVMSG] = Game::OnServerMessage;
  packetHandlers[PT_COMMAND] = Game::OnRcon;
  packetHandlers[PT_EXISTING_PLAYERS] = Game::OnAllOthers;
  packetHandlers[PT_JOIN_GAME] = Game::OnJoinGame;
  packetHandlers[PT_GAME_INFO] = Game::OnGameInfo;
  packetHandlers[PT_LEFT_GAME] = Game::OnLeftGame;
  packetHandlers[PT_VOICE] = Game::OnVoice;
  packetHandlers[PT_DISCORD_ACTIVITY] = Game::OnDiscordActivity;
  packetHandlers[Net::ID_DISCONNECTION_NOTIFICATION] = Connection::OnDisconnectOrLostConnection;
  packetHandlers[Net::ID_CONNECTION_LOST] = Connection::OnDisconnectOrLostConnection;
}

bool Network::Connect(std::string hostAddress, int hostPort) {
  if (!g_netclient->Connect(hostAddress.c_str(), hostPort)) {
    return false;
  }

  serverIp_ = hostAddress;
  serverPort_ = hostPort;
  client_->DownloadWBFile();
  client_->IsReadyToJoin = true;
  return true;
}

void Network::Disconnect() {
  g_netclient->Disconnect();
}

bool Network::IsConnected() const {
  return !connection_lost_ && g_netclient->IsConnected();
}

void Network::Receive() {
  g_netclient->Pulse();
}

bool Network::HandlePacket(unsigned char* data, std::uint32_t size) {
  try {
    SPDLOG_TRACE("Received packet: {}", (int)data[0]);
    packetHandlers[(int)data[0]](client_, Packet{data, size});
  } catch (std::exception&) {
    error = data[0];
  }
  return true;
}

void Network::Send(unsigned char* packet, int length, Net::PacketPriority priority, Net::PacketReliability reliable) {
  g_netclient->SendPacket(packet, length, reliable, priority);
}

int Network::GetPing() {
  return g_netclient->GetPing();
}

void Network::UpdateMyId(uint64_t id) {
  playerID = id;
  if (LocalPlayer) {
    LocalPlayer->id = id;
  }
}

std::string Network::GetServerIp() const {
  return serverIp_;
}

std::uint32_t Network::GetServerPort() const {
  return serverPort_;
}

void Network::LoadNetworkLibrary() {
  try {
    static dylib lib("znet");
    auto create_net_client_func = lib.get_function<Net::NetClient*()>("CreateNetClient");
    g_netclient = create_net_client_func();
  } catch (std::exception& ex) {
    SPDLOG_ERROR("LoadNetworkLibrary error: {}", ex.what());
    // If loading the network library fails, then GMP will not work.
    std::abort();
  }
  SPDLOG_DEBUG("znet dynamic library loaded: {}", (void*)g_netclient);
}
