#include "transport.h"
#include "machine_identity.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <sys/socket.h>
#endif

#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Net {
namespace {

struct Mailbox {
  std::deque<SteamNetConnectionStatusChangedCallback_t> statuses;
  std::deque<ConnectionEvent> events;
};

struct Runtime {
  std::recursive_mutex mutex;
  std::size_t references = 0;
  ISteamNetworkingSockets* sockets = nullptr;
  bool has_machine_identity = false;
  std::unordered_map<HSteamNetConnection, Mailbox*> connections;
  std::unordered_map<HSteamListenSocket, Mailbox*> listeners;
};

Runtime& SharedRuntime() {
  static Runtime runtime;
  return runtime;
}

// RunCallbacks operates on the process-wide interface. Route native events to
// endpoint mailboxes here; never execute another endpoint's gameplay on the
// thread that happened to pump GNS first.
void ConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* status) {
  auto& runtime = SharedRuntime();
  std::lock_guard<std::recursive_mutex> lock(runtime.mutex);
  auto connection = runtime.connections.find(status->m_hConn);
  if (connection != runtime.connections.end()) {
    connection->second->statuses.push_back(*status);
    return;
  }
  auto listener = runtime.listeners.find(status->m_info.m_hListenSocket);
  if (listener != runtime.listeners.end()) {
    listener->second->statuses.push_back(*status);
  }
}

class RuntimeReference {
 public:
  RuntimeReference() {
    auto& runtime = SharedRuntime();
    std::lock_guard<std::recursive_mutex> lock(runtime.mutex);
    if (runtime.references == 0) {
      const auto machine_identity = ReadMachineIdentity();
      SteamNetworkingIdentity identity;
      identity.Clear();
      if (machine_identity && !identity.SetGenericBytes(machine_identity->data(), machine_identity->size())) {
        throw std::runtime_error("GameNetworkingSockets rejected the machine identity");
      }
      SteamNetworkingErrMsg error{};
      // GNS includes this identity in its generated unsigned connection cert.
      // The certificate key can rotate; the machine identity must not.
      if (!GameNetworkingSockets_Init(machine_identity ? &identity : nullptr, error)) {
        throw std::runtime_error(std::string("GameNetworkingSockets initialization failed: ") + error);
      }
      runtime.sockets = SteamNetworkingSockets();
      runtime.has_machine_identity = machine_identity.has_value();
    }
    ++runtime.references;
  }

  ~RuntimeReference() {
    auto& runtime = SharedRuntime();
    std::lock_guard<std::recursive_mutex> lock(runtime.mutex);
    if (--runtime.references == 0) {
      runtime.connections.clear();
      runtime.listeners.clear();
      GameNetworkingSockets_Kill();
      runtime.sockets = nullptr;
      runtime.has_machine_identity = false;
    }
  }
};

const char* ReasonText(CloseReason reason) {
  switch (reason) {
    case CloseReason::None: return "Connection closed";
    case CloseReason::Normal: return "Disconnected";
    case CloseReason::ServerFull: return "Server is full";
    case CloseReason::Banned: return "IP address is banned";
    case CloseReason::Shutdown: return "Server is shutting down";
    case CloseReason::Kicked: return "Kicked by server";
    case CloseReason::Failure: return "Connection failed";
  }
  return "Connection failed";
}

CloseReason PeerReason(int reason) {
  switch (static_cast<CloseReason>(reason)) {
    case CloseReason::Normal:
    case CloseReason::ServerFull:
    case CloseReason::Banned:
    case CloseReason::Shutdown:
    case CloseReason::Kicked:
    case CloseReason::Failure:
      return static_cast<CloseReason>(reason);
    default:
      return CloseReason::Failure;
  }
}

void ReportError(const char* operation, ConnectionHandle connection, int result) {
  std::fprintf(stderr, "GNS %s failed (connection=%u, result=%d)\n", operation, connection, result);
}

std::array<SteamNetworkingConfigValue_t, 2> ConnectionOptions() {
  std::array<SteamNetworkingConfigValue_t, 2> options{};
  options[0].SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
                    reinterpret_cast<void*>(&ConnectionStatusChanged));
  // Direct IP using GNS unsigned certificates, which carry our machine hash.
  // Encryption remains enabled; the machine claim is not authenticated by a CA.
  // Value 1 still attempts CA authentication and fails without a CA outside
  // localhost. Value 2 explicitly uses unsigned certificates on both endpoints.
  options[1].SetInt32(k_ESteamNetworkingConfig_IP_AllowWithoutAuth, 2);
  return options;
}

