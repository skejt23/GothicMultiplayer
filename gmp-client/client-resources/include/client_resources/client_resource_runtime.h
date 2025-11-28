#pragma once

#include <optional>
#include <string>
#include <vector>

#include "client_resources/client_script.h"
#include "game_client.hpp"
#include "resource/loader.h"
#include "sol/sol.hpp"

class ClientResourceRuntime {
public:
  ClientResourceRuntime();
  ~ClientResourceRuntime();

  bool LoadResources(std::vector<gmp::client::GameClient::ResourcePayload> payloads, std::string& error_message);
  void UnloadResources();
  void ProcessTimers();

  std::optional<sol::table> GetExports(const std::string& resource_name) const;

  sol::state& GetLuaState();

  void SetServerInfoProvider(gmp::client::GameClient& game_client);

private:
  struct ResourceInstance {
    std::string name;
    sol::table env;
    sol::table exports;
    std::optional<gmp::resource::ResourcePack> pack;
    bool started = false;
  };

  void SetupRequire(ResourceInstance& instance);
  bool ExecuteEntryPoints(ResourceInstance& instance, std::string& error_message);
  bool InvokeLifecycle(ResourceInstance& instance, const char* hook, std::string& error_message);

  ClientScript script_;
  std::vector<ResourceInstance> resources_;
};
