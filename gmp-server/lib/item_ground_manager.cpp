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

#include "item_ground_manager.h"

#include <limits>
#include <stdexcept>
#include <utility>

ItemGroundManager::ItemGround& ItemGroundManager::Create(CreateOptions options) {
  ItemGround item;
  item.id = AllocateId();
  item.instance = std::move(options.instance);
  item.amount = options.amount;
  item.physics_enabled = options.physics_enabled;
  item.position = options.position;
  item.rotation = options.rotation;
  item.world = std::move(options.world);
  item.virtual_world = options.virtual_world;

  auto [it, _] = items_.emplace(item.id, std::move(item));
  return it->second;
}

bool ItemGroundManager::Destroy(ItemGroundId id) {
  return items_.erase(id) > 0;
}

ItemGroundManager::ItemGround* ItemGroundManager::Get(ItemGroundId id) {
  auto it = items_.find(id);
  return it != items_.end() ? &it->second : nullptr;
}

const ItemGroundManager::ItemGround* ItemGroundManager::Get(ItemGroundId id) const {
  auto it = items_.find(id);
  return it != items_.end() ? &it->second : nullptr;
}

void ItemGroundManager::Clear() {
  items_.clear();
}

bool ItemGroundManager::Reserve(ItemGroundId id, PlayerId player_id) {
  auto* item = Get(id);
  if (!item || player_id == 0 || item->reserved_by) return false;
  item->reserved_by = player_id;
  return true;
}

void ItemGroundManager::ReleaseReservation(ItemGroundId id, PlayerId player_id) {
  auto* item = Get(id);
  if (item && item->reserved_by == player_id) item->reserved_by.reset();
}

std::unordered_map<ItemGroundManager::ItemGroundId, ItemGroundManager::ItemGround>& ItemGroundManager::Items() {
  return items_;
}

const std::unordered_map<ItemGroundManager::ItemGroundId, ItemGroundManager::ItemGround>& ItemGroundManager::Items() const {
  return items_;
}

ItemGroundManager::ItemGroundId ItemGroundManager::AllocateId() {
  // Never reuse an identity while viewers or a Lua callback can still hold it.
  if (next_id_ == 0) throw std::overflow_error("Ground item identity space exhausted");
  return next_id_++;
}
