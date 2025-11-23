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

#include "packet.h"

#include <cstring>

#include "game_server.h"

extern Net::NetServer* g_net_server;
extern GameServer* g_server;

namespace {
constexpr unsigned char kScriptPacketId = Net::PT_EXTENDED_4_SCRIPTS;

Net::PacketReliability ToReliabilitySafe(int reliability) {
  switch (reliability) {
    case Net::UNRELIABLE:
      return Net::UNRELIABLE;
    case Net::RELIABLE:
      return Net::RELIABLE;
    case Net::RELIABLE_ORDERED:
      return Net::RELIABLE_ORDERED;
    default:
      return Net::RELIABLE;
  }
}
}  // namespace

Packet::Packet() { SyncDataPointer(); }

Packet::Packet(const unsigned char* dataPtr, std::uint32_t size, Net::ConnectionHandle connection) : buffer_(dataPtr, dataPtr + size) {
  id = connection;
  SyncDataPointer();
}

Packet::Packet(const Packet& other)
    : buffer_(other.buffer_), write_bit_pos_(other.write_bit_pos_), read_bit_pos_(other.read_bit_pos_), id(other.id) {
  SyncDataPointer();
}

Packet& Packet::operator=(const Packet& other) {
  if (this != &other) {
    buffer_ = other.buffer_;
    write_bit_pos_ = other.write_bit_pos_;
    read_bit_pos_ = other.read_bit_pos_;
    id = other.id;
    SyncDataPointer();
  }
  return *this;
}

Packet::Packet(Packet&& other) noexcept
    : buffer_(std::move(other.buffer_)), write_bit_pos_(other.write_bit_pos_), read_bit_pos_(other.read_bit_pos_), id(other.id) {
  SyncDataPointer();
  other.reset();
}

Packet& Packet::operator=(Packet&& other) noexcept {
  if (this != &other) {
    buffer_ = std::move(other.buffer_);
    write_bit_pos_ = other.write_bit_pos_;
    read_bit_pos_ = other.read_bit_pos_;
    id = other.id;
    SyncDataPointer();
    other.reset();
  }
  return *this;
}

void Packet::reset() {
  buffer_.clear();
  write_bit_pos_ = 0;
  read_bit_pos_ = 0;
  id = 0;
  SyncDataPointer();
}

void Packet::send(int playerid, int Reliability) {
  if (!g_net_server || !g_server) {
    throw std::runtime_error("Network server not initialized");
  }

  auto connection = g_server->GetPlayerManager().GetConnectionHandle(static_cast<PlayerManager::PlayerId>(playerid));
  if (!connection) {
    throw std::runtime_error("Player not found");
  }

  AlignWriteToByte();

  if (buffer_.empty() || buffer_[0] != kScriptPacketId) {
    buffer_.insert(buffer_.begin(), kScriptPacketId);
    write_bit_pos_ += 8;
    read_bit_pos_ += 8;
    SyncDataPointer();
  }

  AlignWriteToByte();
  g_net_server->Send(data, length, Net::MEDIUM_PRIORITY, ToReliabilitySafe(Reliability), 0, *connection);
}

void Packet::sendToAll(int Reliability) {
  if (!g_net_server || !g_server) {
    throw std::runtime_error("Network server not initialized");
  }

  AlignWriteToByte();

  if (buffer_.empty() || buffer_[0] != kScriptPacketId) {
    buffer_.insert(buffer_.begin(), kScriptPacketId);
    write_bit_pos_ += 8;
    read_bit_pos_ += 8;
    SyncDataPointer();
  }

  AlignWriteToByte();
  auto reliability = ToReliabilitySafe(Reliability);
  g_server->GetPlayerManager().ForEachPlayer([&](const PlayerManager::Player& player) {
    g_net_server->Send(data, length, Net::MEDIUM_PRIORITY, reliability, 0, player.connection);
  });
}

void Packet::writeBool(bool value) {
  std::size_t byte_index = write_bit_pos_ / 8;
  if (byte_index >= buffer_.size()) {
    buffer_.push_back(0);
  }

  const std::size_t bit_offset = write_bit_pos_ % 8;
  if (value) {
    buffer_[byte_index] |= static_cast<std::uint8_t>(1u << bit_offset);
  }
  ++write_bit_pos_;
  SyncDataPointer();
}

void Packet::writeInt8(int value) { WriteIntegral<std::int8_t>(static_cast<std::int8_t>(value)); }

void Packet::writeUInt8(int value) { WriteIntegral<std::uint8_t>(static_cast<std::uint8_t>(value)); }

void Packet::writeInt16(int value) { WriteIntegral<std::int16_t>(static_cast<std::int16_t>(value)); }

