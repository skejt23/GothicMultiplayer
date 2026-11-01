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

#include "item_ground.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <limits>

#include "gothic_events.h"
#include "lua_helpers.h"
#include "shared/event.h"

using namespace Gothic_II_Addon;

namespace gmp::gothic {
namespace {

constexpr float kExistingItemSearchRadius = 200.0f;
constexpr std::uint8_t kPhysicsRefreshFrames = 3;

int ResolveItemIndex(const std::string& instance) {
  if (instance.empty() || Gothic_II_Addon::parser == nullptr) {
    return -1;
  }
  return Gothic_II_Addon::parser->GetIndex(instance.c_str());
}

float DistanceSquared(oCItem* item, const glm::vec3& position) {
  if (item == nullptr) {
    return std::numeric_limits<float>::max();
  }

  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  item->GetPositionWorld(x, y, z);
  const glm::vec3 item_position{x, y, z};
  const glm::vec3 delta = item_position - position;
  return delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
}

}  // namespace

/* luagmp (class)
 *
 * Represents a synchronized server-side ground item visible on the client.
 *
 * @version  0.3.0
 * @name     ItemGround
 * @side     client
 * @category Item
 *
 */
LuaItemGround::LuaItemGround(std::uint32_t id) : id_(id) {
}

/* luagmp (property)
 *
 * Represents the unique ground item id.
 *
 * @name     id
 * @return   (number)
 */
std::uint32_t LuaItemGround::getId() const {
  return id_;
}

/* luagmp (property)
 *
 * Represents the Gothic item instance name.
 *
 * @name     instance
 * @return   (string)
 */
std::string LuaItemGround::getInstance() const {
  auto* item_ground = ClientItemGroundManager::Instance().GetById(id_);
  return item_ground ? item_ground->instance : std::string{};
}

/* luagmp (property)
 *
 * Represents the item amount.
 *
 * @name     amount
 * @return   (number)
 */
std::int32_t LuaItemGround::getAmount() const {
  auto* item_ground = ClientItemGroundManager::Instance().GetById(id_);
  return item_ground ? item_ground->amount : 0;
}

ClientItemGroundManager& ClientItemGroundManager::Instance() {
  static ClientItemGroundManager instance;
  return instance;
}

void ClientItemGroundManager::RememberPendingTake(std::int32_t instance_id, std::int32_t amount, std::int32_t previous_amount) {
  if (instance_id <= 0 || amount <= 0) {
    return;
  }

  pending_takes_by_instance_[instance_id].push_back(PendingInventoryAdd{instance_id, amount, std::max<std::int32_t>(0, previous_amount)});
}

std::optional<ClientItemGroundManager::PendingInventoryAdd> ClientItemGroundManager::ConsumePendingTake(std::int32_t instance_id) {
  auto it = pending_takes_by_instance_.find(instance_id);
  if (it == pending_takes_by_instance_.end() || it->second.empty()) {
    return std::nullopt;
  }

  PendingInventoryAdd pending = it->second.front();
  it->second.pop_front();
  if (it->second.empty()) {
    pending_takes_by_instance_.erase(it);
  }
  return pending;
}

void ClientItemGroundManager::Upsert(std::uint32_t id, const std::string& instance, std::int32_t amount, bool physics_enabled,
                                     const glm::vec3& position, const glm::vec3& rotation) {
  if (id == 0 || instance.empty()) {
    return;
  }

  const bool exists = items_.contains(id);
  auto& item_ground = items_[id];
  item_ground.id = id;
  item_ground.instance = instance;
  item_ground.amount = std::max<std::int32_t>(1, amount);
  item_ground.physics_enabled = physics_enabled;
  item_ground.position = position;
  item_ground.rotation = rotation;

  if (item_ground.item == nullptr) {
    item_ground.item = FindExistingItem(item_ground.instance, item_ground.position);
  }
  if (item_ground.item == nullptr) {
    item_ground.item = CreateItem(item_ground.instance, item_ground.amount, item_ground.physics_enabled, item_ground.position, item_ground.rotation);
  } else {
    ApplyTransform(item_ground.item, item_ground.amount, item_ground.physics_enabled, item_ground.position, item_ground.rotation);
  }

  if (item_ground.item != nullptr) {
    ids_by_item_[item_ground.item] = id;
    if (item_ground.physics_enabled) {
      item_ground.pending_physics_refreshes = kPhysicsRefreshFrames;
    }
  }

  if (!exists) {
    EventManager::Instance().TriggerEvent(kEventOnItemGroundCreateName, LuaItemGround(id));
  }
}

void ClientItemGroundManager::Destroy(std::uint32_t id, bool emit_event) {
  auto it = items_.find(id);
  if (it == items_.end()) {
    return;
  }

  if (emit_event) {
    EventManager::Instance().TriggerEvent(kEventOnItemGroundDestroyName, LuaItemGround(id));
  }

  if (it->second.item != nullptr) {
    ids_by_item_.erase(it->second.item);
    it->second.item->RemoveVobFromWorld();
  }
  items_.erase(it);
}

void ClientItemGroundManager::DetachItem(std::uint32_t id) {
  auto it = items_.find(id);
  if (it == items_.end()) {
    return;
  }

  if (it->second.item != nullptr) {
    ids_by_item_.erase(it->second.item);
    it->second.item = nullptr;
  }
}

void ClientItemGroundManager::Clear(bool emit_event) {
  for (auto& [_, item_ground] : items_) {
    if (item_ground.item != nullptr) {
      item_ground.item->RemoveVobFromWorld();
    }
  }
  items_.clear();
  ids_by_item_.clear();
  pending_takes_by_instance_.clear();

  if (emit_event) {
    EventManager::Instance().TriggerEvent(kEventOnItemsGroundDestroyName, 0);
  }
}

void ClientItemGroundManager::RefreshPending() {
  if (ogame == nullptr || ogame->GetGameWorld() == nullptr || zfactory == nullptr || Gothic_II_Addon::parser == nullptr) {
    return;
  }

  for (auto& [id, item_ground] : items_) {
    if (item_ground.item != nullptr) {
      if (item_ground.pending_physics_refreshes > 0) {
        ApplyPhysicsState(item_ground.item, item_ground.physics_enabled);
        --item_ground.pending_physics_refreshes;
      }
      continue;
    }
    if (ResolveItemIndex(item_ground.instance) < 0) {
      continue;
    }

    item_ground.item = FindExistingItem(item_ground.instance, item_ground.position);
    if (item_ground.item == nullptr) {
      item_ground.item =
          CreateItem(item_ground.instance, item_ground.amount, item_ground.physics_enabled, item_ground.position, item_ground.rotation);
    } else {
      ApplyTransform(item_ground.item, item_ground.amount, item_ground.physics_enabled, item_ground.position, item_ground.rotation);
    }

    if (item_ground.item != nullptr) {
      ids_by_item_[item_ground.item] = id;
      if (item_ground.physics_enabled) {
        item_ground.pending_physics_refreshes = kPhysicsRefreshFrames;
      }
    }
  }
}

ClientItemGroundManager::ItemGround* ClientItemGroundManager::GetById(std::uint32_t id) {
  auto it = items_.find(id);
  return it != items_.end() ? &it->second : nullptr;
}

const ClientItemGroundManager::ItemGround* ClientItemGroundManager::GetById(std::uint32_t id) const {
  auto it = items_.find(id);
  return it != items_.end() ? &it->second : nullptr;
}

const std::unordered_map<std::uint32_t, ClientItemGroundManager::ItemGround>& ClientItemGroundManager::Items() const {
  return items_;
}

std::optional<std::uint32_t> ClientItemGroundManager::GetIdByItem(oCItem* item) const {
  auto it = ids_by_item_.find(item);
  if (it == ids_by_item_.end()) {
    return std::nullopt;
  }
  return it->second;
}

oCItem* ClientItemGroundManager::FindExistingItem(const std::string& instance, const glm::vec3& position) const {
  if (ogame == nullptr || ogame->GetGameWorld() == nullptr) {
    return nullptr;
  }

  const int index = ResolveItemIndex(instance);
  if (index < 0) {
    return nullptr;
  }

  oCItem* best_item = nullptr;
  float best_distance = kExistingItemSearchRadius * kExistingItemSearchRadius;
  zCListSort<oCItem>* item_list = ogame->GetGameWorld()->voblist_items;
  for (int i = 0; item_list && i < item_list->GetNumInList(); ++i) {
    item_list = item_list->GetNextInList();
    if (item_list == nullptr) {
      break;
    }

    oCItem* item = item_list->GetData();
    if (item == nullptr || ids_by_item_.contains(item) || item->GetInstance() != index) {
      continue;
    }

    const float distance = DistanceSquared(item, position);
    if (distance < best_distance) {
      best_distance = distance;
      best_item = item;
    }
  }

  return best_item;
}

oCItem* ClientItemGroundManager::CreateItem(const std::string& instance, std::int32_t amount, bool physics_enabled, const glm::vec3& position,
                                            const glm::vec3& rotation) const {
  if (ogame == nullptr || ogame->GetGameWorld() == nullptr || zfactory == nullptr || Gothic_II_Addon::parser == nullptr) {
    return nullptr;
  }

  const int index = ResolveItemIndex(instance);
  if (index < 0) {
    SPDLOG_WARN("Could not find ground item instance {}", instance);
    return nullptr;
  }

  auto* item = static_cast<oCItem*>(zfactory->CreateItem(index));
  if (item == nullptr) {
    SPDLOG_WARN("Could not create ground item instance {}", instance);
    return nullptr;
  }

  ApplyTransform(item, amount, physics_enabled, position, rotation);
  ogame->GetGameWorld()->AddVob(item);
  ApplyPhysicsState(item, physics_enabled);
  return item;
}

void ClientItemGroundManager::ApplyTransform(oCItem* item, std::int32_t amount, bool physics_enabled, const glm::vec3& position,
                                             const glm::vec3& rotation) const {
  if (item == nullptr) {
    return;
  }

  item->amount = std::max<std::int32_t>(1, amount);
  zMAT4 matrix = item->GetNewTrafoObjToWorld();
  matrix.SetByEulerAngles(lua_helpers::LuaRotationToGothicEuler(zVEC3(rotation.x, rotation.y, rotation.z)));
  matrix.SetTranslation(zVEC3(position.x, position.y, position.z));
  item->SetTrafoObjToWorld(matrix);
  ApplyPhysicsState(item, physics_enabled);
}

void ClientItemGroundManager::ApplyPhysicsState(oCItem* item, bool physics_enabled) const {
  if (item == nullptr) {
    return;
  }

  item->SetStaticVob(false);
  item->SetPhysicsEnabled(physics_enabled ? 1 : 0);
  if (physics_enabled) {
    item->SetSleeping(false);
  }
}

/* luagmp (method)
 * Checks whether this handle still refers to an existing synchronized ground item.
 * @name isValid
 * @return (boolean) True while the item exists.
 */
bool LuaItemGround::isValid() const {
  return ClientItemGroundManager::Instance().GetById(id_) != nullptr;
}

/* luagmp (class)
 *
 * Provides client-side lookup for synchronized ground items visible in the current world.
 *
 * @version  0.3.0
 * @name     ItemGroundManager
 * @side     client
 * @category Item
 *
 * @static
 */
/* luagmp (method)
 *
 * Returns a synchronized ground item by id.
 *
 * @version  0.3.0
 * @name     getById
 * @side     client
 * @category Item
 * @param    (number) id        Ground item id.
 * @return   (ItemGround|nil)   Ground item object or nil if missing.
 *
 * @static
 */
sol::object Function_ItemGround_GetById(std::uint32_t id, sol::this_state state) {
  sol::state_view lua(state);
  if (ClientItemGroundManager::Instance().GetById(id) == nullptr) {
    return sol::nil;
  }
  return sol::make_object(lua, LuaItemGround(id));
}

/* luagmp (method)
 *
 * Returns the first synchronized ground item with the given item instance.
 *
 * @version  0.3.0
 * @name     findByInstance
 * @side     client
 * @category Item
 * @param    (string) instance  Gothic item instance name.
 * @return   (ItemGround|nil)   Ground item object or nil if missing.
 *
 * @static
 */
sol::object Function_ItemGround_FindByInstance(const std::string& instance, sol::this_state state) {
  sol::state_view lua(state);
  for (const auto& [id, item_ground] : ClientItemGroundManager::Instance().Items()) {
    if (item_ground.instance == instance) {
      return sol::make_object(lua, LuaItemGround(id));
    }
  }
  return sol::nil;
}

void BindItemGround(sol::state& lua) {
  auto item_ground_type = lua.new_usertype<LuaItemGround>("ItemGround", sol::no_constructor);
  item_ground_type["id"] = sol::property(&LuaItemGround::getId);
  item_ground_type["isValid"] = &LuaItemGround::isValid;
  item_ground_type["instance"] = sol::property(&LuaItemGround::getInstance);
  item_ground_type["amount"] = sol::property(&LuaItemGround::getAmount);
  sol::table manager = lua.create_table();
  manager["getById"] = Function_ItemGround_GetById;
  manager["findByInstance"] = Function_ItemGround_FindByInstance;
  lua["ItemGroundManager"] = manager;
}

}  // namespace gmp::gothic
