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

#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>
#include <string_view>
#include <vector>

#include "Lua/event_bind.h"
#include "Script.h"
#include "Shared/lua_runtime/bind_helpers.h"
#include "Shared/lua_runtime/lua_diagnostics.h"
#include "Shared/lua_runtime/timer_manager.h"
#include "game_server.h"

namespace fs = std::filesystem;

Resource::Resource(std::string name) : name_(std::move(name)) {
}

bool Resource::Load(LuaScript& lua_script, TimerManager& timer_manager, const std::filesystem::path& root_path,
                    const std::vector<std::string>& scripts) {
  if (loaded_) {
    SPDLOG_WARN("Resource '{}' is already loaded", name_);
    return false;
  }

  SPDLOG_INFO("Loading resource '{}'...", name_);

  // Create isolated environment that inherits from lua.globals()
  auto& lua = lua_script.GetLuaState();
  env_ = sol::environment(lua, sol::create, lua.globals());
  start_hooks_.clear();
  stop_hooks_.clear();
  start_hook_ids_.clear();
  stop_hook_ids_.clear();

  if (!LoadScripts(lua, root_path, scripts)) {
    SPDLOG_ERROR("Resource '{}' failed to load: script error", name_);
    ResetRuntimeState(timer_manager);
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

  // Call onResourceStart lifecycle hook if present
  if (!CallOnResourceStart()) {
    SPDLOG_ERROR("Resource '{}' failed to start", name_);
    CallOnResourceStop();
    ResetRuntimeState(timer_manager);
    return false;
  }

  generation_++;

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

  ResetRuntimeState(timer_manager);

  SPDLOG_INFO("Resource '{}' unloaded", name_);
}

void Resource::ResetRuntimeState(TimerManager& timer_manager) {
  timer_manager.KillTimersForResource(name_);
  lua::bindings::RemoveHandlersForResource(name_);
  if (g_server) {
    g_server->DestroyNpcsForResource(name_);
  }
  env_ = sol::environment();
  exports_ = sol::nil;
  start_hooks_.clear();
  stop_hooks_.clear();
  start_hook_ids_.clear();
  stop_hook_ids_.clear();
  loaded_ = false;
}

bool Resource::Reload(LuaScript& lua_script, TimerManager& timer_manager, const std::filesystem::path& root_path,
                      const std::vector<std::string>& scripts) {
  SPDLOG_INFO("Reloading resource '{}'...", name_);
  Unload(timer_manager);
  return Load(lua_script, timer_manager, root_path, scripts);
}

bool Resource::LoadScripts(sol::state& lua, const std::filesystem::path& root_path, const std::vector<std::string>& scripts) {
  for (const auto& script : scripts) {
    const auto script_path = root_path / fs::path(script);
    if (!fs::exists(script_path) || !fs::is_regular_file(script_path)) {
      SPDLOG_ERROR("  Resource '{}' script '{}' does not exist or is not a file", name_, script);
      return false;
    }

    SPDLOG_DEBUG("  Loading script '{}'", script);
    if (!ExecuteScript(lua, script_path.generic_string())) {
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
      ::lua::diagnostics::LogSyntaxError(err.what(), {name_, "loading script", scriptPath});
      return false;
    }

    // Execute in the resource environment
    sol::protected_function pf = chunk;
    sol::set_environment(env_, pf);
    sol::protected_function_result result = pf();

    if (!result.valid()) {
      sol::error err = result;
      ::lua::diagnostics::LogRuntimeError(err.what(), {name_, "executing script", scriptPath});
      return false;
    }

    CaptureLifecycleHooks();
    SPDLOG_DEBUG("  Loaded script '{}'", fs::path(scriptPath).filename().string());
    return true;

  } catch (const sol::error& e) {
    ::lua::diagnostics::LogRuntimeError(e.what(), {name_, "loading script", scriptPath});
    return false;
  }
}

void Resource::CaptureLifecycleHooks() {
  CaptureLifecycleHook("onResourceStart");
  CaptureLifecycleHook("onResourceStop");
}

void Resource::CaptureLifecycleHook(const char* hook) {
  sol::object fn = env_[hook];
  if (!fn.valid() || fn.get_type() != sol::type::function) {
    return;
  }

  sol::protected_function pf = fn;
  const void* identity = ::lua::bind_helpers::GetLuaIdentity(pf);

  auto& hooks = std::string_view(hook) == "onResourceStart" ? start_hooks_ : stop_hooks_;
  auto& hook_ids = std::string_view(hook) == "onResourceStart" ? start_hook_ids_ : stop_hook_ids_;
  if (std::find(hook_ids.begin(), hook_ids.end(), identity) != hook_ids.end()) {
    return;
  }

  hooks.emplace_back(std::move(pf));
  hook_ids.push_back(identity);
}

bool Resource::CallOnResourceStart() {
  for (auto& on_start : start_hooks_) {
    try {
      sol::protected_function pf = on_start;
      sol::set_environment(env_, pf);
      sol::protected_function_result result = pf();

      if (!result.valid()) {
        sol::error err = result;
        ::lua::diagnostics::LogRuntimeError(err.what(), {name_, "lifecycle hook", "onResourceStart"});
        return false;
      }
    } catch (const sol::error& e) {
      ::lua::diagnostics::LogRuntimeError(e.what(), {name_, "lifecycle hook", "onResourceStart"});
      return false;
    }
  }
  return true;
}

void Resource::CallOnResourceStop() {
  for (auto it = stop_hooks_.rbegin(); it != stop_hooks_.rend(); ++it) {
    try {
      sol::protected_function pf = *it;
      sol::set_environment(env_, pf);
      sol::protected_function_result result = pf();

      if (!result.valid()) {
        sol::error err = result;
        ::lua::diagnostics::LogRuntimeError(err.what(), {name_, "lifecycle hook", "onResourceStop"});
      }
    } catch (const sol::error& e) {
      ::lua::diagnostics::LogRuntimeError(e.what(), {name_, "lifecycle hook", "onResourceStop"});
    }
  }
}
