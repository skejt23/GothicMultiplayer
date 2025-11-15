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

#include "resource.h"

#include <algorithm>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <vector>

#include "Script.h"
#include "shared/lua_runtime/timer_manager.h"

namespace fs = std::filesystem;

Resource::Resource(std::string name) : name_(std::move(name)) {}

bool Resource::Load(LuaScript& lua_script, TimerManager& timer_manager) {
  if (loaded_) {
    SPDLOG_WARN("Resource '{}' is already loaded", name_);
    return false;
  }

  SPDLOG_INFO("Loading resource '{}'...", name_);

  // Create isolated environment that inherits from lua.globals()
  auto& lua = lua_script.GetLuaState();
  env_ = sol::environment(lua, sol::create, lua.globals());

  const std::string base_path = "resources/" + name_;

  auto load_directory = [&](const std::string& path, const char* label) {
    if (!fs::exists(path) || !fs::is_directory(path)) {
      return true;
    }

    SPDLOG_DEBUG("  Loading {} scripts from '{}'", label, path);
    return LoadScriptsFromDirectory(lua, path);
  };

  // Load shared scripts first (shared between client and server)
  if (!load_directory(base_path + "/shared", "shared")) {
    SPDLOG_ERROR("Resource '{}' failed to load: shared script error", name_);
    timer_manager.KillTimersForResource(name_);
    env_ = sol::environment();
    exports_ = sol::nil;
    return false;
  }

  // Then load server-only scripts
  if (!load_directory(base_path + "/server", "server")) {
    SPDLOG_ERROR("Resource '{}' failed to load: server script error", name_);
    timer_manager.KillTimersForResource(name_);
    env_ = sol::environment();
    exports_ = sol::nil;
    return false;
  }

  // Capture exports table if defined
  sol::object exports_obj = env_["exports"];
  if (exports_obj.is<sol::table>()) {
    exports_ = exports_obj.as<sol::table>();
    SPDLOG_DEBUG("  Captured exports table for resource '{}'", name_);
  } else {
    exports_ = sol::nil;
  }

  loaded_ = true;
  generation_++;

  // Call onResourceStart lifecycle hook if present
  CallOnResourceStart();

  SPDLOG_INFO("Resource '{}' loaded successfully (generation {})", name_, generation_);
  return true;
}

void Resource::Unload(TimerManager& timer_manager) {
  if (!loaded_) {
    return;
  }

  SPDLOG_INFO("Unloading resource '{}'...", name_);

  // Call onResourceStop lifecycle hook if present
  CallOnResourceStop();

  // Kill all timers owned by this resource
  timer_manager.KillTimersForResource(name_);

  // Clear environment and exports
  env_ = sol::environment();
  exports_ = sol::nil;
  loaded_ = false;

  SPDLOG_INFO("Resource '{}' unloaded", name_);
}

bool Resource::Reload(LuaScript& lua_script, TimerManager& timer_manager) {
  SPDLOG_INFO("Reloading resource '{}'...", name_);
  Unload(timer_manager);
  return Load(lua_script, timer_manager);
}

bool Resource::LoadScriptsFromDirectory(sol::state& lua, const std::string& directory) {
  if (!fs::exists(directory) || !fs::is_directory(directory)) {
    return true;
  }

  // Collect all .lua files
  std::vector<std::string> script_files;
  for (const auto& entry : fs::directory_iterator(directory)) {
    if (entry.is_regular_file() && entry.path().extension() == ".lua") {
      script_files.push_back(entry.path().string());
    }
  }

  // Sort for consistent load order
  std::sort(script_files.begin(), script_files.end());

  // Execute each script in the resource environment
  for (const auto& script_path : script_files) {
    if (!ExecuteScript(lua, script_path)) {
      return false;
    }
  }

  return true;
}

bool Resource::ExecuteScript(sol::state& lua, const std::string& scriptPath) {
  try {
    // Load the script file
    sol::load_result chunk = lua.load_file(scriptPath);
    if (!chunk.valid()) {
      sol::error err = chunk;
      SPDLOG_ERROR("  Failed to load script '{}': {}", scriptPath, err.what());
      return false;
    }

    // Execute in the resource environment
    sol::protected_function pf = chunk;
    sol::set_environment(env_, pf);
    sol::protected_function_result result = pf();

    if (!result.valid()) {
      sol::error err = result;
      SPDLOG_ERROR("  Failed to execute script '{}': {}", scriptPath, err.what());
      return false;
    }

    SPDLOG_DEBUG("  Loaded script '{}'", fs::path(scriptPath).filename().string());
    return true;

  } catch (const sol::error& e) {
    SPDLOG_ERROR("  Exception loading script '{}': {}", scriptPath, e.what());
    return false;
  }
}

void Resource::CallOnResourceStart() {
  sol::object on_start = env_["onResourceStart"];
  if (on_start.valid() && on_start.get_type() == sol::type::function) {
    try {
      sol::protected_function pf = on_start;
      sol::set_environment(env_, pf);
      sol::protected_function_result result = pf();

      if (!result.valid()) {
        sol::error err = result;
        SPDLOG_ERROR("  Error in onResourceStart for '{}': {}", name_, err.what());
      }
    } catch (const sol::error& e) {
      SPDLOG_ERROR("  Exception in onResourceStart for '{}': {}", name_, e.what());
    }
  }
}

void Resource::CallOnResourceStop() {
  sol::object on_stop = env_["onResourceStop"];
  if (on_stop.valid() && on_stop.get_type() == sol::type::function) {
    try {
      sol::protected_function pf = on_stop;
      sol::set_environment(env_, pf);
      sol::protected_function_result result = pf();

      if (!result.valid()) {
        sol::error err = result;
        SPDLOG_ERROR("  Error in onResourceStop for '{}': {}", name_, err.what());
      }
    } catch (const sol::error& e) {
      SPDLOG_ERROR("  Exception in onResourceStop for '{}': {}", name_, e.what());
    }
  }
}
