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

#include "lua_helpers.h"

#include <cmath>
#include <limits>

#include "Shared/lua_runtime/bind_helpers.h"
#include "discord_presence.h"

namespace gmp::gothic::lua_helpers {

using namespace Gothic_II_Addon;

bool ReadIntField(sol::table table, const char* key, int index, int& out) {
  sol::optional<double> named = table.get<sol::optional<double>>(key);
  if (named) {
    out = static_cast<int>(*named);
    return true;
  }

  sol::optional<double> indexed = table.get<sol::optional<double>>(index);
  if (indexed) {
    out = static_cast<int>(*indexed);
    return true;
  }

  return false;
}

bool ReadVec2(sol::object value, int& x, int& y) {
  if (value.is<::lua::types::Vec2>()) {
    const auto& vec = value.as<::lua::types::Vec2>();
    x = static_cast<int>(vec.x);
    y = static_cast<int>(vec.y);
    return true;
  }

  if (!value.is<sol::table>()) {
    return false;
  }

  sol::table table = value.as<sol::table>();
  return ReadIntField(table, "x", 1, x) && ReadIntField(table, "y", 2, y);
}

bool ReadVec3(sol::object value, float& x, float& y, float& z) {
  if (value.is<::lua::types::Vec3>()) {
    const auto& vec = value.as<::lua::types::Vec3>();
    x = vec.x;
    y = vec.y;
    z = vec.z;
    return true;
  }

  if (!value.is<sol::table>()) {
    return false;
  }

  sol::table table = value.as<sol::table>();
  sol::optional<float> x_value = table["x"];
  sol::optional<float> y_value = table["y"];
  sol::optional<float> z_value = table["z"];
  if (!x_value) {
    x_value = table[1];
  }
  if (!y_value) {
    y_value = table[2];
  }
  if (!z_value) {
    z_value = table[3];
  }
  if (!x_value || !y_value || !z_value) {
    return false;
  }

  x = *x_value;
  y = *y_value;
  z = *z_value;
  return true;
}

bool ReadSize(sol::object value, int& width, int& height) {
  if (value.is<::lua::types::Vec2>()) {
    const auto& vec = value.as<::lua::types::Vec2>();
    width = static_cast<int>(vec.x);
    height = static_cast<int>(vec.y);
    return true;
  }

  if (!value.is<sol::table>()) {
    return false;
  }

  sol::table table = value.as<sol::table>();
  return ReadIntField(table, "width", 1, width) && ReadIntField(table, "height", 2, height);
}

bool ReadRect(sol::object value, int& x, int& y, int& width, int& height) {
  if (value.is<::lua::types::Vec4>()) {
    const auto& vec = value.as<::lua::types::Vec4>();
    x = static_cast<int>(vec.x);
    y = static_cast<int>(vec.y);
    width = static_cast<int>(vec.z);
    height = static_cast<int>(vec.w);
    return true;
  }

  if (!value.is<sol::table>()) {
    return false;
  }

  sol::table table = value.as<sol::table>();
  return ReadIntField(table, "x", 1, x) && ReadIntField(table, "y", 2, y) && ReadIntField(table, "width", 3, width) &&
         ReadIntField(table, "height", 4, height);
}

bool ReadColor(sol::object value, int& r, int& g, int& b, int& a) {
  if (value.is<::lua::types::Vec3>()) {
    const auto& vec = value.as<::lua::types::Vec3>();
    r = static_cast<int>(vec.x);
    g = static_cast<int>(vec.y);
    b = static_cast<int>(vec.z);
    return true;
  }

  if (value.is<::lua::types::Vec4>()) {
    const auto& vec = value.as<::lua::types::Vec4>();
    r = static_cast<int>(vec.x);
    g = static_cast<int>(vec.y);
    b = static_cast<int>(vec.z);
    a = static_cast<int>(vec.w);
    return true;
  }

  if (!value.is<sol::table>()) {
    return false;
  }

  sol::table table = value.as<sol::table>();
  bool changed = false;
  changed = ReadIntField(table, "r", 1, r) || changed;
  changed = ReadIntField(table, "g", 2, g) || changed;
  changed = ReadIntField(table, "b", 3, b) || changed;
  changed = ReadIntField(table, "a", 4, a) || changed;
  return changed;
}

DiscordActivityState& GetDiscordActivityState() {
  static DiscordActivityState state;
  return state;
}

void ApplyDiscordActivityState(const DiscordActivityState& state) {
  DiscordRichPresence::Instance().UpdateActivity(state.state, state.details, 0, 0, state.large_image_key, state.large_image_text,
                                                 state.small_image_key, state.small_image_text);
}

void ClearDiscordActivityState() {
  GetDiscordActivityState() = DiscordActivityState{};
  DiscordRichPresence::Instance().ClearActivity();
}

bool CanApplyVisual() {
  return zresMan != nullptr;
}

::lua::types::Mat4 ToLuaMat4(const zMAT4& mat) {
  ::lua::types::Mat4 result;
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      result.mat_[col][row] = mat.v[row].n[col];
    }
  }
  return result;
}

zMAT4 ToZenMat4(const ::lua::types::Mat4& mat) {
  zMAT4 result;
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      result.v[row].n[col] = mat.mat_[col][row];
    }
  }
  return result;
}

zCRoute* FindRouteByName(const std::string& start_wp, const std::string& end_wp) {
  if (!ogame) {
    return nullptr;
  }

  zCWorld* world = ogame->GetGameWorld();
  if (!world || !world->wayNet) {
    return nullptr;
  }

  zSTRING start(start_wp.c_str());
  zSTRING end(end_wp.c_str());
  return world->wayNet->FindRoute(start, end, nullptr);
}

