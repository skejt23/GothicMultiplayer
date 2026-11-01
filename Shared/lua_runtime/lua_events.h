#pragma once

#include <any>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Shared/lua_runtime/lua_diagnostics.h"
#include "Shared/lua_runtime/lua_event_contract.h"
#include "shared/event.h"
#include "sol/sol.hpp"

namespace gmp::lua {

struct LuaEventCallback {
  sol::protected_function callback;
  std::string event_name;
  std::string resource_name;

  LuaEventCallback() = default;
  LuaEventCallback(sol::protected_function function, std::string event, std::string resource = {})
      : callback(std::move(function)), event_name(std::move(event)), resource_name(std::move(resource)) {
  }

  lua_State* lua_state() const {
    return callback.lua_state();
  }

  template <typename... Args>
  sol::protected_function_result operator()(Args&&... args) {
    sol::protected_function_result result = callback(std::forward<Args>(args)...);
    if (!result.valid()) {
      const sol::error error = result;
      ::lua::diagnostics::LogRuntimeError(error.what(), {resource_name, "event handler", event_name});
    }
    return result;
  }
};

struct LuaEventProxyArgs {
  std::any event;
  LuaEventCallback callback;
};

using LuaEventProxy = std::function<void(LuaEventProxyArgs)>;
using LuaEventProxyMap = std::map<std::string, LuaEventProxy>;

// The server supplies resource ownership and execution context. The client needs neither hook.
struct EventOwnerHooks {
  std::function<std::optional<std::string>()> current_owner;
  std::function<void(const std::string&, std::function<void()>)> execute;
};

// One registry per runtime domain, shared by all Lua states in that domain.
// It stores no Lua references; handlers belong to EventManager and are removed on resource unload/reset.
class LuaEventBindings {
public:
  explicit LuaEventBindings(LuaEventProxyMap& proxies, EventOwnerHooks hooks = {});

  bool AddEvent(const std::string& name, sol::optional<bool> allow_remote);
  bool AddEventHandler(const std::string& name, sol::protected_function callback, sol::optional<int> priority);
  bool CallEvent(const std::string& name, sol::object options, sol::variadic_args args);
  void CancelEvent();
  bool IsEventCancelled() const;
  void RemoveEvent(const std::string& name);
  bool RemoveEventHandler(const std::string& name, sol::protected_function callback);
  void SetEventEnabled(const std::string& name, bool enabled);
  void SetEventValue(int value);

  bool TriggerRemoteEvent(const std::string& name, EventMetadata metadata, const std::vector<sol::object>& args);
  void RemoveHandlersForOwner(const std::string& owner);
  void Reset();

private:
  struct HandlerRegistration {
    const void* identity;
    EventManager::EventHandlerId id;
    std::string owner;
  };

  std::optional<std::string> CurrentOwner() const;
  std::optional<LuaEventProxy> GetProxy(const std::string& name) const;
  void EraseCustomEvent(std::string name);

  LuaEventProxyMap& proxies_;
  EventOwnerHooks hooks_;
  std::unordered_map<std::string, std::string> custom_owners_;
  std::unordered_set<std::string> remote_events_;
  std::unordered_map<std::string, std::vector<HandlerRegistration>> handlers_;
};

void BindCommonEvents(sol::state& lua, LuaEventBindings& events);

}  // namespace gmp::lua