bool ConfigureLanes(ConnectionHandle connection) {
  // Ordered gameplay and transient state/voice use independent outbound lanes.
  // State and voice need no separate lanes because neither has ordering promises.
  // Both lanes share bandwidth equally. Items stay ordered with world changes.
  const auto result = SharedRuntime().sockets->ConfigureConnectionLanes(connection, 2, nullptr, nullptr);
  if (result != k_EResultOK) ReportError("ConfigureConnectionLanes", connection, result);
  return result == k_EResultOK;
}

bool ResolveAddress(const char* host, std::uint32_t port, SteamNetworkingIPAddr& address) {
  if (!host || !*host || port == 0 || port > 65535) return false;
  address.Clear();
  if (address.ParseString(host)) {
    address.m_port = static_cast<uint16>(port);
    return true;
  }

  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  addrinfo* result = nullptr;
  if (getaddrinfo(host, nullptr, &hints, &result) != 0) return false;
  std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> resolved(result, &freeaddrinfo);
  for (auto* entry = result; entry; entry = entry->ai_next) {
    char numeric_host[NI_MAXHOST]{};
    if (getnameinfo(entry->ai_addr, static_cast<int>(entry->ai_addrlen), numeric_host,
                    sizeof(numeric_host), nullptr, 0, NI_NUMERICHOST) == 0 && address.ParseString(numeric_host)) {
      address.m_port = static_cast<uint16>(port);
      return true;
    }
  }
  return false;
}

std::string AddressString(const SteamNetworkingIPAddr& address) {
  char text[SteamNetworkingIPAddr::k_cchMaxString]{};
  address.ToString(text, sizeof(text), false);
  return text;
}

std::string MachineIdentityString(const SteamNetworkingIdentity& identity) {
  int size = 0;
  const auto* bytes = identity.GetGenericBytes(size);
  if (!bytes || size != static_cast<int>(kMachineIdentityBytes)) return {};
  constexpr char hex[] = "0123456789abcdef";
  std::string text(kMachineIdentityBytes * 2, '0');
  for (std::size_t i = 0; i < kMachineIdentityBytes; ++i) {
    text[i * 2] = hex[bytes[i] >> 4];
    text[i * 2 + 1] = hex[bytes[i] & 15];
  }
  return text;
}

struct MessageReleaser {
  void operator()(SteamNetworkingMessage_t* message) const { message->Release(); }
};
using Message = std::unique_ptr<SteamNetworkingMessage_t, MessageReleaser>;

bool SendMessage(ConnectionHandle connection, const void* data, std::uint32_t size, TrafficClass traffic) {
  if (!connection || !data || size == 0 || size > k_cbMaxSteamNetworkingSocketsMessageSizeSend) return false;
  auto* raw_message = SteamNetworkingUtils()->AllocateMessage(static_cast<int>(size));
  if (!raw_message) {
    ReportError("AllocateMessage", connection, k_EResultFail);
    return false;
  }
  Message message(raw_message);
  std::memcpy(message->m_pData, data, size);
  message->m_conn = connection;
  switch (traffic) {
    case TrafficClass::Control:
      message->m_idxLane = 0;
      message->m_nFlags = k_nSteamNetworkingSend_Reliable;
      break;
    case TrafficClass::State:
    case TrafficClass::Voice:
      message->m_idxLane = 1;
      message->m_nFlags = k_nSteamNetworkingSend_Unreliable | k_nSteamNetworkingSend_NoNagle;
      break;
    default:
      return false;
  }
  int64 result = 0;
  raw_message = message.release();
  // GNS takes ownership even on a send error.
  SharedRuntime().sockets->SendMessages(1, &raw_message, &result, true);
  if (result < 0) {
    ReportError("SendMessages", connection, static_cast<int>(-result));
    return false;
  }
  return true;
}

