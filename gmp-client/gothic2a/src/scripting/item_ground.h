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
#include <deque>
#include <glm/glm.hpp>
#include <optional>
#include <sol/sol.hpp>
#include <string>
#include <unordered_map>

#include "ZenGin/zGothicAPI.h"

namespace gmp::gothic {

class LuaItemGround {
public:
  explicit LuaItemGround(std::uint32_t id = 0);

  std::uint32_t getId() const;
  bool isValid() const;
  std::string getInstance() const;
  std::int32_t getAmount() const;

private:
  std::uint32_t id_{0};
};

class ClientItemGroundManager {
public:
  struct ItemGround {
    std::uint32_t id{0};
    std::string instance;
    std::int32_t amount{1};
    bool physics_enabled{false};
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    oCItem* item{nullptr};
    std::uint8_t pending_physics_refreshes{0};
  };

  struct PendingInventoryAdd {
    std::int32_t instance_id{0};
    std::int32_t amount{0};
    std::int32_t previous_amount{0};
  };

  static ClientItemGroundManager& Instance();

  void Upsert(std::uint32_t id, const std::string& instance, std::int32_t amount, bool physics_enabled, const glm::vec3& position,
              const glm::vec3& rotation);
  void Destroy(std::uint32_t id, bool emit_event);
  void DetachItem(std::uint32_t id);
  void Clear(bool emit_event);
  void RefreshPending();

  ItemGround* GetById(std::uint32_t id);
  const ItemGround* GetById(std::uint32_t id) const;
  const std::unordered_map<std::uint32_t, ItemGround>& Items() const;
  std::optional<std::uint32_t> GetIdByItem(oCItem* item) const;
  void RememberPendingTake(std::int32_t instance_id, std::int32_t amount, std::int32_t previous_amount);
  std::optional<PendingInventoryAdd> ConsumePendingTake(std::int32_t instance_id);

private:
  oCItem* FindExistingItem(const std::string& instance, const glm::vec3& position) const;
  oCItem* CreateItem(const std::string& instance, std::int32_t amount, bool physics_enabled, const glm::vec3& position,
                     const glm::vec3& rotation) const;
  void ApplyTransform(oCItem* item, std::int32_t amount, bool physics_enabled, const glm::vec3& position, const glm::vec3& rotation) const;
  void ApplyPhysicsState(oCItem* item, bool physics_enabled) const;

  std::unordered_map<std::uint32_t, ItemGround> items_;
  std::unordered_map<oCItem*, std::uint32_t> ids_by_item_;
  std::unordered_map<std::int32_t, std::deque<PendingInventoryAdd>> pending_takes_by_instance_;
};

void BindItemGround(sol::state& lua);

}  // namespace gmp::gothic
