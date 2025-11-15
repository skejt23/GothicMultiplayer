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
#include <string>

#include "sol/sol.hpp"

// Forward declarations
class LuaScript;
class TimerManager;

// Represents a single resource with isolated Lua environment
// Resources can contain scripts, assets, and other content
// Server-side resources load from resources_src/<name>/server/ and resources_src/<name>/shared/
class Resource {
public:
  explicit Resource(std::string name);
  ~Resource() = default;

  // Load all scripts from the resource directories
  // Executes shared/ scripts first, then server/ scripts
  // Creates sol::environment, executes scripts, captures exports, calls onResourceStart
  bool Load(LuaScript& lua_script, TimerManager& timer_manager);

  // Unload the resource
  // Calls onResourceStop, clears environment and exports, kills owned timers
  void Unload(TimerManager& timer_manager);

  // Reload the resource (unload then load)
  bool Reload(LuaScript& lua_script, TimerManager& timer_manager);

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
  // Discover and load all .lua files from a directory
  bool LoadScriptsFromDirectory(sol::state& lua, const std::string& directory);

  // Execute a single script file in the resource environment
  bool ExecuteScript(sol::state& lua, const std::string& scriptPath);

  // Call lifecycle hooks
  void CallOnResourceStart();
  void CallOnResourceStop();

  std::string name_;
  sol::environment env_;
  sol::table exports_;
  std::uint32_t generation_ = 0;
  bool loaded_ = false;
};