NetworkStats ReadStats(ConnectionHandle connection) {
  NetworkStats result;
  SteamNetConnectionRealTimeStatus_t status{};
  SteamNetConnectionRealTimeLaneStatus_t lanes[2]{};
  if (!connection || SharedRuntime().sockets->GetConnectionRealTimeStatus(connection, &status, 2, lanes) != k_EResultOK) {
    return result;
  }
  result.available = true;
  result.ping_ms = status.m_nPing;
  result.local_quality = status.m_flConnectionQualityLocal;
  result.remote_quality = status.m_flConnectionQualityRemote;
  result.outgoing_packets_per_second = status.m_flOutPacketsPerSec;
  result.outgoing_bytes_per_second = status.m_flOutBytesPerSec;
  result.incoming_packets_per_second = status.m_flInPacketsPerSec;
  result.incoming_bytes_per_second = status.m_flInBytesPerSec;
  result.send_rate_bytes_per_second = status.m_nSendRateBytesPerSecond;
  result.pending_unreliable_bytes = status.m_cbPendingUnreliable;
  result.pending_reliable_bytes = status.m_cbPendingReliable;
  result.unacknowledged_reliable_bytes = status.m_cbSentUnackedReliable;
  // The aggregate m_usecQueueTime is explicitly undefined with multiple lanes.
  result.control_queue_time_usec = lanes[0].m_usecQueueTime;
  result.realtime_queue_time_usec = lanes[1].m_usecQueueTime;
  return result;
}

std::string ReadDiagnostics(ConnectionHandle connection) {
  if (!connection) return {};
  std::vector<char> text(4096);
  int result = SharedRuntime().sockets->GetDetailedConnectionStatus(connection, text.data(), static_cast<int>(text.size()));
  if (result > 0) {
    text.resize(static_cast<std::size_t>(result));
    result = SharedRuntime().sockets->GetDetailedConnectionStatus(connection, text.data(), static_cast<int>(text.size()));
  }
  return result == 0 ? std::string(text.data()) : std::string{};
}

bool TerminalState(ESteamNetworkingConnectionState state) {
  return state == k_ESteamNetworkingConnectionState_ClosedByPeer ||
         state == k_ESteamNetworkingConnectionState_ProblemDetectedLocally;
}

ConnectionEvent TerminalEvent(const SteamNetConnectionStatusChangedCallback_t& status) {
  const bool local_failure = status.m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally;
  const auto reason = local_failure ? CloseReason::Failure : PeerReason(status.m_info.m_eEndReason);
  return {local_failure || reason == CloseReason::Failure ? ConnectionEvent::Kind::Failure : ConnectionEvent::Kind::Closed,
          status.m_hConn, reason, status.m_info.m_szEndDebug};
}

}  // namespace

struct ClientTransport::Impl {
  std::unique_ptr<RuntimeReference> runtime;
  Mailbox mailbox;
  ConnectionHandle connection = kInvalidConnection;
  std::uint64_t generation = 0;
  bool connected = false;
  MessageCallback message_callback;
  EventCallback event_callback;

  void Close(CloseReason reason, bool notify, bool allow_linger = true) {
    if (!connection) return;
    const auto closed = connection;
    SharedRuntime().sockets->CloseConnection(closed, static_cast<int>(reason), ReasonText(reason),
                                            allow_linger && reason == CloseReason::Normal);
    SharedRuntime().connections.erase(closed);
    connection = kInvalidConnection;
    connected = false;
    mailbox.statuses.clear();
    if (notify) mailbox.events.push_back({reason == CloseReason::Failure ? ConnectionEvent::Kind::Failure : ConnectionEvent::Kind::Closed,
                                         closed, reason, ReasonText(reason)});
  }

  void ProcessStatuses() {
    while (!mailbox.statuses.empty()) {
      auto status = mailbox.statuses.front();
      mailbox.statuses.pop_front();
      if (status.m_hConn != connection) continue;
      if (status.m_info.m_eState == k_ESteamNetworkingConnectionState_Connected && !connected) {
        connected = true;
        mailbox.events.push_back({ConnectionEvent::Kind::Connected, connection, CloseReason::None, {}});
      } else if (TerminalState(status.m_info.m_eState)) {
        mailbox.events.push_back(TerminalEvent(status));
        SharedRuntime().sockets->CloseConnection(connection, 0, nullptr, false);
        SharedRuntime().connections.erase(connection);
        connection = kInvalidConnection;
        connected = false;
      }
    }
  }
};

