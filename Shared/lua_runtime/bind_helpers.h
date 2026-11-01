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

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <glm/vec3.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "sol/sol.hpp"

namespace lua::bind_helpers {

using LuaResult = std::tuple<sol::object, sol::object>;
LuaResult MakeFailure(sol::state_view lua, std::string_view message);
LuaResult MakeSuccess(sol::state_view lua, sol::object value);

std::uint8_t ClampByte(int value);
float NormalizeDegrees(float degrees);
std::optional<float> GetOptionalFloat(const sol::table& table, const char* lower_key, const char* upper_key);
std::optional<std::string> GetOptionalString(const sol::table& table, const char* lower_key, const char* upper_key);
bool ReadStringArgument(sol::variadic_args arguments, std::string& value);
std::vector<sol::object> CopyArguments(sol::state_view lua, const sol::variadic_args& arguments, std::size_t first = 0);
const void* GetLuaIdentity(const sol::object& object);
const void* GetLuaIdentity(const sol::protected_function& function);
std::optional<std::filesystem::path> ResolveDataPath(const std::string& relative, bool allow_root = true,
                                                     std::string_view data_root = "data/internal");
sol::table MakeVec3Table(sol::state_view lua, const glm::vec3& position);
sol::table MakeVec3Table(sol::state_view lua, const glm::vec3& position, float angle);
sol::table MakeVec3Table(sol::state_view lua, float x, float y, float z);
std::optional<std::pair<int, int>> ReadTimeTable(const sol::object& value);
sol::object MakeTimeTable(sol::this_state state, int hour, int minute);

/**
 * @brief Restore a renderer viewport to the full configured resolution
 *
 * The renderer type must provide vid_xdim and vid_ydim members together with
 * a SetViewport(x, y, width, height) method. Passing nullptr is a no-op.
 *
 * @tparam Renderer Renderer implementation type
 * @param renderer Renderer whose viewport should be restored
 */
template <typename Renderer>
void RestoreFullViewport(Renderer* renderer) {
  if (renderer) {
    renderer->SetViewport(0, 0, renderer->vid_xdim, renderer->vid_ydim);
  }
}

}  // namespace lua::bind_helpers
