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

#include <memory>
#include <string>

#include "Shared/lua_runtime/lua_math.h"
#include "ZenGin/zGothicAPI.h"
#include "sol/sol.hpp"

namespace gmp::gothic {

class LuaVob {
public:
  explicit LuaVob(const std::string& model);
  ~LuaVob() = default;

  std::string getObjectName() const;
  void setObjectName(const std::string& name);

  ::lua::types::Mat4 getWorldTransform() const;
  void setWorldTransform(const ::lua::types::Mat4& matrix);

  sol::object getParent(sol::this_state ts) const;
  void setParent(sol::optional<LuaVob> parent);

  bool getCdDynamic() const;
  void setCdDynamic(bool enabled);

  bool getCdStatic() const;
  void setCdStatic(bool enabled);

  float getFarClipZScale() const;
  void setFarClipZScale(float scale);

  std::string getVisual() const;
  void setVisual(const std::string& visual);

  float getVisualAlpha() const;
  void setVisualAlpha(float alpha);

  void setPosition(float x, float y, float z);
  sol::table getPosition(sol::this_state s) const;
  void setRotationDegrees(float x, float y, float z);
  sol::table getRotationDegrees(sol::this_state s) const;

  void addToWorld(sol::optional<LuaVob> parent);
  void removeFromWorld();
  void floor();

  static LuaVob FromExisting(Gothic_II_Addon::zCVob* vob);
  Gothic_II_Addon::zCVob* handle() const;

private:
  struct VobInstance {
    Gothic_II_Addon::zCVob* vob{nullptr};
    bool owned{false};
    std::string pending_visual;
    ~VobInstance();
  };

  explicit LuaVob(std::shared_ptr<VobInstance> instance);

  Gothic_II_Addon::zCVob* vob() const;
  void ApplyPendingVisual();

  std::shared_ptr<VobInstance> instance_;
};

void BindVob(sol::state& lua);

}  // namespace gmp::gothic