ClientTransport::ClientTransport() : impl_(std::make_unique<Impl>()) {}

ClientTransport::~ClientTransport() {
  std::lock_guard<std::recursive_mutex> lock(SharedRuntime().mutex);
  // The final runtime owner may shut down immediately after this destructor.
  // Send the close now; waiting for queued gameplay to drain would lose it.
  impl_->Close(CloseReason::Normal, false, false);
}

void ClientTransport::SetCallbacks(MessageCallback message, EventCallback event) {
  std::lock_guard<std::recursive_mutex> lock(SharedRuntime().mutex);
  impl_->message_callback = std::move(message);
  impl_->event_callback = std::move(event);
}

bool ClientTransport::Connect(const char* host, std::uint32_t port) {
  {
    std::lock_guard<std::recursive_mutex> lock(SharedRuntime().mutex);
    if (!impl_->runtime) impl_->runtime = std::make_unique<RuntimeReference>();
    if (!SharedRuntime().has_machine_identity) {
      std::fprintf(stderr, "GMP cannot connect: a stable OS machine identity is unavailable\n");
      return false;
    }
  }
  SteamNetworkingIPAddr address;
  if (!ResolveAddress(host, port, address)) return false;
  std::lock_guard<std::recursive_mutex> lock(SharedRuntime().mutex);
  impl_->Close(CloseReason::Normal, false);
  impl_->mailbox.events.clear();
  impl_->mailbox.statuses.clear();
  ++impl_->generation;
  auto options = ConnectionOptions();
  const auto connection = SharedRuntime().sockets->ConnectByIPAddress(address, static_cast<int>(options.size()), options.data());
  if (!connection) return false;
  if (!ConfigureLanes(connection)) {
    SharedRuntime().sockets->CloseConnection(connection, static_cast<int>(CloseReason::Failure), "Unable to configure traffic lanes", false);
    return false;
  }
  impl_->connection = connection;
  SharedRuntime().connections.emplace(connection, &impl_->mailbox);
  return true;
}

void ClientTransport::Disconnect(CloseReason reason) {
  std::lock_guard<std::recursive_mutex> lock(SharedRuntime().mutex);
  impl_->Close(reason, true);
}

bool ClientTransport::IsConnected() const {
  std::lock_guard<std::recursive_mutex> lock(SharedRuntime().mutex);
  return impl_->connected;
}

void ClientTransport::Pulse() {
  std::deque<ConnectionEvent> events;
  EventCallback event_callback;
  std::uint64_t generation;
  {
    std::lock_guard<std::recursive_mutex> lock(SharedRuntime().mutex);
    if (impl_->runtime) SharedRuntime().sockets->RunCallbacks();
    impl_->ProcessStatuses();
    events.swap(impl_->mailbox.events);
    event_callback = impl_->event_callback;
    generation = impl_->generation;
  }
  for (const auto& event : events) {
    {
      std::lock_guard<std::recursive_mutex> lock(SharedRuntime().mutex);
      if (generation != impl_->generation) break;
      if (event.kind == ConnectionEvent::Kind::Connected && (!impl_->connected || event.connection != impl_->connection)) continue;
    }
    if (event_callback) event_callback(event);
  }

  // Bound each Pulse so a continuously sending peer cannot monopolize a frame.
  for (int received = 0; received < 256; ++received) {
    Message message;
    MessageCallback callback;
    {
      std::lock_guard<std::recursive_mutex> lock(SharedRuntime().mutex);
      if (!impl_->connected) break;
      SteamNetworkingMessage_t* raw_message = nullptr;
      const int count = SharedRuntime().sockets->ReceiveMessagesOnConnection(impl_->connection, &raw_message, 1);
      if (count <= 0) break;
      message.reset(raw_message);
      callback = impl_->message_callback;
    }
    // Release on every path, including exceptions thrown by gameplay callbacks.
    if (callback) callback(static_cast<unsigned char*>(message->m_pData), static_cast<std::uint32_t>(message->m_cbSize));
  }
}

bool ClientTransport::Send(const void* data, std::uint32_t size, TrafficClass traffic) {
  std::lock_guard<std::recursive_mutex> lock(SharedRuntime().mutex);
  return impl_->connected && SendMessage(impl_->connection, data, size, traffic);
}

