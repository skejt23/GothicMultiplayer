#include <gtest/gtest.h>
#include <steam/isteamnetworkingutils.h>
#include <steam/steamnetworkingsockets.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "network_test_port.h"
#include "transport/machine_identity.h"
#include "transport/transport.h"

namespace {
using namespace std::chrono_literals;
using Event = Net::ConnectionEvent;

// Restore process-global simulation settings even when a fatal assertion exits
// a test. The fixture's server keeps the GNS runtime alive during restoration.
class ScopedNetworkSettings {
public:
  ~ScopedNetworkSettings() {
    for (auto it = previous_.rbegin(); it != previous_.rend(); ++it) {
      SteamNetworkingUtils()->SetConfigValueStruct(*it, k_ESteamNetworkingConfig_Global, 0);
    }
  }

  bool Float(ESteamNetworkingConfigValue key, float value) {
    float previous = 0;
    std::size_t size = sizeof(previous);
    if (SteamNetworkingUtils()->GetConfigValue(key, k_ESteamNetworkingConfig_Global, 0, nullptr, &previous, &size) <= 0) return false;
    SteamNetworkingConfigValue_t saved;
    saved.SetFloat(key, previous);
    previous_.push_back(saved);
    return SteamNetworkingUtils()->SetGlobalConfigValueFloat(key, value);
  }

  bool Int(ESteamNetworkingConfigValue key, std::int32_t value) {
    std::int32_t previous = 0;
    std::size_t size = sizeof(previous);
    if (SteamNetworkingUtils()->GetConfigValue(key, k_ESteamNetworkingConfig_Global, 0, nullptr, &previous, &size) <= 0) return false;
    SteamNetworkingConfigValue_t saved;
    saved.SetInt32(key, previous);
    previous_.push_back(saved);
    return SteamNetworkingUtils()->SetGlobalConfigValueInt32(key, value);
  }

private:
  std::vector<SteamNetworkingConfigValue_t> previous_;
};

struct ClientProbe {
  std::vector<Event> events;
  std::vector<std::string> messages;
  Net::ClientTransport transport;

  ClientProbe() {
    transport.SetCallbacks(
        [this](unsigned char* bytes, std::uint32_t size) { messages.emplace_back(reinterpret_cast<char*>(bytes), size); },
        [this](const Event& event) { events.push_back(event); });
  }

  std::size_t Count(Event::Kind kind) const {
    return static_cast<std::size_t>(std::count_if(events.begin(), events.end(), [kind](const auto& event) { return event.kind == kind; }));
  }
};

class TransportTest : public ::testing::Test {
protected:
  struct Received {
    Net::ConnectionHandle connection;
    std::string payload;
  };

  std::vector<Event> server_events;
  std::vector<Received> server_messages;
  Net::ServerTransport server;
  std::vector<std::unique_ptr<ClientProbe>> clients;

  void SetUp() override {
    server.SetCallbacks(
        [this](Net::ConnectionHandle connection, unsigned char* bytes, std::uint32_t size) {
          server_messages.push_back({connection, std::string(reinterpret_cast<char*>(bytes), size)});
        },
        [this](const Event& event) { server_events.push_back(event); });
    ASSERT_TRUE(StartTestListener(server));
  }

  void TearDown() override {
    for (auto& client : clients) client->transport.Disconnect();
    server.Stop();
  }

  ClientProbe& NewClient() {
    clients.push_back(std::make_unique<ClientProbe>());
    return *clients.back();
  }

  void Pump() {
    server.Pulse();
    for (auto& client : clients) client->transport.Pulse();
  }

  template <typename Predicate>
  bool PumpUntil(Predicate predicate, std::chrono::milliseconds timeout = 5s) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    do {
      Pump();
      if (predicate()) return true;
      std::this_thread::sleep_for(1ms);
    } while (std::chrono::steady_clock::now() < deadline);
    return predicate();
  }

  std::size_t AcceptedCount() const {
    return static_cast<std::size_t>(std::count_if(server_events.begin(), server_events.end(), [](const auto& event) {
      return event.kind == Event::Kind::Connected;
    }));
  }

