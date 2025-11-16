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

#include "resource_manager.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>
#include <toml.hpp>

#include "Script.h"
#include "shared/lua_runtime/timer_manager.h"

namespace fs = std::filesystem;

namespace {

constexpr const char* kResourcesDirectory = "resources";
constexpr const char* kMetadataFileName = "resource.toml";

std::optional<ResourceManager::ResourceMetadata> LoadResourceMetadata(const fs::path& resource_root, const std::string& resource_name) {
  const fs::path metadata_path = resource_root / kMetadataFileName;
  if (!fs::exists(metadata_path)) {
    SPDLOG_DEBUG("Resource '{}' has no {} metadata", resource_name, kMetadataFileName);
    return std::nullopt;
  }

  try {
    const toml::value metadata = toml::parse(metadata_path.string());
    if (!metadata.contains("version")) {
      SPDLOG_WARN("Resource '{}' metadata is missing a non-empty 'version' field; skipping client pack", resource_name);
      return std::nullopt;
    }

    ResourceManager::ResourceMetadata meta;
    meta.version = toml::find<std::string>(metadata, "version");
    if (meta.version.empty()) {
      SPDLOG_WARN("Resource '{}' metadata declared an empty version; skipping client pack", resource_name);
      return std::nullopt;
    }

    const std::string author = toml::find_or(metadata, "author", std::string{});
    if (!author.empty()) {
      meta.author = author;
    }

    const std::string description = toml::find_or(metadata, "description", std::string{});
    if (!description.empty()) {
      meta.description = description;
    }

    return meta;
  } catch (const toml::syntax_error& err) {
    SPDLOG_ERROR("Failed to parse metadata for resource '{}': {}", resource_name, err.what());
  } catch (const std::exception& ex) {
    SPDLOG_ERROR("Unexpected error while reading metadata for resource '{}': {}", resource_name, ex.what());
  }

  return std::nullopt;
}

}  // namespace

thread_local Resource* ResourceManager::current_resource_ = nullptr;
ResourceManager* ResourceManager::active_instance_ = nullptr;

ResourceManager::ResourceManager() {
  active_instance_ = this;
}

ResourceManager::~ResourceManager() {
  if (active_instance_ == this) {
    active_instance_ = nullptr;
  }
}

std::vector<std::string> ResourceManager::DiscoverResources() {
  std::vector<std::string> discovered;
  discovered_resources_.clear();
  const std::string base_path = kResourcesDirectory;

  if (!fs::exists(base_path) || !fs::is_directory(base_path)) {
    SPDLOG_WARN("resources directory does not exist");
    return discovered;
  }

  SPDLOG_INFO("Discovering resources from '{}'...", base_path);

  for (const auto& entry : fs::directory_iterator(base_path)) {
    if (!entry.is_directory()) {
      continue;
    }

    std::string resource_name = entry.path().filename().string();
    DiscoveredResource descriptor;
    descriptor.name = resource_name;
    descriptor.root_path = entry.path();
    descriptor.metadata = LoadResourceMetadata(descriptor.root_path, descriptor.name);

    discovered_resources_.push_back(descriptor);
    discovered.push_back(resource_name);
    SPDLOG_DEBUG("  Found resource: '{}'{}", resource_name, descriptor.metadata ? " (metadata detected)" : "");
  }

  // Sort for consistent load order
  std::sort(discovered.begin(), discovered.end());
  std::sort(discovered_resources_.begin(), discovered_resources_.end(), [](const DiscoveredResource& lhs, const DiscoveredResource& rhs) {
    return lhs.name < rhs.name;
  });

  SPDLOG_INFO("Discovered {} resource(s)", discovered.size());
  return discovered;
}

bool ResourceManager::LoadResource(const std::string& name, LuaScript& lua_script) {
  // Check if already loaded
  if (resources_.find(name) != resources_.end() && resources_[name]->IsLoaded()) {
    SPDLOG_WARN("Resource '{}' is already loaded", name);
    return false;
  }

  // Create new resource if it doesn't exist
  if (resources_.find(name) == resources_.end()) {
    resources_[name] = std::make_unique<Resource>(name);
  }

  // Set execution context
  ScopedResourceContext ctx(*resources_[name]);

  // Load the resource
  return resources_[name]->Load(lua_script, lua_script.GetTimerManager());
}

void ResourceManager::UnloadResource(const std::string& name, LuaScript& lua_script) {
  auto it = resources_.find(name);
  if (it == resources_.end() || !it->second->IsLoaded()) {
    SPDLOG_WARN("Resource '{}' is not loaded", name);
    return;
  }

  // Set execution context
  ScopedResourceContext ctx(*it->second);

  // Unload the resource
  it->second->Unload(lua_script.GetTimerManager());
}

bool ResourceManager::ReloadResource(const std::string& name, LuaScript& lua_script) {
  auto it = resources_.find(name);
  if (it == resources_.end()) {
    SPDLOG_WARN("Resource '{}' does not exist", name);
    return false;
  }

  // Set execution context
  ScopedResourceContext ctx(*it->second);

  // Reload the resource
  return it->second->Reload(lua_script, lua_script.GetTimerManager());
}

