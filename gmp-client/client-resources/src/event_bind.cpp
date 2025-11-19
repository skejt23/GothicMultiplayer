#include "client_resources/event_bind.h"

#include <spdlog/spdlog.h>
#include <map>
#include <string>
#include <any>

#include "client_resources/client_events.h"
#include "shared/event.h"

namespace gmp::client::lua::bindings {

namespace {

struct LuaProxyArgs {
  std::any event;
  sol::protected_function callback;
};

static std::map<std::string, std::function<void(LuaProxyArgs)>> kLuaEventProxies;

void RegisterProxies() {
  kLuaEventProxies[kEventOnRenderName] = {[](LuaProxyArgs args) {
    // onRender has no arguments for now
    args.callback();
  }};
}

std::optional<std::function<void(LuaProxyArgs)>> GetProxy(std::string event_name) {
  return kLuaEventProxies[event_name];
}

} // namespace

void BindEvents(sol::state& lua) {
  RegisterProxies();

  // Ensure events are registered in EventManager
  EventManager::Instance().RegisterEvent(kEventOnRenderName);

  lua.set_function("addEventHandler", [](std::string event_name, sol::protected_function lua_callback) -> bool {
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
  });
}

void ResetEvents() {
  EventManager::Instance().Reset();
  EventManager::Instance().RegisterEvent(kEventOnRenderName);
}

} // namespace gmp::client::lua::bindings
