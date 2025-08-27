
/*
MIT License

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

#include "event_bind.h"

#include <spdlog/spdlog.h>

#include <map>
#include <string>

#include "../server_events.h"
#include "lua.h"
#include "shared/event.h"

namespace lua {
namespace bindings {

namespace {

struct LuaProxyArgs {
  std::any event;
  sol::protected_function callback;
};

static std::map<std::string, std::function<void(LuaProxyArgs)>> kLuaEventProxies;

void RegisterProxies() {
  kLuaEventProxies[kEventOnClockUpdateName] = {[](LuaProxyArgs args) {
    OnClockUpdateEvent clock_update_event = std::any_cast<OnClockUpdateEvent>(args.event);
    args.callback(clock_update_event.day, clock_update_event.hour, clock_update_event.min);
  }};
  kLuaEventProxies[kEventOnPlayerConnectName] = {[](LuaProxyArgs args) {
    std::uint64_t player_guid = std::any_cast<std::uint64_t>(args.event);
    args.callback(player_guid);
  }};
  kLuaEventProxies[kEventOnPlayerDisconnectName] = {[](LuaProxyArgs args) {
    std::uint64_t player_guid = std::any_cast<std::uint64_t>(args.event);
    args.callback(player_guid);
  }};
  kLuaEventProxies[kEventOnPlayerMessageName] = {[](LuaProxyArgs args) {
    OnPlayerMessageEvent player_message_event = std::any_cast<OnPlayerMessageEvent>(args.event);
    args.callback(player_message_event.pid, player_message_event.text);
  }};
  kLuaEventProxies[kEventOnPlayerWhisperName] = {[](LuaProxyArgs args) {
    OnPlayerWhisperEvent player_whisper_event = std::any_cast<OnPlayerWhisperEvent>(args.event);
    args.callback(player_whisper_event.from_id, player_whisper_event.to_id, player_whisper_event.text);
  }};
  kLuaEventProxies[kEventOnPlayerChangeClassName] = {[](LuaProxyArgs args) {
    OnPlayerChangeClassEvent player_changeclass_event = std::any_cast<OnPlayerChangeClassEvent>(args.event);
    args.callback(player_changeclass_event.pid, player_changeclass_event.cid);
  }};
}

std::optional<std::function<void(LuaProxyArgs)>> GetProxy(std::string event_name) {
  return kLuaEventProxies[event_name];
}
}  // namespace

void BindEvents(sol::state& lua) {
  RegisterProxies();

  lua["addEventHandler"] = [&lua](std::string event_name, sol::protected_function lua_callback) -> bool {
    SPDLOG_TRACE("addEventHandler({})", event_name);

    auto proxy = GetProxy(event_name);
    if (!proxy) {
      SPDLOG_ERROR("addEventHandler: event with name {} doesn't exist!", event_name);
      return false;
    }

    auto callback = [proxy, lua_callback](std::any event) {
      LuaProxyArgs args;
      args.event = event;
      args.callback = lua_callback;
      (*proxy)(args);
    };
    return EventManager::Instance().SubscribeToEvent(event_name, callback);
  };
}
}  // namespace bindings
}  // namespace lua