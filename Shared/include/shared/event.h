
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

#include <any>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

class EventManager {
public:
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

  // Triggers an event with arguments.
  // Returns true if the event was triggered, false if it doesn't exist.
  template <typename... Args>
  bool TriggerEvent(const std::string& eventName, Args... args) {
    if (!EventExists(eventName)) {
      return false;
    }

    for (auto& callback : events_[eventName]) {
      callback(args...);
    }

    return true;
  }

  // Subscribes to an event.
  // Returns true if the subscription was successful, false if the event doesn't exist.
  template <typename T>
  bool SubscribeToEvent(const std::string& eventName, T&& callback) {
    if (!EventExists(eventName)) {
      return false;
    }

    events_[eventName].push_back(callback);
    return true;
  }

  // Unsubscribes from an event.
  // Returns true if the unsubscription was successful, false if the event doesn't exist.
  template <typename T>
  bool UnsubscribeFromEvent(const std::string& eventName, T&& callback) {
    if (!EventExists(eventName)) {
      return false;
    }

    auto& callbacks = events_[eventName];
    callbacks.erase(std::remove(callbacks.begin(), callbacks.end(), callback), callbacks.end());
    return true;
  }

  // Clears all registered events and listeners.
  void Reset();

  static EventManager& Instance();

private:
  std::unordered_map<std::string, std::vector<std::function<void(std::any)>>> events_;
};