std::int32_t ClientTransport::GetPing() const { return GetNetworkStats().ping_ms; }

NetworkStats ClientTransport::GetNetworkStats() const {
  std::lock_guard<std::recursive_mutex> lock(SharedRuntime().mutex);
  return ReadStats(impl_->connection);
}

std::string ClientTransport::GetDiagnostics() const {
  std::lock_guard<std::recursive_mutex> lock(SharedRuntime().mutex);
  return ReadDiagnostics(impl_->connection);
}

ConnectionHandle ClientTransport::GetConnection() const {
  std::lock_guard<std::recursive_mutex> lock(SharedRuntime().mutex);
  return impl_->connection;
}

struct ServerTransport::Impl {
  struct Peer {
    std::string ip;
    std::string identity;
    bool connected = false;
    std::deque<Message> pending_messages;
  };

  std::unique_ptr<RuntimeReference> runtime;
  Mailbox mailbox;
  HSteamListenSocket listener = k_HSteamListenSocket_Invalid;
  HSteamNetPollGroup poll_group = k_HSteamNetPollGroup_Invalid;
  std::uint32_t slots = 0;
  std::uint32_t port = 0;
  std::string address;
  std::unordered_map<ConnectionHandle, Peer> peers;
  bool has_pending_messages = false;
  MessageCallback message_callback;
  EventCallback event_callback;
  AdmissionCheck admission_check;

  bool Close(ConnectionHandle connection, CloseReason reason, const std::string& detail, bool notify) {
    auto peer = peers.find(connection);
    if (peer == peers.end()) return false;
    const auto text = detail.empty() ? ReasonText(reason) : detail;
    const bool result = SharedRuntime().sockets->CloseConnection(connection, static_cast<int>(reason), text.c_str(),
                                                                reason == CloseReason::Normal);
    SharedRuntime().connections.erase(connection);
    peers.erase(peer);
    if (notify) mailbox.events.push_back({reason == CloseReason::Failure ? ConnectionEvent::Kind::Failure : ConnectionEvent::Kind::Closed,
                                         connection, reason, text});
    return result;
  }

  void ProcessStatuses() {
    while (!mailbox.statuses.empty()) {
      auto status = mailbox.statuses.front();
      mailbox.statuses.pop_front();
      const auto connection = status.m_hConn;
      if (status.m_info.m_eState == k_ESteamNetworkingConnectionState_Connecting &&
          status.m_info.m_hListenSocket == listener && listener && peers.find(connection) == peers.end()) {
        const auto ip = AddressString(status.m_info.m_addrRemote);
        CloseReason rejected = CloseReason::None;
        try {
          if (admission_check) rejected = admission_check(ip);
        } catch (...) {
          SharedRuntime().sockets->CloseConnection(connection, static_cast<int>(CloseReason::Failure), "Admission check failed", false);
          throw;
        }
        if (rejected == CloseReason::None && peers.size() >= slots) rejected = CloseReason::ServerFull;
        if (rejected != CloseReason::None) {
          SharedRuntime().sockets->CloseConnection(connection, static_cast<int>(rejected), ReasonText(rejected), false);
          continue;
        }

        // Reserve the slot before accepting, counting accepted handshakes as well
        // as fully connected peers. All admission work runs on the server thread.
        peers.emplace(connection, Peer{ip, {}, false, {}});
        SharedRuntime().connections.emplace(connection, &mailbox);
        if (!ConfigureLanes(connection) || !SharedRuntime().sockets->SetConnectionPollGroup(connection, poll_group)) {
          Close(connection, CloseReason::Failure, "Unable to assign connection resources", false);
          continue;
        }
        const auto result = SharedRuntime().sockets->AcceptConnection(connection);
        if (result != k_EResultOK) {
          ReportError("AcceptConnection", connection, result);
          Close(connection, CloseReason::Failure, "Unable to accept connection", false);
        }
      } else if (auto peer = peers.find(connection); peer != peers.end()) {
        if (status.m_info.m_eState == k_ESteamNetworkingConnectionState_Connected && !peer->second.connected) {
          peer->second.identity = MachineIdentityString(status.m_info.m_identityRemote);
          peer->second.connected = true;
          mailbox.events.push_back({ConnectionEvent::Kind::Connected, connection, CloseReason::None, {}});
        } else if (TerminalState(status.m_info.m_eState)) {
          const bool notify = peer->second.connected;
          SharedRuntime().sockets->CloseConnection(connection, 0, nullptr, false);
          SharedRuntime().connections.erase(connection);
          peers.erase(peer);
          if (notify) mailbox.events.push_back(TerminalEvent(status));
        }
      } else if (TerminalState(status.m_info.m_eState)) {
        // A peer may disappear before its connecting callback is processed.
        // CloseConnection also releases these never-admitted native handles.
        SharedRuntime().sockets->CloseConnection(connection, 0, nullptr, false);
      }
    }
  }