  bool Connect(ClientProbe& client) {
    const auto accepted_before = AcceptedCount();
    return client.transport.Connect("127.0.0.1", server.GetPort()) &&
           PumpUntil([&] { return client.transport.IsConnected() && AcceptedCount() > accepted_before; });
  }

  Net::ConnectionHandle LastAccepted() const {
    for (auto it = server_events.rbegin(); it != server_events.rend(); ++it) {
      if (it->kind == Event::Kind::Connected) return it->connection;
    }
    return Net::kInvalidConnection;
  }
};

TEST_F(TransportTest, ConnectsAndReportsIpPingAndNativeDiagnostics) {
  auto& client = NewClient();
  ASSERT_TRUE(Connect(client));
  EXPECT_EQ(client.Count(Event::Kind::Connected), 1u);
  EXPECT_EQ(server.GetConnectionCount(), 1u);
  EXPECT_EQ(server.GetPlayerIp(LastAccepted()), "127.0.0.1");
  EXPECT_TRUE(client.transport.GetNetworkStats().available);
  EXPECT_TRUE(server.GetNetworkStats(LastAccepted()).available);
  EXPECT_GE(client.transport.GetPing(), -1);
  EXPECT_GE(server.GetPing(LastAccepted()), -1);
  EXPECT_FALSE(client.transport.GetDiagnostics().empty());
  EXPECT_FALSE(server.GetDiagnostics(LastAccepted()).empty());
}

TEST_F(TransportTest, ConnectsEncryptedWithoutLocalhostAuthenticationExemption) {
  ScopedNetworkSettings settings;
  // GNS normally permits unsigned certificates on localhost regardless of the
  // regular IP policy. Disable that exemption to exercise the VPS auth path.
  ASSERT_TRUE(settings.Int(k_ESteamNetworkingConfig_IPLocalHost_AllowWithoutAuth, 0));
  server.Stop();
  ASSERT_TRUE(StartTestListener(server));

  auto& client = NewClient();
  ASSERT_TRUE(Connect(client));
  EXPECT_EQ(server.GetPlayerIdentity(LastAccepted()).size(), Net::kMachineIdentityBytes * 2);
  for (const auto connection : {client.transport.GetConnection(), LastAccepted()}) {
    SteamNetConnectionInfo_t info{};
    ASSERT_TRUE(SteamNetworkingSockets()->GetConnectionInfo(connection, &info));
    EXPECT_NE(info.m_nFlags & k_nSteamNetworkConnectionInfoFlags_Unauthenticated, 0);
    EXPECT_EQ(info.m_nFlags & k_nSteamNetworkConnectionInfoFlags_Unencrypted, 0);
    EXPECT_EQ(info.m_nFlags & k_nSteamNetworkConnectionInfoFlags_LoopbackBuffers, 0);
  }
}

TEST(MachineIdentityTest, NormalizesOsIdentifiersAndPreservesThePublishedDerivation) {
  const auto identity = Net::DeriveMachineIdentity("00112233-4455-6677-8899-aabbccddeeff");
  ASSERT_TRUE(identity.has_value());
  // Pin the derivation: changing the domain, encoding, or algorithm would
  // change every player's persisted identity, even on an unchanged machine.
  constexpr Net::MachineIdentity expected{
      0xdc, 0xd6, 0xbb, 0xe8, 0xe9, 0x34, 0xbe, 0x31, 0xc5, 0xf2, 0x66, 0xbc, 0x18, 0x5d, 0xca, 0x02,
      0x08, 0x94, 0x3f, 0xab, 0x29, 0x83, 0xe6, 0xa5, 0x14, 0x26, 0x5d, 0xe0, 0xec, 0x1a, 0xb7, 0x24};
  EXPECT_EQ(*identity, expected);
  EXPECT_EQ(Net::DeriveMachineIdentity("00112233445566778899aabbccddeeff\n"), identity);
  EXPECT_EQ(Net::DeriveMachineIdentity(" {00112233-4455-6677-8899-AABBCCDDEEFF}\r\n"), identity);
  EXPECT_NE(Net::DeriveMachineIdentity("00112233445566778899aabbccddeefe"), identity);
}

