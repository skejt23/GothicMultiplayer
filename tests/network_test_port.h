#pragma once

#include <atomic>
#include <cstdint>
#include <stdexcept>

#include "transport/transport.h"

// GNS requires an explicit listen port. Keep the listener when testing the
// transport; GameServer fixtures use a short-lived probe before writing config.
inline bool StartTestListener(Net::ServerTransport& server, std::uint32_t slots = 8) {
  static std::atomic<std::uint32_t> next_port{40000};
  for (int attempt = 0; attempt < 512; ++attempt) {
    const auto port = 40000 + (next_port.fetch_add(1) - 40000) % 20000;
    if (server.Start(port, slots)) return true;
  }
  return false;
}

inline std::uint32_t FindTestServerPort() {
  Net::ServerTransport probe;
  if (!StartTestListener(probe)) throw std::runtime_error("No free test listen port in the test range");
  const auto port = probe.GetPort();
  probe.Stop();
  return port;
}
