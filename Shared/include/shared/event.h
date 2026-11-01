
/*
MIT License

Copyright (c) 2023 Gothic Multiplayer Team (pampi, skejt23, mecio)

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

#include <algorithm>
#include <any>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class EventManager {
public:
  using EventHandlerId = std::uint64_t;

  struct EventDispatchResult {
    bool dispatched = false;
    bool cancelled = false;
    std::optional<int> value;
  };

  EventManager() = default;
  ~EventManager() = default;

  // Registers an event.
  // Returns true if the event was registered, false if it already exists.
  bool RegisterEvent(const std::string& eventName);

  // Unregisters an event.
  // Returns true if the event was unregistered, false if it doesn't exist.
  bool UnregisterEvent(const std::string& eventName);

  // Checks if an event exists.
  bool EventExists(const std::string& eventName);

  // Triggers an event.
  // Returns true if the event was triggered, false if it doesn't exist.
  bool TriggerEvent(const std::string& eventName);

  // Dispatches an event and returns detailed information.
  EventDispatchResult DispatchEvent(const std::string& eventName, const std::any& event);

  // Triggers an event with arguments.
  // Returns true if the event was triggered, false if it doesn't exist.
  // Note: Arguments are passed by const reference to ensure all subscribers
  // receive the same data. The std::any wrapper will copy the event object
  // once, and each callback receives a reference to that copy.
  template <typename T>
  bool TriggerEvent(const std::string& eventName, const T& event) {
    std::any wrapped_event = event;
    return DispatchEvent(eventName, wrapped_event).dispatched;
  }

  // Subscribes to an event.
  // Returns true if the subscription was successful, false if the event doesn't exist.
  template <typename T>
  bool SubscribeToEvent(const std::string& eventName, T&& callback) {
    return SubscribeToEventWithPriority(eventName, std::forward<T>(callback), 9999).has_value();
  }

  // Subscribes to an event with priority.
  // Returns handler id on success, std::nullopt if event doesn't exist.
  template <typename T>
  std::optional<EventHandlerId> SubscribeToEventWithPriority(const std::string& eventName, T&& callback, int priority) {
    if (!EventExists(eventName)) {
      return std::nullopt;
    }

    auto& handlers = events_[eventName].handlers;
    EventHandlerId handler_id = next_handler_id_++;
    handlers.push_back(EventHandler{handler_id, priority, std::forward<T>(callback)});
    std::stable_sort(handlers.begin(), handlers.end(),
                     [](const EventHandler& left, const EventHandler& right) { return left.priority < right.priority; });
    return handler_id;
  }

  // Unsubscribes from an event.
  // Returns true if the unsubscription was successful, false if the event doesn't exist.
  template <typename T>
  bool UnsubscribeFromEvent(const std::string& eventName, T&& callback) {
    if (!EventExists(eventName)) {
      return false;
    }

    auto& handlers = events_[eventName].handlers;
    handlers.erase(std::remove_if(handlers.begin(), handlers.end(),
                                  [&callback](const EventHandler& handler) { return handler.callback == callback; }),
                   handlers.end());
    return true;
  }

  // Unsubscribes from an event by handler id.
  bool UnsubscribeFromEvent(const std::string& eventName, EventHandlerId handler_id);

  // Cancels the currently running event.
  void CancelCurrentEvent();

  // Sets current event value.
  void SetCurrentEventValue(int value);

  // Checks if current event is cancelled.
  bool IsCurrentEventCancelled() const;

  // Returns current event value (if set).
  std::optional<int> CurrentEventValue() const;

  // Toggles an event on or off.
  bool ToggleEvent(const std::string& eventName, bool enabled);

  // Clears all registered events and listeners.
  void Reset();

  static EventManager& Instance();

private:
  struct EventHandler {
    EventHandlerId id;
    int priority;
    std::function<void(std::any)> callback;
  };

  struct EventState {
    bool enabled = true;
    std::vector<EventHandler> handlers;
  };

  struct EventContext {
    bool cancelled = false;
    std::optional<int> value;
  };

  std::unordered_map<std::string, EventState> events_;
  std::vector<EventContext> context_stack_;
  EventHandlerId next_handler_id_ = 1;
};
