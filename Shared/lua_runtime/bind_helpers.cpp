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

#include "bind_helpers.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace lua::bind_helpers {
namespace {

template <typename T>
const void* GetIdentity(const T& value) {
  lua_State* state = value.lua_state();
  sol::stack::push(state, value);
  const void* identity = lua_topointer(state, -1);
  lua_pop(state, 1);
  return identity;
}

template <typename T>
std::optional<T> GetOptional(const sol::table& table, const char* lower_key, const char* upper_key) {
  if (auto value = table.get<sol::optional<T>>(lower_key); value) {
    return *value;
  }
  if (auto value = table.get<sol::optional<T>>(upper_key); value) {
    return *value;
  }
  return std::nullopt;
}

}  // namespace

/**
 * @brief Clamp an integer to the range of an unsigned byte
 *
 * @param value Value to clamp
 * @return Value clamped to the inclusive range [0, 255]
 */
std::uint8_t ClampByte(int value) {
  return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

LuaResult MakeFailure(sol::state_view lua, std::string_view message) {
  return {sol::make_object(lua, sol::nil), sol::make_object(lua, lua.create_table_with("message", std::string(message)))};
}

LuaResult MakeSuccess(sol::state_view lua, sol::object value) {
  return {std::move(value), sol::make_object(lua, sol::nil)};
}

/**
 * @brief Normalize an angle in degrees
 *
 * @param degrees Angle in degrees
 * @return Equivalent angle in the half-open range [0, 360)
 */
float NormalizeDegrees(float degrees) {
  degrees = std::fmod(degrees, 360.0f);
  if (degrees < 0.0f) {
    degrees += 360.0f;
  }
  return degrees;
}

/**
 * @brief Read an optional floating-point field using lowercase and uppercase keys
 *
 * The lowercase key takes precedence when both fields are present.
 *
 * @param table Lua table to inspect
 * @param lower_key Preferred lowercase field name
 * @param upper_key Fallback uppercase field name
 * @return Field value, or std::nullopt when neither field contains a compatible value
 */
std::optional<float> GetOptionalFloat(const sol::table& table, const char* lower_key, const char* upper_key) {
  return GetOptional<float>(table, lower_key, upper_key);
}

/**
 * @brief Read an optional string field using lowercase and uppercase keys
 *
 * The lowercase key takes precedence when both fields are present.
 *
 * @param table Lua table to inspect
 * @param lower_key Preferred lowercase field name
 * @param upper_key Fallback uppercase field name
 * @return Field value, or std::nullopt when neither field contains a compatible value
 */
std::optional<std::string> GetOptionalString(const sol::table& table, const char* lower_key, const char* upper_key) {
  return GetOptional<std::string>(table, lower_key, upper_key);
}

/**
 * @brief Read the first string from a variadic Lua argument list
 *
 * @param arguments Arguments to search
 * @param value Destination for the first string found; unchanged on failure
 * @return true when a string was found, false otherwise
 */
bool ReadStringArgument(sol::variadic_args arguments, std::string& value) {
  for (std::size_t index = 0; index < arguments.size(); ++index) {
    const sol::object argument = arguments[index];
    if (argument.get_type() == sol::type::string) {
      value = argument.as<std::string>();
      return true;
    }
  }
  return false;
}

/**
 * @brief Copy a range of variadic Lua arguments into owning Lua objects
 *
 * @param lua Lua state that owns the copied objects
 * @param arguments Arguments to copy
 * @param first Zero-based index of the first argument to copy
 * @return Copied arguments, or an empty vector when first is out of range
 */
std::vector<sol::object> CopyArguments(sol::state_view lua, const sol::variadic_args& arguments, std::size_t first) {
  if (first >= arguments.size()) {
    return {};
  }

  std::vector<sol::object> values;
  values.reserve(arguments.size() - first);
  for (std::size_t index = first; index < arguments.size(); ++index) {
    values.emplace_back(sol::make_object(lua, arguments[index]));
  }
  return values;
}

/**
 * @brief Get the opaque Lua identity of an object
 *
 * The returned pointer is intended only for identity comparisons within the
 * object's Lua state and must not be dereferenced.
 *
 * @param object Lua object whose identity should be read
 * @return Opaque identity pointer supplied by Lua, or nullptr when Lua does not expose one
 */
const void* GetLuaIdentity(const sol::object& object) {
  return GetIdentity(object);
}

/**
 * @brief Get the opaque Lua identity of a protected function
 *
 * The returned pointer is intended only for identity comparisons within the
 * function's Lua state and must not be dereferenced.
 *
 * @param function Lua function whose identity should be read
 * @return Opaque identity pointer supplied by Lua, or nullptr when Lua does not expose one
 */
const void* GetLuaIdentity(const sol::protected_function& function) {
  return GetIdentity(function);
}

/**
 * @brief Resolve a relative path below a data directory
 *
 * Absolute paths, empty paths, parent-directory components, and paths outside
 * the selected data root are rejected. Containment is checked lexically; this
 * function does not resolve symbolic links.
 *
 * @param relative Relative path to resolve
 * @param allow_root Whether the data root itself is a valid result
 * @param data_root Data directory relative to the current working directory
 * @return Normalized path, or std::nullopt when the path is invalid
 */
std::optional<std::filesystem::path> ResolveDataPath(const std::string& relative, bool allow_root, std::string_view data_root) {
  const std::filesystem::path requested(relative);
  if (requested.empty() || requested.is_absolute()) {
    return std::nullopt;
  }
  for (const auto& part : requested) {
    if (part == "..") {
      return std::nullopt;
    }
  }

  const std::filesystem::path root = (std::filesystem::current_path() / std::filesystem::path{std::string(data_root)}).lexically_normal();
  const std::filesystem::path full = (root / requested.lexically_normal()).lexically_normal();
  const std::string full_string = full.generic_string();
  std::string root_prefix = root.generic_string();
  if (!root_prefix.empty() && root_prefix.back() != '/') {
    root_prefix.push_back('/');
  }

  if (full == root) {
    if (!allow_root) {
      return std::nullopt;
    }
    return full;
  }
  if (full_string.rfind(root_prefix, 0) != 0) {
    return std::nullopt;
  }
  return full;
}

/**
 * @brief Create a Lua table from a three-dimensional GLM vector
 *
 * @param lua Lua state that will own the table
 * @param position Vector used for the x, y, and z fields
 * @return Table containing x, y, and z fields
 */
sol::table MakeVec3Table(sol::state_view lua, const glm::vec3& position) {
  return MakeVec3Table(lua, position.x, position.y, position.z);
}

/**
 * @brief Create a Lua position table with an angle
 *
 * @param lua Lua state that will own the table
 * @param position Vector used for the x, y, and z fields
 * @param angle Value used for the angle field
 * @return Table containing x, y, z, and angle fields
 */
sol::table MakeVec3Table(sol::state_view lua, float x, float y, float z) {
  sol::table table = lua.create_table();
  table["x"] = x;
  table["y"] = y;
  table["z"] = z;
  return table;
}

/**
 * @brief Create a Lua table from individual vector components
 *
 * @param lua Lua state that will own the table
 * @param x Value used for the x field
 * @param y Value used for the y field
 * @param z Value used for the z field
 * @return Table containing x, y, and z fields
 */
sol::table MakeVec3Table(sol::state_view lua, const glm::vec3& position, float angle) {
  sol::table table = MakeVec3Table(lua, position);
  table["angle"] = angle;
  return table;
}

/**
 * @brief Read a time value from a Lua table
 *
 * This function requires integer hour and min fields but does not validate
 * their ranges.
 *
 * @param value Lua value expected to contain a table
 * @return Hour/minute pair, or std::nullopt for an invalid table
 */
std::optional<std::pair<int, int>> ReadTimeTable(const sol::object& value) {
  if (!value.is<sol::table>()) {
    return std::nullopt;
  }

  sol::table table = value.as<sol::table>();
  sol::object hour = table["hour"];
  sol::object minute = table["minute"];
  if (!hour.is<int>() || !minute.is<int>()) {
    return std::nullopt;
  }

  return std::pair<int, int>{hour.as<int>(), minute.as<int>()};
}

/**
 * @brief Create a Lua table containing a time value
 *
 * @param state Lua state that will own the table
 * @param hour Value used for the hour field
 * @param minute Value used for the min field
 * @return Lua object containing the new time table
 */
sol::object MakeTimeTable(sol::this_state state, int hour, int minute) {
  sol::state_view lua(state);
  sol::table table = lua.create_table();
  table["hour"] = hour;
  table["minute"] = minute;
  return sol::make_object(lua, table);
}

}  // namespace lua::bind_helpers
