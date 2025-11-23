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
#include <stdexcept>
#include <string>
#include <vector>

#include "net_enums.h"
#include "znet_server.h"

class Packet {
public:
  Packet();
  Packet(const unsigned char* data, std::uint32_t size, Net::ConnectionHandle connection = 0);
  Packet(const Packet& other);
  Packet& operator=(const Packet& other);
  Packet(Packet&& other) noexcept;
  Packet& operator=(Packet&& other) noexcept;

  void reset();

  void send(int playerid, int Reliability);
  void sendToAll(int Reliability);

  void writeBool(bool value);
  void writeInt8(int value);
  void writeUInt8(int value);
  void writeInt16(int value);
  void writeUInt16(int value);
  void writeInt32(int value);
  void writeUInt32(int value);
  void writeFloat(float value);
  void writeString(const std::string& value);
  void writeBlob(const std::vector<std::uint8_t>& value);

  bool readBool();
  std::int8_t readInt8();
  std::uint8_t readUInt8();
  std::int16_t readInt16();
  std::uint16_t readUInt16();
  std::int32_t readInt32();
  std::uint32_t readUInt32();
  float readFloat();
  std::string readString();
  std::vector<std::uint8_t> readBlob();

  unsigned char* data_ptr() { return data; }
  const unsigned char* data_ptr() const { return data; }
  std::uint32_t size() const { return length; }
  Net::ConnectionHandle sender() const { return id; }

  unsigned char* data = nullptr;
  std::uint32_t length = 0;
  Net::ConnectionHandle id{0};

private:
  template <typename TInt>
  void WriteIntegral(TInt value);

  template <typename TInt>
  TInt ReadIntegral();

  void WriteBytes(const unsigned char* bytes, std::size_t count);
  void ReadBytes(unsigned char* out, std::size_t count);
  void AlignWriteToByte();
  void AlignReadToByte();
  void SyncDataPointer();

  std::vector<std::uint8_t> buffer_;
  std::size_t write_bit_pos_{0};
  std::size_t read_bit_pos_{0};
};