TEST(MachineIdentityTest, RejectsMissingUninitializedAndMalformedOsIdentifiers) {
  for (const auto* invalid : {"", " \r\n", "uninitialized", "00000000000000000000000000000000",
                              "00000000-0000-0000-0000-000000000000", "00112233445566778899aabbccddeef",
                              "00112233445566778899aabbccddeeff0", "00112233445566778899aabbccddeefg",
                              "00112233_4455-6677-8899-aabbccddeeff", "00112233-4455-6677-8899-aabbccddee f"}) {
    EXPECT_FALSE(Net::DeriveMachineIdentity(invalid).has_value()) << invalid;
  }
}

TEST_F(TransportTest, ReportsTheIdentityFromTheEncryptedGnsHandshake) {
  auto& client = NewClient();
  ASSERT_TRUE(Connect(client));
  const auto text = server.GetPlayerIdentity(LastAccepted());
  ASSERT_EQ(text.size(), Net::kMachineIdentityBytes * 2);
  ASSERT_TRUE(std::all_of(text.begin(), text.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }));

  SteamNetConnectionInfo_t info{};
  ASSERT_TRUE(SteamNetworkingSockets()->GetConnectionInfo(LastAccepted(), &info));
  int size = 0;
  const auto* bytes = info.m_identityRemote.GetGenericBytes(size);
  ASSERT_NE(bytes, nullptr);
  ASSERT_EQ(size, Net::kMachineIdentityBytes);
  for (int i = 0; i < size; ++i) {
    EXPECT_EQ(std::stoul(text.substr(i * 2, 2), nullptr, 16), bytes[i]);
  }
  // Certificate transport is not a claim of externally authenticated hardware.
  EXPECT_NE(info.m_nFlags & k_nSteamNetworkConnectionInfoFlags_Unauthenticated, 0);
  EXPECT_EQ(info.m_nFlags & k_nSteamNetworkConnectionInfoFlags_Unencrypted, 0);
}

TEST_F(TransportTest, MachineIdentitySurvivesReconnectAndChangingServers) {
  auto& client = NewClient();
  ASSERT_TRUE(Connect(client));
  const auto first_connection = LastAccepted();
  const auto identity = server.GetPlayerIdentity(first_connection);
  ASSERT_EQ(identity.size(), Net::kMachineIdentityBytes * 2);
  client.transport.Disconnect();
  ASSERT_TRUE(PumpUntil([&] { return server.GetConnectionCount() == 0; }));
  EXPECT_TRUE(server.GetPlayerIdentity(first_connection).empty());
  ASSERT_TRUE(Connect(client));
  EXPECT_NE(LastAccepted(), first_connection);
  EXPECT_EQ(server.GetPlayerIdentity(LastAccepted()), identity);

  // Distinct sessions on the same OS installation deliberately share identity.
  auto& second_client = NewClient();
  ASSERT_TRUE(Connect(second_client));
  EXPECT_EQ(server.GetPlayerIdentity(LastAccepted()), identity);
  EXPECT_EQ(server.GetConnectionCount(), 2u);

  Net::ServerTransport other_server;
  Net::ConnectionHandle other_connection = Net::kInvalidConnection;
  other_server.SetCallbacks({}, [&](const Event& event) {
    if (event.kind == Event::Kind::Connected) other_connection = event.connection;
  });
  ASSERT_TRUE(StartTestListener(other_server));
  client.transport.Disconnect();
  ASSERT_TRUE(PumpUntil([&] { return server.GetConnectionCount() == 1; }));
  ASSERT_TRUE(client.transport.Connect("127.0.0.1", other_server.GetPort()));
  ASSERT_TRUE(PumpUntil([&] {
    other_server.Pulse();
    return client.transport.IsConnected() && other_connection != Net::kInvalidConnection;
  }));
  EXPECT_EQ(other_server.GetPlayerIdentity(other_connection), identity);
}