  void Stop(bool notify) {
    if (!listener && !poll_group) return;
    SharedRuntime().sockets->RunCallbacks();
    for (const auto& status : mailbox.statuses) {
      if (status.m_info.m_eState == k_ESteamNetworkingConnectionState_Connecting && peers.find(status.m_hConn) == peers.end()) {
        SharedRuntime().sockets->CloseConnection(status.m_hConn, static_cast<int>(CloseReason::Shutdown), ReasonText(CloseReason::Shutdown), false);
      }
    }
    mailbox.statuses.clear();
    while (!peers.empty()) Close(peers.begin()->first, CloseReason::Shutdown, {}, notify);
    if (listener) {
      SharedRuntime().listeners.erase(listener);
      SharedRuntime().sockets->CloseListenSocket(listener);
      listener = k_HSteamListenSocket_Invalid;
    }
    if (poll_group) {
      SharedRuntime().sockets->DestroyPollGroup(poll_group);
      poll_group = k_HSteamNetPollGroup_Invalid;
    }
    port = 0;
    address.clear();
    has_pending_messages = false;
  }
};

ServerTransport::ServerTransport() : impl_(std::make_unique<Impl>()) {}

ServerTransport::~ServerTransport() {
  std::lock_guard<std::recursive_mutex> lock(SharedRuntime().mutex);
  impl_->Stop(false);
}

void ServerTransport::SetCallbacks(MessageCallback message, EventCallback event) {
  std::lock_guard<std::recursive_mutex> lock(SharedRuntime().mutex);
  impl_->message_callback = std::move(message);
  impl_->event_callback = std::move(event);
}

void ServerTransport::SetAdmissionCheck(AdmissionCheck check) {
  std::lock_guard<std::recursive_mutex> lock(SharedRuntime().mutex);
  impl_->admission_check = std::move(check);
}

bool ServerTransport::Start(std::uint32_t port, std::uint32_t slots) {
  if (port == 0 || port > 65535 || slots == 0) return false;
  std::lock_guard<std::recursive_mutex> lock(SharedRuntime().mutex);
  if (impl_->listener) return false;
  if (!impl_->runtime) impl_->runtime = std::make_unique<RuntimeReference>();
  impl_->poll_group = SharedRuntime().sockets->CreatePollGroup();
  if (!impl_->poll_group) return false;
  SteamNetworkingIPAddr local;
  local.Clear();
  local.m_port = static_cast<uint16>(port);
  auto options = ConnectionOptions();
  impl_->listener = SharedRuntime().sockets->CreateListenSocketIP(local, static_cast<int>(options.size()), options.data());
  if (!impl_->listener) {
    impl_->Stop(false);
    return false;
  }
  SharedRuntime().listeners.emplace(impl_->listener, &impl_->mailbox);
  impl_->slots = slots;
  if (!SharedRuntime().sockets->GetListenSocketAddress(impl_->listener, &local)) {
    impl_->Stop(false);
    return false;
  }
  impl_->port = local.m_port;
  impl_->address = AddressString(local);
  return true;
}

void ServerTransport::Stop() {
  std::lock_guard<std::recursive_mutex> lock(SharedRuntime().mutex);
  impl_->Stop(true);
}

