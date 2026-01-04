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

#include "sol/sol.hpp"

namespace gmp::gothic {

// Gothic-specific event names
constexpr const char* kEventOnInitName = "onInit";
constexpr const char* kEventOnExitName = "onExit";
constexpr const char* kEventOnRenderName = "onRender";
constexpr const char* kEventOnKeyDownName = "onKeyDown";
constexpr const char* kEventOnKeyUpName = "onKeyUp";
constexpr const char* kEventOnMouseDownName = "onMouseDown";
constexpr const char* kEventOnMouseUpName = "onMouseUp";
constexpr const char* kEventOnMouseMoveName = "onMouseMove";
constexpr const char* kEventOnMouseWheelName = "onMouseWheel";
constexpr const char* kEventOnPlayerCreateName = "onPlayerCreate";
constexpr const char* kEventOnPlayerDestroyName = "onPlayerDestroy";
constexpr const char* kEventOnPlayerMessageName = "onPlayerMessage";

// Gothic-specific event structs
struct OnKeyEvent {
  int key;
};

struct OnMouseButtonEvent {
  int button;
};

struct OnMouseMoveEvent {
  float x;
  float y;
};

struct OnMouseWheelEvent {
  float z;
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

// Bind Gothic-specific events to Lua
void BindGothicEvents(sol::state& lua);

// Reset Gothic-specific events (call when disconnecting/reconnecting)
void ResetGothicEvents();

}  // namespace gmp::gothic