TEST_F(TransportTest, RejectsAllButOneConcurrentHandshakeWhenFull) {
  server.Stop();
  ASSERT_TRUE(StartTestListener(server, 1));
  for (int i = 0; i < 8; ++i) {
    auto& client = NewClient();
    ASSERT_TRUE(client.transport.Connect("127.0.0.1", server.GetPort()));
  }
  ASSERT_TRUE(PumpUntil([&] {
    // GetConnectionCount includes reserved handshakes, not only connected peers.
    EXPECT_LE(server.GetConnectionCount(), 1u);
    // The client can observe Connected one pump before the server callback.
    return AcceptedCount() == 1 &&
           std::all_of(clients.begin(), clients.end(), [](const auto& client) { return !client->events.empty(); });
  }));
  EXPECT_EQ(AcceptedCount(), 1u);
  std::size_t accepted = 0;
  std::size_t rejected = 0;
  for (const auto& client : clients) {
    if (client->transport.IsConnected()) {
      ++accepted;
    } else {
      ++rejected;
      ASSERT_FALSE(client->events.empty());
      EXPECT_EQ(client->events.back().kind, Event::Kind::Closed);
      EXPECT_EQ(client->events.back().reason, Net::CloseReason::ServerFull);
      EXPECT_EQ(client->transport.GetConnection(), Net::kInvalidConnection);
    }
  }
  EXPECT_EQ(accepted, 1u);
  EXPECT_EQ(rejected, 7u);
}

TEST_F(TransportTest, RejectsBannedIpBeforeAcceptingAndCanAdmitAfterUnban) {
  std::vector<std::string> checked_ips;
  server.SetAdmissionCheck([&](const std::string& ip) {
    checked_ips.push_back(ip);
    return ip == "127.0.0.1" ? Net::CloseReason::Banned : Net::CloseReason::None;
  });
  auto& client = NewClient();
  ASSERT_TRUE(client.transport.Connect("127.0.0.1", server.GetPort()));
  ASSERT_TRUE(PumpUntil([&] { return !client.events.empty(); }));
  EXPECT_EQ(checked_ips, std::vector<std::string>{"127.0.0.1"});
  EXPECT_EQ(client.events.back().reason, Net::CloseReason::Banned);
  EXPECT_EQ(client.Count(Event::Kind::Connected), 0u);
  EXPECT_EQ(AcceptedCount(), 0u);
  EXPECT_EQ(server.GetConnectionCount(), 0u);
  server.SetAdmissionCheck({});
  ASSERT_TRUE(Connect(client));
}

TEST_F(TransportTest, SendsAndReceivesEveryDeliveryClassInBothDirections) {
  auto& client = NewClient();
  ASSERT_TRUE(Connect(client));
  const Net::TrafficClass classes[] = {Net::TrafficClass::Control, Net::TrafficClass::State, Net::TrafficClass::Voice};
  std::vector<std::string> expected;
  for (int i = 0; i < 3; ++i) {
    expected.push_back("class-" + std::to_string(i));
    const auto& payload = expected.back();
    ASSERT_TRUE(client.transport.Send(payload.data(), static_cast<std::uint32_t>(payload.size()), classes[i]));
    ASSERT_TRUE(server.Send(payload.data(), static_cast<std::uint32_t>(payload.size()), classes[i], LastAccepted()));
  }
  ASSERT_TRUE(PumpUntil([&] { return client.messages.size() == 3 && server_messages.size() == 3; }));
  for (const auto& payload : expected) {
    EXPECT_EQ(std::count(client.messages.begin(), client.messages.end(), payload), 1);
    EXPECT_EQ(std::count_if(server_messages.begin(), server_messages.end(), [&](const auto& message) {
      return message.connection == LastAccepted() && message.payload == payload;
    }), 1);
  }
}

TEST_F(TransportTest, ReliableMessagesRemainOrderedWithSimulatedLossAndLatency) {
  auto& client = NewClient();
  ASSERT_TRUE(Connect(client));
  ScopedNetworkSettings settings;
  ASSERT_TRUE(settings.Float(k_ESteamNetworkingConfig_FakePacketLoss_Send, 20.0f));
  ASSERT_TRUE(settings.Int(k_ESteamNetworkingConfig_FakePacketLag_Send, 15));
  constexpr std::size_t count = 80;
  std::vector<std::string> expected;
  for (std::size_t i = 0; i < count; ++i) {
    expected.push_back(std::to_string(i) + std::string(1024, 'x'));
    const auto& payload = expected.back();
    ASSERT_TRUE(client.transport.Send(payload.data(), static_cast<std::uint32_t>(payload.size()), Net::TrafficClass::Control));
    ASSERT_TRUE(server.Send(payload.data(), static_cast<std::uint32_t>(payload.size()), Net::TrafficClass::Control, LastAccepted()));
  }
  ASSERT_TRUE(PumpUntil([&] { return client.messages.size() == count && server_messages.size() == count; }, 10s));
  EXPECT_EQ(client.messages, expected);
  for (std::size_t i = 0; i < count; ++i) EXPECT_EQ(server_messages[i].payload, expected[i]);
}

