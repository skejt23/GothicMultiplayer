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

#include "sol/sol.hpp"

namespace gmp::gothic {

// Gothic-specific event names
constexpr const char* kEventOnInitName = "onInit";
constexpr const char* kEventOnExitName = "onExit";
constexpr const char* kEventOnRenderName = "onRender";
constexpr const char* kEventOnTimeName = "onTime";
constexpr const char* kEventOnWeatherChangeName = "onWeatherChange";
constexpr const char* kEventOnKeyDownName = "onKeyDown";
constexpr const char* kEventOnKeyUpName = "onKeyUp";
constexpr const char* kEventOnPasteName = "onPaste";
constexpr const char* kEventOnCommandName = "onCommand";
constexpr const char* kEventOnMouseDownName = "onMouseDown";
constexpr const char* kEventOnMouseUpName = "onMouseUp";
constexpr const char* kEventOnMouseMoveName = "onMouseMove";
constexpr const char* kEventOnMouseWheelName = "onMouseWheel";
constexpr const char* kEventOnOpenInventoryName = "onOpenInventory";
constexpr const char* kEventOnCloseInventoryName = "onCloseInventory";
constexpr const char* kEventOnInventorySlotChangeName = "onInventorySlotChange";
constexpr const char* kEventOnWorldChangeName = "onWorldChange";
constexpr const char* kEventOnWorldEnterName = "onWorldEnter";
constexpr const char* kEventOnEquipName = "onEquip";
constexpr const char* kEventOnDropItemName = "onDropItem";
constexpr const char* kEventOnTakeItemName = "onTakeItem";
constexpr const char* kEventOnUseItemName = "onUseItem";
constexpr const char* kEventOnItemGroundCreateName = "onItemGroundCreate";
constexpr const char* kEventOnItemGroundDestroyName = "onItemGroundDestroy";
constexpr const char* kEventOnItemsGroundDestroyName = "onItemsGroundDestroy";
constexpr const char* kEventOnUnequipName = "onUnequip";
constexpr const char* kEventOnPlayerCreateName = "onPlayerCreate";
constexpr const char* kEventOnPlayerDestroyName = "onPlayerDestroy";
constexpr const char* kEventOnPlayerMessageName = "onPlayerMessage";
constexpr const char* kEventOnPlayerRespawnName = "onPlayerRespawn";
constexpr const char* kEventOnPlayerSpawnName = "onPlayerSpawn";
constexpr const char* kEventOnPlayerSummonName = "onPlayerSummon";
constexpr const char* kEventOnPlayerTeleportName = "onPlayerTeleport";
constexpr const char* kEventOnPlayerDeathName = "onPlayerDeath";
constexpr const char* kEventOnPlayerChangePingName = "onPlayerChangePing";
constexpr const char* kEventOnVoiceChatStateChangeName = "onVoiceChatStateChange";
constexpr const char* kEventOnVoiceTransmitStartName = "onVoiceTransmitStart";
constexpr const char* kEventOnVoiceTransmitStopName = "onVoiceTransmitStop";
constexpr const char* kEventOnVoiceChannelChangeName = "onVoiceChannelChange";
constexpr const char* kEventOnPlayerVoiceStartName = "onPlayerVoiceStart";
constexpr const char* kEventOnPlayerVoiceStopName = "onPlayerVoiceStop";

// Gothic-specific event structs
struct OnKeyEvent {
  int key;
};

struct OnPasteEvent {
  std::string text;
};

struct OnCommandEvent {
  std::string command;
  std::string params;
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

struct OnTimeEvent {
  int day;
  int hour;
  int min;
};

struct OnWeatherChangeEvent {
  int old_weather_type;
  int new_weather_type;
};

struct OnInventorySlotChangeEvent {
  int from;
  int to;
};

struct OnWorldChangeEvent {
  std::string world;
  std::string waypoint;
};

struct OnWorldEnterEvent {
  std::string world;
};

struct OnItemEvent {
  std::string item;
};

struct OnDropItemEvent {
  std::string item;
  int amount;
};

struct OnTakeItemEvent {
  std::string item;
  bool synchronized;
  int amount;
  std::optional<std::uint32_t> item_ground_id;
};

struct OnUseItemEvent {
  std::string item;
  std::string scheme;
  int from;
  int to;
};

struct PlayerLifecycleEvent {
  std::uint64_t player_id;
};

struct OnPlayerSummonEvent {
  std::uint32_t player_id;
  std::uint16_t spell_id;
  std::string summon_instance;
  std::uint32_t npc_id;
};

struct OnPlayerTeleportEvent {
  std::uint32_t player_id;
  std::string destination;
};

struct OnPlayerPingEvent {
  std::uint64_t player_id;
  int ping;
};

struct OnPlayerMessageEvent {
  std::optional<std::uint64_t> sender_id;
  std::uint8_t r;
  std::uint8_t g;
  std::uint8_t b;
  std::string message;
};

struct OnVoiceChatStateEvent {
  bool enabled;
  float range;
};

struct OnVoiceChannelChangeEvent {
  std::string old_channel;
  std::string new_channel;
};

struct OnPlayerVoiceEvent {
  std::uint64_t player_id;
};

// Bind Gothic-specific events to Lua
void BindGothicEvents(sol::state& lua);
bool TriggerRemoteEvent(const std::string& event_name, std::uint32_t source_element, const std::vector<sol::object>& args);

// Reset Gothic-specific events (call when disconnecting/reconnecting)
void ResetGothicEvents();

}  // namespace gmp::gothic
