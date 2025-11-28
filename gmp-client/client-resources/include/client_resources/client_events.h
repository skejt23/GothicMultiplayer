#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "packet.h"

namespace gmp::client {

constexpr const char* kEventOnInitName = "onInit";
constexpr const char* kEventOnExitName = "onExit";
constexpr const char* kEventOnPacketName = "onPacket";
constexpr const char* kEventOnRenderName = "onRender";
constexpr const char* kEventOnKeyDownName = "onKeyDown";
constexpr const char* kEventOnKeyUpName = "onKeyUp";
constexpr const char* kEventOnPlayerCreateName = "onPlayerCreate";
constexpr const char* kEventOnPlayerDestroyName = "onPlayerDestroy";
constexpr const char* kEventOnPlayerMessageName = "onPlayerMessage";

struct OnPacketEvent {
  gmp::client::Packet packet;
};

struct OnKeyEvent {
  int key;
};

struct PlayerLifecycleEvent {
  std::uint64_t player_id;
};

struct OnPlayerMessageEvent {
  std::optional<std::uint64_t> sender_id;
  std::uint8_t r;
  std::uint8_t g;
  std::uint8_t b;
  std::string message;
};

} // namespace gmp::client