void ResourceManager::BindResourceAwareTimer(LuaScript& lua_script) {
  auto& lua = lua_script.GetLuaState();
  auto& timer_manager = lua_script.GetTimerManager();

  // Helper to copy variadic arguments
  auto copy_args = [](sol::state_view lua, const sol::variadic_args& args) {
    std::vector<sol::object> copied;
    copied.reserve(args.size());
    for (auto arg : args) {
      copied.emplace_back(sol::make_object(lua, arg));
    }
    return copied;
  };

  // Override setTimer to capture the current resource for ownership tracking
  lua.set_function("setTimer", [&timer_manager, copy_args](sol::protected_function func, int interval, int execute_times, sol::variadic_args args,
                                                           sol::this_state ts) {
    sol::state_view lua(ts);
    auto copied_arguments = copy_args(lua, args);
    auto timer_interval = std::chrono::milliseconds(interval);
    std::uint32_t times = execute_times > 0 ? static_cast<std::uint32_t>(execute_times) : 0u;

    // Capture current resource name for ownership tracking
    std::string owner_resource;
    if (auto* current_res = ResourceManager::GetCurrentResource()) {
      owner_resource = current_res->GetName();
    }

    return static_cast<int>(timer_manager.CreateTimer(std::move(func), timer_interval, times, std::move(copied_arguments), owner_resource));
  });

  timer_manager.SetOwnerContextExecutor([this](const std::string& owner_name, std::function<void()> callback) {
    auto resource_opt = GetResource(owner_name);
    if (!resource_opt || !resource_opt->get().IsLoaded()) {
      SPDLOG_DEBUG("Skipping timer callback for resource '{}' because it is not loaded", owner_name);
      return;
    }

    ScopedResourceContext ctx(resource_opt->get());
    callback();
  });
}

void ResourceManager::CreateExportsProxy(sol::state& lua) {
  sol::table exports_proxy = lua.create_table();
  sol::table exports_meta = lua.create_table();

  exports_meta[sol::meta_function::index] = [](sol::this_state s, sol::stack_object /*table*/, const std::string& res_name) -> sol::object {
    sol::state_view L(s);

    auto* manager = ResourceManager::GetActiveInstance();
    if (!manager) {
      return sol::object(L, sol::in_place, sol::nil);
    }

    auto resource_opt = manager->GetResource(res_name);
    if (!resource_opt || !resource_opt->get().IsLoaded()) {
      return sol::object(L, sol::in_place, sol::nil);
    }

    Resource& resource = resource_opt->get();
    sol::table exports = resource.GetExports();
    if (!exports.valid()) {
      return sol::object(L, sol::in_place, sol::nil);
    }

    sol::table proxy = L.create_table();
    sol::table meta = L.create_table();

    meta[sol::meta_function::index] = [res_name](sol::this_state inner_state, sol::stack_object /*table*/, const std::string& key) -> sol::object {
      sol::state_view inner(inner_state);

      auto* manager = ResourceManager::GetActiveInstance();
      if (!manager) {
        return sol::object(inner, sol::in_place, sol::nil);
      }

      auto resource_opt_inner = manager->GetResource(res_name);
      if (!resource_opt_inner || !resource_opt_inner->get().IsLoaded()) {
        return sol::object(inner, sol::in_place, sol::nil);
      }

      sol::table exports_table = resource_opt_inner->get().GetExports();
      sol::object value = exports_table[key];
      if (value.get_type() == sol::type::function) {
        sol::protected_function func = value;
        auto wrapper = sol::make_object(inner, [res_name, func](sol::variadic_args args) {
          auto* manager = ResourceManager::GetActiveInstance();
          if (!manager) {
            return func(sol::as_args(args));
          }

          auto resource_opt_exec = manager->GetResource(res_name);
          if (!resource_opt_exec || !resource_opt_exec->get().IsLoaded()) {
            return func(sol::as_args(args));
          }

          ScopedResourceContext ctx(resource_opt_exec->get());
          return func(sol::as_args(args));
        });
        return wrapper;
      }

      return value;
    };

    meta[sol::meta_function::new_index] = [res_name](sol::stack_object /*table*/, const std::string& key, const sol::object& value) {
      auto* manager = ResourceManager::GetActiveInstance();
      if (!manager) {
        return;
      }

      auto resource_opt_inner = manager->GetResource(res_name);
      if (!resource_opt_inner || !resource_opt_inner->get().IsLoaded()) {
        return;
      }

      sol::table exports_table = resource_opt_inner->get().GetExports();
      exports_table[key] = value;
    };

    proxy[sol::metatable_key] = meta;
    return sol::object(L, proxy);
  };

  exports_proxy[sol::metatable_key] = exports_meta;
  lua["exports"] = exports_proxy;

  SPDLOG_DEBUG("Created global exports proxy");
}

std::optional<std::reference_wrapper<Resource>> ResourceManager::GetResource(const std::string& name) {
  auto it = resources_.find(name);
  if (it != resources_.end()) {
    return std::ref(*it->second);
  }
  return std::nullopt;
}

Resource* ResourceManager::GetCurrentResource() {
  return current_resource_;
}

ResourceManager* ResourceManager::GetActiveInstance() {
  return active_instance_;
}

void ResourceManager::SetCurrentResource(Resource* resource) {
  current_resource_ = resource;
}

// ScopedResourceContext implementation
ResourceManager::ScopedResourceContext::ScopedResourceContext(Resource& resource) : previous_(ResourceManager::GetCurrentResource()) {
  ResourceManager::SetCurrentResource(&resource);
}

ResourceManager::ScopedResourceContext::~ScopedResourceContext() {
  ResourceManager::SetCurrentResource(previous_);
}
