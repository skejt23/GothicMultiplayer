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

#include "Lua/event_bind.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>
#include <toml.hpp>

#include "Script.h"
#include "Shared/lua_runtime/bind_helpers.h"
#include "Shared/lua_runtime/timer_manager.h"

namespace fs = std::filesystem;

namespace {

// Schedules a timer while retaining the resource that owns its callback.
struct ResourceSetTimerBinding {
  TimerManager& timer_manager;
  int operator()(sol::protected_function func, int interval, int execute_times, sol::variadic_args args, sol::this_state ts) const {
    sol::state_view state(ts);
    auto copied_arguments = ::lua::bind_helpers::CopyArguments(state, args);
    std::string owner;
    if (auto* resource = ResourceManager::GetCurrentResource())
      owner = resource->GetName();
    return static_cast<int>(timer_manager.CreateTimer(std::move(func), std::chrono::milliseconds(interval),
                                                      execute_times > 0 ? static_cast<std::uint32_t>(execute_times) : 0u, std::move(copied_arguments),
                                                      owner));
  }
};

// Restores the owner's execution context while a resource timer runs.
struct ResourceTimerContext {
  ResourceManager& manager;
  void operator()(const std::string& owner, std::function<void()> callback) const {
    auto resource = manager.GetResource(owner);
    if (!resource || !resource->get().IsLoaded()) {
      SPDLOG_DEBUG("Skipping timer callback for resource '{}' because it is not loaded", owner);
      return;
    }
    ResourceManager::ScopedResourceContext context(resource->get());
    callback();
  }
};

// Calls a resource export in the execution context of its owner.
struct ResourceExportCall {
  std::string resource_name;
  sol::protected_function function;
  sol::protected_function_result operator()(sol::variadic_args args) const {
    auto* manager = ResourceManager::GetActiveInstance();
    auto resource = manager ? manager->GetResource(resource_name) : std::nullopt;
    if (!resource || !resource->get().IsLoaded())
      return function(sol::as_args(args));
    ResourceManager::ScopedResourceContext context(resource->get());
    return function(sol::as_args(args));
  }
};

// Reads one export, wrapping functions so nested resource calls preserve ownership.
struct ResourceExportIndex {
  std::string resource_name;
  sol::object operator()(sol::this_state ts, sol::stack_object, const std::string& key) const {
    sol::state_view state(ts);
    auto* manager = ResourceManager::GetActiveInstance();
    auto resource = manager ? manager->GetResource(resource_name) : std::nullopt;
    if (!resource || !resource->get().IsLoaded())
      return sol::nil;
    sol::table exports = resource->get().GetExports();
    sol::object value = exports[key];
    if (value.get_type() == sol::type::function)
      return sol::make_object(state, sol::as_function(ResourceExportCall{resource_name, value.as<sol::protected_function>()}));
    return value;
  }
};

// Writes one export only while the resource remains loaded.
struct ResourceExportNewIndex {
  std::string resource_name;
  void operator()(sol::stack_object, const std::string& key, const sol::object& value) const {
    auto* manager = ResourceManager::GetActiveInstance();
    auto resource = manager ? manager->GetResource(resource_name) : std::nullopt;
    if (!resource || !resource->get().IsLoaded())
      return;
    sol::table exports = resource->get().GetExports();
    exports[key] = value;
  }
};

/* luagmp (global)
 * Access to exports of loaded resources. Read `exports[resourceName][exportName]`; function calls run in the target resource context.
 * @name exports
 * @side server
 * @return (table) Proxy over loaded resource exports.
 */
sol::object ResourceExportsIndex(sol::this_state ts, sol::stack_object, const std::string& name) {
  sol::state_view state(ts);
  auto* manager = ResourceManager::GetActiveInstance();
  auto resource = manager ? manager->GetResource(name) : std::nullopt;
  if (!resource || !resource->get().IsLoaded())
    return sol::nil;
  sol::table exports = resource->get().GetExports();
  if (!exports.valid())
    return sol::nil;
  sol::table proxy = state.create_table();
  sol::table meta = state.create_table();
  meta[sol::meta_function::index] = ResourceExportIndex{name};
  meta[sol::meta_function::new_index] = ResourceExportNewIndex{name};
  proxy[sol::metatable_key] = meta;
  return sol::make_object(state, proxy);
}

constexpr const char* kResourcesDirectory = "resources";
constexpr const char* kMetadataFileName = "resource.toml";

struct ResourceMetadataResult {
  std::optional<ResourceManager::ResourceMetadata> metadata;
};

bool IsServerScriptPath(const std::string& path) {
  return path.rfind("shared/", 0) == 0 || path.rfind("server/", 0) == 0;
}

bool NormalizeResourceScriptPath(const std::string& script_path, std::string& normalized, std::string& error) {
  if (script_path.empty()) {
    error = "script path is empty";
    return false;
  }

  fs::path path(script_path);
  if (path.is_absolute() || path.has_root_name() || path.has_root_directory()) {
    error = "script path must be relative";
    return false;
  }

  for (const auto& part : path) {
    if (part == fs::path("..")) {
      error = "script path cannot contain '..'";
      return false;
    }
  }

  if (path.extension() != ".lua") {
    error = "script path must point to a .lua file";
    return false;
  }

  normalized = path.lexically_normal().generic_string();
  if (!IsServerScriptPath(normalized)) {
    error = "server resource scripts must be under shared/ or server/";
    return false;
  }

  return true;
}

std::optional<ResourceMetadataResult> LoadResourceMetadata(const fs::path& resource_root, const std::string& resource_name) {
  ResourceMetadataResult result;
  const fs::path metadata_path = resource_root / kMetadataFileName;
  if (!fs::exists(metadata_path)) {
    SPDLOG_DEBUG("Skipping directory '{}' because it has no {}", resource_name, kMetadataFileName);
    return std::nullopt;
  }

  try {
    const toml::value metadata = toml::parse(metadata_path.string());
    if (!metadata.contains("version")) {
      SPDLOG_ERROR("Resource '{}' metadata is missing required 'version' field", resource_name);
      return result;
    }

    ResourceManager::ResourceMetadata meta;
    meta.version = toml::find<std::string>(metadata, "version");
    if (meta.version.empty()) {
      SPDLOG_ERROR("Resource '{}' metadata declared an empty version", resource_name);
      return result;
    }

    if (!metadata.contains("scripts")) {
      SPDLOG_ERROR("Resource '{}' metadata is missing required 'scripts' field", resource_name);
      return result;
    }

    std::vector<std::string> declared_scripts = toml::find<std::vector<std::string>>(metadata, "scripts");
    meta.scripts.reserve(declared_scripts.size());
    for (const auto& script : declared_scripts) {
      std::string normalized;
      std::string error;
      if (!NormalizeResourceScriptPath(script, normalized, error)) {
        SPDLOG_ERROR("Resource '{}' has invalid script '{}': {}", resource_name, script, error);
        return result;
      }

      meta.scripts.push_back(std::move(normalized));
    }

    const std::string author = toml::find_or(metadata, "author", std::string{});
    if (!author.empty()) {
      meta.author = author;
    }

    const std::string description = toml::find_or(metadata, "description", std::string{});
    if (!description.empty()) {
      meta.description = description;
    }

    result.metadata = std::move(meta);
    return result;
  } catch (const toml::syntax_error& err) {
    SPDLOG_ERROR("Failed to parse metadata for resource '{}': {}", resource_name, err.what());
  } catch (const std::exception& ex) {
    SPDLOG_ERROR("Unexpected error while reading metadata for resource '{}': {}", resource_name, ex.what());
  }

  return result;
}

}  // namespace

