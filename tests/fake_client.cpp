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
#include <spdlog/spdlog.h>

#include <dylib.hpp>
#include <functional>
#include <mutex>

#include "net_enums.h"
#include "znet_client.h"

using namespace Net;

static std::function<NetClient*()> create_net_client_func;
static std::mutex create_net_client_mutex;

namespace {
void LoadNetworkLibrary() {
  try {
    static dylib lib("znet");
    create_net_client_func = lib.get_function<NetClient*()>("CreateNetClient");
  } catch (std::exception& ex) {
    SPDLOG_ERROR("LoadNetworkLibrary error: {}", ex.what());
    throw;
  }
}

void InitClientNetworkLayer() {
  std::scoped_lock lock(create_net_client_mutex);
  if (!create_net_client_func) {
    LoadNetworkLibrary();
  }
}

template <typename TContainer = std::vector<std::uint8_t>, typename Packet>
void SerializeAndSend(NetClient* client, const Packet& packet, Net::PacketPriority priority, Net::PacketReliability reliable) {
  TContainer buffer;
  auto written_size = bitsery::quickSerialization<bitsery::OutputBufferAdapter<TContainer>>(buffer, packet);
  client->SendPacket(buffer.data(), written_size, reliable, priority);
}

}  // namespace

FakeClient::FakeClient(const std::string& username, FakeClientObserver* observer) : username_(username), observer_(observer) {
  InitClientNetworkLayer();
  client_ = create_net_client_func();
  client_->AddPacketHandler(*this);
  client_thread_ = std::thread([this]() {
    running_ = true;
    while (running_) {
      if (client_->IsConnected()) {
        client_->Pulse();
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  });
}

FakeClient::~FakeClient() {
  running_ = false;
  client_thread_.join();
  client_->RemovePacketHandler(*this);
  delete client_;
}

bool FakeClient::Connect(const std::string& host, uint16_t port) {
  return client_->Connect(host.c_str(), port);
}

bool FakeClient::HandlePacket(unsigned char* data, std::uint32_t size) {
  Net::PacketID packet_id = static_cast<Net::PacketID>(data[0]);
  if (packet_id == Net::PacketID::PT_INITIAL_INFO) {
    InitialInfoPacket packet;
    using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
    auto state = bitsery::quickDeserialization<InputAdapter>({data, size}, packet);
    SPDLOG_DEBUG("[{}] received: {}", username_, packet);
    SentJoinGamePacket();
    return true;
  } else if (packet_id == Net::PacketID::PT_JOIN_GAME) {
    JoinGamePacket packet;
    using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
    auto state = bitsery::quickDeserialization<InputAdapter>({data, size}, packet);
    SPDLOG_DEBUG("[{}] received: {}", username_, packet);
    if (observer_) {
      observer_->OnJoinGamePacket(packet);
    }
    return true;
  }
  else if (packet_id == Net::PacketID::PT_EXISTING_PLAYERS) {
    ExistingPlayersPacket packet;
    using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
    auto state = bitsery::quickDeserialization<InputAdapter>({data, size}, packet);
    if (observer_) {
      observer_->OnExistingPlayersPacket(packet);
    }
    return true;
  }
  SPDLOG_INFO("[{}] Unhandled packet: {}", username_, PacketIDToString(packet_id));
  return true;
}

void FakeClient::SentJoinGamePacket() {
  JoinGamePacket packet;
  packet.packet_type = PT_JOIN_GAME;
  packet.player_name = username_;
  packet.position = position_;
  packet.normal = rotation_;
  SerializeAndSend(client_, packet, IMMEDIATE_PRIORITY, RELIABLE);
}
