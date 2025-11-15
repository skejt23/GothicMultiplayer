
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

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "sol/sol.hpp"

class TimerManager {
public:
  using TimerId = std::uint32_t;
  using OwnerContextExecutor = std::function<void(const std::string&, std::function<void()>)>;

  TimerManager();

  TimerId CreateTimer(sol::protected_function callback, std::chrono::milliseconds interval, std::uint32_t execute_times,
                      std::vector<sol::object> arguments, std::string owner_resource = "");

  void KillTimer(TimerId id);

  // Kill all timers owned by a specific resource
  void KillTimersForResource(const std::string& resource_name);

  std::optional<std::chrono::milliseconds> GetInterval(TimerId id) const;
  void SetInterval(TimerId id, std::chrono::milliseconds interval);

  std::optional<std::uint32_t> GetExecuteTimes(TimerId id) const;
  void SetExecuteTimes(TimerId id, std::uint32_t execute_times);

  void SetOwnerContextExecutor(OwnerContextExecutor executor);

  void ProcessTimers();
  void Clear();

private:
  struct Timer {
    TimerId id;
    sol::protected_function callback;
    std::vector<sol::object> arguments;
    std::chrono::milliseconds interval;
    std::uint32_t remaining_executions;
    bool infinite;
    std::chrono::steady_clock::time_point next_call;
    std::string owner_resource;  // Empty string for global/unowned timers
  };

  TimerId next_id_;
  std::unordered_map<TimerId, Timer> timers_;
  OwnerContextExecutor owner_context_executor_;
};
