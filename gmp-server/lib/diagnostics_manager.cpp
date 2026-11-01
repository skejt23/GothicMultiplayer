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

#include "diagnostics_manager.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "net_enums.h"

namespace {

std::string FormatFixed(double value, int precision = 1) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(precision) << value;
  return oss.str();
}

std::string FormatInteger(std::uint64_t value) {
  return std::to_string(value);
}

std::string FormatRate(std::uint64_t value, double seconds) {
  if (seconds <= 0.0) {
    return "0.0";
  }
  return FormatFixed(static_cast<double>(value) / seconds);
}

std::string FormatBytesPerSecond(std::uint64_t bytes, double seconds) {
  if (seconds <= 0.0) {
    return "0.0 B/s";
  }

  double value = static_cast<double>(bytes) / seconds;
  const char* unit = "B/s";
  if (value >= 1024.0) {
    value /= 1024.0;
    unit = "KiB/s";
  }
  if (value >= 1024.0) {
    value /= 1024.0;
    unit = "MiB/s";
  }

  std::ostringstream oss;
  oss << std::fixed << std::setprecision(1) << value << ' ' << unit;
  return oss.str();
}

std::string FormatMilliseconds(std::uint64_t ns) {
  return FormatFixed(static_cast<double>(ns) / 1000000.0, 2) + " ms";
}

std::string FormatMicroseconds(std::uint64_t ns) {
  return FormatFixed(static_cast<double>(ns) / 1000.0, 1) + " us";
}

std::string FormatSeconds(std::chrono::steady_clock::duration duration) {
  const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
  return std::to_string(seconds) + " s";
}

std::string PacketName(std::uint8_t packet_id) {
  return Net::PacketIDToString(static_cast<Net::PacketID>(packet_id));
}

void AppendTable(std::ostringstream& oss, const DiagnosticsManager::StatsTable& table) {
  oss << table.title << '\n';
  oss << std::string(table.title.size(), '=') << "\n\n";

  if (table.columns.empty()) {
    return;
  }

  std::vector<std::size_t> widths(table.columns.size(), 0);
  for (std::size_t i = 0; i < table.columns.size(); ++i) {
    widths[i] = table.columns[i].size();
  }
  for (const auto& row : table.rows) {
    const auto column_count = std::min(row.size(), widths.size());
    for (std::size_t i = 0; i < column_count; ++i) {
      widths[i] = std::max(widths[i], row[i].size());
    }
  }

  auto append_row = [&](const std::vector<std::string>& values) {
    for (std::size_t i = 0; i < widths.size(); ++i) {
      if (i != 0) {
        oss << "  ";
      }

      const std::string empty;
      const auto& value = i < values.size() ? values[i] : empty;
      oss << std::left << std::setw(static_cast<int>(widths[i])) << value;
    }
    oss << '\n';
  };

  append_row(table.columns);
  for (std::size_t i = 0; i < widths.size(); ++i) {
    if (i != 0) {
      oss << "  ";
    }
    oss << std::string(widths[i], '-');
  }
  oss << '\n';

  for (const auto& row : table.rows) {
    append_row(row);
  }
}

}  // namespace

DiagnosticsManager& DiagnosticsManager::Instance() {
  static DiagnosticsManager instance;
  return instance;
}

DiagnosticsManager::DiagnosticsManager() {
  Reset();
}

void DiagnosticsManager::Reset() {
  const auto now = std::chrono::steady_clock::now();
  started_at_ = now;
  window_started_at_ = now;
  frame_started_at_ = now;
  frame_active_ = false;
  last_window_seconds_ = 1.0;
  server_snapshot_ = {};
  current_frame_ = {};
  last_frame_ = {};
  ResetPackets(incoming_total_);
  ResetPackets(outgoing_total_);
  ResetPackets(incoming_current_);
  ResetPackets(outgoing_current_);
  ResetPackets(incoming_last_);
  ResetPackets(outgoing_last_);
}

void DiagnosticsManager::BeginFrame() {
  frame_started_at_ = std::chrono::steady_clock::now();
  frame_active_ = true;
}

