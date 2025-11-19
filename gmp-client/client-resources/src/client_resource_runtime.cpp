#include "client_resources/client_resource_runtime.h"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>

ClientResourceRuntime::ClientResourceRuntime() = default;
ClientResourceRuntime::~ClientResourceRuntime() = default;

bool ClientResourceRuntime::LoadResources(std::vector<gmp::client::GameClient::ResourcePayload> payloads, std::string& error_message) {
  UnloadResources();

  if (payloads.empty()) {
    return true;
  }

  resources_.reserve(payloads.size());

  for (auto& payload : payloads) {
    ResourceInstance instance;
    instance.name = payload.descriptor.name;

    // Create environment table with globals fallback
    instance.env = sol::table(script_.GetLuaState(), sol::create);
    sol::table globals = script_.GetLuaState().globals();
    sol::table mt = script_.GetLuaState().create_table_with(sol::meta_function::index, globals);
    instance.env[sol::metatable_key] = mt;

    try {
      instance.pack.emplace(gmp::resource::ResourcePackLoader::LoadFromMemory(std::move(payload.manifest_json), std::move(payload.archive_bytes)));
    } catch (const std::exception& ex) {
      error_message = fmt::format("Failed to initialize client resource '{}': {}", payload.descriptor.name, ex.what());
      SPDLOG_ERROR(error_message);
      UnloadResources();
      return false;
    }

    resources_.push_back(std::move(instance));
    ResourceInstance& stored_instance = resources_.back();

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

  return true;
}

void ClientResourceRuntime::UnloadResources() {
  if (!resources_.empty()) {
    for (auto it = resources_.rbegin(); it != resources_.rend(); ++it) {
      if (it->started) {
        std::string error_message;
        if (!InvokeLifecycle(*it, "onResourceStop", error_message)) {
          SPDLOG_WARN(error_message);
        }
      }
    }
    resources_.clear();
  }

  script_.GetTimerManager().Clear();
}

void ClientResourceRuntime::ProcessTimers() {
  script_.GetTimerManager().ProcessTimers();
}

std::optional<sol::table> ClientResourceRuntime::GetExports(const std::string& resource_name) const {
  for (const auto& instance : resources_) {
    if (instance.name == resource_name && instance.exports.valid()) {
      return instance.exports;
    }
  }
  return std::nullopt;
}

void ClientResourceRuntime::SetupRequire(ResourceInstance& instance) {
  auto& lua = script_.GetLuaState();
  sol::table module_cache = lua.create_table();
  instance.env["package"] = lua.create_table_with("loaded", module_cache);

  ResourceInstance* instance_ptr = &instance;

  instance.env.set_function("require", [this, instance_ptr, module_cache](const std::string& module_name, sol::this_state ts) mutable -> sol::object {
    sol::state_view lua_state(ts);

    sol::object cached = module_cache[module_name];
    if (cached.valid() && cached.get_type() != sol::type::nil) {
      return cached;
    }

    if (!instance_ptr->pack) {
      throw sol::error(fmt::format("Resource '{}' has no pack loaded", instance_ptr->name));
    }

    std::string normalized = module_name;
    std::replace(normalized.begin(), normalized.end(), '.', '/');

    const std::array<std::string, 6> search_paths = {"client/" + normalized + ".luac",
                                                     "client/" + normalized + ".lua",
                                                     "shared/" + normalized + ".luac",
                                                     "shared/" + normalized + ".lua",
                                                     normalized + ".luac",
                                                     normalized + ".lua"};

    gmp::resource::LoadedFile file;
    std::string used_path;
    bool loaded = false;
    for (const auto& candidate : search_paths) {
      try {
        file = instance_ptr->pack->LoadFile(candidate, true);
        used_path = candidate;
        loaded = true;
        break;
      } catch (const std::exception&) {
        continue;
      }
    }

    if (!loaded) {
      throw sol::error(fmt::format("module '{}' not found in resource '{}'", module_name, instance_ptr->name));
    }

    sol::load_result chunk = lua_state.load_buffer(reinterpret_cast<const char*>(file.data.data()), file.data.size(), used_path.c_str());
    if (!chunk.valid()) {
      sol::error err = chunk;
      throw err;
    }

    sol::protected_function pf = chunk;

    // Manually set the environment as the first upvalue (_ENV)
    // sol::set_environment fails here because the upvalue is nameless (debug info stripped)
    lua_State* L = pf.lua_state();
    pf.push();                 // Push function
    instance_ptr->env.push();  // Push environment table

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

    sol::load_result chunk = lua.load_buffer(reinterpret_cast<const char*>(file.data.data()), file.data.size(), entrypoint.c_str());
    if (!chunk.valid()) {
      sol::error err = chunk;
      error_message = fmt::format("Resource '{}': syntax error in '{}': {}", instance.name, entrypoint, err.what());
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
      error_message = fmt::format("Resource '{}': runtime error in '{}': {}", instance.name, entrypoint, err.what());
      return false;
    }
  }

  return true;
}

bool ClientResourceRuntime::InvokeLifecycle(ResourceInstance& instance, const char* hook, std::string& error_message) {
  sol::object fn = instance.env[hook];
  if (!fn.valid() || fn.get_type() != sol::type::function) {
    return true;
  }

  sol::protected_function pf = fn;
  // Function already has the correct environment captured from its creation
  sol::protected_function_result result = pf();
  if (!result.valid()) {
    sol::error err = result;
    error_message = fmt::format("Resource '{}': error in {}: {}", instance.name, hook, err.what());
    return false;
  }

  return true;
}