TEST_F(TransportTest, ClientDisconnectIsGracefulAndReleasesItsSlot) {
  auto& client = NewClient();
  ASSERT_TRUE(Connect(client));
  const auto server_connection = LastAccepted();
  client.transport.Disconnect();
  ASSERT_TRUE(PumpUntil([&] { return server.GetConnectionCount() == 0 && client.Count(Event::Kind::Closed) == 1; }));
  EXPECT_FALSE(client.transport.IsConnected());
  EXPECT_EQ(client.transport.GetConnection(), Net::kInvalidConnection);
  ASSERT_FALSE(server_events.empty());
  EXPECT_EQ(server_events.back().connection, server_connection);
  EXPECT_EQ(server_events.back().kind, Event::Kind::Closed);
  EXPECT_EQ(server_events.back().reason, Net::CloseReason::Normal);
  EXPECT_FALSE(server.GetNetworkStats(server_connection).available);
  EXPECT_TRUE(server.GetPlayerIp(server_connection).empty());
}

TEST_F(TransportTest, KickAndShutdownHaveDistinctApplicationReasons) {
  auto& client = NewClient();
  ASSERT_TRUE(Connect(client));
  ASSERT_TRUE(server.CloseConnection(LastAccepted(), Net::CloseReason::Kicked, "test kick"));
  ASSERT_TRUE(PumpUntil([&] { return client.Count(Event::Kind::Closed) == 1; }));
  EXPECT_EQ(client.events.back().reason, Net::CloseReason::Kicked);
  EXPECT_EQ(client.events.back().detail, "test kick");
  ASSERT_TRUE(Connect(client));
  server.Stop();
  ASSERT_TRUE(PumpUntil([&] { return client.Count(Event::Kind::Closed) == 2; }));
  EXPECT_EQ(client.events.back().reason, Net::CloseReason::Shutdown);
  EXPECT_FALSE(server.IsRunning());
  EXPECT_EQ(server.GetConnectionCount(), 0u);
}

TEST_F(TransportTest, PacketBlackholeProducesUnexpectedConnectionLoss) {
  ScopedNetworkSettings settings;
  ASSERT_TRUE(settings.Int(k_ESteamNetworkingConfig_TimeoutConnected, 1000));
  auto& client = NewClient();
  ASSERT_TRUE(Connect(client));
  ASSERT_TRUE(settings.Float(k_ESteamNetworkingConfig_FakePacketLoss_Send, 100.0f));
  ASSERT_TRUE(settings.Float(k_ESteamNetworkingConfig_FakePacketLoss_Recv, 100.0f));
  const std::string payload = "force liveness check";
  ASSERT_TRUE(client.transport.Send(payload.data(), static_cast<std::uint32_t>(payload.size()), Net::TrafficClass::Control));
  ASSERT_TRUE(server.Send(payload.data(), static_cast<std::uint32_t>(payload.size()), Net::TrafficClass::Control, LastAccepted()));
  ASSERT_TRUE(PumpUntil([&] { return client.Count(Event::Kind::Failure) == 1 && server.GetConnectionCount() == 0; }, 8s));
  EXPECT_EQ(client.events.back().reason, Net::CloseReason::Failure);
  EXPECT_FALSE(client.transport.IsConnected());
  EXPECT_EQ(server_events.back().kind, Event::Kind::Failure);
}

