
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

#include "client.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <string>

#include "MessageIdentifiers.h"
#include "RakNetTypes.h"

using namespace RakNet;

#define RAKNET_PASSWORD "YOUR_PASS"

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

bool RakNetClient::Connect(const char* address, std::uint32_t port) {
  peer_ = RakPeerInterface::GetInstance();
  peer_->SetTimeoutTime(1500, UNASSIGNED_SYSTEM_ADDRESS);
  SocketDescriptor socketDescriptor(0, 0);
  socketDescriptor.socketFamily = AF_INET;
  peer_->Startup(1, &socketDescriptor, 1);
  peer_->SetOccasionalPing(true);
  std::string hostPassword = RAKNET_PASSWORD;
  ConnectionAttemptResult connectionAttemptResult =
      peer_->Connect(address, port, hostPassword.c_str(), hostPassword.size());
  if (connectionAttemptResult != CONNECTION_ATTEMPT_STARTED) {
    return false;
  }
  RakNet::Packet* packet = nullptr;
  do {
    packet = peer_->Receive();
  } while (!packet);

  MessageID message = packet->data[0];
  serverAddress_ = packet->systemAddress;
  peer_->DeallocatePacket(packet);
  if (message != ID_CONNECTION_REQUEST_ACCEPTED) {
    SPDLOG_ERROR("Connection not accepted ({}).", message);
    return false;
  }
  isConnected_ = true;
  return true;
}

void RakNetClient::Disconnect() {
  if (!isConnected_) {
    return;
  }

  isConnected_ = false;
  peer_->CloseConnection(serverAddress_, true, 11, ::IMMEDIATE_PRIORITY);
  RakNet::RakPeerInterface::DestroyInstance(peer_);
}

bool RakNetClient::IsConnected() const {
  return isConnected_;
}

bool RakNetClient::SendPacket(unsigned char* data, std::uint32_t size, PacketReliability packetReliability,
                              PacketPriority packetPriority) {
  // TODO: VALIDATION AND ENCRYPTION.
  peer_->Send(reinterpret_cast<const char*>(data), size, ToRakNetPacketPriority(packetPriority),
              ToRakNetPacketReliability(packetReliability), 0, serverAddress_, false);
  return true;
}

void RakNetClient::Pulse() {
  for (RakNet::Packet* packet = peer_->Receive(); packet; peer_->DeallocatePacket(packet), packet = peer_->Receive()) {
    std::for_each(packetHandlers_.begin(), packetHandlers_.end(),
                  [packet](auto& handler) { handler->HandlePacket(packet->data, packet->length); });
  }
}

void RakNetClient::AddPacketHandler(PacketHandler& packetHandler) {
  packetHandlers_.insert(&packetHandler);
}

void RakNetClient::RemovePacketHandler(PacketHandler& packetHandler) {
  packetHandlers_.erase(&packetHandler);
}

std::uint32_t RakNetClient::GetPing() const {
  return peer_->GetAveragePing(serverAddress_);
}

}  // namespace Net

Net::NetClient* CreateNetClient() {
  return new Net::RakNetClient;
}