void Packet::writeUInt16(int value) { WriteIntegral<std::uint16_t>(static_cast<std::uint16_t>(value)); }

void Packet::writeInt32(int value) { WriteIntegral<std::int32_t>(static_cast<std::int32_t>(value)); }

void Packet::writeUInt32(int value) { WriteIntegral<std::uint32_t>(static_cast<std::uint32_t>(value)); }

void Packet::writeFloat(float value) {
  static_assert(sizeof(float) == sizeof(std::uint32_t), "Unexpected float size");
  std::uint32_t repr;
  std::memcpy(&repr, &value, sizeof(float));
  writeUInt32(repr);
}

void Packet::writeString(const std::string& value) {
  AlignWriteToByte();
  writeUInt32(static_cast<std::uint32_t>(value.size()));
  WriteBytes(reinterpret_cast<const unsigned char*>(value.data()), value.size());
}

void Packet::writeBlob(const std::vector<std::uint8_t>& value) {
  AlignWriteToByte();
  writeUInt32(static_cast<std::uint32_t>(value.size()));
  WriteBytes(reinterpret_cast<const unsigned char*>(value.data()), value.size());
}

bool Packet::readBool() {
  if (read_bit_pos_ >= buffer_.size() * 8) {
    throw std::runtime_error("Attempted to read past end of packet");
  }
  const std::size_t byte_index = read_bit_pos_ / 8;
  const std::size_t bit_offset = read_bit_pos_ % 8;
  const bool value = (buffer_[byte_index] >> bit_offset) & 0x1;
  ++read_bit_pos_;
  return value;
}

std::int8_t Packet::readInt8() { return ReadIntegral<std::int8_t>(); }

std::uint8_t Packet::readUInt8() { return ReadIntegral<std::uint8_t>(); }

std::int16_t Packet::readInt16() { return ReadIntegral<std::int16_t>(); }

std::uint16_t Packet::readUInt16() { return ReadIntegral<std::uint16_t>(); }

std::int32_t Packet::readInt32() { return ReadIntegral<std::int32_t>(); }

std::uint32_t Packet::readUInt32() { return ReadIntegral<std::uint32_t>(); }

float Packet::readFloat() {
  auto repr = readUInt32();
  float value;
  std::memcpy(&value, &repr, sizeof(float));
  return value;
}

std::string Packet::readString() {
  AlignReadToByte();
  auto size = readUInt32();
  std::string result(size, '\0');
  if (size > 0) {
    ReadBytes(reinterpret_cast<unsigned char*>(result.data()), size);
  }
  return result;
}

std::vector<std::uint8_t> Packet::readBlob() {
  AlignReadToByte();
  auto size = readUInt32();
  std::vector<std::uint8_t> blob(size);
  if (size > 0) {
    ReadBytes(blob.data(), size);
  }
  return blob;
}

template <typename TInt>
void Packet::WriteIntegral(TInt value) {
  AlignWriteToByte();
  unsigned char bytes[sizeof(TInt)];
  std::memcpy(bytes, &value, sizeof(TInt));
  WriteBytes(bytes, sizeof(TInt));
}

template <typename TInt>
TInt Packet::ReadIntegral() {
  AlignReadToByte();
  TInt value{};
  ReadBytes(reinterpret_cast<unsigned char*>(&value), sizeof(TInt));
  return value;
}

void Packet::WriteBytes(const unsigned char* bytes, std::size_t count) {
  const std::size_t byte_pos = write_bit_pos_ / 8;
  if (buffer_.size() < byte_pos + count) {
    buffer_.resize(byte_pos + count, 0);
  }
  std::memcpy(buffer_.data() + byte_pos, bytes, count);
  write_bit_pos_ += count * 8;
  SyncDataPointer();
}

void Packet::ReadBytes(unsigned char* out, std::size_t count) {
  const std::size_t byte_pos = read_bit_pos_ / 8;
  if (byte_pos + count > buffer_.size()) {
    throw std::runtime_error("Attempted to read past end of packet");
  }
  std::memcpy(out, buffer_.data() + byte_pos, count);
  read_bit_pos_ += count * 8;
}

void Packet::AlignWriteToByte() {
  if (write_bit_pos_ % 8 != 0) {
    write_bit_pos_ += 8 - (write_bit_pos_ % 8);
  }
}

void Packet::AlignReadToByte() {
  if (read_bit_pos_ % 8 != 0) {
    read_bit_pos_ += 8 - (read_bit_pos_ % 8);
  }
}

void Packet::SyncDataPointer() {
  length = static_cast<std::uint32_t>(buffer_.size());
  data = buffer_.empty() ? nullptr : buffer_.data();
}
