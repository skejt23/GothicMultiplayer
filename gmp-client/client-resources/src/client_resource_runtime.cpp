/*
MIT License

Copyright (c) 2025 Gothic Multiplayer Team

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

#include "client_resources/client_resource_runtime.h"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <memory>
#include <string_view>

#include "Shared/lua_runtime/lua_diagnostics.h"
#include "Shared/lua_runtime/bind_helpers.h"
#include "Shared/lua_runtime/shared_bind.h"
#include "shared/event.h"

ClientResourceRuntime::ClientResourceRuntime() = default;
ClientResourceRuntime::~ClientResourceRuntime() = default;

bool ClientResourceRuntime::LoadResources(std::vector<gmp::client::GameClient::ResourcePayload> payloads, std::string& error_message) {
  UnloadResources();

  if (payloads.empty()) {
    return true;
  }

  resources_.reserve(payloads.size());

  for (auto& payload : payloads) {
    auto instance = std::make_unique<ResourceInstance>();
    instance->name = payload.descriptor.name;

    // Create environment table with globals fallback
    instance->env = sol::table(script_.GetLuaState(), sol::create);
    sol::table globals = script_.GetLuaState().globals();
    sol::table mt = script_.GetLuaState().create_table_with(sol::meta_function::index, globals);
    instance->env[sol::metatable_key] = mt;

    try {
      instance->pack.emplace(gmp::resource::ResourcePackLoader::LoadFromMemory(std::move(payload.manifest_json), std::move(payload.archive_bytes)));
    } catch (const std::exception& ex) {
      error_message = fmt::format("Failed to initialize client resource '{}': {}", payload.descriptor.name, ex.what());
      SPDLOG_ERROR(error_message);
      UnloadResources();
      return false;
    }

    resources_.push_back(std::move(instance));
    ResourceInstance& stored_instance = *resources_.back();

    SetupRequire(stored_instance);

    if (!ExecuteEntryPoints(stored_instance, error_message)) {
      SPDLOG_ERROR(error_message);
      resources_.pop_back();
      UnloadResources();
      return false;
    }

    sol::object exports_obj = stored_instance.env["exports"];
    if (exports_obj.is<sol::table>()) {
      stored_instance.exports = exports_obj.as<sol::table>();
    } else {
      stored_instance.exports = sol::nil;
    }

    if (!InvokeLifecycle(stored_instance, "onResourceStart", error_message)) {
      SPDLOG_ERROR(error_message);
      resources_.pop_back();
      UnloadResources();
      return false;
    }
    stored_instance.started = true;

    const auto entrypoint_count = stored_instance.pack->GetManifest().entrypoints.size();
    SPDLOG_INFO("Loaded client resource '{}' ({} entrypoint(s))", stored_instance.name, entrypoint_count);
  }

  EventManager::Instance().TriggerEvent("onInit", 0);
  return true;
}

void ClientResourceRuntime::UnloadResources() {
  if (!resources_.empty()) {
    const bool had_started_resource =
        std::any_of(resources_.begin(), resources_.end(), [](const auto& resource) { return resource->started; });

    if (had_started_resource) {
      EventManager::Instance().TriggerEvent("onExit", 0);
    }

    for (auto it = resources_.rbegin(); it != resources_.rend(); ++it) {
      if ((*it)->started) {
        std::string error_message;
        if (!InvokeLifecycle(**it, "onResourceStop", error_message)) {
          SPDLOG_WARN(error_message);
        }
      }
    }
    resources_.clear();
  }

  script_.GetTimerManager().Clear();
  if (reset_callback_) {
    reset_callback_();
  }
  lua_gc(script_.GetLuaState().lua_state(), LUA_GCCOLLECT, 0);
}

void ClientResourceRuntime::ProcessTimers() {
  script_.GetTimerManager().ProcessTimers();
}

std::optional<sol::table> ClientResourceRuntime::GetExports(const std::string& resource_name) const {
  auto it = std::find_if(resources_.begin(), resources_.end(), [&](const auto& r) { return r->name == resource_name; });
  if (it != resources_.end()) {
    return (*it)->exports;
  }
  return std::nullopt;
}

sol::state& ClientResourceRuntime::GetLuaState() {
  return script_.GetLuaState();
}

void ClientResourceRuntime::SetServerInfoProvider(gmp::client::GameClient& game_client) {
  lua::bindings::SetServerInfoProvider({
      [&game_client]() {
        const auto& name = game_client.GetServerName();
        if (!name.empty()) {
          return name;
        }

        const auto& host = game_client.GetServerIp();
        const auto port = game_client.GetServerPort();
        return port != 0 && !host.empty() ? fmt::format("{}:{}", host, port) : host;
      },
      [&game_client]() { return static_cast<int>(game_client.GetMaxSlots()); },
      [&game_client]() {
        std::vector<int> player_ids;
        for (const auto& [id, _] : game_client.GetOnlinePlayers()) player_ids.push_back(static_cast<int>(id));
        return player_ids;
      },
      [&game_client]() { return static_cast<int>(game_client.GetOnlinePlayers().size()); },
  });
}

void ClientResourceRuntime::SetupRequire(ResourceInstance& instance) {
  auto& lua = script_.GetLuaState();
  sol::table module_cache = lua.create_table();
  instance.env["package"] = lua.create_table_with("loaded", module_cache);

  // Capture the resource name to safely look it up later
  std::string resource_name = instance.name;

  instance.env.set_function(
      "require", [this, resource_name, module_cache](const std::string& module_name, sol::this_state ts) mutable -> sol::object {
        sol::state_view lua_state(ts);

        sol::object cached = module_cache[module_name];
        if (cached.valid() && cached.get_type() != sol::type::nil) {
          return cached;
        }

        // Find the resource instance by name to avoid dangling pointer/index issues
        auto it = std::find_if(resources_.begin(), resources_.end(), [&resource_name](const auto& r) { return r->name == resource_name; });
        if (it == resources_.end()) {
          throw sol::error(fmt::format("Resource '{}' is no longer loaded", resource_name));
        }
        ResourceInstance& instance_ref = **it;

        if (!instance_ref.pack) {
          throw sol::error(fmt::format("Resource '{}' has no pack loaded", instance_ref.name));
        }

        CaptureLifecycleHooks(instance_ref);

        std::string normalized = module_name;
        std::replace(normalized.begin(), normalized.end(), '.', '/');

        const std::array<std::string, 6> search_paths = {"client/" + normalized + ".luac",
                                                         "client/" + normalized + ".lua",
                                                         "shared/" + normalized + ".luac",
                                                         "shared/" + normalized + ".lua",
                                                         normalized + ".luac",
                                                         normalized + ".lua"};

        std::optional<gmp::resource::LoadedFile> file_opt;
        std::string used_path;
        for (const auto& candidate : search_paths) {
          file_opt = instance_ref.pack->TryLoadFile(candidate, true);
          if (file_opt) {
            used_path = candidate;
            break;
          }
        }

        if (!file_opt) {
          throw sol::error(fmt::format("module '{}' not found in resource '{}'", module_name, resource_name));
        }

        gmp::resource::LoadedFile& file = *file_opt;

        const std::string chunk_name = "@" + used_path;
        sol::load_result chunk =
            lua_state.load_buffer(reinterpret_cast<const char*>(file.data.data()), file.data.size(), chunk_name.c_str());
        if (!chunk.valid()) {
          sol::error err = chunk;
          throw err;
        }

        sol::protected_function pf = chunk;

        // Manually set the environment as the first upvalue (_ENV)
        // sol::set_environment fails here because the upvalue is nameless (debug info stripped)
        lua_State* L = pf.lua_state();
        pf.push();                // Push function
        instance_ref.env.push();  // Push environment table

        const char* upvalue_name = lua_setupvalue(L, -2, 1);
        if (!upvalue_name) {
          // This might happen if the module has no upvalues (doesn't use globals)
          // We pop the env table, but we don't error out as it might be valid.
          lua_pop(L, 1);
        }

        lua_pop(L, 1);  // Pop function

        sol::protected_function_result result = pf();
        if (!result.valid()) {
          sol::error err = result;
          throw err;
        }

        sol::object exported = result.get<sol::object>();
        CaptureLifecycleHooks(instance_ref);
        if (!exported.valid() || exported.get_type() == sol::type::nil) {
          exported = sol::make_object(lua_state, true);
        }

        module_cache[module_name] = exported;
        return exported;
      });
}

bool ClientResourceRuntime::ExecuteEntryPoints(ResourceInstance& instance, std::string& error_message) {
  if (!instance.pack) {
    error_message = fmt::format("Resource '{}' was not initialized with a pack", instance.name);
    return false;
  }

  const auto& manifest = instance.pack->GetManifest();
  auto& lua = script_.GetLuaState();

  if (manifest.entrypoints.empty()) {
    SPDLOG_WARN("Resource '{}' declares no entrypoints", instance.name);
    return true;
  }

  for (const auto& entrypoint : manifest.entrypoints) {
    gmp::resource::LoadedFile file;
    try {
      file = instance.pack->LoadFile(entrypoint, true);
    } catch (const std::exception& ex) {
      error_message = fmt::format("Resource '{}': failed to load '{}' from archive: {}", instance.name, entrypoint, ex.what());
      return false;
    }

    const std::string chunk_name = "@" + entrypoint;
    sol::load_result chunk = lua.load_buffer(reinterpret_cast<const char*>(file.data.data()), file.data.size(), chunk_name.c_str());
    if (!chunk.valid()) {
      sol::error err = chunk;
      ::lua::diagnostics::LogSyntaxError(err.what(), {instance.name, "loading entrypoint", entrypoint});
      error_message = fmt::format("Resource '{}': failed to load entrypoint '{}': {}", instance.name, entrypoint, err.what());
      return false;
    }

    sol::protected_function pf = chunk;

    // Manually set the environment as the first upvalue (_ENV)
    // sol::set_environment fails here because the upvalue is nameless (debug info stripped)
    lua_State* L = pf.lua_state();
    pf.push();            // Push function
    instance.env.push();  // Push environment table

    const char* upvalue_name = lua_setupvalue(L, -2, 1);
    if (!upvalue_name) {
      SPDLOG_ERROR("Failed to set environment for resource '{}': no upvalue found", instance.name);
      lua_pop(L, 1);  // Pop the env table if setupvalue failed
    }

    lua_pop(L, 1);  // Pop function

    sol::protected_function_result result = pf();
    if (!result.valid()) {
      sol::error err = result;
      ::lua::diagnostics::LogRuntimeError(err.what(), {instance.name, "executing entrypoint", entrypoint});
      error_message = fmt::format("Resource '{}': failed to execute entrypoint '{}': {}", instance.name, entrypoint, err.what());
      return false;
    }
    CaptureLifecycleHooks(instance);
  }

  return true;
}

void ClientResourceRuntime::CaptureLifecycleHooks(ResourceInstance& instance) {
  CaptureLifecycleHook(instance, "onResourceStart");
  CaptureLifecycleHook(instance, "onResourceStop");
}

void ClientResourceRuntime::CaptureLifecycleHook(ResourceInstance& instance, const char* hook) {
  sol::object fn = instance.env[hook];
  if (!fn.valid() || fn.get_type() != sol::type::function) {
    return;
  }

  sol::protected_function pf = fn;
  const void* identity = ::lua::bind_helpers::GetLuaIdentity(pf);

  auto& hooks = std::string_view(hook) == "onResourceStart" ? instance.start_hooks : instance.stop_hooks;
  auto& hook_ids = std::string_view(hook) == "onResourceStart" ? instance.start_hook_ids : instance.stop_hook_ids;
  if (std::find(hook_ids.begin(), hook_ids.end(), identity) != hook_ids.end()) {
    return;
  }

  hooks.emplace_back(std::move(pf));
  hook_ids.push_back(identity);
}

bool ClientResourceRuntime::InvokeLifecycle(ResourceInstance& instance, const char* hook, std::string& error_message) {
  const bool is_start = std::string_view(hook) == "onResourceStart";
  auto& hooks = is_start ? instance.start_hooks : instance.stop_hooks;
  if (hooks.empty()) {
    return true;
  }

  auto invoke_hook = [&](sol::protected_function& pf) {
    sol::protected_function_result result = pf();
    if (!result.valid()) {
      sol::error err = result;
      ::lua::diagnostics::LogRuntimeError(err.what(), {instance.name, "lifecycle hook", hook});
      error_message = fmt::format("Resource '{}': lifecycle hook '{}' failed", instance.name, hook);
      return false;
    }
    return true;
  };

  if (is_start) {
    for (auto& lifecycle_hook : hooks) {
      if (!invoke_hook(lifecycle_hook)) {
        return false;
      }
    }
  } else {
    for (auto it = hooks.rbegin(); it != hooks.rend(); ++it) {
      if (!invoke_hook(*it)) {
        return false;
      }
    }
  }

  return true;
}
