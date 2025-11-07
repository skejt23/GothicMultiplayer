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

#include <lua.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace shared {

namespace {

// Writer callback for lua_dump: appends bytecode chunks to a vector
static int Writer(lua_State* /*L*/, const void* p, size_t sz, void* ud) {
  auto* out = static_cast<std::vector<std::uint8_t>*>(ud);
  const auto* bytes = static_cast<const std::uint8_t*>(p);
  out->insert(out->end(), bytes, bytes + sz);
  return 0;  // 0 signals success to Lua
}

}  // namespace

std::vector<std::uint8_t> CompileLuaFileToBytecode(const std::string& file_path, bool strip_debug) {
  std::vector<std::uint8_t> bytecode;

  lua_State* L = luaL_newstate();
  if (!L) {
    throw std::runtime_error("Lua: failed to create state");
  }

  // We don't need standard libraries to compile; loading/parsing is enough.
  // Load the Lua source file into a precompiled chunk on the stack.
  const int load_status = luaL_loadfilex(L, file_path.c_str(), nullptr);
  if (load_status != LUA_OK) {
    const char* err = lua_tostring(L, -1);
    std::string msg = err ? err : "unknown error";
    lua_close(L);
    throw std::runtime_error("Lua load error: " + msg);
  }

  // Dump the compiled chunk to bytecode buffer.
  const int dump_status = lua_dump(L, Writer, &bytecode, strip_debug ? 1 : 0);

  // Pop the chunk from the stack regardless of dump status
  lua_pop(L, 1);
  lua_close(L);

  if (dump_status != 0) {
    throw std::runtime_error("Lua dump error: failed to generate bytecode");
  }

  return bytecode;
}

}  // namespace shared
