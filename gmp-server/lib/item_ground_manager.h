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
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <glm/glm.hpp>

class ItemGroundManager {
public:
  using ItemGroundId = std::uint32_t;
  using PlayerId = std::uint32_t;

  struct CreateOptions {
    std::string instance;
    std::int32_t amount{1};
    bool physics_enabled{false};
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    std::string world;
    std::int32_t virtual_world{0};
  };

  struct ItemGround {
    ItemGroundId id{0};
    std::string instance;
    std::int32_t amount{1};
    bool physics_enabled{false};
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    std::string world;
    std::int32_t virtual_world{0};
    std::unordered_set<PlayerId> streamed_to;
    // A callback may re-enter game APIs; reserve before dispatching pickup.
    std::optional<PlayerId> reserved_by;
  };

  ItemGround& Create(CreateOptions options);
  bool Destroy(ItemGroundId id);
  ItemGround* Get(ItemGroundId id);
  const ItemGround* Get(ItemGroundId id) const;
  bool Reserve(ItemGroundId id, PlayerId player_id);
  void ReleaseReservation(ItemGroundId id, PlayerId player_id);
  void Clear();

  std::unordered_map<ItemGroundId, ItemGround>& Items();
  const std::unordered_map<ItemGroundId, ItemGround>& Items() const;

private:
  ItemGroundId AllocateId();

  std::unordered_map<ItemGroundId, ItemGround> items_;
  ItemGroundId next_id_{1};
};