TEST_F(TransportTest, ReconnectDoesNotDispatchUndrainedEventsFromPreviousConnection) {
  auto& client = NewClient();
  ASSERT_TRUE(Connect(client));
  const auto previous = client.transport.GetConnection();
  client.transport.Disconnect();
  // Deliberately do not Pulse between local disconnect and reconnect.
  client.events.clear();
  ASSERT_TRUE(Connect(client));
  EXPECT_NE(client.transport.GetConnection(), previous);
  ASSERT_EQ(client.events.size(), 1u);
  EXPECT_EQ(client.events.front().kind, Event::Kind::Connected);
  ASSERT_TRUE(PumpUntil([&] { return server.GetConnectionCount() == 1; }));
}

TEST_F(TransportTest, StopReleasesConnectionsAndPollGroupAndAllowsPortReuse) {
  auto& client = NewClient();
  ASSERT_TRUE(Connect(client));
  const auto port = server.GetPort();
  const auto closed_connection = LastAccepted();
  server.Stop();
  server.Stop();
  ASSERT_TRUE(PumpUntil([&] { return !client.transport.IsConnected(); }));
  EXPECT_EQ(server.GetConnectionCount(), 0u);
  EXPECT_EQ(server.GetPort(), 0u);
  EXPECT_TRUE(server.GetAddress().empty());
  EXPECT_FALSE(server.CloseConnection(closed_connection));
  ASSERT_TRUE(server.Start(port, 2));
  ASSERT_TRUE(Connect(client));
  const std::string payload = "new poll group";
  ASSERT_TRUE(client.transport.Send(payload.data(), static_cast<std::uint32_t>(payload.size()), Net::TrafficClass::Control));
  ASSERT_TRUE(PumpUntil([&] { return !server_messages.empty(); }));
  EXPECT_EQ(server_messages.back().payload, payload);
  EXPECT_NE(server_messages.back().connection, closed_connection);
}

TEST_F(TransportTest, SendFailuresAreReportedForInvalidAndClosedConnections) {
  auto& client = NewClient();
  const char payload = 'x';
  EXPECT_FALSE(client.transport.Send(&payload, 1, Net::TrafficClass::Control));
  EXPECT_FALSE(server.Send(&payload, 1, Net::TrafficClass::Control, Net::kInvalidConnection));
  ASSERT_TRUE(Connect(client));
  const auto connection = LastAccepted();
  EXPECT_FALSE(client.transport.Send(nullptr, 1, Net::TrafficClass::Control));
  EXPECT_FALSE(client.transport.Send(&payload, 0, Net::TrafficClass::Control));
  EXPECT_FALSE(client.transport.Send(&payload, 1, static_cast<Net::TrafficClass>(99)));
  client.transport.Disconnect();
  ASSERT_TRUE(PumpUntil([&] { return server.GetConnectionCount() == 0; }));
  EXPECT_FALSE(client.transport.Send(&payload, 1, Net::TrafficClass::Voice));
  EXPECT_FALSE(server.Send(&payload, 1, Net::TrafficClass::State, connection));
}

