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

#include "lua/lua_compiler.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <lua.hpp>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {

class LuaCompilerTest : public ::testing::Test {
protected:
  void SetUp() override {
    test_dir_ = std::filesystem::current_path() / "lua_test_files";
    std::filesystem::create_directories(test_dir_);
  }

  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove_all(test_dir_, ec);
  }

  void WriteTestFile(const std::string& filename, const std::string& content) {
    std::filesystem::path file_path = test_dir_ / filename;
    std::ofstream ofs(file_path, std::ios::binary);
    ASSERT_TRUE(ofs.good()) << "Unable to create test file: " << filename;
    ofs << content;
  }

  std::filesystem::path GetTestFilePath(const std::string& filename) {
    return test_dir_ / filename;
  }

  bool VerifyBytecode(const std::vector<std::uint8_t>& bytecode) {
    // Lua bytecode should start with the magic header
    // LUA_SIGNATURE = "\x1bLua"
    if (bytecode.size() < 4)
      return false;
    return bytecode[0] == 0x1b && bytecode[1] == 'L' && bytecode[2] == 'u' && bytecode[3] == 'a';
  }

  bool CanExecuteBytecode(const std::vector<std::uint8_t>& bytecode) {
    lua_State* L = luaL_newstate();
    if (!L)
      return false;

    // Load standard libraries for testing
    luaL_openlibs(L);

    int status = luaL_loadbuffer(L, reinterpret_cast<const char*>(bytecode.data()), bytecode.size(), "test_chunk");

    bool success = (status == LUA_OK);

    if (success) {
      // Try to execute the chunk
      status = lua_pcall(L, 0, 0, 0);
      success = (status == LUA_OK);
    }

    lua_close(L);
    return success;
  }

  std::filesystem::path test_dir_;
};

// Test basic compilation of a simple Lua script
TEST_F(LuaCompilerTest, CompilesSimpleScript) {
  WriteTestFile("simple.lua", "print('Hello, World!')");

  auto bytecode = shared::CompileLuaFileToBytecode(GetTestFilePath("simple.lua").string());

  EXPECT_FALSE(bytecode.empty());
  EXPECT_TRUE(VerifyBytecode(bytecode));
  EXPECT_TRUE(CanExecuteBytecode(bytecode));
}

// Test compilation with debug symbols stripped
TEST_F(LuaCompilerTest, CompilesWithDebugStripped) {
  WriteTestFile("with_debug.lua", "local x = 42\nprint(x)");

  auto bytecode_stripped = shared::CompileLuaFileToBytecode(GetTestFilePath("with_debug.lua").string(), true);
  auto bytecode_with_debug = shared::CompileLuaFileToBytecode(GetTestFilePath("with_debug.lua").string(), false);

  EXPECT_FALSE(bytecode_stripped.empty());
  EXPECT_FALSE(bytecode_with_debug.empty());

  // Stripped bytecode should be smaller or equal
  EXPECT_LE(bytecode_stripped.size(), bytecode_with_debug.size());

  EXPECT_TRUE(VerifyBytecode(bytecode_stripped));
  EXPECT_TRUE(VerifyBytecode(bytecode_with_debug));
}

// Test compilation of a more complex script with functions
TEST_F(LuaCompilerTest, CompilesComplexScript) {
  WriteTestFile("complex.lua", R"(
    local function add(a, b)
      return a + b
    end
    
    local function multiply(a, b)
      return a * b
    end
    
    local result = add(5, 3) * multiply(2, 4)
    return result
  )");

  auto bytecode = shared::CompileLuaFileToBytecode(GetTestFilePath("complex.lua").string());

  EXPECT_FALSE(bytecode.empty());
  EXPECT_TRUE(VerifyBytecode(bytecode));
  EXPECT_TRUE(CanExecuteBytecode(bytecode));
}

// Test compilation with Lua tables and loops
TEST_F(LuaCompilerTest, CompilesScriptWithTablesAndLoops) {
  WriteTestFile("tables.lua", R"(
    local items = {1, 2, 3, 4, 5}
    local sum = 0
    for i, v in ipairs(items) do
      sum = sum + v
    end
    return sum
  )");

  auto bytecode = shared::CompileLuaFileToBytecode(GetTestFilePath("tables.lua").string());

  EXPECT_FALSE(bytecode.empty());
  EXPECT_TRUE(VerifyBytecode(bytecode));
  EXPECT_TRUE(CanExecuteBytecode(bytecode));
}

