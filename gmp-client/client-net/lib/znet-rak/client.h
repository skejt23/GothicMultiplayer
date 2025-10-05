
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

#include <RakNetTypes.h>
#include <RakPeerInterface.h>

#include <functional>
#include <unordered_set>

#include "znet_client.h"

namespace Net {

class RakNetClient : public NetClient {
public:
  ~RakNetClient() override = default;

  void Pulse() override;
  bool Connect(const char* address, std::uint32_t port) override;
  void Disconnect() override;
  bool IsConnected() const override;
  bool SendPacket(unsigned char* data, std::uint32_t size, PacketReliability packetReliability,
                  PacketPriority packetPriority) override;

  void AddPacketHandler(PacketHandler& packetHandler) override;
  void RemovePacketHandler(PacketHandler& packetHandler) override;
  std::uint32_t GetPing() const override;

private:
  RakNet::RakPeerInterface* peer_;
  RakNet::SystemAddress serverAddress_;
  std::unordered_set<PacketHandler*> packetHandlers_;
  bool isConnected_{false};
};

}  // namespace Net

extern "C" {
__declspec(dllexport) Net::NetClient* CreateNetClient();
}