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

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "packets.h"
#include "transport/transport.h"

// A protocol peer with explicit polling on the test thread. It joins after
// initial info and reports world readiness after its own spawn snapshot.
class FakeClient {
public:
  explicit FakeClient(const std::string& username = "TestUser");
  bool Connect(const std::string& host, uint16_t port = 57005);
  void Pulse();
  void Disconnect();
  bool IsConnected() const;

  const std::optional<InitialInfoPacket>& InitialInfo() const { return initial_info_; }
  const std::vector<JoinGamePacket>& Joins() const { return joins_; }
  const std::vector<PlayerSpawnPacket>& Spawns() const { return spawns_; }
  const std::vector<DisconnectionInfoPacket>& Departures() const { return departures_; }
  const std::vector<MessagePacket>& Messages() const { return messages_; }
  const std::string& Error() const { return error_; }
  const PlayerSpawnPacket* FindSpawn(std::uint32_t player_id) const;
  bool SendActorAction(ActorActionPacket action);

private:
  void HandlePacket(unsigned char* data, std::uint32_t size);
  void SendJoinGame();

  std::string username_;
  Net::ClientTransport client_;
  std::optional<InitialInfoPacket> initial_info_;
  std::vector<JoinGamePacket> joins_;
  std::vector<PlayerSpawnPacket> spawns_;
  std::vector<DisconnectionInfoPacket> departures_;
  std::vector<MessagePacket> messages_;
  std::string error_;
};