zCWayNet* GetWayNet() {
  if (!ogame) {
    return nullptr;
  }

  zCWorld* world = ogame->GetGameWorld();
  if (!world) {
    return nullptr;
  }

  return world->wayNet;
}

zCWorld* GetGameWorld() {
  if (!ogame) {
    return nullptr;
  }

  return ogame->GetGameWorld();
}

zVEC3 LuaRotationToGothicEuler(const zVEC3& degrees) {
  // Gothic's zMAT4 Euler helpers use radians and the opposite sign from zCVob::RotateWorld*.
  return zVEC3(-degrees[VX] * RAD, -degrees[VY] * RAD, -degrees[VZ] * RAD);
}

zVEC3 GothicEulerToLuaRotation(const zVEC3& radians) {
  return zVEC3(-radians[VX] * DEGREE, -radians[VY] * DEGREE, -radians[VZ] * DEGREE);
}

float GetAngleDegreesFromDirection(const zVEC3& direction) {
  const float x = direction[VX];
  const float z = direction[VZ];

  float angle = std::atan2(x, z) * DEGREE;
  if (angle < 0.0f) {
    angle += 360.0f;
  }
  return angle;
}

void AddAngle(sol::table& tbl, const zVEC3& direction) {
  tbl["angle"] = GetAngleDegreesFromDirection(direction);
}

void AddWaypointAngle(sol::table& tbl, zCWaypoint* waypoint) {
  if (!waypoint) {
    return;
  }

  if (zCVobWaypoint* waypoint_vob = waypoint->GetVob()) {
    AddAngle(tbl, waypoint_vob->GetAtVectorWorld());
    return;
  }

  AddAngle(tbl, waypoint->dir);
}

sol::table MakeWaypointPositionTable(sol::state_view lua, zCWaypoint* waypoint) {
  const zVEC3 position = waypoint->GetPositionWorld();
  sol::table tbl = ::lua::bind_helpers::MakeVec3Table(lua, position[VX], position[VY], position[VZ]);
  AddWaypointAngle(tbl, waypoint);
  return tbl;
}

sol::table MakeWaypointTable(sol::state_view lua, zCWaypoint* waypoint) {
  sol::table tbl = MakeWaypointPositionTable(lua, waypoint);
  tbl["name"] = waypoint->GetName().ToChar();
  return tbl;
}

sol::table MakeFreepointPositionTable(sol::state_view lua, zCVobSpot* freepoint) {
  const zVEC3 position = freepoint->GetPositionWorld();
  sol::table tbl = ::lua::bind_helpers::MakeVec3Table(lua, position[VX], position[VY], position[VZ]);
  AddAngle(tbl, freepoint->GetAtVectorWorld());
  return tbl;
}

sol::table MakeFreepointTable(sol::state_view lua, zCVobSpot* freepoint) {
  sol::table tbl = MakeFreepointPositionTable(lua, freepoint);
  tbl["name"] = freepoint->GetObjectName().ToChar();
  return tbl;
}

std::vector<zCVobSpot*> GetFreepoints() {
  std::vector<zCVobSpot*> freepoints;
  zCWorld* world = GetGameWorld();
  if (!world) {
    return freepoints;
  }

  zCArray<zCVob*> freepoint_vobs;
  world->SearchVobListByClass(zCVobSpot::classDef, freepoint_vobs, &world->globalVobTree);
  freepoints.reserve(freepoint_vobs.GetNumInList());
  for (int i = 0; i < freepoint_vobs.GetNumInList(); ++i) {
    zCVob* vob = freepoint_vobs[i];
    if (!vob) {
      continue;
    }

    freepoints.push_back(static_cast<zCVobSpot*>(vob));
  }

  return freepoints;
}

zCVobSpot* FindFreepointByName(const std::string& freepoint_name) {
  for (zCVobSpot* freepoint : GetFreepoints()) {
    if (!freepoint) {
      continue;
    }

    if (freepoint_name == freepoint->GetObjectName().ToChar()) {
      return freepoint;
    }
  }

  return nullptr;
}

std::pair<zCVobSpot*, zCVobSpot*> FindNearestFreepointPair(float x, float y, float z, float max_distance) {
  const bool limit_distance = max_distance > 0.0f;
  const float max_distance_sq = max_distance * max_distance;
  zCVobSpot* nearest = nullptr;
  zCVobSpot* second_nearest = nullptr;
  float nearest_sq = std::numeric_limits<float>::max();
  float second_nearest_sq = std::numeric_limits<float>::max();

  for (zCVobSpot* freepoint : GetFreepoints()) {
    if (!freepoint) {
      continue;
    }

    const zVEC3 position = freepoint->GetPositionWorld();
    const float dx = position[VX] - x;
    const float dy = position[VY] - y;
    const float dz = position[VZ] - z;
    const float distance_sq = (dx * dx) + (dy * dy) + (dz * dz);
    if (limit_distance && distance_sq > max_distance_sq) {
      continue;
    }

    if (distance_sq < nearest_sq) {
      second_nearest = nearest;
      second_nearest_sq = nearest_sq;
      nearest = freepoint;
      nearest_sq = distance_sq;
      continue;
    }

    if (distance_sq < second_nearest_sq) {
      second_nearest = freepoint;
      second_nearest_sq = distance_sq;
    }
  }

  return {nearest, second_nearest};
}

}  // namespace gmp::gothic::lua_helpers
