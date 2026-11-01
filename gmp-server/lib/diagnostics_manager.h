/*
MIT License

Copyright (c) 2026 Gothic Multiplayer Team

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

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class DiagnosticsManager {
public:
  struct StatsTable {
    std::string title;
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
  };

  struct ServerSnapshot {
    std::size_t connected_players{0};
    std::size_t ingame_players{0};
    std::size_t loaded_resources{0};
    std::size_t client_packages{0};
    std::size_t streamed_player_links{0};
    std::size_t ground_items{0};
    std::size_t streamed_ground_item_links{0};
  };

  static DiagnosticsManager& Instance();

  void Reset();
  void BeginFrame();
  void EndFrame();
  void Pulse();

  void SetServerSnapshot(ServerSnapshot snapshot);
  void RecordIncomingPacket(std::uint8_t packet_id, std::uint32_t bytes);
  void RecordOutgoingPacket(std::uint8_t packet_id, std::uint32_t bytes);
  void RecordSendFailure(std::uint8_t packet_id);
  void RecordPacketRejected(std::uint8_t packet_id);
  void RecordPacketHandlerTime(std::uint8_t packet_id, std::chrono::nanoseconds elapsed);

  std::string BuildReport() const;

private:
  struct PacketCounters {
    std::uint64_t count{0};
    std::uint64_t bytes{0};
    std::uint64_t rejected{0};
    std::uint64_t send_failures{0};
    std::uint64_t handler_time_ns{0};
    std::uint64_t max_handler_time_ns{0};
  };

  struct FrameCounters {
    std::uint64_t count{0};
    std::uint64_t total_time_ns{0};
    std::uint64_t max_time_ns{0};
  };

  DiagnosticsManager();

  StatsTable BuildServerTable() const;
  StatsTable BuildPacketTable() const;

  static void AddPacket(PacketCounters& counters, std::uint32_t bytes);
  static void ResetPackets(std::array<PacketCounters, 256>& counters);

  std::chrono::steady_clock::time_point started_at_{};
  std::chrono::steady_clock::time_point window_started_at_{};
  std::chrono::steady_clock::time_point frame_started_at_{};
  bool frame_active_{false};
  double last_window_seconds_{1.0};

  ServerSnapshot server_snapshot_{};

  FrameCounters current_frame_{};
  FrameCounters last_frame_{};

  std::array<PacketCounters, 256> incoming_total_{};
  std::array<PacketCounters, 256> outgoing_total_{};
  std::array<PacketCounters, 256> incoming_current_{};
  std::array<PacketCounters, 256> outgoing_current_{};
  std::array<PacketCounters, 256> incoming_last_{};
  std::array<PacketCounters, 256> outgoing_last_{};
};
