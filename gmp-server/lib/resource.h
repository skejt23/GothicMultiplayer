/*
MIT License

Copyright (c) 2022 Gothic Multiplayer Team (pampi, skejt23, mecio)

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

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "sol/sol.hpp"

// Forward declarations
class LuaScript;
class TimerManager;

// Represents a single resource with isolated Lua environment
// Resources can contain scripts, assets, and other content
// Server-side scripts load in the order declared by resource.toml.
class Resource {
public:
  explicit Resource(std::string name);
  ~Resource() = default;

  // Load all scripts listed in resource.toml.
  // Creates sol::environment, executes scripts, captures exports, calls onResourceStart
  bool Load(LuaScript& lua_script, TimerManager& timer_manager, const std::filesystem::path& root_path,
            const std::vector<std::string>& scripts);

  // Unload the resource
  // Calls onResourceStop, clears environment and exports, kills owned timers
  void Unload(TimerManager& timer_manager);

  // Reload the resource (unload then load)
  bool Reload(LuaScript& lua_script, TimerManager& timer_manager, const std::filesystem::path& root_path,
              const std::vector<std::string>& scripts);

  // Accessors
  const std::string& GetName() const {
    return name_;
  }
  bool IsLoaded() const {
    return loaded_;
  }
  std::uint32_t GetGeneration() const {
    return generation_;
  }
  sol::environment& GetEnvironment() {
    return env_;
  }
  const sol::environment& GetEnvironment() const {
    return env_;
  }
  sol::table GetExports() const {
    return exports_;
  }

private:
  // Execute each resource.toml script in declared order.
  bool LoadScripts(sol::state& lua, const std::filesystem::path& root_path, const std::vector<std::string>& scripts);

  // Execute a single script file in the resource environment
  bool ExecuteScript(sol::state& lua, const std::string& scriptPath);

  // Capture lifecycle hooks after each script so later files do not overwrite
  // earlier resource-level hooks.
  void CaptureLifecycleHooks();
  void CaptureLifecycleHook(const char* hook);

  // Call lifecycle hooks
  bool CallOnResourceStart();
  void CallOnResourceStop();

  // Remove everything registered or retained by a failed/stopped resource.
  void ResetRuntimeState(TimerManager& timer_manager);

  std::string name_;
  sol::environment env_;
  sol::table exports_;
  std::vector<sol::protected_function> start_hooks_;
  std::vector<sol::protected_function> stop_hooks_;
  std::vector<const void*> start_hook_ids_;
  std::vector<const void*> stop_hook_ids_;
  std::uint32_t generation_ = 0;
  bool loaded_ = false;
};
