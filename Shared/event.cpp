
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

#include "shared/event.h"

#include <string>

bool EventManager::RegisterEvent(const std::string& eventName) {
  if (EventExists(eventName)) {
    return false;
  }

  events_[eventName] = EventState{};
  return true;
}

bool EventManager::UnregisterEvent(const std::string& eventName) {
  if (!EventExists(eventName)) {
    return false;
  }

  events_.erase(eventName);
  return true;
}

bool EventManager::EventExists(const std::string& eventName) {
  return events_.find(eventName) != events_.end();
}

bool EventManager::TriggerEvent(const std::string& eventName) {
  return DispatchEvent(eventName, std::nullopt).dispatched;
}

EventManager::EventDispatchResult EventManager::DispatchEvent(const std::string& eventName, const std::any& event) {
  auto it = events_.find(eventName);
  if (it == events_.end()) {
    return {false, false, std::nullopt};
  }

  if (!it->second.enabled) {
    return {false, false, std::nullopt};
  }

  // Snapshot ids so handlers can unsubscribe or unregister the event during dispatch.
  std::vector<EventHandlerId> handler_ids;
  handler_ids.reserve(it->second.handlers.size());
  for (const auto& handler : it->second.handlers) {
    handler_ids.push_back(handler.id);
  }

  context_stack_.emplace_back();
  for (auto handler_id : handler_ids) {
    auto event_it = events_.find(eventName);
    if (event_it == events_.end() || !event_it->second.enabled) {
      break;
    }

    auto& handlers = event_it->second.handlers;
    auto handler_it = std::find_if(handlers.begin(), handlers.end(), [handler_id](const EventHandler& handler) {
      return handler.id == handler_id;
    });
    if (handler_it == handlers.end()) {
      continue;
    }

    auto callback = handler_it->callback;
    callback(event);
  }

  bool cancelled = context_stack_.back().cancelled;
  auto value = context_stack_.back().value;
  context_stack_.pop_back();
  return {true, cancelled, value};
}

bool EventManager::UnsubscribeFromEvent(const std::string& eventName, EventHandlerId handler_id) {
  auto it = events_.find(eventName);
  if (it == events_.end()) {
    return false;
  }

  auto& handlers = it->second.handlers;
  auto before_size = handlers.size();
  handlers.erase(std::remove_if(handlers.begin(), handlers.end(),
                                [handler_id](const EventHandler& handler) { return handler.id == handler_id; }),
                 handlers.end());
  return handlers.size() != before_size;
}

void EventManager::CancelCurrentEvent() {
  if (!context_stack_.empty()) {
    context_stack_.back().cancelled = true;
  }
}

void EventManager::SetCurrentEventValue(int value) {
  if (!context_stack_.empty()) {
    context_stack_.back().value = value;
  }
}

bool EventManager::IsCurrentEventCancelled() const {
  if (context_stack_.empty()) {
    return false;
  }

  return context_stack_.back().cancelled;
}

std::optional<int> EventManager::CurrentEventValue() const {
  if (context_stack_.empty()) {
    return std::nullopt;
  }

  return context_stack_.back().value;
}

bool EventManager::ToggleEvent(const std::string& eventName, bool enabled) {
  auto it = events_.find(eventName);
  if (it == events_.end()) {
    return false;
  }

  it->second.enabled = enabled;
  return true;
}

void EventManager::Reset() {
  events_.clear();
  context_stack_.clear();
  next_handler_id_ = 1;
}

EventManager& EventManager::Instance() {
  static EventManager instance;
  return instance;
}
