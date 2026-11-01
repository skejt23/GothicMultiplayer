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

#include "Shared/lua_runtime/lua_value_codec.h"

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_set>

#include "Shared/lua_runtime/bind_helpers.h"

namespace gmp::lua {

namespace {

constexpr int kMaxDepth = 32;

using Json = NLOHMANN_JSON_NAMESPACE::json;

std::string LuaTypeToString(sol::type type) {
  switch (type) {
    case sol::type::none:
      return "none";
    case sol::type::nil:
      return "nil";
    case sol::type::string:
      return "string";
    case sol::type::number:
      return "number";
    case sol::type::thread:
      return "thread";
    case sol::type::boolean:
      return "boolean";
    case sol::type::function:
      return "function";
    case sol::type::userdata:
      return "userdata";
    case sol::type::lightuserdata:
      return "lightuserdata";
    case sol::type::table:
      return "table";
    case sol::type::poly:
      return "poly";
  }
  return "unknown";
}

bool EncodeLuaObject(sol::state_view lua, const sol::object& obj, Json& out, std::string& error, int depth,
                     std::unordered_set<const void*>& visited) {
  if (depth > kMaxDepth) {
    error = "Lua value exceeded maximum serialization depth";
    return false;
  }

  (void)lua;
  switch (obj.get_type()) {
    case sol::type::nil:
      out = {{"t", "nil"}};
      return true;
    case sol::type::boolean:
      out = {{"t", "boolean"}, {"v", obj.as<bool>()}};
      return true;
    case sol::type::number:
      out = {{"t", "number"}, {"v", obj.as<double>()}};
      return true;
    case sol::type::string:
      out = {{"t", "string"}, {"v", obj.as<std::string>()}};
      return true;
    case sol::type::table: {
      const void* identity = ::lua::bind_helpers::GetLuaIdentity(obj);
      if (identity != nullptr && !visited.insert(identity).second) {
        error = "Lua table contains a cyclic reference";
        return false;
      }

      sol::table table = obj.as<sol::table>();
      Json entries = Json::array();
      for (const auto& kv : table) {
        Json key_json;
        Json value_json;
        sol::object key = kv.first;
        sol::object value = kv.second;
        if (!EncodeLuaObject(lua, key, key_json, error, depth + 1, visited)) {
          return false;
        }
        if (!EncodeLuaObject(lua, value, value_json, error, depth + 1, visited)) {
          return false;
        }
        entries.push_back({{"k", std::move(key_json)}, {"v", std::move(value_json)}});
      }
      out = {{"t", "table"}, {"v", std::move(entries)}};
      if (identity != nullptr) {
        visited.erase(identity);
      }
      return true;
    }
    default:
      error = "Unsupported Lua value type: " + LuaTypeToString(obj.get_type());
      return false;
  }
}

bool DecodeLuaObject(sol::state_view lua, const Json& input, sol::object& out, std::string& error, int depth) {
  if (depth > kMaxDepth) {
    error = "Lua value exceeded maximum deserialization depth";
    return false;
  }

  if (!input.is_object()) {
    error = "Lua value payload is not an object";
    return false;
  }

  auto type_it = input.find("t");
  if (type_it == input.end() || !type_it->is_string()) {
    error = "Lua value payload missing type tag";
    return false;
  }

  const std::string type = *type_it;
  if (type == "nil") {
    out = sol::make_object(lua, sol::lua_nil);
    return true;
  }
  if (type == "boolean") {
    if (!input.contains("v")) {
      error = "Lua boolean payload missing value";
      return false;
    }
    out = sol::make_object(lua, input["v"].get<bool>());
    return true;
  }
  if (type == "number") {
    if (!input.contains("v")) {
      error = "Lua number payload missing value";
      return false;
    }
    out = sol::make_object(lua, input["v"].get<double>());
    return true;
  }
  if (type == "string") {
    if (!input.contains("v")) {
      error = "Lua string payload missing value";
      return false;
    }
    out = sol::make_object(lua, input["v"].get<std::string>());
    return true;
  }
  if (type == "table") {
    if (!input.contains("v") || !input["v"].is_array()) {
      error = "Lua table payload missing entries";
      return false;
    }
    sol::table table = lua.create_table();
    for (const auto& entry : input["v"]) {
      if (!entry.is_object() || !entry.contains("k") || !entry.contains("v")) {
        error = "Lua table entry malformed";
        return false;
      }
      sol::object key;
      sol::object value;
      if (!DecodeLuaObject(lua, entry["k"], key, error, depth + 1)) {
        return false;
      }
      if (key.get_type() == sol::type::nil) {
        error = "Lua table entry has nil key";
        return false;
      }
      if (!DecodeLuaObject(lua, entry["v"], value, error, depth + 1)) {
        return false;
      }
      table.set(key, value);
    }
    out = sol::make_object(lua, table);
    return true;
  }

  error = "Unknown Lua value type tag: " + type;
  return false;
}

bool EncodeLuaArgsImpl(sol::state_view lua, const std::vector<sol::object>& args, std::string& payload, std::string& error) {
  Json root;
  Json values = Json::array();
  std::unordered_set<const void*> visited;
  for (const auto& arg : args) {
    Json encoded;
    if (!EncodeLuaObject(lua, arg, encoded, error, 0, visited)) {
      return false;
    }
    values.push_back(std::move(encoded));
  }
  root["args"] = std::move(values);
  payload = root.dump();
  return true;
}

}  // namespace

bool EncodeLuaArgs(sol::state_view lua, const sol::variadic_args& args, std::string& payload, std::string& error) {
  std::vector<sol::object> values = ::lua::bind_helpers::CopyArguments(lua, args);
  return EncodeLuaArgsImpl(lua, values, payload, error);
}

bool EncodeLuaArgs(sol::state_view lua, const std::vector<sol::object>& args, std::string& payload, std::string& error) {
  return EncodeLuaArgsImpl(lua, args, payload, error);
}

bool DecodeLuaArgs(sol::state_view lua, std::string_view payload, std::vector<sol::object>& args, std::string& error) {
  Json root;
  try {
    root = Json::parse(payload.begin(), payload.end());
  } catch (const Json::parse_error& ex) {
    error = ex.what();
    return false;
  }

  if (!root.is_object() || !root.contains("args") || !root["args"].is_array()) {
    error = "Lua payload missing args array";
    return false;
  }

  args.clear();
  for (const auto& value : root["args"]) {
    sol::object decoded;
    if (!DecodeLuaObject(lua, value, decoded, error, 0)) {
      return false;
    }
    args.push_back(decoded);
  }
  return true;
}

bool EncodeLuaObject(sol::state_view lua, const sol::object& obj, Json& out, std::string& error) {
  std::unordered_set<const void*> visited;
  return EncodeLuaObject(lua, obj, out, error, 0, visited);
}

bool DecodeLuaObject(sol::state_view lua, const Json& input, sol::object& out, std::string& error) {
  return DecodeLuaObject(lua, input, out, error, 0);
}

}  // namespace gmp::lua
