/*
MIT License

Copyright (c) 2026 Gothic Multiplayer Team.

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

#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "Shared/lua_runtime/lua_diagnostics.h"

namespace {

std::string ExecuteFailingScript(std::string_view source) {
  sol::state lua;
  lua.open_libraries(sol::lib::base);
  ::lua::diagnostics::InstallErrorHandler(lua);

  sol::load_result chunk = lua.load_buffer(source.data(), source.size(), "@diagnostics_test.lua");
  EXPECT_TRUE(chunk.valid());
  if (!chunk.valid()) {
    sol::error error = chunk;
    return error.what();
  }

  sol::protected_function function = chunk;
  sol::protected_function_result result = function();
  EXPECT_FALSE(result.valid());
  if (result.valid()) {
    return {};
  }

  sol::error error = result;
  return error.what();
}

TEST(LuaDiagnostics, CapturesCallstackLocalsAndVarargs) {
  constexpr std::string_view source = R"(
local function inner(input, ...)
  local password = "damian"
  local count = 7
  local object = {}
  error("deliberate failure")
end

local function outer()
  inner("argument", "extra")
end

outer()
)";

  const std::string report = ExecuteFailingScript(source);

  EXPECT_NE(report.find("diagnostics_test.lua:6: deliberate failure"), std::string::npos);
  EXPECT_NE(report.find("callstack:"), std::string::npos);
  EXPECT_NE(report.find("in function 'inner'"), std::string::npos);
  EXPECT_NE(report.find("in function 'outer'"), std::string::npos);
  EXPECT_NE(report.find("locals:"), std::string::npos);
  EXPECT_NE(report.find("input = \"argument\" (string)"), std::string::npos);
  EXPECT_NE(report.find("password = \"damian\" (string)"), std::string::npos);
  EXPECT_NE(report.find("count = 7 (number)"), std::string::npos);
  EXPECT_NE(report.find("object = 0x"), std::string::npos);
  EXPECT_NE(report.find("...1 = \"extra\" (string)"), std::string::npos);
}

TEST(LuaDiagnostics, DoesNotInvokeUserToStringMetamethods) {
  constexpr std::string_view source = R"(
local dangerous = setmetatable({}, {
  __tostring = function()
    error("__tostring must not execute")
  end
})
error("original failure")
)";

  const std::string report = ExecuteFailingScript(source);

  EXPECT_NE(report.find("original failure"), std::string::npos);
  EXPECT_EQ(report.find("__tostring must not execute"), std::string::npos);
  EXPECT_NE(report.find("dangerous = 0x"), std::string::npos);
}

TEST(LuaDiagnostics, DoesNotRedactLocalValues) {
  constexpr std::string_view source = R"(
local password = "top-secret"
error("failure with sensitive local")
)";

  const std::string report = ExecuteFailingScript(source);

  EXPECT_NE(report.find("password = \"top-secret\" (string)"), std::string::npos);
  EXPECT_EQ(report.find("<redacted>"), std::string::npos);
}

TEST(LuaDiagnostics, CapturesBoundFunctionArgumentsAndGroupsLocalsByFrame) {
  sol::state lua;
  lua.open_libraries(sol::lib::base);
  ::lua::diagnostics::InstallErrorHandler(lua);
  lua.set_function("sendMessageToAll", [](int, int, int, const std::string&) {});

  constexpr std::string_view source = R"(function sendStuff(r, g, b)
  local prefix = "[SERVER]"
  sendMessageToAll(r, g, b)
end

function onResourceStart()
  local color = 0
  sendStuff(color, color, color)
end
)";
  sol::load_result chunk = lua.load_buffer(source.data(), source.size(), "@resources/prototype/server/main.lua");
  ASSERT_TRUE(chunk.valid());

  sol::protected_function define_functions = chunk;
  ASSERT_TRUE(define_functions().valid());

  sol::protected_function on_resource_start = lua["onResourceStart"];
  sol::protected_function_result result = on_resource_start();
  ASSERT_FALSE(result.valid());

  sol::error error = result;
  const std::string report = error.what();
  EXPECT_TRUE(report.starts_with("bad argument #4 to 'sendMessageToAll' (string expected, got no value)\ncallstack:"));
  EXPECT_NE(report.find("callstack:"), std::string::npos);
  EXPECT_NE(report.find("[C]: in function 'sendMessageToAll'"), std::string::npos);
  EXPECT_NE(report.find("resources/prototype/server/main.lua:3: in function 'sendStuff'"), std::string::npos);
  EXPECT_NE(report.find("arguments:"), std::string::npos);
  EXPECT_NE(report.find("#1 number: 0"), std::string::npos);
  EXPECT_NE(report.find("#2 number: 0"), std::string::npos);
  EXPECT_NE(report.find("#3 number: 0"), std::string::npos);
  EXPECT_NE(report.find("#4 <missing>"), std::string::npos);
  EXPECT_NE(report.find("locals:"), std::string::npos);
  EXPECT_NE(report.find("main.lua:3 in function 'sendStuff':"), std::string::npos);
  EXPECT_NE(report.find("r = 0 (number)"), std::string::npos);
  EXPECT_NE(report.find("g = 0 (number)"), std::string::npos);
  EXPECT_NE(report.find("b = 0 (number)"), std::string::npos);
  EXPECT_NE(report.find("prefix = \"[SERVER]\" (string)"), std::string::npos);
  EXPECT_NE(report.find("main.lua:8 in function <resources/prototype/server/main.lua:6>:"), std::string::npos);
  EXPECT_NE(report.find("color = 0 (number)"), std::string::npos);
}

}  // namespace
