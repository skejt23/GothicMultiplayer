
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

  events_[eventName];
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
  if (!EventExists(eventName)) {
    return false;
  }

  for (auto& callback : events_[eventName]) {
    callback(std::nullopt);
  }

  return true;
}

void EventManager::Reset() {
  events_.clear();
}

EventManager& EventManager::Instance() {
  static EventManager instance;
  return instance;
}