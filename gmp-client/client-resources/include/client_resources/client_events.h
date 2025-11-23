#pragma once

#include <cstdint>
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

struct OnPacketEvent {
  gmp::client::Packet packet;
};

struct OnKeyEvent {
  int key;
};

struct PlayerLifecycleEvent {
  std::uint64_t player_id;
};

} // namespace gmp::client
