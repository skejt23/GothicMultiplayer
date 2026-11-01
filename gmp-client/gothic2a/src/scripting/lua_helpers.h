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

#pragma once

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "Shared/lua_runtime/lua_math.h"
#include "sol/sol.hpp"

#include "ZenGin/zGothicAPI.h"

namespace gmp::gothic::lua_helpers {

inline constexpr float kCursorMinSensitivity = 1.0f;
inline constexpr float kCursorMaxSensitivity = 10.0f;
inline constexpr int kCursorDefaultSizePx = 96;
inline constexpr std::array<int, 8> kCursorMouseButtonCodes = {
    MOUSE_BUTTONLEFT, MOUSE_BUTTONRIGHT, MOUSE_BUTTONMID, MOUSE_XBUTTON1,
    MOUSE_XBUTTON2,   MOUSE_XBUTTON3,    MOUSE_XBUTTON4,  MOUSE_XBUTTON5};

inline constexpr float kSoundMinVolume = 0.0f;
inline constexpr float kSoundMaxVolume = 1.0f;
inline constexpr float kSoundMinBalance = -1.0f;
inline constexpr float kSoundMaxBalance = 1.0f;

bool ReadIntField(sol::table table, const char* key, int index, int& out);
bool ReadVec2(sol::object value, int& x, int& y);
bool ReadVec3(sol::object value, float& x, float& y, float& z);
bool ReadSize(sol::object value, int& width, int& height);
bool ReadRect(sol::object value, int& x, int& y, int& width, int& height);
bool ReadColor(sol::object value, int& r, int& g, int& b, int& a);

struct DiscordActivityState {
  std::string state;
  std::string details;
  std::string large_image_key;
  std::string large_image_text;
  std::string small_image_key;
  std::string small_image_text;
};

DiscordActivityState& GetDiscordActivityState();
void ApplyDiscordActivityState(const DiscordActivityState& state);
void ClearDiscordActivityState();

bool CanApplyVisual();
::lua::types::Mat4 ToLuaMat4(const Gothic_II_Addon::zMAT4& mat);
Gothic_II_Addon::zMAT4 ToZenMat4(const ::lua::types::Mat4& mat);

Gothic_II_Addon::zCRoute* FindRouteByName(const std::string& start_wp, const std::string& end_wp);
Gothic_II_Addon::zCWayNet* GetWayNet();
Gothic_II_Addon::zCWorld* GetGameWorld();

Gothic_II_Addon::zVEC3 LuaRotationToGothicEuler(const Gothic_II_Addon::zVEC3& degrees);
Gothic_II_Addon::zVEC3 GothicEulerToLuaRotation(const Gothic_II_Addon::zVEC3& radians);
float GetAngleDegreesFromDirection(const Gothic_II_Addon::zVEC3& direction);
void AddAngle(sol::table& tbl, const Gothic_II_Addon::zVEC3& direction);
void AddWaypointAngle(sol::table& tbl, Gothic_II_Addon::zCWaypoint* waypoint);
sol::table MakeWaypointPositionTable(sol::state_view lua, Gothic_II_Addon::zCWaypoint* waypoint);
sol::table MakeWaypointTable(sol::state_view lua, Gothic_II_Addon::zCWaypoint* waypoint);
sol::table MakeFreepointPositionTable(sol::state_view lua, Gothic_II_Addon::zCVobSpot* freepoint);
sol::table MakeFreepointTable(sol::state_view lua, Gothic_II_Addon::zCVobSpot* freepoint);
std::vector<Gothic_II_Addon::zCVobSpot*> GetFreepoints();
Gothic_II_Addon::zCVobSpot* FindFreepointByName(const std::string& freepoint_name);
std::pair<Gothic_II_Addon::zCVobSpot*, Gothic_II_Addon::zCVobSpot*> FindNearestFreepointPair(float x, float y, float z,
                                                                                              float max_distance);

}  // namespace gmp::gothic::lua_helpers
