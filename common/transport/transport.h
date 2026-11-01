#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace Net {

// Ephemeral transport handle. Never persist it or use it as a gameplay PlayerId.
using ConnectionHandle = std::uint32_t;
inline constexpr ConnectionHandle kInvalidConnection = 0;

// Control is reliable and ordered. State and Voice are unreliable, low-latency
// snapshots; application state sequence numbers remain responsible for freshness.
// World-item updates use Control: they must remain ordered with world changes.
// No ordering is promised between traffic classes.
enum class TrafficClass { Control, State, Voice };

enum class CloseReason : int {
  None = 0,
  Normal = 1000,
  ServerFull = 1001,
  Banned = 1002,
  Shutdown = 1003,
  Kicked = 1004,
  Failure = 1005,
};

struct ConnectionEvent {
  enum class Kind { Connected, Closed, Failure };
  Kind kind;
  ConnectionHandle connection;
  CloseReason reason = CloseReason::None;
  std::string detail;
};

// Live GNS connection estimates, not cumulative gameplay byte/message counters.
// Quality is a fraction [0,1], or -1 when GNS has no estimate yet.
struct NetworkStats {
  bool available = false;
  std::int32_t ping_ms = -1;
  float local_quality = -1.0f;
  float remote_quality = -1.0f;
  float outgoing_packets_per_second = 0.0f;
  float outgoing_bytes_per_second = 0.0f;
  float incoming_packets_per_second = 0.0f;
  float incoming_bytes_per_second = 0.0f;
  std::int32_t send_rate_bytes_per_second = 0;
  std::int32_t pending_unreliable_bytes = 0;
  std::int32_t pending_reliable_bytes = 0;
  std::int32_t unacknowledged_reliable_bytes = 0;
  std::int64_t control_queue_time_usec = 0;
  std::int64_t realtime_queue_time_usec = 0;
};

// Concrete direct-IP endpoints. Each owns a shared reference to the process-wide
// GNS runtime, acquired lazily by Connect/Start (after any server daemon fork).
// Distinct endpoints may be used on distinct threads. Callbacks run
// only from that endpoint's Pulse; do not destroy the endpoint from a callback.
// Message memory is borrowed for the duration of the message callback only.
class ClientTransport final {
 public:
  using MessageCallback = std::function<void(unsigned char*, std::uint32_t)>;
  using EventCallback = std::function<void(const ConnectionEvent&)>;

  ClientTransport();
  ~ClientTransport();
  ClientTransport(const ClientTransport&) = delete;
  ClientTransport& operator=(const ClientTransport&) = delete;

  void SetCallbacks(MessageCallback message, EventCallback event);
  bool Connect(const char* host, std::uint32_t port);
  void Disconnect(CloseReason reason = CloseReason::Normal);
  bool IsConnected() const;
  void Pulse();
  bool Send(const void* data, std::uint32_t size, TrafficClass traffic);
  std::int32_t GetPing() const;
  NetworkStats GetNetworkStats() const;
  std::string GetDiagnostics() const;
  ConnectionHandle GetConnection() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

class ServerTransport final {
 public:
  using MessageCallback = std::function<void(ConnectionHandle, unsigned char*, std::uint32_t)>;
  using EventCallback = std::function<void(const ConnectionEvent&)>;
  using AdmissionCheck = std::function<CloseReason(const std::string&)>;

  ServerTransport();
  ~ServerTransport();
  ServerTransport(const ServerTransport&) = delete;
  ServerTransport& operator=(const ServerTransport&) = delete;

  void SetCallbacks(MessageCallback message, EventCallback event);
  void SetAdmissionCheck(AdmissionCheck check);
  bool Start(std::uint32_t port, std::uint32_t slots);
  void Stop();
  void Pulse();
  bool Send(const void* data, std::uint32_t size, TrafficClass traffic, ConnectionHandle connection);
  bool CloseConnection(ConnectionHandle connection, CloseReason reason = CloseReason::Normal, const std::string& detail = {});
  std::string GetPlayerIp(ConnectionHandle connection) const;
  // 64 lowercase hex characters from the peer's GNS certificate, or empty if
  // unavailable. Self-asserted machine identity, not authenticated hardware.
  std::string GetPlayerIdentity(ConnectionHandle connection) const;
  std::int32_t GetPing(ConnectionHandle connection) const;
  NetworkStats GetNetworkStats(ConnectionHandle connection) const;
  std::string GetDiagnostics(ConnectionHandle connection) const;
  std::uint32_t GetPort() const;
  std::string GetAddress() const;
  std::size_t GetConnectionCount() const;
  bool IsRunning() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace Net