TEST_F(TransportTest, CallbacksAndAdmissionRunOnTheirEndpointsPulseThread) {
  auto& client = NewClient();
  const auto client_thread = std::this_thread::get_id();
  std::thread::id server_thread;
  std::atomic<bool> wrong_thread{false};
  std::atomic<int> admissions{0};
  std::atomic<int> accepted{0};
  std::atomic<int> echoed{0};
  bool connected = false;
  bool received = false;
  server.SetAdmissionCheck([&](const std::string&) {
    if (std::this_thread::get_id() != server_thread) wrong_thread = true;
    ++admissions;
    return Net::CloseReason::None;
  });
  server.SetCallbacks(
      [&](Net::ConnectionHandle connection, unsigned char* bytes, std::uint32_t size) {
        if (std::this_thread::get_id() != server_thread) wrong_thread = true;
        if (server.Send(bytes, size, Net::TrafficClass::Control, connection)) ++echoed;
      },
      [&](const Event& event) {
        if (std::this_thread::get_id() != server_thread) wrong_thread = true;
        if (event.kind == Event::Kind::Connected) ++accepted;
      });
  client.transport.SetCallbacks(
      [&](unsigned char* bytes, std::uint32_t size) {
        if (std::this_thread::get_id() != client_thread) wrong_thread = true;
        received = std::string(reinterpret_cast<char*>(bytes), size) == "thread ownership";
      },
      [&](const Event& event) {
        if (std::this_thread::get_id() != client_thread) wrong_thread = true;
        connected = event.kind == Event::Kind::Connected;
      });
  ASSERT_TRUE(client.transport.Connect("127.0.0.1", server.GetPort()));

  // Pumping the shared native interface from the client must only queue the
  // server's incoming connection. Admission is server gameplay, not a GNS callback.
  const auto unpumped_deadline = std::chrono::steady_clock::now() + 50ms;
  while (std::chrono::steady_clock::now() < unpumped_deadline) {
    client.transport.Pulse();
    std::this_thread::sleep_for(1ms);
  }
  EXPECT_EQ(admissions.load(), 0);
  EXPECT_EQ(accepted.load(), 0);

  std::atomic<bool> stop{false};
  std::thread worker([&] {
    server_thread = std::this_thread::get_id();
    while (!stop) {
      server.Pulse();
      std::this_thread::sleep_for(1ms);
    }
  });
  // No fatal assertions while the worker is joinable.
  bool sent = false;
  const auto deadline = std::chrono::steady_clock::now() + 5s;
  while (!received && std::chrono::steady_clock::now() < deadline) {
    client.transport.Pulse();
    if (connected && !sent) {
      const std::string payload = "thread ownership";
      sent = client.transport.Send(payload.data(), static_cast<std::uint32_t>(payload.size()), Net::TrafficClass::Control);
    }
    std::this_thread::sleep_for(1ms);
  }
  stop = true;
  worker.join();
  // Captures above are local; the fixture calls Stop after they are destroyed.
  server.SetCallbacks({}, {});
  server.SetAdmissionCheck({});
  client.transport.SetCallbacks({}, {});
  EXPECT_FALSE(wrong_thread.load());
  EXPECT_EQ(admissions.load(), 1);
  EXPECT_EQ(accepted.load(), 1);
  EXPECT_EQ(echoed.load(), 1);
  EXPECT_TRUE(sent);
  EXPECT_TRUE(received);
}

TEST_F(TransportTest, NativeSendBufferLimitReturnsFailureAndConnectionRemainsUsable) {
  auto& client = NewClient();
  ASSERT_TRUE(Connect(client));
  ASSERT_TRUE(SteamNetworkingUtils()->SetConnectionConfigValueInt32(
      client.transport.GetConnection(), k_ESteamNetworkingConfig_SendBufferSize, 4096));
  ASSERT_TRUE(SteamNetworkingUtils()->SetConnectionConfigValueInt32(
      LastAccepted(), k_ESteamNetworkingConfig_SendBufferSize, 4096));
  const std::string oversized(8192, 'x');
  EXPECT_FALSE(client.transport.Send(oversized.data(), static_cast<std::uint32_t>(oversized.size()), Net::TrafficClass::Control));
  EXPECT_FALSE(server.Send(oversized.data(), static_cast<std::uint32_t>(oversized.size()), Net::TrafficClass::Control, LastAccepted()));
  const std::string valid = "still connected";
  ASSERT_TRUE(client.transport.Send(valid.data(), static_cast<std::uint32_t>(valid.size()), Net::TrafficClass::Control));
  ASSERT_TRUE(server.Send(valid.data(), static_cast<std::uint32_t>(valid.size()), Net::TrafficClass::Control, LastAccepted()));
  ASSERT_TRUE(PumpUntil([&] { return !client.messages.empty() && !server_messages.empty(); }));
  EXPECT_EQ(client.messages, std::vector<std::string>{valid});
  ASSERT_EQ(server_messages.size(), 1u);
  EXPECT_EQ(server_messages.front().payload, valid);
}

TEST_F(TransportTest, AdmissionExceptionRejectsHandshakeAndDoesNotReserveSlot) {
  server.SetAdmissionCheck([](const std::string&) -> Net::CloseReason { throw std::runtime_error("admission error"); });
  auto& client = NewClient();
  ASSERT_TRUE(client.transport.Connect("127.0.0.1", server.GetPort()));
  bool threw = false;
  const auto deadline = std::chrono::steady_clock::now() + 5s;
  while (!threw && std::chrono::steady_clock::now() < deadline) {
    try {
      Pump();
    } catch (const std::runtime_error&) {
      threw = true;
    }
    std::this_thread::sleep_for(1ms);
  }
  ASSERT_TRUE(threw);
  ASSERT_TRUE(PumpUntil([&] { return client.Count(Event::Kind::Failure) == 1; }));
  EXPECT_EQ(server.GetConnectionCount(), 0u);
  EXPECT_EQ(AcceptedCount(), 0u);
  server.SetAdmissionCheck({});
  ASSERT_TRUE(Connect(client));
}

