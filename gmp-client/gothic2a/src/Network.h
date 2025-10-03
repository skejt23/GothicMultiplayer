
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

#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>

#include "net_enums.h"
#include "znet_client.h"

class GameClient;

struct Packet {
  unsigned char* data = nullptr;
  std::uint32_t length = 0;
};

class Network : public Net::NetClient::PacketHandler {
public:
  bool connection_lost_ = false;
  std::uint8_t error;

  Network(GameClient*);
  ~Network() override;

  bool Connect(std::string hostAddress, int hostPort);
  void Disconnect();
  bool IsConnected() const;
  int GetPing();

  void Send(const char* data, int size, Net::PacketPriority priority, Net::PacketReliability reliability) {
    Send(reinterpret_cast<unsigned char*>(const_cast<char*>(data)), size, priority, reliability);
  }
  void Send(unsigned char*, int, Net::PacketPriority, Net::PacketReliability);
  void Receive();

  inline uint64_t GetMyId() {
    return playerID;
  }
  void UpdateMyId(uint64_t);
  std::string GetServerIp() const;
  std::uint32_t GetServerPort() const;

  static void LoadNetworkLibrary();

private:
  void AddPacketHandlers();
  bool HandlePacket(unsigned char* data, std::uint32_t size) override;

  GameClient* client_;
  uint64_t playerID;
  std::map<int, std::function<void(GameClient*, Packet packet)> > packetHandlers;
  std::string serverIp_;
  std::uint32_t serverPort_{0};
};
