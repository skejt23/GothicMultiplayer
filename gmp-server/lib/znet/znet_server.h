
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

using ConnectionHandle = std::uint64_t;

class PacketHandler {
public:
  virtual ~PacketHandler() = default;
  virtual bool HandlePacket(ConnectionHandle connectionHandle, unsigned char* data, std::uint32_t size) = 0;
};

class NetServer {
public:
  virtual ~NetServer() = default;

  // Needs to be called periodically in order to retrieve packets.
  virtual void Pulse() = 0;

  virtual bool Start(std::uint32_t port, std::uint32_t slots) = 0;

  virtual bool Send(unsigned char* data, std::uint32_t size, PacketPriority packetPriority,
                    PacketReliability packetReliability, std::uint32_t channel, ConnectionHandle id) = 0;

  virtual bool Send(const char* data, std::uint32_t size, PacketPriority packetPriority,
                    PacketReliability packetReliability, std::uint32_t channel, ConnectionHandle id) = 0;

  virtual void AddToBanList(const char* IP, std::uint32_t milliseconds) = 0;
  virtual void AddToBanList(ConnectionHandle id, std::uint32_t milliseconds) = 0;
  virtual void RemoveFromBanList(const char* IP) = 0;
  virtual bool IsBanned(const char* IP) = 0;

  virtual const char* GetPlayerIp(ConnectionHandle id) = 0;

  virtual void AddPacketHandler(PacketHandler& packetHandler) = 0;
  virtual void RemovePacketHandler(PacketHandler& packetHandler) = 0;
  virtual std::uint32_t GetPort() const = 0;
  virtual std::string GetAddress() const = 0;
};

}  // namespace Net