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

#include <cstdint>
#include <string>

#include "lua_vob.h"
#include "sol/sol.hpp"

namespace gmp::gothic {

class LuaCamera {
public:
  bool setMode(const std::string& mode);
  std::string getMode();

  void setPosition(float x, float y, float z);
  sol::table getPosition(sol::this_state ts);

  void setRotationDegrees(float x, float y, float z);
  sol::table getRotationDegrees(sol::this_state ts);

  bool setTargetVob(const LuaVob& vob);
  bool setTargetPlayer(std::int64_t player_id);

  void setFOV(float fov);
  float getFOV();

  bool getModeChangeEnabled();
  void setModeChangeEnabled(bool enabled);

  bool getMovementEnabled();
  void setMovementEnabled(bool enabled);
  static void ApplyMovementLock();

  sol::object getTargetVob(sol::this_state ts);
  void setTargetVobValue(sol::object value);
};

void BindCamera(sol::state& lua);
void ResetCamera();

}  // namespace gmp::gothic
