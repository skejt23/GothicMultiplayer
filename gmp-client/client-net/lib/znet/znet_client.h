
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
#include <string>

#include "net_enums.h"

namespace Net {

class NetClient {
public:
  class PacketHandler {
  public:
    virtual ~PacketHandler() = default;
    virtual bool HandlePacket(unsigned char* data, std::uint32_t size) = 0;
  };

  virtual ~NetClient() = default;

  // Needs to be called periodically in order to retrieve packets.
  virtual void Pulse() = 0;

  virtual bool Connect(const char* address, std::uint32_t port) = 0;
  virtual void Disconnect() = 0;
  virtual bool IsConnected() const = 0;

  virtual bool SendPacket(unsigned char* data, std::uint32_t size, PacketReliability packetReliability, PacketPriority packetPriority) = 0;

  virtual void AddPacketHandler(PacketHandler& packetHandler) = 0;
  virtual void RemovePacketHandler(PacketHandler& packetHandler) = 0;

  virtual std::uint32_t GetPing() const = 0;
};

}  // namespace Net