// Test error handling for non-existent file
TEST_F(LuaCompilerTest, ThrowsOnNonExistentFile) {
  EXPECT_THROW({ shared::CompileLuaFileToBytecode(GetTestFilePath("nonexistent.lua").string()); }, std::runtime_error);
}

// Test error handling for invalid Lua syntax
TEST_F(LuaCompilerTest, ThrowsOnSyntaxError) {
  WriteTestFile("syntax_error.lua", "local x = ");  // Incomplete statement

  EXPECT_THROW({ shared::CompileLuaFileToBytecode(GetTestFilePath("syntax_error.lua").string()); }, std::runtime_error);
}

// Test error handling for invalid Lua code
TEST_F(LuaCompilerTest, ThrowsOnInvalidLuaCode) {
  WriteTestFile("invalid.lua", "function end");  // Invalid function syntax

  EXPECT_THROW({ shared::CompileLuaFileToBytecode(GetTestFilePath("invalid.lua").string()); }, std::runtime_error);
}

// Test compilation of empty file
TEST_F(LuaCompilerTest, CompilesEmptyFile) {
  WriteTestFile("empty.lua", "");

  auto bytecode = shared::CompileLuaFileToBytecode(GetTestFilePath("empty.lua").string());

  EXPECT_FALSE(bytecode.empty());
  EXPECT_TRUE(VerifyBytecode(bytecode));
  EXPECT_TRUE(CanExecuteBytecode(bytecode));
}

// Test compilation with comments
TEST_F(LuaCompilerTest, CompilesScriptWithComments) {
  WriteTestFile("comments.lua", R"(
    -- This is a single-line comment
    local x = 10
    
    --[[
      This is a multi-line comment
      It can span multiple lines
    ]]
    
    local y = 20
    return x + y
  )");

  auto bytecode = shared::CompileLuaFileToBytecode(GetTestFilePath("comments.lua").string());

  EXPECT_FALSE(bytecode.empty());
  EXPECT_TRUE(VerifyBytecode(bytecode));
  EXPECT_TRUE(CanExecuteBytecode(bytecode));
}

// Test compilation with string literals
TEST_F(LuaCompilerTest, CompilesScriptWithStrings) {
  WriteTestFile("strings.lua", R"(
    local str1 = "Hello"
    local str2 = 'World'
    local str3 = [[Multi-line
    string]]
    local str4 = [===[Raw string]===]
    return str1 .. " " .. str2
  )");

  auto bytecode = shared::CompileLuaFileToBytecode(GetTestFilePath("strings.lua").string());

  EXPECT_FALSE(bytecode.empty());
  EXPECT_TRUE(VerifyBytecode(bytecode));
  EXPECT_TRUE(CanExecuteBytecode(bytecode));
}

// Test that bytecode is deterministic (same input produces same output)
TEST_F(LuaCompilerTest, BytecodeIsDeterministic) {
  WriteTestFile("deterministic.lua", "local x = 42\nreturn x * 2");

  auto bytecode1 = shared::CompileLuaFileToBytecode(GetTestFilePath("deterministic.lua").string(), true);
  auto bytecode2 = shared::CompileLuaFileToBytecode(GetTestFilePath("deterministic.lua").string(), true);

  EXPECT_EQ(bytecode1, bytecode2);
}

// Test compilation with UTF-8 content
TEST_F(LuaCompilerTest, CompilesScriptWithUTF8) {
  WriteTestFile("utf8.lua", R"(
    local message = "Привет, мир! 你好世界!"
    return message
  )");

  auto bytecode = shared::CompileLuaFileToBytecode(GetTestFilePath("utf8.lua").string());

  EXPECT_FALSE(bytecode.empty());
  EXPECT_TRUE(VerifyBytecode(bytecode));
  EXPECT_TRUE(CanExecuteBytecode(bytecode));
}

// Test compilation of script with metatables
TEST_F(LuaCompilerTest, CompilesScriptWithMetatables) {
  WriteTestFile("metatables.lua", R"(
    local mt = {
      __add = function(a, b)
        return {value = a.value + b.value}
      end
    }
    local obj1 = setmetatable({value = 10}, mt)
    local obj2 = setmetatable({value = 20}, mt)
    local result = obj1 + obj2
    return result.value
  )");

  auto bytecode = shared::CompileLuaFileToBytecode(GetTestFilePath("metatables.lua").string());

  EXPECT_FALSE(bytecode.empty());
  EXPECT_TRUE(VerifyBytecode(bytecode));
  EXPECT_TRUE(CanExecuteBytecode(bytecode));
}

}  // namespace

int main(int argc, char** argv) {
  ::testing::GTEST_FLAG(catch_exceptions) = false;
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
