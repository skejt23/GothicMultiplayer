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

#include "lua_route.h"

#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "ZenGin/zGothicAPI.h"
#include "lua_helpers.h"

using namespace Gothic_II_Addon;

namespace gmp::gothic {

/* luagmp (func)
 *
 * This function will enable/disable waynet rendering.
 *
 * @version  0.3.0
 * @name     toggleDrawWaynet
 * @side     client
 * @category Render
 * @param    (boolean) enabled    True to render waynet, false to disable.
 *
 */
void Function_ToggleDrawWaynet(bool enabled) {
  if (!ogame) {
    return;
  }

  ogame->SetDrawWaynet(enabled);
}

/* luagmp (class)
 *
 * This class represents a route between two waypoint names computed from Zengin zCRoute class.
 *
 * @version  0.3.0
 * @name     Route
 * @side     client
 * @category World
 *
 */
/* luagmp (constructor)
 *
 * Creates a path between two waypoint names.
 *
 * @param    (string) startWp   Name of the start waypoint.
 * @param    (string) endWp     Name of the end waypoint.
 *
 */
LuaRoute::LuaRoute(const std::string& start_wp, const std::string& end_wp) : start_wp_(start_wp), end_wp_(end_wp) {
}

/* luagmp (method)
 *
 * This method will return the start waypoint name.
 *
 * @name     getStart
 * @return   (string)
 *
 */
const std::string& LuaRoute::getStart() const {
  return start_wp_;
}

/* luagmp (method)
 *
 * This method will return the end waypoint name.
 *
 * @name     getEnd
 * @return   (string)
 *
 */
const std::string& LuaRoute::getEnd() const {
  return end_wp_;
}

std::vector<std::string> LuaRoute::ComputeWaypointNames() const {
  std::vector<std::string> result;

  zCRoute* route = lua_helpers::FindRouteByName(start_wp_, end_wp_);
  if (!route) {
    return result;
  }

  zSTRING start(start_wp_.c_str());
  zCWayNet* way_net = lua_helpers::GetWayNet();
  if (!way_net) {
    delete route;
    return result;
  }

  zCWaypoint* start_waypoint = way_net->GetWaypoint(start);
  if (start_waypoint) {
    route->SetStart(start_waypoint);
    result.emplace_back(start_wp_);
  }

  while (zCWaypoint* waypoint = route->GetNextWP()) {
    const std::string waypoint_name = waypoint->GetName().ToChar();
    if (!result.empty() && result.back() == waypoint_name) {
      continue;
    }
    result.emplace_back(waypoint_name);
  }

  delete route;
  return result;
}

/* luagmp (method)
 *
 * This method will return the number of waypoints in the computed route.
 *
 * @name     getWaypointCount
 * @return   (number)      Number of waypoints.
 *
 */
int LuaRoute::getWaypointCount() const {
  return static_cast<int>(ComputeWaypointNames().size());
}

/* luagmp (func)
 *
 * This function will return world position of a waypoint by name.
 *
 * @version  0.3.0
 * @name     getWaypoint
 * @side     client
 * @category World
 * @param    (string) name       Waypoint name.
 * @return   ({x, y, z, angle}|nil)     Waypoint position table or nil.
 *
 */
sol::object Function_GetWaypoint(const std::string& waypoint_name, sol::this_state ts) {
  sol::state_view lua(ts);
  zCWayNet* way_net = lua_helpers::GetWayNet();
  if (!way_net) {
    return sol::nil;
  }

  zSTRING name(waypoint_name.c_str());
  zCWaypoint* waypoint = way_net->GetWaypoint(name);
  if (!waypoint) {
    return sol::nil;
  }

  return sol::make_object(lua, lua_helpers::MakeWaypointPositionTable(lua, waypoint));
}

/* luagmp (func)
 *
 * This function will return world position of a freepoint by name.
 *
 * @version  0.3.0
 * @name     getFreepoint
 * @side     client
 * @category World
 * @param    (string) name       Freepoint name.
 * @return   ({x, y, z, angle}|nil)     Freepoint position table or nil.
 *
 */
sol::object Function_GetFreepoint(const std::string& freepoint_name, sol::this_state ts) {
  sol::state_view lua(ts);
  zCVobSpot* freepoint = lua_helpers::FindFreepointByName(freepoint_name);
  if (!freepoint) {
    return sol::nil;
  }

  return sol::make_object(lua, lua_helpers::MakeFreepointPositionTable(lua, freepoint));
}

/* luagmp (func)
 *
 * This function will return the nearest waypoint for a given position.
 *
 * @version  0.3.0
 * @name     getNearestWaypoint
 * @side     client
 * @category World
 * @param    (number) x                Position X.
 * @param    (number) y                Position Y.
 * @param    (number) z                Position Z.
 * @param    (number|nil) distance     Optional maximum search distance.
 * @return   ({name, x, y, z, angle}|nil)     Waypoint information table or nil.
 *
 */
sol::object Function_GetNearestWaypoint(float x, float y, float z, sol::optional<float> distance, sol::this_state ts) {
  sol::state_view lua(ts);
  zCWayNet* way_net = lua_helpers::GetWayNet();
  if (!way_net) {
    return sol::nil;
  }

  const bool limit_distance = distance.has_value() && distance.value() > 0.0f;
  const float max_distance_sq = distance.value_or(-1.0f) * distance.value_or(-1.0f);
  zCWaypoint* nearest = nullptr;
  float nearest_sq = std::numeric_limits<float>::max();

  for (zCListSort<zCWaypoint>* node = way_net->wplist.next; node; node = node->next) {
    zCWaypoint* waypoint = node->data;
    if (!waypoint) {
      continue;
    }

    const zVEC3 position = waypoint->GetPositionWorld();
    const float dx = position[VX] - x;
    const float dy = position[VY] - y;
    const float dz = position[VZ] - z;
    const float distance_sq = (dx * dx) + (dy * dy) + (dz * dz);
    if (limit_distance && distance_sq > max_distance_sq) {
      continue;
    }

    if (distance_sq < nearest_sq) {
      nearest_sq = distance_sq;
      nearest = waypoint;
    }
  }

  if (!nearest) {
    return sol::nil;
  }

  return sol::make_object(lua, lua_helpers::MakeWaypointTable(lua, nearest));
}

/* luagmp (func)
 *
 * This function will return the nearest freepoint for a given position.
 *
 * @version  0.3.0
 * @name     getNearestFreepoint
 * @side     client
 * @category World
 * @param    (number) x                Position X.
 * @param    (number) y                Position Y.
 * @param    (number) z                Position Z.
 * @param    (number|nil) distance     Optional maximum search distance.
 * @return   ({name, x, y, z, angle}|nil)     Freepoint information table or nil.
 *
 */
sol::object Function_GetNearestFreepoint(float x, float y, float z, sol::optional<float> distance, sol::this_state ts) {
  sol::state_view lua(ts);
  const auto [nearest, _] = lua_helpers::FindNearestFreepointPair(x, y, z, distance.value_or(-1.0f));
  if (!nearest) {
    return sol::nil;
  }

  return sol::make_object(lua, lua_helpers::MakeFreepointTable(lua, nearest));
}

/* luagmp (func)
 *
 * This function will return the second nearest waypoint for a given position.
 *
 * @version  0.3.0
 * @name     getNextNearestWaypoint
 * @side     client
 * @category World
 * @param    (number) x                Position X.
 * @param    (number) y                Position Y.
 * @param    (number) z                Position Z.
 * @return   ({name, x, y, z, angle}|nil)     Waypoint information table or nil.
 *
 */
sol::object Function_GetNextNearestWaypoint(float x, float y, float z, sol::this_state ts) {
  sol::state_view lua(ts);
  zCWayNet* way_net = lua_helpers::GetWayNet();
  if (!way_net) {
    return sol::nil;
  }

  zCWaypoint* nearest = nullptr;
  zCWaypoint* second_nearest = nullptr;
  float nearest_sq = std::numeric_limits<float>::max();
  float second_nearest_sq = std::numeric_limits<float>::max();

  for (zCListSort<zCWaypoint>* node = way_net->wplist.next; node; node = node->next) {
    zCWaypoint* waypoint = node->data;
    if (!waypoint) {
      continue;
    }

    const zVEC3 position = waypoint->GetPositionWorld();
    const float dx = position[VX] - x;
    const float dy = position[VY] - y;
    const float dz = position[VZ] - z;
    const float distance_sq = (dx * dx) + (dy * dy) + (dz * dz);
    if (distance_sq < nearest_sq) {
      second_nearest = nearest;
      second_nearest_sq = nearest_sq;
      nearest = waypoint;
      nearest_sq = distance_sq;
      continue;
    }

    if (distance_sq < second_nearest_sq) {
      second_nearest = waypoint;
      second_nearest_sq = distance_sq;
    }
  }

  if (!second_nearest) {
    return sol::nil;
  }

  return sol::make_object(lua, lua_helpers::MakeWaypointTable(lua, second_nearest));
}

/* luagmp (func)
 *
 * This function will return the second nearest freepoint for a given position.
 *
 * @version  0.3.0
 * @name     getNextNearestFreepoint
 * @side     client
 * @category World
 * @param    (number) x                Position X.
 * @param    (number) y                Position Y.
 * @param    (number) z                Position Z.
 * @return   ({name, x, y, z, angle}|nil)     Freepoint information table or nil.
 *
 */
sol::object Function_GetNextNearestFreepoint(float x, float y, float z, sol::this_state ts) {
  sol::state_view lua(ts);
  const auto [_, second_nearest] = lua_helpers::FindNearestFreepointPair(x, y, z, -1.0f);
  if (!second_nearest) {
    return sol::nil;
  }

  return sol::make_object(lua, lua_helpers::MakeFreepointTable(lua, second_nearest));
}

/* luagmp (func)
 *
 * This function will return the list of all waypoints from current world.
 *
 * @version  0.3.0
 * @name     getWaypoints
 * @side     client
 * @category World
 * @return   ({...})                 Table of waypoints {name, x, y, z, angle}.
 *
 */
sol::table Function_GetWaypoints(sol::this_state ts) {
  sol::state_view lua(ts);
  sol::table waypoints = lua.create_table();
  zCWayNet* way_net = lua_helpers::GetWayNet();
  if (!way_net) {
    return waypoints;
  }

  int index = 1;
  for (zCListSort<zCWaypoint>* node = way_net->wplist.next; node; node = node->next) {
    zCWaypoint* waypoint = node->data;
    if (!waypoint) {
      continue;
    }

    waypoints[index++] = lua_helpers::MakeWaypointTable(lua, waypoint);
  }

  return waypoints;
}

/* luagmp (method)
 * Returns a one-based Lua array of waypoint names.
 * @name getWaypointNames
 * @return (table) Waypoint names.
 */
sol::as_table_t<std::vector<std::string>> LuaRoute::getWaypointNames() const {
  return sol::as_table(ComputeWaypointNames());
}

void BindRoute(sol::state& lua) {
  sol::usertype<LuaRoute> route_type = lua.new_usertype<LuaRoute>("Route", sol::constructors<LuaRoute(const std::string&, const std::string&)>());

  route_type["getStart"] = &LuaRoute::getStart;
  route_type["getEnd"] = &LuaRoute::getEnd;
  route_type["getWaypointCount"] = &LuaRoute::getWaypointCount;
  route_type["getWaypointNames"] = &LuaRoute::getWaypointNames;

  lua["toggleDrawWaynet"] = Function_ToggleDrawWaynet;

  lua["getWaypoint"] = Function_GetWaypoint;
  lua["getNearestWaypoint"] = Function_GetNearestWaypoint;
  lua["getNextNearestWaypoint"] = Function_GetNextNearestWaypoint;
  lua["getWaypoints"] = Function_GetWaypoints;
  lua["getFreepoint"] = Function_GetFreepoint;
  lua["getNearestFreepoint"] = Function_GetNearestFreepoint;
  lua["getNextNearestFreepoint"] = Function_GetNextNearestFreepoint;
}

}  // namespace gmp::gothic
