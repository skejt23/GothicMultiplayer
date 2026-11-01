#include "lua_events.h"

#include <spdlog/spdlog.h>

#include "bind_helpers.h"

namespace gmp::lua {
namespace {

struct CustomEvent : EventMetadata {
  std::vector<sol::object> args;
};

void Proxy_CustomEvent(LuaEventProxyArgs args) {
  const auto& event = std::any_cast<const CustomEvent&>(args.event);
  sol::state_view lua(args.callback.lua_state());
  std::vector<sol::object> values;
  values.reserve(event.args.size() + 1);
  values.emplace_back(sol::make_object(lua, MakeEventContext(lua, event)));
  values.insert(values.end(), event.args.begin(), event.args.end());
  args.callback(sol::as_args(values));
}

}  // namespace

LuaEventBindings::LuaEventBindings(LuaEventProxyMap& proxies, EventOwnerHooks hooks) : proxies_(proxies), hooks_(std::move(hooks)) {
}

std::optional<std::string> LuaEventBindings::CurrentOwner() const {
  return hooks_.current_owner ? hooks_.current_owner() : std::optional<std::string>{std::string{}};
}

std::optional<LuaEventProxy> LuaEventBindings::GetProxy(const std::string& name) const {
  const auto it = proxies_.find(name);
  if (it != proxies_.end())
    return it->second;
  if (custom_owners_.contains(name))
    return Proxy_CustomEvent;
  return std::nullopt;
}

/* luagmp (func)
 * Registers a custom event. On the server, the current resource owns it and its handlers.
 * @name addEvent
 * @side shared
 * @category Event
 * @param (string) event_name Unique event name.
 * @param (boolean|nil) allow_remote_trigger Whether peers may send this event; false by default.
 * @return (boolean) True if registered.
 */
bool LuaEventBindings::AddEvent(const std::string& name, sol::optional<bool> allow_remote) {
  const auto owner = CurrentOwner();
  if (!owner || !EventManager::Instance().RegisterEvent(name))
    return false;
  custom_owners_[name] = *owner;
  if (allow_remote.value_or(false))
    remote_events_.insert(name);
  else
    remote_events_.erase(name);
  return true;
}

/* luagmp (func)
 * Subscribes to a built-in or custom event. Custom handlers receive (context, ...), including nil arguments.
 * Server handlers execute in their owning resource's context and are removed when that resource unloads.
 * @name addEventHandler
 * @side shared
 * @category Event
 * @param (string) event_name Event name.
 * @param (function) lua_callback Handler; retain the same function for removal.
 * @param (number|nil) priority Lower priorities execute first; default 9999.
 * @return (boolean) Whether the subscription was installed.
 */
bool LuaEventBindings::AddEventHandler(const std::string& name, sol::protected_function callback, sol::optional<int> priority) {
  const auto proxy = GetProxy(name);
  const auto owner = CurrentOwner();
  if (!proxy || !owner)
    return false;
  const auto identity = ::lua::bind_helpers::GetLuaIdentity(callback);
  const auto execute = hooks_.execute;
  auto dispatch = [proxy = *proxy, callback = std::move(callback), name, owner = *owner, execute](std::any event) {
    auto invoke = [proxy, callback, name, owner, event = std::move(event)]() mutable {
      proxy(LuaEventProxyArgs{std::move(event), LuaEventCallback(callback, name, owner)});
    };
    if (execute)
      execute(owner, std::move(invoke));
    else
      invoke();
  };
  const auto id = EventManager::Instance().SubscribeToEventWithPriority(name, std::move(dispatch), priority.value_or(9999));
  if (!id)
    return false;
  handlers_[name].push_back({identity, *id, *owner});
  return true;
}

/* luagmp (func)
 * Dispatches a local custom event. Handlers receive (context, ...); payload arguments are never interpreted as metadata.
 * @name callEvent
 * @side shared
 * @category Event
 * @param (string) name Custom event registered with [addEvent](addEvent.md).
 * @param (table|nil) options Metadata {sourceId = positive integer}; pass nil for no metadata.
 * @param (any...) ... Payload arguments, passed unchanged and in order, including nil.
 * @return (boolean) True if dispatched and not cancelled.
 */
bool LuaEventBindings::CallEvent(const std::string& name, sol::object options, sol::variadic_args args) {
  if (!custom_owners_.contains(name))
    return false;
  CustomEvent event;
  event.source_element = ReadEventSource(options);
  event.args = std::vector<sol::object>(args.begin(), args.end());
  const auto result = EventManager::Instance().DispatchEvent(name, event);
  return result.dispatched && !result.cancelled;
}

/* luagmp (func)
 * Cancels the current cancellable event.
 * @name cancelEvent
 * @side shared
 * @category Event
 */
void LuaEventBindings::CancelEvent() {
  EventManager::Instance().CancelCurrentEvent();
}

/* luagmp (func)
 * Checks whether the current event has been cancelled.
 * @name isEventCancelled
 * @side shared
 * @category Event
 * @return (boolean) Current cancellation state.
 */
bool LuaEventBindings::IsEventCancelled() const {
  return EventManager::Instance().IsCurrentEventCancelled();
}

void LuaEventBindings::EraseCustomEvent(std::string name) {
  EventManager::Instance().UnregisterEvent(name);
  custom_owners_.erase(name);
  remote_events_.erase(name);
  handlers_.erase(name);
}

/* luagmp (func)
 * Removes a custom event and its handlers. On the server, only its owning resource may remove it.
 * @name removeEvent
 * @side shared
 * @category Event
 * @param (string) event_name Custom event name.
 */
void LuaEventBindings::RemoveEvent(const std::string& name) {
  const auto it = custom_owners_.find(name);
  const auto owner = CurrentOwner();
  if (it != custom_owners_.end() && owner && it->second == *owner)
    EraseCustomEvent(name);
}

/* luagmp (func)
 * Unsubscribes the original callback passed to [addEventHandler](addEventHandler.md).
 * On the server, a resource may remove only its own subscription.
 * @name removeEventHandler
 * @side shared
 * @category Event
 * @param (string) event_name Event name.
 * @param (function) lua_callback The same function object used to subscribe.
 * @return (boolean) Whether a handler was removed.
 */
bool LuaEventBindings::RemoveEventHandler(const std::string& name, sol::protected_function callback) {
  const auto owner = CurrentOwner();
  const auto it = handlers_.find(name);
  if (!owner || it == handlers_.end())
    return false;
  const auto identity = ::lua::bind_helpers::GetLuaIdentity(callback);
  auto& handlers = it->second;
  for (auto handler = handlers.begin(); handler != handlers.end(); ++handler) {
    if (handler->identity == identity && handler->owner == *owner) {
      const bool removed = EventManager::Instance().UnsubscribeFromEvent(name, handler->id);
      handlers.erase(handler);
      return removed;
    }
  }
  return false;
}

/* luagmp (func)
 * Enables or disables dispatch of an event.
 * @name setEventEnabled
 * @side shared
 * @category Event
 * @param (string) event_name Event name.
 * @param (boolean) enabled Whether dispatch is enabled.
 */
void LuaEventBindings::SetEventEnabled(const std::string& name, bool enabled) {
  if (!EventManager::Instance().ToggleEvent(name, enabled))
    SPDLOG_ERROR("setEventEnabled: event '{}' does not exist", name);
}

/* luagmp (func)
 * Sets the result value of the current event; the meaning depends on the built-in event.
 * @name setEventValue
 * @side shared
 * @category Event
 * @param (number) event_value New event result.
 */
void LuaEventBindings::SetEventValue(int value) {
  EventManager::Instance().SetCurrentEventValue(value);
}

bool LuaEventBindings::TriggerRemoteEvent(const std::string& name, EventMetadata metadata, const std::vector<sol::object>& args) {
  if (!custom_owners_.contains(name) || !remote_events_.contains(name))
    return false;
  CustomEvent event;
  static_cast<EventMetadata&>(event) = std::move(metadata);
  event.remote = true;
  event.args = args;
  const auto result = EventManager::Instance().DispatchEvent(name, event);
  return result.dispatched && !result.cancelled;
}

void LuaEventBindings::RemoveHandlersForOwner(const std::string& owner) {
  std::vector<std::string> owned_events;
  for (const auto& [name, event_owner] : custom_owners_) {
    if (event_owner == owner)
      owned_events.push_back(name);
  }
  for (const auto& name : owned_events) EraseCustomEvent(name);
  for (auto it = handlers_.begin(); it != handlers_.end();) {
    auto& handlers = it->second;
    for (auto handler = handlers.begin(); handler != handlers.end();) {
      if (handler->owner == owner) {
        EventManager::Instance().UnsubscribeFromEvent(it->first, handler->id);
        handler = handlers.erase(handler);
      } else {
        ++handler;
      }
    }
    if (handlers.empty())
      it = handlers_.erase(it);
    else
      ++it;
  }
}

void LuaEventBindings::Reset() {
  while (!custom_owners_.empty()) EraseCustomEvent(custom_owners_.begin()->first);
  for (const auto& [name, handlers] : handlers_) {
    for (const auto& handler : handlers) EventManager::Instance().UnsubscribeFromEvent(name, handler.id);
  }
  handlers_.clear();
  remote_events_.clear();
}

void BindCommonEvents(sol::state& lua, LuaEventBindings& events) {
  lua.set_function("addEvent", &LuaEventBindings::AddEvent, &events);
  lua.set_function("addEventHandler", &LuaEventBindings::AddEventHandler, &events);
  lua.set_function("callEvent", &LuaEventBindings::CallEvent, &events);
  lua.set_function("cancelEvent", &LuaEventBindings::CancelEvent, &events);
  lua.set_function("isEventCancelled", &LuaEventBindings::IsEventCancelled, &events);
  lua.set_function("removeEvent", &LuaEventBindings::RemoveEvent, &events);
  lua.set_function("removeEventHandler", &LuaEventBindings::RemoveEventHandler, &events);
  lua.set_function("setEventEnabled", &LuaEventBindings::SetEventEnabled, &events);
  lua.set_function("setEventValue", &LuaEventBindings::SetEventValue, &events);
}

}  // namespace gmp::lua
