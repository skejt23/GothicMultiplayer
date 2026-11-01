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

#include "Shared/lua_runtime/lua_utility.h"

namespace {

TEST(LuaUtility, XxHashMatchesKnownVectors) {
  sol::state lua;
  lua.open_libraries(sol::lib::base);
  ::lua::bindings::BindUtilities(lua);

  const auto result = lua.safe_script(R"(
assert(xxh32("") == "02cc5d05")
assert(xxh64("") == "ef46db3751d8e999")
assert(xxh128("") == "99aa06d3014798d86001c324468d497f")
assert(xxh32("a") == "550d7456")
assert(xxh64("a") == "d24ec4f1a98c6e5b")
assert(xxh128("a") == "a96faf705af16834e6c632b61e964e1f")
assert(xxh128("a", 1) == "fdd9b77fdcaf3221d2f6d0996f37a720")
assert(xxh32("abc") == "32d153ff")
assert(xxh64("abc") == "44bc2cf5ad770999")
assert(xxh128("abc") == "06b05ab6733a618578af5f94892f3950")
)", sol::script_pass_on_error);

  if (!result.valid()) {
    const sol::error error = result;
    FAIL() << error.what();
  }
}

TEST(LuaUtility, XxHashHandlesBinaryStringsAndRejectsInvalidArguments) {
  sol::state lua;
  lua.open_libraries(sol::lib::base, sol::lib::string);
  ::lua::bindings::BindUtilities(lua);

  const auto result = lua.safe_script(R"(
for _, hash in ipairs({xxh32, xxh64, xxh128}) do
  assert(hash("a\0b") ~= hash("a"))
  assert(hash("a\0b") ~= hash("a\0c"))
  assert(hash("\0") ~= hash(""))
  assert(not pcall(hash))
  assert(not pcall(hash, nil))
  assert(not pcall(hash, {}))
end
assert(#xxh32(string.rep("\255\0", 128)) == 8)
assert(#xxh64(string.rep("\255\0", 128)) == 16)
assert(#xxh128(string.rep("\255\0", 128)) == 32)
)", sol::script_pass_on_error);

  if (!result.valid()) {
    const sol::error error = result;
    FAIL() << error.what();
  }
}

TEST(LuaUtility, XxHashSupportsTheSameSeedRange) {
  sol::state lua;
  lua.open_libraries(sol::lib::base);
  ::lua::bindings::BindUtilities(lua);

  const auto result = lua.safe_script(R"(
for _, hash in ipairs({xxh32, xxh64, xxh128}) do
  assert(hash("abc", 0) == hash("abc"))
  assert(hash("abc", nil) == hash("abc"))
  assert(hash("abc", 1) ~= hash("abc"))
  assert(hash("abc", 0xffffffff) ~= hash("abc"))
  for _, seed in ipairs({0, 1, 0x7fffffff, 0x80000000, 0xffffffff}) do
    assert(type(hash("abc", seed)) == "string")
  end
end
assert(#xxh32("abc", 123) == 8)
assert(#xxh64("abc", 123) == 16)
assert(#xxh128("abc", 123) == 32)
)", sol::script_pass_on_error);

  if (!result.valid()) {
    const sol::error error = result;
    FAIL() << error.what();
  }
}

TEST(LuaUtility, XxHashRejectsInvalidSeeds) {
  sol::state lua;
  lua.open_libraries(sol::lib::base, sol::lib::math);
  ::lua::bindings::BindUtilities(lua);

  const auto result = lua.safe_script(R"(
for _, hash in ipairs({xxh32, xxh64, xxh128}) do
  for _, seed in ipairs({false, {}, "1", "invalid", -1, 4294967296, math.mininteger, math.maxinteger, 1.5, 0x1p63, math.huge, 0/0}) do
    assert(not pcall(hash, "abc", seed))
  end
end
)", sol::script_pass_on_error);

  if (!result.valid()) {
    const sol::error error = result;
    FAIL() << error.what();
  }
}

}  // namespace
