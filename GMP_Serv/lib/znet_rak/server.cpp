
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

#include "server.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <string_view>

constexpr std::string_view kServerPassword = "YOUR_PASS";

namespace Net {

namespace {
::PacketPriority ToRakNetPacketPriority(Net::PacketPriority packetPriority) {
  switch (packetPriority) {
    case IMMEDIATE_PRIORITY:
      return ::IMMEDIATE_PRIORITY;
    case HIGH_PRIORITY:
      return ::HIGH_PRIORITY;
    case MEDIUM_PRIORITY:
      return ::MEDIUM_PRIORITY;
    case LOW_PRIORITY:
      return ::LOW_PRIORITY;
  }
  return ::MEDIUM_PRIORITY;
}
::PacketReliability ToRakNetPacketReliability(Net::PacketReliability packetPriority) {
  switch (packetPriority) {
    case RELIABLE:
      return ::RELIABLE;
    case RELIABLE_ORDERED:
      return ::RELIABLE_ORDERED;
    case RELIABLE_SEQUENCED:
      return ::RELIABLE_SEQUENCED;
    case UNRELIABLE:
      return ::UNRELIABLE;
    case UNRELIABLE_SEQUENCED:
      return ::UNRELIABLE_SEQUENCED;
  }
  return ::RELIABLE;
}
}  // namespace

RakNetServer::~RakNetServer() {
  if (peer_ != nullptr) {
    peer_->Shutdown(500);
    RakNet::RakPeerInterface::DestroyInstance(peer_);
  }
}

bool RakNetServer::Start(std::uint32_t port, std::uint32_t slots) {
  if (peer_ != nullptr) {
    return false;
  }
  peer_ = RakNet::RakPeerInterface::GetInstance();
  peer_->SetIncomingPassword(kServerPassword.data(), kServerPassword.size());
  peer_->SetTimeoutTime(1000, RakNet::UNASSIGNED_SYSTEM_ADDRESS);
  peer_->SetMaximumIncomingConnections(slots);

  RakNet::SocketDescriptor socketDescriptor{static_cast<unsigned short>(port), nullptr};
  return peer_->Startup(slots, &socketDescriptor, 1) == RakNet::RAKNET_STARTED;
}

void RakNetServer::Pulse() {
  for (RakNet::Packet* packet = peer_->Receive(); packet; peer_->DeallocatePacket(packet), packet = peer_->Receive()) {
    std::for_each(packetHandlers_.begin(), packetHandlers_.end(), [packet](auto& handler) {
      handler->HandlePacket(PlayerId{packet->guid.g}, packet->data, packet->length);
    });
  }
}

bool RakNetServer::Send(unsigned char* data, std::uint32_t size, PacketPriority packetPriority,
                        PacketReliability packetReliability, std::uint32_t channel, PlayerId id) {
  peer_->Send(reinterpret_cast<const char*>(data), size, ToRakNetPacketPriority(packetPriority),
              ToRakNetPacketReliability(packetReliability), 0, RakNet::RakNetGUID(id.guid), false);
  return true;
}
bool RakNetServer::Send(const char* data, std::uint32_t size, PacketPriority packetPriority,
                        PacketReliability packetReliability, std::uint32_t channel, PlayerId id) {
  peer_->Send(reinterpret_cast<const char*>(data), size, ToRakNetPacketPriority(packetPriority),
              ToRakNetPacketReliability(packetReliability), 0, RakNet::RakNetGUID(id.guid), false);
  return true;
}

void RakNetServer::AddPacketHandler(PacketHandler& packetHandler) {
  packetHandlers_.insert(&packetHandler);
}

void RakNetServer::RemovePacketHandler(PacketHandler& packetHandler) {
  packetHandlers_.erase(&packetHandler);
}

void RakNetServer::AddToBanList(const char* IP, std::uint32_t milliseconds) {
  peer_->AddToBanList(IP, milliseconds);
}

void RakNetServer::RemoveFromBanList(const char* IP) {
  peer_->RemoveFromBanList(IP);
}

bool RakNetServer::IsBanned(const char* IP) {
  return peer_->IsBanned(IP);
}

const char* RakNetServer::GetPlayerIp(PlayerId id) {
  auto address = peer_->GetSystemAddressFromGuid(RakNet::RakNetGUID(id.guid));
  // This is safe because RakNet::SystemAddress::ToString() returns a pointer to a static buffer
  return address.ToString(false);
}

void RakNetServer::AddToBanList(PlayerId id, std::uint32_t milliseconds) {
  auto address = peer_->GetSystemAddressFromGuid(RakNet::RakNetGUID(id.guid));
  if (address != RakNet::UNASSIGNED_SYSTEM_ADDRESS) {
    peer_->AddToBanList(address.ToString(false), milliseconds);
  } else {
    SPDLOG_ERROR("AddToBanList: unrecognized player id {}", id.guid);
  }
}

}  // namespace Net

Net::NetServer* CreateNetServer() {
  return new Net::RakNetServer;
}

void DestroyNetServer(Net::NetServer* net_server) {
  delete net_server;
  net_server = nullptr;
}