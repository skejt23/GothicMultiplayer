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

#include <cstdint>
#include <sol/sol.hpp>
#include <string>

#include "../item_ground_manager.h"

namespace lua {
namespace bindings {

class LuaItemGround {
public:
  explicit LuaItemGround(ItemGroundManager::ItemGroundId id = 0);

  ItemGroundManager::ItemGroundId getId() const;
  bool isValid() const;
  std::string getInstance() const;
  std::int32_t getAmount() const;
  bool getPhysicsEnabled() const;
  void setPhysicsEnabled(bool enabled);
  std::string getWorld() const;
  std::int32_t getVirtualWorld() const;
  void setVirtualWorld(std::int32_t virtual_world);
  sol::table getPosition(sol::this_state state) const;
  void setPosition(float x, float y, float z);
  sol::table getRotationDegrees(sol::this_state state) const;
  void setRotationDegrees(float x, float y, float z);

private:
  ItemGroundManager::ItemGroundId id_{0};
};

void BindItemGround(sol::state& lua);

}  // namespace bindings
}  // namespace lua
