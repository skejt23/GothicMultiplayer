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

#include "Script.h"

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

  void CreateTestResource(const std::string& name, const std::string& lua_content, bool create_server_dir = true) {
    auto resource_dir = test_resources_dir_ / name;
    auto server_dir = resource_dir / "server";

    if (create_server_dir) {
      std::filesystem::create_directories(server_dir);
      std::ofstream ofs(server_dir / "main.lua");
      ASSERT_TRUE(ofs.good()) << "Failed to create main.lua for resource " << name;
      ofs << lua_content;
    } else {
      std::filesystem::create_directories(resource_dir);
    }
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
  manager.CreateExportsProxy(lua_script.GetLuaState());

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
  manager.CreateExportsProxy(lua_script.GetLuaState());
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

TEST_F(ResourceManagerTest, ExportsProxyExecutesUnderTargetResourceContext) {
  manager.CreateExportsProxy(lua_script.GetLuaState());

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
  return RUN_ALL_TESTS();
}
