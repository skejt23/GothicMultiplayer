
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

#include "timer_manager.h"

#include <spdlog/spdlog.h>

#include <algorithm>

namespace {
constexpr std::chrono::milliseconds kMinimumInterval{50};
}

TimerManager::TimerManager() : next_id_(1) {
}

TimerManager::TimerId TimerManager::CreateTimer(sol::protected_function callback, std::chrono::milliseconds interval, std::uint32_t execute_times,
                                                std::vector<sol::object> arguments) {
  if (interval < kMinimumInterval) {
    interval = kMinimumInterval;
  }

  Timer timer{};
  timer.id = next_id_++;
  timer.callback = std::move(callback);
  timer.arguments = std::move(arguments);
  timer.interval = interval;
  timer.remaining_executions = execute_times;
  timer.infinite = execute_times == 0;
  timer.next_call = std::chrono::steady_clock::now() + interval;

  timers_.emplace(timer.id, std::move(timer));
  return timer.id;
}

void TimerManager::KillTimer(TimerId id) {
  timers_.erase(id);
}

std::optional<std::chrono::milliseconds> TimerManager::GetInterval(TimerId id) const {
  if (auto it = timers_.find(id); it != timers_.end()) {
    return it->second.interval;
  }
  return std::nullopt;
}

void TimerManager::SetInterval(TimerId id, std::chrono::milliseconds interval) {
  if (auto it = timers_.find(id); it != timers_.end()) {
    if (interval < kMinimumInterval) {
      interval = kMinimumInterval;
    }
    it->second.interval = interval;
    it->second.next_call = std::chrono::steady_clock::now() + interval;
  }
}

std::optional<std::uint32_t> TimerManager::GetExecuteTimes(TimerId id) const {
  if (auto it = timers_.find(id); it != timers_.end()) {
    if (it->second.infinite) {
      return 0u;
    }
    return it->second.remaining_executions;
  }
  return std::nullopt;
}

void TimerManager::SetExecuteTimes(TimerId id, std::uint32_t execute_times) {
  if (auto it = timers_.find(id); it != timers_.end()) {
    it->second.remaining_executions = execute_times;
    it->second.infinite = execute_times == 0;
  }
}

void TimerManager::ProcessTimers() {
  if (timers_.empty()) {
    return;
  }

  std::vector<TimerId> to_remove;
  to_remove.reserve(timers_.size());

  auto now = std::chrono::steady_clock::now();

  for (auto& [id, timer] : timers_) {
    if (now < timer.next_call) {
      continue;
    }

    sol::protected_function_result result = timer.callback(sol::as_args(timer.arguments));
    if (!result.valid()) {
      sol::error error = result;
      SPDLOG_ERROR("Timer {} callback failed: {}", id, error.what());
    }

    if (!timer.infinite) {
      if (timer.remaining_executions == 0) {
        to_remove.push_back(id);
        continue;
      }

      if (--timer.remaining_executions == 0) {
        to_remove.push_back(id);
        continue;
      }
    }

    now = std::chrono::steady_clock::now();
    timer.next_call = now + timer.interval;
  }

  for (auto id : to_remove) {
    timers_.erase(id);
  }
}

void TimerManager::Clear() {
  timers_.clear();
}