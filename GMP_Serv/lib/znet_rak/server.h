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

#include <RakPeerInterface.h>

#include <cstdint>
#include <unordered_set>

#include "znet_server.h"

namespace Net {

class RakNetServer : public NetServer {
public:
  ~RakNetServer() override;

  bool Start(std::uint32_t port, std::uint32_t slots) override;

  void Pulse() override;

  void AddPacketHandler(PacketHandler& packetHandler) override;
  void RemovePacketHandler(PacketHandler& packetHandler) override;

  bool Send(unsigned char* data, std::uint32_t size, PacketPriority packetPriority, PacketReliability packetReliability,
            std::uint32_t channel, PlayerId id) override;

  bool Send(const char* data, std::uint32_t size, PacketPriority packetPriority, PacketReliability packetReliability,
            std::uint32_t channel, PlayerId id) override;

  void AddToBanList(const char* IP, std::uint32_t milliseconds) override;
  void AddToBanList(PlayerId id, std::uint32_t milliseconds) override;
  void RemoveFromBanList(const char* IP) override;
  bool IsBanned(const char* IP) override;

  const char* GetPlayerIp(PlayerId id) override;

private:
  RakNet::RakPeerInterface* peer_{nullptr};
  std::unordered_set<PacketHandler*> packetHandlers_;
};

}  // namespace Net

extern "C" {
#ifdef _MSC_VER
__declspec(dllexport) Net::NetServer* CreateNetServer();
__declspec(dllexport) void DestroyNetServer(Net::NetServer* net_server);
#else
[[gnu::visibility("default")]] Net::NetServer* CreateNetServer();
[[gnu::visibility("default")]] void DestroyNetServer(Net::NetServer* net_server);
#endif
}