void DiagnosticsManager::EndFrame() {
  if (frame_active_) {
    const auto elapsed = std::chrono::steady_clock::now() - frame_started_at_;
    const auto elapsed_ns = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
    ++current_frame_.count;
    current_frame_.total_time_ns += elapsed_ns;
    current_frame_.max_time_ns = std::max(current_frame_.max_time_ns, elapsed_ns);
    frame_active_ = false;
  }

  Pulse();
}

void DiagnosticsManager::Pulse() {
  const auto now = std::chrono::steady_clock::now();
  const auto elapsed = now - window_started_at_;
  if (elapsed < std::chrono::seconds(1)) {
    return;
  }

  last_window_seconds_ = std::max(0.001, std::chrono::duration<double>(elapsed).count());
  window_started_at_ = now;

  incoming_last_ = incoming_current_;
  outgoing_last_ = outgoing_current_;
  last_frame_ = current_frame_;

  ResetPackets(incoming_current_);
  ResetPackets(outgoing_current_);
  current_frame_ = {};
}

void DiagnosticsManager::SetServerSnapshot(ServerSnapshot snapshot) {
  server_snapshot_ = snapshot;
}

void DiagnosticsManager::RecordIncomingPacket(std::uint8_t packet_id, std::uint32_t bytes) {
  AddPacket(incoming_total_[packet_id], bytes);
  AddPacket(incoming_current_[packet_id], bytes);
}

void DiagnosticsManager::RecordOutgoingPacket(std::uint8_t packet_id, std::uint32_t bytes) {
  AddPacket(outgoing_total_[packet_id], bytes);
  AddPacket(outgoing_current_[packet_id], bytes);
}

void DiagnosticsManager::RecordSendFailure(std::uint8_t packet_id) {
  ++outgoing_total_[packet_id].send_failures;
  ++outgoing_current_[packet_id].send_failures;
}

void DiagnosticsManager::RecordPacketRejected(std::uint8_t packet_id) {
  ++incoming_total_[packet_id].rejected;
  ++incoming_current_[packet_id].rejected;
}

void DiagnosticsManager::RecordPacketHandlerTime(std::uint8_t packet_id, std::chrono::nanoseconds elapsed) {
  const auto ns = static_cast<std::uint64_t>(elapsed.count());
  incoming_total_[packet_id].handler_time_ns += ns;
  incoming_total_[packet_id].max_handler_time_ns = std::max(incoming_total_[packet_id].max_handler_time_ns, ns);
  incoming_current_[packet_id].handler_time_ns += ns;
  incoming_current_[packet_id].max_handler_time_ns = std::max(incoming_current_[packet_id].max_handler_time_ns, ns);
}

std::string DiagnosticsManager::BuildReport() const {
  std::ostringstream oss;
  oss << "Gothic Multiplayer diagnostics\n";
  oss << "==============================\n\n";
  oss << "Last completed stats window: " << FormatFixed(last_window_seconds_, 2) << " s\n\n";
  oss << "Outgoing gameplay counts include only messages accepted by the transport, not peer delivery acknowledgements.\n\n";

  AppendTable(oss, BuildServerTable());
  oss << '\n';
  AppendTable(oss, BuildPacketTable());
  return oss.str();
}

