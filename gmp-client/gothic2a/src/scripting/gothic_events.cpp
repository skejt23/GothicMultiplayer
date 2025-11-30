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

#include "gothic_events.h"

#include <spdlog/spdlog.h>

#include <any>
#include <functional>
#include <map>
#include <string>

#include "shared/event.h"

namespace gmp::gothic {

namespace {

struct LuaProxyArgs {
  std::any event;
  sol::protected_function callback;
};

std::map<std::string, std::function<void(LuaProxyArgs)>> g_gothic_event_proxies;

void RegisterGothicEventProxies() {
  g_gothic_event_proxies[kEventOnInitName] = [](LuaProxyArgs args) { args.callback(); };

  g_gothic_event_proxies[kEventOnExitName] = [](LuaProxyArgs args) { args.callback(); };

  g_gothic_event_proxies[kEventOnRenderName] = [](LuaProxyArgs args) {
    // onRender has no arguments for now
    args.callback();
  };

  g_gothic_event_proxies[kEventOnKeyDownName] = [](LuaProxyArgs args) {
    OnKeyEvent event = std::any_cast<OnKeyEvent>(args.event);
    args.callback(event.key);
  };

  g_gothic_event_proxies[kEventOnKeyUpName] = [](LuaProxyArgs args) {
    OnKeyEvent event = std::any_cast<OnKeyEvent>(args.event);
    args.callback(event.key);
  };

  g_gothic_event_proxies[kEventOnPlayerCreateName] = [](LuaProxyArgs args) {
    PlayerLifecycleEvent event = std::any_cast<PlayerLifecycleEvent>(args.event);
    args.callback(event.player_id);
  };

  g_gothic_event_proxies[kEventOnPlayerDestroyName] = [](LuaProxyArgs args) {
    PlayerLifecycleEvent event = std::any_cast<PlayerLifecycleEvent>(args.event);
    args.callback(event.player_id);
  };

  g_gothic_event_proxies[kEventOnPlayerMessageName] = [](LuaProxyArgs args) {
    OnPlayerMessageEvent event = std::any_cast<OnPlayerMessageEvent>(args.event);
    sol::state_view lua(args.callback.lua_state());
    sol::object sender = event.sender_id.has_value() ? sol::make_object(lua, event.sender_id.value()) : sol::lua_nil;
    args.callback(sender, event.r, event.g, event.b, event.message);
  };
}

void RegisterGothicEventsInManager() {
  EventManager::Instance().RegisterEvent(kEventOnInitName);
  EventManager::Instance().RegisterEvent(kEventOnExitName);
  EventManager::Instance().RegisterEvent(kEventOnRenderName);
  EventManager::Instance().RegisterEvent(kEventOnKeyDownName);
  EventManager::Instance().RegisterEvent(kEventOnKeyUpName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerCreateName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerDestroyName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerMessageName);
}

}  // namespace

void BindGothicEvents(sol::state& lua) {
  RegisterGothicEventProxies();
  RegisterGothicEventsInManager();

  lua.set_function("addEventHandler", [](std::string event_name, sol::protected_function lua_callback) -> bool {
    SPDLOG_TRACE("addEventHandler({})", event_name);

    auto it = g_gothic_event_proxies.find(event_name);
    if (it == g_gothic_event_proxies.end()) {
      SPDLOG_ERROR("addEventHandler: event with name {} doesn't exist!", event_name);
      return false;
    }

    auto proxy = it->second;
    auto callback = [proxy, lua_callback](std::any event) {
      LuaProxyArgs args;
      args.event = event;
      args.callback = lua_callback;
      proxy(args);
    };

    return EventManager::Instance().SubscribeToEvent(event_name, callback);
  });
}

void ResetGothicEvents() {
  EventManager::Instance().Reset();
  RegisterGothicEventsInManager();
}

}  // namespace gmp::gothic
