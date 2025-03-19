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

#pragma once

#include <gmock/gmock.h>

#include <atomic>
#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <thread>

#include "packets.h"
#include "znet_client.h"

class FakeClientObserver {
public:
  virtual ~FakeClientObserver() = default;

  virtual void OnExistingPlayersPacket(const ExistingPlayersPacket& packet) = 0;
  virtual void OnJoinGamePacket(const JoinGamePacket& packet) = 0;
};

class FakeObserverMock : public FakeClientObserver {
public:
  MOCK_METHOD(void, OnExistingPlayersPacket, (const ExistingPlayersPacket& packet), (override));
  MOCK_METHOD(void, OnJoinGamePacket, (const JoinGamePacket& packet), (override));
};

class FakeClient : public Net::NetClient::PacketHandler {
public:
  FakeClient(const std::string& username = "TestUser", FakeClientObserver* observer = nullptr);
  ~FakeClient();

  // Connection methods
  bool Connect(const std::string& host, uint16_t port = 57005);

private:
  bool HandlePacket(unsigned char* data, std::uint32_t size) override;
  void SentJoinGamePacket();

  FakeClientObserver* observer_;
  std::string username_;
  Net::NetClient* client_;
  std::thread client_thread_;
  std::atomic<bool> running_{false};
  glm::vec3 position_{0.0f};
  glm::vec3 rotation_{0.0f};
};