DiagnosticsManager::StatsTable DiagnosticsManager::BuildServerTable() const {
  StatsTable table;
  table.title = "Server diagnostics";
  table.columns = {"metric", "value"};

  const auto now = std::chrono::steady_clock::now();
  const auto avg_frame_ns = last_frame_.count == 0 ? 0 : last_frame_.total_time_ns / last_frame_.count;

  table.rows.push_back({"uptime", FormatSeconds(now - started_at_)});
  table.rows.push_back({"connected players", std::to_string(server_snapshot_.connected_players)});
  table.rows.push_back({"ingame players", std::to_string(server_snapshot_.ingame_players)});
  table.rows.push_back({"loaded resources", std::to_string(server_snapshot_.loaded_resources)});
  table.rows.push_back({"client packages", std::to_string(server_snapshot_.client_packages)});
  table.rows.push_back({"streamed player links", std::to_string(server_snapshot_.streamed_player_links)});
  table.rows.push_back({"ground items", std::to_string(server_snapshot_.ground_items)});
  table.rows.push_back({"streamed ground item links", std::to_string(server_snapshot_.streamed_ground_item_links)});
  table.rows.push_back({"frames/sec", FormatRate(last_frame_.count, last_window_seconds_)});
  table.rows.push_back({"avg frame", FormatMilliseconds(avg_frame_ns)});
  table.rows.push_back({"max frame", FormatMilliseconds(last_frame_.max_time_ns)});

  std::uint64_t incoming_bytes = 0;
  std::uint64_t outgoing_bytes = 0;
  std::uint64_t incoming_count = 0;
  std::uint64_t outgoing_count = 0;
  for (std::size_t i = 0; i < incoming_last_.size(); ++i) {
    incoming_bytes += incoming_last_[i].bytes;
    outgoing_bytes += outgoing_last_[i].bytes;
    incoming_count += incoming_last_[i].count;
    outgoing_count += outgoing_last_[i].count;
  }

  table.rows.push_back({"incoming packets/sec", FormatRate(incoming_count, last_window_seconds_)});
  table.rows.push_back({"outgoing packets/sec", FormatRate(outgoing_count, last_window_seconds_)});
  table.rows.push_back({"incoming bytes/sec", FormatBytesPerSecond(incoming_bytes, last_window_seconds_)});
  table.rows.push_back({"outgoing bytes/sec", FormatBytesPerSecond(outgoing_bytes, last_window_seconds_)});
  return table;
}

DiagnosticsManager::StatsTable DiagnosticsManager::BuildPacketTable() const {
  struct PacketRow {
    std::uint8_t packet_id{};
    std::uint64_t activity{};
  };

  std::vector<PacketRow> active_packets;
  active_packets.reserve(256);
  for (std::size_t i = 0; i < incoming_last_.size(); ++i) {
    const auto activity = incoming_last_[i].count + outgoing_last_[i].count + incoming_total_[i].rejected + outgoing_total_[i].send_failures;
    if (activity != 0 || incoming_total_[i].count != 0 || outgoing_total_[i].count != 0) {
      active_packets.push_back(PacketRow{static_cast<std::uint8_t>(i), activity});
    }
  }

  std::sort(active_packets.begin(), active_packets.end(), [](const PacketRow& lhs, const PacketRow& rhs) {
    if (lhs.activity == rhs.activity) {
      return lhs.packet_id < rhs.packet_id;
    }
    return lhs.activity > rhs.activity;
  });

  StatsTable table;
  table.title = "Gameplay message diagnostics";
  table.columns = {"packet", "in/s", "in bytes/s", "out/s", "out bytes/s", "rejects", "send failures", "avg handler", "max handler", "total in", "total out"};

  for (const auto row_info : active_packets) {
    const auto id = row_info.packet_id;
    const auto& in_last = incoming_last_[id];
    const auto& out_last = outgoing_last_[id];
    const auto& in_total = incoming_total_[id];
    const auto& out_total = outgoing_total_[id];
    const auto avg_handler_ns = in_last.count == 0 ? 0 : in_last.handler_time_ns / in_last.count;

    table.rows.push_back({
        PacketName(id),
        FormatRate(in_last.count, last_window_seconds_),
        FormatBytesPerSecond(in_last.bytes, last_window_seconds_),
        FormatRate(out_last.count, last_window_seconds_),
        FormatBytesPerSecond(out_last.bytes, last_window_seconds_),
        FormatInteger(in_total.rejected),
        FormatInteger(out_total.send_failures),
        FormatMicroseconds(avg_handler_ns),
        FormatMicroseconds(in_last.max_handler_time_ns),
        FormatInteger(in_total.count),
        FormatInteger(out_total.count),
    });
  }

  if (table.rows.empty()) {
    table.rows.push_back({"none", "0.0", "0.0 B/s", "0.0", "0.0 B/s", "0", "0", "0.0 us", "0.0 us", "0", "0"});
  }

  return table;
}

void DiagnosticsManager::AddPacket(PacketCounters& counters, std::uint32_t bytes) {
  ++counters.count;
  counters.bytes += bytes;
}

void DiagnosticsManager::ResetPackets(std::array<PacketCounters, 256>& counters) {
  for (auto& counter : counters) {
    counter = {};
  }
}
