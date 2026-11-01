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

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "client_resources/client_script.h"
#include "game_client.hpp"
#include "resource/loader.h"
#include "sol/sol.hpp"

class ClientResourceRuntime {
public:
  using ResetCallback = std::function<void()>;

  ClientResourceRuntime();
  ~ClientResourceRuntime();

  bool LoadResources(std::vector<gmp::client::GameClient::ResourcePayload> payloads, std::string& error_message);
  void UnloadResources();
  void ProcessTimers();

  std::optional<sol::table> GetExports(const std::string& resource_name) const;

  sol::state& GetLuaState();

  void SetServerInfoProvider(gmp::client::GameClient& game_client);

  // Set a callback to be invoked when resources are unloaded (e.g., to reset domain-specific events)
  void SetResetCallback(ResetCallback callback) {
    reset_callback_ = std::move(callback);
  }

private:
  struct ResourceInstance {
    std::string name;
    sol::table env;
    sol::table exports;
    std::optional<gmp::resource::ResourcePack> pack;
    std::vector<sol::protected_function> start_hooks;
    std::vector<sol::protected_function> stop_hooks;
    std::vector<const void*> start_hook_ids;
    std::vector<const void*> stop_hook_ids;
    bool started = false;
  };

  void SetupRequire(ResourceInstance& instance);
  bool ExecuteEntryPoints(ResourceInstance& instance, std::string& error_message);
  void CaptureLifecycleHooks(ResourceInstance& instance);
  void CaptureLifecycleHook(ResourceInstance& instance, const char* hook);
  bool InvokeLifecycle(ResourceInstance& instance, const char* hook, std::string& error_message);

  ClientScript script_;
  std::vector<std::unique_ptr<ResourceInstance>> resources_;
  ResetCallback reset_callback_;
};