thread_local Resource* ResourceManager::current_resource_ = nullptr;
ResourceManager* ResourceManager::active_instance_ = nullptr;

ResourceManager::ResourceManager() {
  active_instance_ = this;
}

ResourceManager::~ResourceManager() {
  // EventManager outlives Lua states; release callbacks while their state still exists.
  for (const auto& [name, resource] : resources_) {
    lua::bindings::RemoveHandlersForResource(name);
  }
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
    auto metadata_result = LoadResourceMetadata(entry.path(), resource_name);
    if (!metadata_result.has_value()) {
      continue;
    }

    DiscoveredResource descriptor;
    descriptor.name = resource_name;
    descriptor.root_path = entry.path();
    descriptor.metadata = std::move(metadata_result->metadata);

    discovered_resources_.push_back(descriptor);
    if (descriptor.metadata) {
      discovered.push_back(resource_name);
    }
    SPDLOG_DEBUG("  Found resource: '{}'{}", resource_name, descriptor.metadata ? " (metadata detected)" : " (invalid metadata)");
  }

  // Sort for consistent load order
  std::sort(discovered.begin(), discovered.end());
  std::sort(discovered_resources_.begin(), discovered_resources_.end(),
            [](const DiscoveredResource& lhs, const DiscoveredResource& rhs) { return lhs.name < rhs.name; });

  const std::size_t valid_count = discovered.size();
  const std::size_t invalid_count = discovered_resources_.size() - valid_count;
  SPDLOG_INFO("Discovered {} valid resource(s) ({} invalid)", valid_count, invalid_count);
  return discovered;
}

