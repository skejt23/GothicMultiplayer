/*
MIT License

Copyright (c) 2025 Gothic Multiplayer Team.

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

#include "fake_client.h"

#include <bitsery/adapter/buffer.h>
#include <bitsery/bitsery.h>

#include <algorithm>
#include <utility>

#include "net_enums.h"

namespace {
template <typename Packet>
bool ReadPacket(unsigned char* data, std::uint32_t size, Packet& packet) {
  const auto state = bitsery::quickDeserialization<bitsery::InputBufferAdapter<unsigned char*>>({data, size}, packet);
  return state.first == bitsery::ReaderError::NoError && state.second;
}

template <typename Packet>
bool SendPacket(Net::ClientTransport& client, Packet& packet) {
  std::vector<std::uint8_t> buffer;
  const auto size = bitsery::quickSerialization<bitsery::OutputBufferAdapter<std::vector<std::uint8_t>>>(buffer, packet);
  return client.Send(buffer.data(), static_cast<std::uint32_t>(size), Net::TrafficClass::Control);
}
}  // namespace

FakeClient::FakeClient(const std::string& username) : username_(username) {
  client_.SetCallbacks([this](unsigned char* data, std::uint32_t size) { HandlePacket(data, size); },
                       [this](const Net::ConnectionEvent& event) {
                         if (event.kind == Net::ConnectionEvent::Kind::Failure ||
                             (event.kind == Net::ConnectionEvent::Kind::Closed && event.reason != Net::CloseReason::Normal)) {
                           error_ = event.detail.empty() ? "Transport connection failed" : event.detail;
                         }
                       });
}

bool FakeClient::Connect(const std::string& host, uint16_t port) {
  initial_info_.reset();
  joins_.clear();
  spawns_.clear();
  departures_.clear();
  messages_.clear();
  error_.clear();
  return client_.Connect(host.c_str(), port);
}

void FakeClient::Pulse() {
  client_.Pulse();
}

void FakeClient::Disconnect() {
  client_.Disconnect();
}

bool FakeClient::IsConnected() const {
  return client_.IsConnected();
}

const PlayerSpawnPacket* FakeClient::FindSpawn(std::uint32_t player_id) const {
  const auto found = std::find_if(spawns_.begin(), spawns_.end(), [player_id](const auto& packet) {
    return packet.player_id == player_id;
  });
  return found == spawns_.end() ? nullptr : &*found;
}

bool FakeClient::SendActorAction(ActorActionPacket action) {
  if (!initial_info_) return false;
  const auto* actor = FindSpawn(initial_info_->player_id);
  if (!actor) return false;
  action.actor_id = actor->player_id;
  action.actor_generation = actor->actor_generation;
  action.source_time_ms = ActorClockMilliseconds();
  return SendPacket(client_, action);
}

void FakeClient::HandlePacket(unsigned char* data, std::uint32_t size) {
  if (size == 0) {
    error_ = "Received an empty gameplay message";
    return;
  }
  switch (static_cast<Net::PacketID>(data[0])) {
    case Net::PT_INITIAL_INFO: {
      InitialInfoPacket packet{};
      if (!ReadPacket(data, size, packet)) break;
      initial_info_ = std::move(packet);
      SendJoinGame();
      return;
    }
    case Net::PT_JOIN_GAME: {
      JoinGamePacket packet{};
      if (!ReadPacket(data, size, packet)) break;
      joins_.push_back(std::move(packet));
      return;
    }
    case Net::PT_PLAYER_SPAWN: {
      PlayerSpawnPacket packet{};
      if (!ReadPacket(data, size, packet)) break;
      const bool local = initial_info_ && initial_info_->player_id == packet.player_id;
      spawns_.push_back(std::move(packet));
      if (local) {
        PlayerWorldEnterPacket ready{};
        ready.packet_type = Net::PT_PLAYER_WORLD_ENTER;
        ready.actor_generation = spawns_.back().actor_generation;
        ready.world_name = initial_info_->map_name;
        if (!SendPacket(client_, ready)) error_ = "Could not send world readiness";
      }
      return;
    }
    case Net::PT_LEFT_GAME: {
      DisconnectionInfoPacket packet{};
      if (!ReadPacket(data, size, packet)) break;
      departures_.push_back(std::move(packet));
      return;
    }
    case Net::PT_MSG: {
      MessagePacket packet{};
      if (!ReadPacket(data, size, packet)) break;
      messages_.push_back(std::move(packet));
      return;
    }
    default:
      return;
  }
  error_ = "Could not deserialize gameplay message " + std::to_string(data[0]);
}

void FakeClient::SendJoinGame() {
  JoinGamePacket packet{};
  packet.packet_type = Net::PT_JOIN_GAME;
  packet.actor_generation = initial_info_->actor_generation;
  packet.normal = glm::vec3(0.0f, 0.0f, 1.0f);
  packet.body_model = "Hum_Body_Naked0";
  packet.head_model = "Hum_Head_Pony";
  packet.player_name = username_;
  if (!SendPacket(client_, packet)) error_ = "Could not send join message";
}