TEST_F(TransportTest, ReceiveCallbacksCanThrowAndRemainingMessagesStillArrive) {
  auto& client = NewClient();
  ASSERT_TRUE(Connect(client));
  const std::string first = "throw";
  const std::string second = "recover";
  auto verify_recovery = [&](bool server_receives) {
    int callback_count = 0;
    std::string received;
    auto on_message = [&](unsigned char* bytes, std::uint32_t size) {
      if (++callback_count == 1) throw std::runtime_error("message callback error");
      received.assign(reinterpret_cast<char*>(bytes), size);
    };
    if (server_receives) {
      server.SetCallbacks([&](Net::ConnectionHandle, unsigned char* bytes, std::uint32_t size) { on_message(bytes, size); }, {});
      EXPECT_TRUE(client.transport.Send(first.data(), static_cast<std::uint32_t>(first.size()), Net::TrafficClass::Control));
      EXPECT_TRUE(client.transport.Send(second.data(), static_cast<std::uint32_t>(second.size()), Net::TrafficClass::Control));
    } else {
      client.transport.SetCallbacks(on_message, {});
      EXPECT_TRUE(server.Send(first.data(), static_cast<std::uint32_t>(first.size()), Net::TrafficClass::Control, LastAccepted()));
      EXPECT_TRUE(server.Send(second.data(), static_cast<std::uint32_t>(second.size()), Net::TrafficClass::Control, LastAccepted()));
    }
    int exceptions = 0;
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (received.empty() && std::chrono::steady_clock::now() < deadline) {
      try {
        Pump();
      } catch (const std::runtime_error&) {
        ++exceptions;
      }
      std::this_thread::sleep_for(1ms);
    }
    server.SetCallbacks({}, {});
    client.transport.SetCallbacks({}, {});
    EXPECT_EQ(exceptions, 1);
    EXPECT_EQ(callback_count, 2);
    EXPECT_EQ(received, second);
  };
  verify_recovery(true);
  verify_recovery(false);
}

TEST(TransportRuntimeTest, ConstructorsStayDormantAndFinalOwnerShutdownAllowsReinitialization) {
  ASSERT_EQ(SteamNetworkingSockets(), nullptr);
  {
    Net::ClientTransport client;
    Net::ServerTransport server;
    client.Pulse();
    server.Pulse();
    EXPECT_FALSE(client.IsConnected());
    EXPECT_FALSE(client.GetNetworkStats().available);
    EXPECT_FALSE(server.IsRunning());
    EXPECT_EQ(SteamNetworkingSockets(), nullptr);
  }
  SteamNetworkingIdentity previous_identity;
  previous_identity.Clear();
  for (int cycle = 0; cycle < 3; ++cycle) {
    auto server = std::make_unique<Net::ServerTransport>();
    ASSERT_TRUE(StartTestListener(*server));
    ASSERT_NE(SteamNetworkingSockets(), nullptr);
    SteamNetworkingIdentity identity;
    ASSERT_TRUE(SteamNetworkingSockets()->GetIdentity(&identity));
    EXPECT_EQ(identity.m_eType, k_ESteamNetworkingIdentityType_GenericBytes);
    if (cycle > 0) EXPECT_TRUE(identity == previous_identity);
    previous_identity = identity;
    auto client = std::make_unique<Net::ClientTransport>();
    ASSERT_TRUE(client->Connect("127.0.0.1", server->GetPort()));
    server.reset();
    // An endpoint with an active connection attempt retains the shared runtime.
    EXPECT_NE(SteamNetworkingSockets(), nullptr);
    client.reset();
    EXPECT_EQ(SteamNetworkingSockets(), nullptr);
  }
}

}  // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