void ResourceManager::LogResourceInfo() const {
  std::size_t valid_scripts_count = 0;
  std::size_t valid_resources = 0;
  std::size_t invalid_resources = 0;
  for (const auto& resource : discovered_resources_) {
    if (resource.metadata) {
      ++valid_resources;
      valid_scripts_count += resource.metadata->scripts.size();
    } else {
      ++invalid_resources;
    }
  }

  SPDLOG_INFO("");
  SPDLOG_INFO("-= Resources =-");
  SPDLOG_INFO("* {:<18}: {}", "Resources valid", valid_resources);
  SPDLOG_INFO("* {:<18}: {}", "Scripts count", valid_scripts_count);
  SPDLOG_INFO("* {:<18}: {}", "Resources invalid", invalid_resources);
}

const ResourceManager::DiscoveredResource* ResourceManager::FindDiscoveredResource(const std::string& name) const {
  auto it = std::find_if(discovered_resources_.begin(), discovered_resources_.end(),
                         [&name](const DiscoveredResource& resource) { return resource.name == name; });
  if (it == discovered_resources_.end()) {
    return nullptr;
  }

  return &*it;
}

const ResourceManager::DiscoveredResource* ResourceManager::FindLoadableResource(const std::string& name) {
  const DiscoveredResource* descriptor = FindDiscoveredResource(name);
  if (descriptor == nullptr) {
    DiscoverResources();
    descriptor = FindDiscoveredResource(name);
  }

  if (descriptor == nullptr) {
    SPDLOG_WARN("Resource '{}' was not discovered from a {}", name, kMetadataFileName);
    return nullptr;
  }

  if (!descriptor->metadata) {
    SPDLOG_WARN("Resource '{}' has invalid metadata", name);
    return nullptr;
  }

  return descriptor;
}

bool ResourceManager::LoadResource(const std::string& name, LuaScript& lua_script) {
  const DiscoveredResource* descriptor = FindLoadableResource(name);
  if (descriptor == nullptr || !descriptor->metadata) {
    return false;
  }

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
  return resources_[name]->Load(lua_script, lua_script.GetTimerManager(), descriptor->root_path, descriptor->metadata->scripts);
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
  const DiscoveredResource* descriptor = FindLoadableResource(name);
  if (descriptor == nullptr || !descriptor->metadata) {
    return false;
  }

  auto it = resources_.find(name);
  if (it == resources_.end()) {
    SPDLOG_WARN("Resource '{}' does not exist", name);
    return false;
  }

  // Set execution context
  ScopedResourceContext ctx(*it->second);

  // Reload the resource
  return it->second->Reload(lua_script, lua_script.GetTimerManager(), descriptor->root_path, descriptor->metadata->scripts);
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

// Registers the server timer implementation with resource ownership tracking.
void ResourceManager::BindResourceAwareTimer(LuaScript& lua_script) {
  auto& timer_manager = lua_script.GetTimerManager();
  lua_script.GetLuaState().set_function("setTimer", ResourceSetTimerBinding{timer_manager});
  timer_manager.SetOwnerContextExecutor(ResourceTimerContext{*this});
}

// Registers the global resource-exports proxy.
void ResourceManager::BindExports(sol::state& lua) {
  sol::table proxy = lua.create_table();
  sol::table meta = lua.create_table();
  meta[sol::meta_function::index] = ResourceExportsIndex;
  proxy[sol::metatable_key] = meta;
  lua["exports"] = proxy;
}
