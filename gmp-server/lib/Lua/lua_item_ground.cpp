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

#include "lua_item_ground.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <utility>

#include "../game_server.h"
#include "Shared/lua_runtime/bind_helpers.h"

namespace lua {
namespace bindings {
namespace {

ItemGroundManager::ItemGround* GetItemGround(ItemGroundManager::ItemGroundId id) {
  return g_server ? g_server->GetItemGroundManager().Get(id) : nullptr;
}

template <typename T>
T GetValueOr(const sol::table& table, const char* field, T fallback) {
  sol::object value = table[field];
  if (!value.valid() || value.get_type() == sol::type::nil) {
    return fallback;
  }

  return value.as<T>();
}

glm::vec3 ReadVec3(const sol::table& table, const char* field, const glm::vec3& fallback) {
  sol::object value = table[field];
  if (value.get_type() != sol::type::table) {
    return fallback;
  }

  sol::table vec = value.as<sol::table>();
  return glm::vec3(vec.get_or("x", fallback.x), vec.get_or("y", fallback.y), vec.get_or("z", fallback.z));
}

sol::object MakeItemGroundObject(ItemGroundManager::ItemGroundId id, sol::this_state state) {
  sol::state_view lua(state);
  if (GetItemGround(id) == nullptr) {
    return sol::nil;
  }
  return sol::make_object(lua, LuaItemGround(id));
}

ItemGroundManager::ItemGroundId ReadItemGroundId(const sol::object& object) {
  if (object.get_type() == sol::type::number) {
    return object.as<ItemGroundManager::ItemGroundId>();
  }
  if (object.is<LuaItemGround>()) {
    return object.as<LuaItemGround>().getId();
  }
  return 0;
}

}  // namespace

/* luagmp (class)
 *
 * Represents a synchronized item placed in the world.
 *
 * @version  0.3.0
 * @name     ItemGround
 * @side     server
 * @category Item
 *
 */
LuaItemGround::LuaItemGround(ItemGroundManager::ItemGroundId id) : id_(id) {
}

/* luagmp (property)
 *
 * Represents the unique ground item id.
 *
 * @name     id
 * @return   (number)
 */
ItemGroundManager::ItemGroundId LuaItemGround::getId() const {
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
  auto* item_ground = GetItemGround(id_);
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
  auto* item_ground = GetItemGround(id_);
  return item_ground ? item_ground->amount : 0;
}

/* luagmp (property)
 *
 * Enables or disables client-side physics for this ground item.
 *
 * @name     physicsEnabled
 * @return   (boolean)
 */
bool LuaItemGround::getPhysicsEnabled() const {
  auto* item_ground = GetItemGround(id_);
  return item_ground ? item_ground->physics_enabled : false;
}

/// Assigns the physicsEnabled property using the same value format as its getter.
void LuaItemGround::setPhysicsEnabled(bool enabled) {
  if (g_server) {
    g_server->SetItemGroundPhysicsEnabled(id_, enabled);
  }
}

/* luagmp (property)
 *
 * Represents the world name where this ground item exists.
 *
 * @name     world
 * @return   (string)
 */
std::string LuaItemGround::getWorld() const {
  auto* item_ground = GetItemGround(id_);
  return item_ground ? item_ground->world : std::string{};
}

/* luagmp (property)
 *
 * Represents the virtual world id where this ground item exists.
 *
 * @name     virtualWorld
 * @return   (number)
 */
std::int32_t LuaItemGround::getVirtualWorld() const {
  auto* item_ground = GetItemGround(id_);
  return item_ground ? item_ground->virtual_world : 0;
}

/// Assigns the virtualWorld property using the same value format as its getter.
void LuaItemGround::setVirtualWorld(std::int32_t virtual_world) {
  if (g_server) {
    g_server->SetItemGroundVirtualWorld(id_, virtual_world);
  }
}

/* luagmp (method)
 *
 * Returns the ground item world position.
 *
 * @name     getPosition
 * @return   ({x, y, z}) Position table.
 *
 */
sol::table LuaItemGround::getPosition(sol::this_state state) const {
  auto* item_ground = GetItemGround(id_);
  return ::lua::bind_helpers::MakeVec3Table(sol::state_view(state), item_ground ? item_ground->position : glm::vec3{0.0f});
}

/* luagmp (method)
 *
 * Sets the ground item world position and synchronizes it to relevant players.
 *
 * @name     setPosition
 * @param    (number) x  X coordinate.
 * @param    (number) y  Y coordinate.
 * @param    (number) z  Z coordinate.
 *
 */
void LuaItemGround::setPosition(float x, float y, float z) {
  if (g_server) {
    g_server->SetItemGroundPosition(id_, glm::vec3{x, y, z});
  }
}

/* luagmp (method)
 *
 * Returns the ground item Euler rotation.
 *
 * @name     getRotationDegrees
 * @return   ({x, y, z}) Rotation table.
 *
 */
sol::table LuaItemGround::getRotationDegrees(sol::this_state state) const {
  auto* item_ground = GetItemGround(id_);
  return ::lua::bind_helpers::MakeVec3Table(sol::state_view(state), item_ground ? item_ground->rotation : glm::vec3{0.0f});
}

/* luagmp (method)
 *
 * Sets the ground item Euler rotation and synchronizes it to relevant players.
 *
 * @name     setRotationDegrees
 * @param    (number) x  X rotation.
 * @param    (number) y  Y rotation.
 * @param    (number) z  Z rotation.
 *
 */
void LuaItemGround::setRotationDegrees(float x, float y, float z) {
  if (g_server) {
    g_server->SetItemGroundRotation(id_, glm::vec3{x, y, z});
  }
}

/* luagmp (method)
 * Checks whether this ground item still exists.
 * @name isValid
 * @return (boolean) True while valid.
 */
bool LuaItemGround::isValid() const {
  return GetItemGround(id_) != nullptr;
}

/* luagmp (class)
 * Static lookup and creation API for synchronized ground items.
 * @name ItemGroundManager
 * @side server
 * @category Item
 * @static
 */
/* luagmp (method)
 *
 * Returns a ground item by id.
 *
 * @version  0.3.0
 * @name     getById
 * @side     server
 * @category Item
 * @param    (number) id        Ground item id.
 * @return   (ItemGround|nil)   Ground item object or nil if missing.
 *
 * @static
 */
sol::object Function_ItemGround_GetById(ItemGroundManager::ItemGroundId id, sol::this_state state) {
  return MakeItemGroundObject(id, state);
}

// Parses creation options for the checked Lua factory.

ItemGroundManager::ItemGroundId CreateItemGroundId(const sol::table& arg) {
  if (g_server == nullptr) {
    return 0;
  }

  auto instance = GetValueOr<std::string>(arg, "instance", {});
  if (instance.empty()) {
    SPDLOG_WARN("ItemGroundManager.create called without an instance");
    return 0;
  }

  ItemGroundManager::CreateOptions options;
  options.instance = instance;
  options.amount = std::max<std::int32_t>(1, GetValueOr<std::int32_t>(arg, "amount", 1));
  options.physics_enabled = GetValueOr<bool>(arg, "physicsEnabled", false);
  options.position = ReadVec3(arg, "position", glm::vec3{0.0f});
  options.rotation = ReadVec3(arg, "rotationDegrees", glm::vec3{0.0f});
  options.world = GetValueOr<std::string>(arg, "world", {});
  options.virtual_world = GetValueOr<std::int32_t>(arg, "virtualWorld", 0);
  return g_server->CreateItemGround(std::move(options));
}

/* luagmp (method)
 *
 * Destroys a synchronized ground item.
 *
 * @version  0.3.0
 * @name     destroy
 * @side     server
 * @category Item
 * @param    (number|ItemGround) itemGround  Ground item id or object.
 * @return   (boolean) True if the ground item was destroyed.
 *
 * @static
 */
bool Function_ItemGround_Destroy(const sol::object& object) {
  if (g_server == nullptr) {
    return false;
  }

  const auto id = ReadItemGroundId(object);
  if (id == 0) {
    SPDLOG_WARN("ItemGroundManager.destroy expects an item ground id or ItemGround object");
    return false;
  }
  return g_server->DestroyItemGround(id);
}

/* luagmp (method)
 * Creates a synchronized ground item.
 * @name create
 * @param (table) options instance, amount, physicsEnabled, position, rotationDegrees, world and virtualWorld.
 * @return (number|nil, table|nil) Ground item id and nil on success; nil and {message = string} on failure.
 * @static
 */
auto Function_ItemGroundManager_Create(sol::table options, sol::this_state state) {
  sol::state_view lua(state);
  const auto id = CreateItemGroundId(options);
  return std::make_tuple(
      id == 0 ? sol::make_object(lua, sol::nil) : sol::make_object(lua, id),
      id == 0 ? sol::make_object(lua, lua.create_table_with("message", "Ground item creation failed")) : sol::make_object(lua, sol::nil));
}

void BindItemGround(sol::state& lua) {
  auto item_ground_type = lua.new_usertype<LuaItemGround>("ItemGround", sol::no_constructor);
  item_ground_type["id"] = sol::property(&LuaItemGround::getId);
  item_ground_type["instance"] = sol::property(&LuaItemGround::getInstance);
  item_ground_type["isValid"] = &LuaItemGround::isValid;
  item_ground_type["amount"] = sol::property(&LuaItemGround::getAmount);
  item_ground_type["physicsEnabled"] = sol::property(&LuaItemGround::getPhysicsEnabled, &LuaItemGround::setPhysicsEnabled);
  item_ground_type["world"] = sol::property(&LuaItemGround::getWorld);
  item_ground_type["virtualWorld"] = sol::property(&LuaItemGround::getVirtualWorld, &LuaItemGround::setVirtualWorld);
  item_ground_type["getPosition"] = &LuaItemGround::getPosition;
  item_ground_type["setPosition"] = &LuaItemGround::setPosition;
  item_ground_type["getRotationDegrees"] = &LuaItemGround::getRotationDegrees;
  item_ground_type["setRotationDegrees"] = &LuaItemGround::setRotationDegrees;
  sol::table manager = lua.create_table();
  manager["getById"] = Function_ItemGround_GetById;
  manager["create"] = Function_ItemGroundManager_Create;
  manager["destroy"] = Function_ItemGround_Destroy;
  lua["ItemGroundManager"] = manager;
}

}  // namespace bindings
}  // namespace lua
