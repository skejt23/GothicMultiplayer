// sol2
//
// The MIT License (MIT)
//
// Copyright (c) 2013-2022 Rapptz, ThePhD and contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
// of the Software, and to permit persons to whom the Software is furnished to do
// so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef SOL_ERROR_HANDLER_HPP
#define SOL_ERROR_HANDLER_HPP

#include <sol/types.hpp>

#include <string>

namespace sol {

namespace detail {
constexpr const char* not_a_number = "not a numeric type";
constexpr const char* not_a_number_or_number_string = "not a numeric type or numeric string";
constexpr const char* not_a_number_integral = "not a numeric type that fits exactly an integer (number maybe has significant decimals)";
constexpr const char* not_a_number_or_number_string_integral =
    "not a numeric type or a numeric string that fits exactly an integer (e.g. number maybe has significant decimals)";

constexpr const char* not_enough_stack_space = "not enough space left on Lua stack";
constexpr const char* not_enough_stack_space_floating = "not enough space left on Lua stack for a floating point number";
constexpr const char* not_enough_stack_space_integral = "not enough space left on Lua stack for an integral number";
constexpr const char* not_enough_stack_space_string = "not enough space left on Lua stack for a string";
constexpr const char* not_enough_stack_space_meta_function_name = "not enough space left on Lua stack for the name of a meta function";
constexpr const char* not_enough_stack_space_userdata = "not enough space left on Lua stack to create a sol2 userdata";
constexpr const char* not_enough_stack_space_generic = "not enough space left on Lua stack to push values";
constexpr const char* not_enough_stack_space_environment = "not enough space left on Lua stack to retrieve environment";
constexpr const char* protected_function_error = "caught (...) unknown error during protected_function call";
}  // namespace detail

inline const char* associated_type_name(lua_State* state, int, type value_type) noexcept {
  if (value_type == type::poly) {
    return "anything";
  }

  const char* name = lua_typename(state, static_cast<int>(value_type));
  return name != nullptr ? name : "unknown";
}

inline int push_type_panic_string(lua_State* state, int index, type expected, type actual, string_view message,
                                  string_view auxiliary_message) noexcept {
  int part_count = 1;
  lua_pushfstring(state, "stack index %d, expected %s, received %s", index, associated_type_name(state, index, expected),
                  associated_type_name(state, index, actual));

  if (!message.empty()) {
    lua_pushliteral(state, ": ");
    lua_pushlstring(state, message.data(), message.size());
    part_count += 2;
  }

  if (!auxiliary_message.empty()) {
    lua_pushstring(state, message.empty() ? ": " : " ");
    lua_pushlstring(state, auxiliary_message.data(), auxiliary_message.size());
    part_count += 2;
  }

  lua_concat(state, part_count);
  return 1;
}

inline int push_argument_error_string(lua_State* state, int index, type expected, type actual, string_view message) noexcept {
  lua_Debug frame{};
  const char* function_name = nullptr;
  if (lua_getstack(state, 0, &frame) != 0 && lua_getinfo(state, "n", &frame) != 0 && frame.name != nullptr && frame.name[0] != '\0') {
    function_name = frame.name;
  }

  if (function_name != nullptr) {
    lua_pushfstring(state, "bad argument #%d to '%s' (%s expected, got %s", index, function_name,
                    associated_type_name(state, index, expected), associated_type_name(state, index, actual));
  } else {
    lua_pushfstring(state, "bad argument #%d (%s expected, got %s", index, associated_type_name(state, index, expected),
                    associated_type_name(state, index, actual));
  }

  int part_count = 1;
  if (expected == actual && !message.empty()) {
    lua_pushliteral(state, ": ");
    lua_pushlstring(state, message.data(), message.size());
    part_count += 2;
  }
  lua_pushliteral(state, ")");
  ++part_count;
  lua_concat(state, part_count);
  return 1;
}

inline int push_argument_error_object(lua_State* state, int index, type expected, type actual, string_view message) noexcept {
  const int argument_count = lua_gettop(state);
  push_argument_error_string(state, index, expected, actual, message);
  const int message_index = lua_gettop(state);

  lua_createtable(state, 0, 6);
  const int error_index = lua_gettop(state);

  lua_pushboolean(state, 1);
  lua_setfield(state, error_index, "__gmp_native_argument_error_v1");
  lua_pushvalue(state, message_index);
  lua_setfield(state, error_index, "message");
  lua_pushinteger(state, index);
  lua_setfield(state, error_index, "bad_argument");
  lua_pushinteger(state, argument_count);
  lua_setfield(state, error_index, "argument_count");
  lua_pushboolean(state, actual == type::none);
  lua_setfield(state, error_index, "missing");

  const int captured_arguments = argument_count < index ? argument_count : index;
  lua_createtable(state, captured_arguments, 0);
  for (int argument = 1; argument <= captured_arguments; ++argument) {
    lua_pushvalue(state, argument);
    lua_rawseti(state, -2, argument);
  }
  lua_setfield(state, error_index, "arguments");
  return 1;
}

inline int type_panic_string(lua_State* state, int index, type expected, type actual, string_view message = "") noexcept(false) {
  push_type_panic_string(state, index, expected, actual, message, "");
  return lua_error(state);
}

inline int type_panic_c_str(lua_State* state, int index, type expected, type actual, const char* message = nullptr) noexcept(false) {
  push_type_panic_string(state, index, expected, actual, message != nullptr ? message : "", "");
  return lua_error(state);
}

struct type_panic_t {
  int operator()(lua_State* state, int index, type expected, type actual) const noexcept(false) {
    return type_panic_c_str(state, index, expected, actual);
  }

  int operator()(lua_State* state, int index, type expected, type actual, string_view message) const noexcept(false) {
    return type_panic_string(state, index, expected, actual, message);
  }
};

inline constexpr type_panic_t type_panic{};

struct constructor_handler {
  int operator()(lua_State* state, int index, type expected, type actual, string_view message) const noexcept(false) {
    push_type_panic_string(state, index, expected, actual, message, "(type check failed in constructor)");
    return lua_error(state);
  }
};

template <typename = void>
struct argument_handler {
  int operator()(lua_State* state, int index, type expected, type actual, string_view message) const noexcept(false) {
    push_argument_error_object(state, index, expected, actual, message);
    return lua_error(state);
  }
};

// sol2 3.3 normally builds a demangled C++ signature here. On 32-bit Windows,
// the missing-argument path corrupts that temporary string before Lua can raise
// the error. A Lua caller needs the parameter index and expected/actual types,
// not a C++ implementation signature, so all bound functions use the stable
// handler above without allocating a temporary C++ string.
template <typename Return, typename... Args>
struct argument_handler<types<Return, Args...>> : argument_handler<void> {};

inline int no_panic(lua_State*, int, type, type, const char* = nullptr) noexcept {
  return 0;
}

inline void type_error(lua_State* state, int expected, int actual) noexcept(false) {
  luaL_error(state, "expected %s, received %s", lua_typename(state, expected), lua_typename(state, actual));
}

inline void type_error(lua_State* state, type expected, type actual) noexcept(false) {
  type_error(state, static_cast<int>(expected), static_cast<int>(actual));
}

inline void type_assert(lua_State* state, int index, type expected, type actual) noexcept(false) {
  if (expected != type::poly && expected != actual) {
    type_panic_c_str(state, index, expected, actual);
  }
}

inline void type_assert(lua_State* state, int index, type expected) {
  type_assert(state, index, expected, type_of(state, index));
}

}  // namespace sol

#endif  // SOL_ERROR_HANDLER_HPP