void ServerTransport::Pulse() {
  std::deque<ConnectionEvent> events;
  EventCallback event_callback;
  {
    std::lock_guard<std::recursive_mutex> lock(SharedRuntime().mutex);
    if (impl_->runtime) SharedRuntime().sockets->RunCallbacks();
    impl_->ProcessStatuses();
    events.swap(impl_->mailbox.events);
    event_callback = impl_->event_callback;
  }
  for (const auto& event : events) {
    if (event_callback) event_callback(event);
  }

  for (int received = 0; received < 1024; ++received) {
    Message message;
    MessageCallback callback;
    {
      std::lock_guard<std::recursive_mutex> lock(SharedRuntime().mutex);
      if (!impl_->poll_group) break;
      if (impl_->has_pending_messages) {
        bool still_pending = false;
        for (auto& entry : impl_->peers) {
          still_pending = still_pending || !entry.second.pending_messages.empty();
          if (entry.second.connected && !entry.second.pending_messages.empty()) {
            message = std::move(entry.second.pending_messages.front());
            entry.second.pending_messages.pop_front();
            break;
          }
        }
        impl_->has_pending_messages = still_pending;
      }
      if (!message) {
        SteamNetworkingMessage_t* raw_message = nullptr;
        const int count = SharedRuntime().sockets->ReceiveMessagesOnPollGroup(impl_->poll_group, &raw_message, 1);
        if (count <= 0) break;
        message.reset(raw_message);
      }
      auto peer = impl_->peers.find(message->m_conn);
      if (peer == impl_->peers.end()) continue;
      if (!peer->second.connected) {
        // The networking thread can deliver a message before the corresponding
        // Connected callback reaches this Pulse. Keep it until gameplay has
        // received Connected instead of discarding a reliable initial request.
        peer->second.pending_messages.push_back(std::move(message));
        impl_->has_pending_messages = true;
        continue;
      }
      callback = impl_->message_callback;
    }
    if (callback) callback(message->m_conn, static_cast<unsigned char*>(message->m_pData), static_cast<std::uint32_t>(message->m_cbSize));
  }
}

bool ServerTransport::Send(const void* data, std::uint32_t size, TrafficClass traffic, ConnectionHandle connection) {
  std::lock_guard<std::recursive_mutex> lock(SharedRuntime().mutex);
  const auto peer = impl_->peers.find(connection);
  return peer != impl_->peers.end() && peer->second.connected && SendMessage(connection, data, size, traffic);
}

bool ServerTransport::CloseConnection(ConnectionHandle connection, CloseReason reason, const std::string& detail) {
  std::lock_guard<std::recursive_mutex> lock(SharedRuntime().mutex);
  return impl_->Close(connection, reason, detail, true);
}

std::string ServerTransport::GetPlayerIp(ConnectionHandle connection) const {
  std::lock_guard<std::recursive_mutex> lock(SharedRuntime().mutex);
  const auto peer = impl_->peers.find(connection);
  return peer == impl_->peers.end() ? std::string{} : peer->second.ip;
}

std::string ServerTransport::GetPlayerIdentity(ConnectionHandle connection) const {
  std::lock_guard<std::recursive_mutex> lock(SharedRuntime().mutex);
  const auto peer = impl_->peers.find(connection);
  return peer == impl_->peers.end() ? std::string{} : peer->second.identity;
}

std::int32_t ServerTransport::GetPing(ConnectionHandle connection) const { return GetNetworkStats(connection).ping_ms; }

NetworkStats ServerTransport::GetNetworkStats(ConnectionHandle connection) const {
  std::lock_guard<std::recursive_mutex> lock(SharedRuntime().mutex);
  return impl_->peers.find(connection) == impl_->peers.end() ? NetworkStats{} : ReadStats(connection);
}

std::string ServerTransport::GetDiagnostics(ConnectionHandle connection) const {
  std::lock_guard<std::recursive_mutex> lock(SharedRuntime().mutex);
  return impl_->peers.find(connection) == impl_->peers.end() ? std::string{} : ReadDiagnostics(connection);
}

std::uint32_t ServerTransport::GetPort() const {
  std::lock_guard<std::recursive_mutex> lock(SharedRuntime().mutex);
  return impl_->port;
}

std::string ServerTransport::GetAddress() const {
  std::lock_guard<std::recursive_mutex> lock(SharedRuntime().mutex);
  return impl_->address;
}

std::size_t ServerTransport::GetConnectionCount() const {
  std::lock_guard<std::recursive_mutex> lock(SharedRuntime().mutex);
  return impl_->peers.size();
}

bool ServerTransport::IsRunning() const {
  std::lock_guard<std::recursive_mutex> lock(SharedRuntime().mutex);
  return impl_->listener != k_HSteamListenSocket_Invalid;
}

}  // namespace Net
