/*
MIT License

Copyright (c) 2025 Gothic Multiplayer Team.

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

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include "Lua/event_bind.h"
#include "Script.h"
#include "Shared/lua_runtime/lua_value_codec.h"
#include "game_server.h"

namespace {

class ResourceManagerTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Create test resources directory structure
    test_resources_dir_ = std::filesystem::current_path() / "resources";
    std::filesystem::create_directories(test_resources_dir_);

    lua_script.GetLuaState().set_function("__test_getCurrentResourceName", []() {
      Resource* current = ResourceManager::GetCurrentResource();
      if (!current) {
        return std::string();
      }
      return current->GetName();
    });
  }

  void TearDown() override {
    // Clean up test resources
    std::error_code ec;
    std::filesystem::remove_all(test_resources_dir_, ec);
  }

  void WriteResourceToml(const std::string& name, const std::vector<std::string>& scripts) {
    auto resource_dir = test_resources_dir_ / name;
    std::filesystem::create_directories(resource_dir);

    std::ofstream ofs(resource_dir / "resource.toml");
    ASSERT_TRUE(ofs.good()) << "Failed to create resource.toml for resource " << name;
    ofs << "version = \"1.0.0\"\n";
    ofs << "author = \"Test\"\n";
    ofs << "description = \"Test resource\"\n";
    ofs << "scripts = [\n";
    for (const auto& script : scripts) {
      ofs << "  \"" << script << "\",\n";
    }
    ofs << "]\n";
  }

  void CreateTestResource(const std::string& name, const std::string& lua_content, bool create_server_dir = true) {
    auto resource_dir = test_resources_dir_ / name;
    auto server_dir = resource_dir / "server";
    std::vector<std::string> scripts;

    if (create_server_dir) {
      std::filesystem::create_directories(server_dir);
      std::ofstream ofs(server_dir / "main.lua");
      ASSERT_TRUE(ofs.good()) << "Failed to create main.lua for resource " << name;
      ofs << lua_content;
      scripts.push_back("server/main.lua");
    } else {
      std::filesystem::create_directories(resource_dir);
    }

    WriteResourceToml(name, scripts);
  }

  std::filesystem::path test_resources_dir_;
  LuaScript lua_script;
  ResourceManager manager;
};

TEST_F(ResourceManagerTest, DiscoversResourceDirectories) {
  CreateTestResource("core", "print('core')");
  CreateTestResource("admin", "print('admin')");
  CreateTestResource("gamemode", "print('gamemode')");

  auto discovered = manager.DiscoverResources();

  ASSERT_EQ(3u, discovered.size());
  // Should be sorted alphabetically
  EXPECT_EQ("admin", discovered[0]);
  EXPECT_EQ("core", discovered[1]);
  EXPECT_EQ("gamemode", discovered[2]);
}

TEST_F(ResourceManagerTest, ReturnsEmptyListWhenResourcesDirDoesNotExist) {
  std::filesystem::remove_all(test_resources_dir_);

  auto discovered = manager.DiscoverResources();

  EXPECT_TRUE(discovered.empty());
}

TEST_F(ResourceManagerTest, LoadsResourceSuccessfully) {
  CreateTestResource("test_resource", R"(
    print('Hello from test_resource')
    exports = {}
    exports.testFunc = function()
      return 42
    end
  )");

  EXPECT_TRUE(manager.LoadResource("test_resource", lua_script));

  auto resource_opt = manager.GetResource("test_resource");
  ASSERT_TRUE(resource_opt.has_value());

  Resource& resource = resource_opt->get();
  EXPECT_EQ("test_resource", resource.GetName());
  EXPECT_TRUE(resource.IsLoaded());
  EXPECT_EQ(1u, resource.GetGeneration());
}

TEST_F(ResourceManagerTest, LoadResourceFailsWhenScriptErrors) {
  CreateTestResource("bad_resource", R"(
    exports = {}
    error('boom')
  )");

  EXPECT_FALSE(manager.LoadResource("bad_resource", lua_script));

  auto resource_opt = manager.GetResource("bad_resource");
  ASSERT_TRUE(resource_opt.has_value());
  EXPECT_FALSE(resource_opt->get().IsLoaded());
  EXPECT_EQ(0u, resource_opt->get().GetGeneration());
}

TEST_F(ResourceManagerTest, LoadResourceFailsWhenStartHookHasMissingArguments) {
  lua_script.GetLuaState().set_function("__requiresArgument", [](int value) { return value; });
  CreateTestResource("bad_start_resource", R"(
    function onResourceStart()
      __requiresArgument()
    end
  )");

  EXPECT_FALSE(manager.LoadResource("bad_start_resource", lua_script));

  auto resource_opt = manager.GetResource("bad_start_resource");
  ASSERT_TRUE(resource_opt.has_value());
  EXPECT_FALSE(resource_opt->get().IsLoaded());
  EXPECT_EQ(0u, resource_opt->get().GetGeneration());
}

TEST_F(ResourceManagerTest, GenerationOnlyAdvancesOnSuccessfulLoad) {
  const std::string resource_name = "flaky_resource";
  CreateTestResource(resource_name, R"(
    error('first load fails')
  )");

  EXPECT_FALSE(manager.LoadResource(resource_name, lua_script));

  auto resource_opt = manager.GetResource(resource_name);
  ASSERT_TRUE(resource_opt.has_value());
  EXPECT_EQ(0u, resource_opt->get().GetGeneration());

  // Fix the script and load again
  auto script_path = test_resources_dir_ / resource_name / "server" / "main.lua";
  std::ofstream ofs(script_path, std::ios::trunc);
  ASSERT_TRUE(ofs.good());
  ofs << R"(
    exports = {}
    exports.answer = function()
      return 42
    end
  )";

  EXPECT_TRUE(manager.LoadResource(resource_name, lua_script));
  EXPECT_TRUE(resource_opt->get().IsLoaded());
  EXPECT_EQ(1u, resource_opt->get().GetGeneration());
}

TEST_F(ResourceManagerTest, PreventsLoadingAlreadyLoadedResource) {
  CreateTestResource("test_resource", "print('test')");

  EXPECT_TRUE(manager.LoadResource("test_resource", lua_script));
  EXPECT_FALSE(manager.LoadResource("test_resource", lua_script));
}

TEST_F(ResourceManagerTest, UnloadsResourceSuccessfully) {
  CreateTestResource("test_resource", "print('test')");

  manager.LoadResource("test_resource", lua_script);
  manager.UnloadResource("test_resource", lua_script);

  auto resource_opt = manager.GetResource("test_resource");
  ASSERT_TRUE(resource_opt.has_value());
  EXPECT_FALSE(resource_opt->get().IsLoaded());
}

TEST_F(ResourceManagerTest, UnloadResourceRemovesOwnedEventHandlers) {
  std::vector<std::string> calls;
  lua_script.GetLuaState().set_function("__record", [&](const std::string& call) { calls.push_back(call); });

  CreateTestResource("event_resource", R"(
    addEvent("cleanup_test")
    addEventHandler("cleanup_test", function()
      __record("cleanup_test")
    end)
  )");

  ASSERT_TRUE(manager.LoadResource("event_resource", lua_script));
  sol::protected_function call_event = lua_script.GetLuaState()["callEvent"];

  sol::protected_function_result first_result = call_event("cleanup_test", sol::nil);
  ASSERT_TRUE(first_result.valid());
  EXPECT_TRUE(first_result.get<bool>());
  ASSERT_EQ(calls.size(), 1u);

  manager.UnloadResource("event_resource", lua_script);
  sol::protected_function_result second_result = call_event("cleanup_test", sol::nil);
  ASSERT_TRUE(second_result.valid());
  EXPECT_FALSE(second_result.get<bool>());
  EXPECT_EQ(calls.size(), 1u);
}

TEST_F(ResourceManagerTest, MultipleServerScriptsKeepDistinctLifecycleHooks) {
  auto server_dir = test_resources_dir_ / "multi_hook_resource" / "server";
  std::filesystem::create_directories(server_dir);
  WriteResourceToml("multi_hook_resource", {"server/b.lua", "server/a.lua"});

  {
    std::ofstream ofs(server_dir / "a.lua");
    ASSERT_TRUE(ofs.good());
    ofs << R"(
      function onResourceStart()
        __record('a_start')
      end

      function onResourceStop()
        __record('a_stop')
      end
    )";
  }

  {
    std::ofstream ofs(server_dir / "b.lua");
    ASSERT_TRUE(ofs.good());
    ofs << R"(
      function onResourceStart()
        __record('b_start')
      end

      function onResourceStop()
        __record('b_stop')
      end
    )";
  }

  std::vector<std::string> calls;
  lua_script.GetLuaState().set_function("__record", [&](const std::string& call) { calls.push_back(call); });

  ASSERT_TRUE(manager.LoadResource("multi_hook_resource", lua_script));

  ASSERT_EQ(calls.size(), 2u);
  EXPECT_EQ(calls[0], "b_start");
  EXPECT_EQ(calls[1], "a_start");

  manager.UnloadResource("multi_hook_resource", lua_script);

  ASSERT_EQ(calls.size(), 4u);
  EXPECT_EQ(calls[2], "a_stop");
  EXPECT_EQ(calls[3], "b_stop");
}

TEST_F(ResourceManagerTest, ReloadsResourceSuccessfully) {
  CreateTestResource("test_resource", "print('test')");

  manager.LoadResource("test_resource", lua_script);

  auto resource_opt = manager.GetResource("test_resource");
  ASSERT_TRUE(resource_opt.has_value());
  std::uint32_t initial_gen = resource_opt->get().GetGeneration();

  EXPECT_TRUE(manager.ReloadResource("test_resource", lua_script));

  EXPECT_EQ(initial_gen + 1, resource_opt->get().GetGeneration());
  EXPECT_TRUE(resource_opt->get().IsLoaded());
}

TEST_F(ResourceManagerTest, GetResourceReturnsNulloptForNonexistentResource) {
  auto resource_opt = manager.GetResource("nonexistent");
  EXPECT_FALSE(resource_opt.has_value());
}

TEST_F(ResourceManagerTest, ExportsProxyCreatesGlobalTable) {
  manager.BindExports(lua_script.GetLuaState());

  auto& lua = lua_script.GetLuaState();

  // Test that the global exports table exists
  sol::object exports_obj = lua["exports"];
  ASSERT_TRUE(exports_obj.valid());
  EXPECT_EQ(sol::type::table, exports_obj.get_type());
}

TEST_F(ResourceManagerTest, ExportsProxyReturnsNilForUnloadedResource) {
  CreateTestResource("resource_a", R"(
    exports = {}
    exports.getValue = function()
      return 100
    end
  )");

  manager.LoadResource("resource_a", lua_script);
  manager.BindExports(lua_script.GetLuaState());
  manager.UnloadResource("resource_a", lua_script);

  auto& lua = lua_script.GetLuaState();
  sol::table exports = lua["exports"];
  sol::object resource_a_exports = exports["resource_a"];

  EXPECT_EQ(sol::type::nil, resource_a_exports.get_type());
}

TEST_F(ResourceManagerTest, ScopedResourceContextRestoresPreviousContext) {
  CreateTestResource("resource_a", "print('a')");
  CreateTestResource("resource_b", "print('b')");

  manager.LoadResource("resource_a", lua_script);
  manager.LoadResource("resource_b", lua_script);

  auto resource_a_opt = manager.GetResource("resource_a");
  auto resource_b_opt = manager.GetResource("resource_b");
  ASSERT_TRUE(resource_a_opt.has_value());
  ASSERT_TRUE(resource_b_opt.has_value());

  EXPECT_EQ(nullptr, ResourceManager::GetCurrentResource());

  {
    ResourceManager::ScopedResourceContext ctx_a(resource_a_opt->get());
    EXPECT_EQ(&resource_a_opt->get(), ResourceManager::GetCurrentResource());

    {
      ResourceManager::ScopedResourceContext ctx_b(resource_b_opt->get());
      EXPECT_EQ(&resource_b_opt->get(), ResourceManager::GetCurrentResource());
    }

    EXPECT_EQ(&resource_a_opt->get(), ResourceManager::GetCurrentResource());
  }

  EXPECT_EQ(nullptr, ResourceManager::GetCurrentResource());
}

TEST_F(ResourceManagerTest, ResourceAwareTimerCapturesOwnership) {
  CreateTestResource("test_resource", R"(
    timer_id = setTimer(function()
      print('timer fired')
    end, 1000, 1)
  )");

  manager.BindResourceAwareTimer(lua_script);
  manager.LoadResource("test_resource", lua_script);

  auto resource_opt = manager.GetResource("test_resource");
  ASSERT_TRUE(resource_opt.has_value());

  sol::environment& env = resource_opt->get().GetEnvironment();
  sol::object timer_id = env["timer_id"];
  EXPECT_TRUE(timer_id.valid());
  EXPECT_EQ(sol::type::number, timer_id.get_type());
}

TEST_F(ResourceManagerTest, TimerCallbackRunsWithinOwningResourceContext) {
  CreateTestResource("timer_resource", R"(
    exports = {}
    exports.timer_owner = nil
    timer_id = setTimer(function()
      exports.timer_owner = __test_getCurrentResourceName()
    end, 50, 1)
  )");

  manager.BindResourceAwareTimer(lua_script);
  manager.LoadResource("timer_resource", lua_script);

  // Wait for the timer interval to elapse and process callbacks
  std::this_thread::sleep_for(std::chrono::milliseconds(60));
  lua_script.ProcessTimers();

  auto resource_opt = manager.GetResource("timer_resource");
  ASSERT_TRUE(resource_opt.has_value());
  sol::table exports = resource_opt->get().GetExports();
  sol::object recorded_owner = exports["timer_owner"];
  ASSERT_TRUE(recorded_owner.valid());
  ASSERT_EQ(sol::type::string, recorded_owner.get_type());
  EXPECT_EQ("timer_resource", recorded_owner.as<std::string>());
}

TEST_F(ResourceManagerTest, CustomEventsKeepPayloadAndTrustedSenderSeparate) {
  CreateTestResource("event_contract", R"(
    exports = {}
    assert(addEvent('contract_outer', true))
    assert(addEvent('contract_inner'))
    local nestedContext
    assert(addEventHandler('contract_inner', function(context, payload)
      nestedContext = context
      assert(payload == 42 and context.sourceId == 8 and not context.remote)
    end))
    local function handler(context, payload)
      assert(callEvent('contract_inner', {sourceId = 8}, 42))
      assert(context ~= nestedContext)
      if context.remote then
        assert(context.senderPlayerId == 23 and context.sourceId == 17)
        assert(payload.senderPlayerId == 999)
        exports.received = true
      else
        assert(payload == 42 and context.senderPlayerId == nil and context.sourceId == nil)
        exports.localReceived = true
      end
    end
    assert(addEventHandler('contract_outer', handler))
    exports.runLocal = function()
      assert(callEvent('contract_outer', nil, 42))
    end
    exports.unsubscribe = function()
      assert(removeEventHandler('contract_outer', handler))
      assert(not removeEventHandler('contract_outer', handler))
    end
  )");
  ASSERT_TRUE(manager.LoadResource("event_contract", lua_script));
  auto& lua = lua_script.GetLuaState();
  sol::table forged_payload = lua.create_table_with("senderPlayerId", 999);
  ASSERT_TRUE(lua::bindings::TriggerRemoteEvent(lua, "contract_outer", 23, 17, {sol::make_object(lua, forged_payload)}));
  auto resource = manager.GetResource("event_contract");
  ASSERT_TRUE(resource);
  sol::table exports = resource->get().GetExports();
  ResourceManager::ScopedResourceContext context(resource->get());
  sol::protected_function run_local = exports["runLocal"];
  ASSERT_TRUE(run_local().valid());
  EXPECT_TRUE(exports["localReceived"].get<bool>());
  EXPECT_TRUE(exports["received"].get<bool>());
  sol::protected_function unsubscribe = exports["unsubscribe"];
  EXPECT_TRUE(unsubscribe().valid());
}

TEST_F(ResourceManagerTest, CustomEventsPreserveVariadicArgumentsAndRemotePolicy) {
  CreateTestResource("event_arguments", R"(
    exports = {localCalls = 0, remoteCalls = 0}
    assert(emitEvent == nil and sendClientEvent == nil)
    assert(addEvent('argument_contract', true))
    assert(addEvent('local_only'))
    assert(addEventHandler('argument_contract', function(context, ...)
      local count = select('#', ...)
      if context.remote then
        assert(context.senderPlayerId == 23 and context.sourceId == nil)
        exports.remoteCalls = exports.remoteCalls + 1
      else
        assert(context.senderPlayerId == nil and context.sourceId == nil)
        exports.localCalls = exports.localCalls + 1
      end
      if count > 0 then
        local first, second, third, fourth = ...
        assert(count == 4 and first == 42 and second == nil and third.value == 'table' and fourth == nil)
      end
      exports.lastCount = count
    end))
    exports.runLocal = function()
      assert(callEvent('argument_contract', nil))
      assert(callEvent('argument_contract', nil, 42, nil, {value = 'table'}, nil))
      assert(not pcall(callEvent, 'argument_contract', 42))
      assert(not pcall(callEvent, 'argument_contract', {sourceId = 1.5}))
      assert(not callEvent('not_registered', nil))
    end
  )");
  ASSERT_TRUE(manager.LoadResource("event_arguments", lua_script));
  auto resource = manager.GetResource("event_arguments");
  ASSERT_TRUE(resource);
  sol::table exports = resource->get().GetExports();
  ResourceManager::ScopedResourceContext context(resource->get());
  sol::protected_function run_local = exports["runLocal"];
  ASSERT_TRUE(run_local().valid());
  EXPECT_EQ(exports["localCalls"].get<int>(), 2);
  EXPECT_EQ(exports["lastCount"].get<int>(), 4);

  auto& lua = lua_script.GetLuaState();
  sol::table table = lua.create_table_with("value", "table");
  const std::vector<sol::object> arguments{sol::make_object(lua, 42), sol::make_object(lua, sol::nil), sol::make_object(lua, table),
                                           sol::make_object(lua, sol::nil)};
  std::string encoded, error;
  ASSERT_TRUE(gmp::lua::EncodeLuaArgs(lua, arguments, encoded, error)) << error;
  std::vector<sol::object> decoded;
  ASSERT_TRUE(gmp::lua::DecodeLuaArgs(lua, encoded, decoded, error)) << error;
  EXPECT_FALSE(lua::bindings::TriggerRemoteEvent(lua, "local_only", 23, 0, decoded));
  EXPECT_FALSE(lua::bindings::TriggerRemoteEvent(lua, "not_registered", 23, 0, decoded));
  ASSERT_TRUE(lua::bindings::TriggerRemoteEvent(lua, "argument_contract", 23, 0, decoded));
  EXPECT_EQ(exports["remoteCalls"].get<int>(), 1);
  EXPECT_EQ(exports["lastCount"].get<int>(), 4);
  ASSERT_TRUE(lua::bindings::TriggerRemoteEvent(lua, "argument_contract", 23, 0, {}));
  EXPECT_EQ(exports["remoteCalls"].get<int>(), 2);
  EXPECT_EQ(exports["lastCount"].get<int>(), 0);
}

TEST_F(ResourceManagerTest, SharedEventBindingsKeepServerResourceOwnership) {
  CreateTestResource("event_owner", R"(
    exports = {calls = 0}
    assert(addEvent('owned_contract'))
    exports.handler = function()
      assert(__test_getCurrentResourceName() == 'event_owner')
      exports.calls = exports.calls + 1
    end
    assert(addEventHandler('owned_contract', exports.handler))
  )");
  CreateTestResource("event_other", "exports = {}");
  ASSERT_TRUE(manager.LoadResource("event_owner", lua_script));
  ASSERT_TRUE(manager.LoadResource("event_other", lua_script));
  auto owner = manager.GetResource("event_owner");
  auto other = manager.GetResource("event_other");
  ASSERT_TRUE(owner && other);
  auto& lua = lua_script.GetLuaState();
  sol::table owner_exports = owner->get().GetExports();
  lua["foreign_handler"] = owner_exports["handler"];
  {
    ResourceManager::ScopedResourceContext context(other->get());
    ASSERT_TRUE(lua.safe_script(R"(
      removeEvent('owned_contract')
      assert(not removeEventHandler('owned_contract', foreign_handler))
      assert(callEvent('owned_contract', nil))
    )", sol::script_pass_on_error).valid());
  }
  EXPECT_EQ(owner_exports["calls"].get<int>(), 1);
  {
    ResourceManager::ScopedResourceContext context(owner->get());
    ASSERT_TRUE(lua.safe_script(R"(
      assert(removeEventHandler('owned_contract', foreign_handler))
      removeEvent('owned_contract')
      assert(not callEvent('owned_contract', nil))
    )", sol::script_pass_on_error).valid());
  }
}

TEST_F(ResourceManagerTest, RouteRepresentsACompletePathWithConsistentWaypointCount) {
  const auto navigation = std::filesystem::current_path() / "data" / "navigation";
  std::filesystem::create_directories(navigation);
  const auto file = navigation / "api_route_contract.json";
  {
    std::ofstream data(file);
    data << R"({"zen":"API_ROUTE_CONTRACT.ZEN","waypoints":[
      {"name":"A","position":{"x":0,"y":0,"z":0,"angle":0},"connectsTo":["B"]},
      {"name":"B","position":{"x":10,"y":0,"z":0,"angle":0},"connectsTo":["C"]},
      {"name":"C","position":{"x":20,"y":0,"z":0,"angle":0},"connectsTo":[]}
    ]})";
  }
  const auto result = lua_script.GetLuaState().safe_script(R"(
    assert(Way == nil)
    local route = Route.new('API_ROUTE_CONTRACT.ZEN', 'A', 'C')
    local names = route:getWaypointNames()
    assert(type(names) == 'table' and #names == 3 and names[0] == nil)
    assert(names[1] == 'A' and names[2] == 'B' and names[3] == 'C')
    assert(route:getWaypointCount() == #names and route:getStart() == 'A' and route:getEnd() == 'C')
    local missing = Route.new('API_ROUTE_CONTRACT.ZEN', 'A', 'UNKNOWN')
    assert(missing:getWaypointCount() == 0 and #missing:getWaypointNames() == 0)
  )", sol::script_pass_on_error);
  std::filesystem::remove(file);
  if (!result.valid()) {
    const sol::error error = result;
    FAIL() << error.what();
  }
}

TEST_F(ResourceManagerTest, ExportsProxyExecutesUnderTargetResourceContext) {
  manager.BindExports(lua_script.GetLuaState());

  CreateTestResource("resource_b", R"(
    exports = {}
    exports.getOwner = function()
      return __test_getCurrentResourceName()
    end
  )");

  CreateTestResource("resource_a", R"(
    local exportsProxy = exports
    exports = {}
    exports.other_owner = nil

    exports.invoke = function()
      local target = exportsProxy.resource_b
      if target ~= nil then
        exports.other_owner = target.getOwner()
      else
        exports.other_owner = ""
      end
    end
  )");

  manager.LoadResource("resource_b", lua_script);
  manager.LoadResource("resource_a", lua_script);

  auto& lua = lua_script.GetLuaState();
  sol::table exports_proxy = lua["exports"];
  ASSERT_TRUE(exports_proxy.valid());
  sol::object meta_obj = exports_proxy[sol::metatable_key];
  ASSERT_TRUE(meta_obj.valid());
  sol::table meta_table = meta_obj;
  sol::object index_fn = meta_table[sol::meta_function::index];
  ASSERT_TRUE(index_fn.valid());

  auto resource_a_opt = manager.GetResource("resource_a");
  ASSERT_TRUE(resource_a_opt.has_value());
  sol::table resource_a_exports = resource_a_opt->get().GetExports();
  ASSERT_TRUE(resource_a_exports.valid());
  sol::protected_function invoke = resource_a_exports["invoke"];
  ASSERT_TRUE(invoke.valid());

  ResourceManager::ScopedResourceContext ctx(resource_a_opt->get());
  auto call_result = invoke();
  ASSERT_TRUE(call_result.valid());

  sol::object other_owner = resource_a_exports["other_owner"];
  ASSERT_TRUE(other_owner.valid());
  ASSERT_EQ(sol::type::string, other_owner.get_type());
  EXPECT_EQ("resource_b", other_owner.as<std::string>());
}

}  // namespace

int main(int argc, char** argv) {
  ::testing::GTEST_FLAG(catch_exceptions) = false;
  ::testing::InitGoogleTest(&argc, argv);
  const auto project = std::filesystem::current_path();
  const auto scratch = project / "build" / "resource-manager-tests";
  std::filesystem::create_directories(scratch);
  std::filesystem::current_path(scratch);
  const auto result = RUN_ALL_TESTS();
  std::filesystem::current_path(project);
  return result;
}
