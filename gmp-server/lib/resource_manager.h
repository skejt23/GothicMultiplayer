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

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "resource.h"
#include "sol/sol.hpp"

// Forward declarations
class LuaScript;
class TimerManager;

// Manages all resources on the server
// Handles discovery, loading, unloading, and the exports proxy system
class ResourceManager {
public:
  ResourceManager();
  ~ResourceManager();

  // Auto-discover resources from resources/ directory
  std::vector<std::string> DiscoverResources();

  // Load a resource by name
  bool LoadResource(const std::string& name, LuaScript& lua_script);

  // Unload a resource by name
  void UnloadResource(const std::string& name, LuaScript& lua_script);

  // Reload a resource by name
  bool ReloadResource(const std::string& name, LuaScript& lua_script);

  // Create the global exports proxy table
  // This allows resources to call exports.otherResource.func()
  void CreateExportsProxy(sol::state& lua);

  // Override setTimer in Lua to capture current resource for ownership tracking
  void BindResourceAwareTimer(LuaScript& lua_script);

  // Get a resource by name (non-owning reference)
  std::optional<std::reference_wrapper<Resource>> GetResource(const std::string& name);

  // Get all loaded resources
  const std::unordered_map<std::string, std::unique_ptr<Resource>>& GetResources() const {
    return resources_;
  }

  // RAII guard for setting current resource context
  class ScopedResourceContext {
  public:
    explicit ScopedResourceContext(Resource& resource);
    ~ScopedResourceContext();
    ScopedResourceContext(const ScopedResourceContext&) = delete;
    ScopedResourceContext& operator=(const ScopedResourceContext&) = delete;

  private:
    Resource* previous_;  // Non-owning, nullable (context can be null)
  };

  // Get the current resource being executed (for timer/event ownership tracking)
  // Returns non-owning pointer, may be nullptr if no resource is executing
  static Resource* GetCurrentResource();
  static ResourceManager* GetActiveInstance();

  template <typename Callable>
  decltype(auto) WithResourceContext(Resource& resource, Callable&& callable) {
    ScopedResourceContext ctx(resource);
    return std::forward<Callable>(callable)();
  }

private:
  static void SetCurrentResource(Resource* resource);  // Non-owning

  std::unordered_map<std::string, std::unique_ptr<Resource>> resources_;
  static thread_local Resource* current_resource_;  // Non-owning execution context tracker
  static ResourceManager* active_instance_;
};
