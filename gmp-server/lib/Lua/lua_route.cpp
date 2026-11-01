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

#include "Lua/lua_route.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <limits>
#include <nlohmann/json.hpp>
#include <queue>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Shared/lua_runtime/bind_helpers.h"
#include "game_server.h"

namespace lua::bindings {
namespace {

std::string ExtractWorldStem(std::string world) {
  std::replace(world.begin(), world.end(), '\\', '/');
  std::filesystem::path world_path(world);
  return world_path.stem().string();
}

std::string BuildWaynetFileName(const std::string& world) {
  std::string file_stem = ExtractWorldStem(world);
  std::transform(file_stem.begin(), file_stem.end(), file_stem.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return file_stem + ".json";
}

struct WaypointNode {
  std::string name;
  glm::vec3 position{0.0f, 0.0f, 0.0f};
  float angle{0.0f};
  std::vector<int> neighbors;
};

struct FreepointNode {
  std::string name;
  glm::vec3 position{0.0f, 0.0f, 0.0f};
  float angle{0.0f};
};

class WaynetGraph {
public:
  bool Load(const std::filesystem::path& path) {
    nodes_.clear();
    freepoints_.clear();
    name_to_index_.clear();
    freepoint_name_to_index_.clear();
    zen_world_.clear();

    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
      SPDLOG_WARN("Waynet load failed: cannot open '{}'", path.string());
      return false;
    }

    nlohmann::json root;
    try {
      input >> root;
    } catch (const nlohmann::json::exception& ex) {
      SPDLOG_ERROR("Waynet load failed: invalid JSON in '{}': {}", path.string(), ex.what());
      return false;
    }

    if (!root.is_object()) {
      SPDLOG_ERROR("Waynet load failed: root in '{}' must be an object", path.string());
      return false;
    }

    const auto zen_it = root.find("zen");
    if (zen_it == root.end() || !zen_it->is_string() || zen_it->get<std::string>().empty()) {
      SPDLOG_ERROR("Waynet load failed: '{}' is missing required non-empty string field 'zen'", path.string());
      return false;
    }
    zen_world_ = zen_it->get<std::string>();

    const auto waypoints_it = root.find("waypoints");
    if (waypoints_it == root.end() || !waypoints_it->is_array()) {
      SPDLOG_ERROR("Waynet load failed: '{}' must contain array field 'waypoints'", path.string());
      return false;
    }

    const auto& waypoints = *waypoints_it;
    nodes_.reserve(waypoints.size());

    for (const auto& wp : waypoints) {
      if (!wp.is_object()) {
        SPDLOG_WARN("Waynet load skipped malformed waypoint entry (expected object)");
        continue;
      }

      const auto name_it = wp.find("name");
      const auto pos_it = wp.find("position");
      if (name_it == wp.end() || !name_it->is_string() || pos_it == wp.end() || !pos_it->is_object()) {
        SPDLOG_WARN("Waynet load skipped waypoint missing 'name' or 'position'");
        continue;
      }

      const auto x_it = pos_it->find("x");
      const auto y_it = pos_it->find("y");
      const auto z_it = pos_it->find("z");
      const auto angle_it = pos_it->find("angle");
      if (x_it == pos_it->end() || y_it == pos_it->end() || z_it == pos_it->end() || angle_it == pos_it->end() || !x_it->is_number() ||
          !y_it->is_number() || !z_it->is_number() || !angle_it->is_number()) {
        SPDLOG_WARN("Waynet load skipped waypoint '{}' with malformed position", name_it->get<std::string>());
        continue;
      }

      WaypointNode node;
      node.name = name_it->get<std::string>();
      node.position = glm::vec3(x_it->get<float>(), y_it->get<float>(), z_it->get<float>());
      node.angle = angle_it->get<float>();

      const int index = static_cast<int>(nodes_.size());
      auto [_, inserted] = name_to_index_.emplace(node.name, index);
      if (!inserted) {
        SPDLOG_WARN("Waynet load found duplicate waypoint '{}'; keeping first entry", node.name);
        continue;
      }

      nodes_.push_back(std::move(node));
    }

    for (const auto& wp : waypoints) {
      if (!wp.is_object()) {
        continue;
      }

      const auto name_it = wp.find("name");
      const auto conn_it = wp.find("connectsTo");
      if (name_it == wp.end() || !name_it->is_string() || conn_it == wp.end() || !conn_it->is_array()) {
        continue;
      }

      const auto source_it = name_to_index_.find(name_it->get<std::string>());
      if (source_it == name_to_index_.end()) {
        continue;
      }

      WaypointNode& source = nodes_[source_it->second];
      for (const auto& target_name_json : *conn_it) {
        if (!target_name_json.is_string()) {
          continue;
        }

        const auto target_it = name_to_index_.find(target_name_json.get<std::string>());
        if (target_it == name_to_index_.end()) {
          continue;
        }

        const int target_index = target_it->second;
        if (std::find(source.neighbors.begin(), source.neighbors.end(), target_index) == source.neighbors.end()) {
          source.neighbors.push_back(target_index);
        }
      }
    }

    if (nodes_.empty()) {
      SPDLOG_WARN("Waynet load completed but no valid waypoints were found in '{}'", path.string());
      return false;
    }

    const auto freepoints_it = root.find("freepoints");
    if (freepoints_it != root.end()) {
      if (!freepoints_it->is_array()) {
        SPDLOG_ERROR("Waynet load failed: '{}' field 'freepoints' must be an array", path.string());
        return false;
      }

      freepoints_.reserve(freepoints_it->size());
      for (const auto& fp : *freepoints_it) {
        if (!fp.is_object()) {
          SPDLOG_WARN("Waynet load skipped malformed freepoint entry (expected object)");
          continue;
        }

        const auto name_it = fp.find("name");
        const auto pos_it = fp.find("position");
        if (name_it == fp.end() || !name_it->is_string() || pos_it == fp.end() || !pos_it->is_object()) {
          SPDLOG_WARN("Waynet load skipped freepoint missing 'name' or 'position'");
          continue;
        }

        const std::string freepoint_name = name_it->get<std::string>();

        const auto x_it = pos_it->find("x");
        const auto y_it = pos_it->find("y");
        const auto z_it = pos_it->find("z");
        const auto angle_it = pos_it->find("angle");
        if (x_it == pos_it->end() || y_it == pos_it->end() || z_it == pos_it->end() || angle_it == pos_it->end() || !x_it->is_number() ||
            !y_it->is_number() || !z_it->is_number() || !angle_it->is_number()) {
          SPDLOG_WARN("Waynet load skipped freepoint '{}' with malformed position", freepoint_name);
          continue;
        }

        FreepointNode node;
        node.name = freepoint_name;
        node.position = glm::vec3(x_it->get<float>(), y_it->get<float>(), z_it->get<float>());
        node.angle = angle_it->get<float>();

        const int index = static_cast<int>(freepoints_.size());
        auto [_, inserted] = freepoint_name_to_index_.emplace(node.name, index);
        if (!inserted) {
          SPDLOG_WARN("Waynet load found duplicate freepoint '{}'; keeping first entry", node.name);
          continue;
        }

        freepoints_.push_back(std::move(node));
      }
    }

    return true;
  }

  const std::string& GetZenWorld() const {
    return zen_world_;
  }

  std::vector<std::string> FindPathAStar(const std::string& start_name, const std::string& end_name) const {
    std::vector<std::string> empty;

    const auto start_it = name_to_index_.find(start_name);
    const auto end_it = name_to_index_.find(end_name);
    if (start_it == name_to_index_.end() || end_it == name_to_index_.end()) {
      return empty;
    }

    const int start = start_it->second;
    const int goal = end_it->second;
    if (start == goal) {
      return {nodes_[start].name};
    }

    struct QueueNode {
      int index;
      float f_score;
      bool operator<(const QueueNode& other) const {
        return f_score > other.f_score;
      }
    };

    const std::size_t count = nodes_.size();
    const float inf = std::numeric_limits<float>::infinity();

    std::vector<float> g_score(count, inf);
    std::vector<float> f_score(count, inf);
    std::vector<int> came_from(count, -1);
    std::vector<bool> closed(count, false);

    auto estimate_cost = [&](int a, int b) {
      const glm::vec3 delta = nodes_[a].position - nodes_[b].position;
      return std::abs(delta.x) + std::abs(delta.y) + std::abs(delta.z);
    };

    g_score[start] = 0.0f;
    f_score[start] = estimate_cost(start, goal);

    std::priority_queue<QueueNode> open;
    open.push({start, f_score[start]});

    while (!open.empty()) {
      const QueueNode current = open.top();
      open.pop();

      if (closed[current.index]) {
        continue;
      }

      if (current.index == goal) {
        std::vector<std::string> path;
        int trace = goal;
        while (trace != -1) {
          path.push_back(nodes_[trace].name);
          trace = came_from[trace];
        }
        std::reverse(path.begin(), path.end());
        return path;
      }

      closed[current.index] = true;

      const WaypointNode& node = nodes_[current.index];
      for (int neighbor : node.neighbors) {
        if (neighbor < 0 || static_cast<std::size_t>(neighbor) >= count) {
          continue;
        }

        const float edge_cost = estimate_cost(current.index, neighbor);
        const float tentative = g_score[current.index] + edge_cost;
        if (tentative >= g_score[neighbor]) {
          continue;
        }

        came_from[neighbor] = current.index;
        g_score[neighbor] = tentative;
        f_score[neighbor] = tentative + estimate_cost(neighbor, goal);
        if (closed[neighbor]) {
          closed[neighbor] = false;
        }
        open.push({neighbor, f_score[neighbor]});
      }
    }

    return empty;
  }

  const WaypointNode* FindWaypointByName(const std::string& waypoint_name) const {
    const auto it = name_to_index_.find(waypoint_name);
    if (it == name_to_index_.end()) {
      return nullptr;
    }

    const int index = it->second;
    if (index < 0 || static_cast<std::size_t>(index) >= nodes_.size()) {
      return nullptr;
    }

    return &nodes_[index];
  }

  const FreepointNode* FindFreepointByName(const std::string& freepoint_name) const {
    const auto it = freepoint_name_to_index_.find(freepoint_name);
    if (it == freepoint_name_to_index_.end()) {
      return nullptr;
    }

    const int index = it->second;
    if (index < 0 || static_cast<std::size_t>(index) >= freepoints_.size()) {
      return nullptr;
    }

    return &freepoints_[index];
  }

  std::pair<const WaypointNode*, const WaypointNode*> FindNearestPair(const glm::vec3& position, float max_distance) const {
    const bool limit_distance = max_distance > 0.0f;
    const float max_distance_sq = max_distance * max_distance;
    const WaypointNode* nearest = nullptr;
    const WaypointNode* second_nearest = nullptr;
    float nearest_sq = std::numeric_limits<float>::max();
    float second_nearest_sq = std::numeric_limits<float>::max();

    for (const WaypointNode& node : nodes_) {
      const glm::vec3 delta = node.position - position;
      const float distance_sq = glm::dot(delta, delta);
      if (limit_distance && distance_sq > max_distance_sq) {
        continue;
      }

      if (distance_sq < nearest_sq) {
        second_nearest = nearest;
        second_nearest_sq = nearest_sq;
        nearest = &node;
        nearest_sq = distance_sq;
        continue;
      }

      if (distance_sq < second_nearest_sq) {
        second_nearest = &node;
        second_nearest_sq = distance_sq;
      }
    }

    return {nearest, second_nearest};
  }

  std::pair<const FreepointNode*, const FreepointNode*> FindNearestFreepointPair(const glm::vec3& position, float max_distance) const {
    const bool limit_distance = max_distance > 0.0f;
    const float max_distance_sq = max_distance * max_distance;
    const FreepointNode* nearest = nullptr;
    const FreepointNode* second_nearest = nullptr;
    float nearest_sq = std::numeric_limits<float>::max();
    float second_nearest_sq = std::numeric_limits<float>::max();

    for (const FreepointNode& node : freepoints_) {
      const glm::vec3 delta = node.position - position;
      const float distance_sq = glm::dot(delta, delta);
      if (limit_distance && distance_sq > max_distance_sq) {
        continue;
      }

      if (distance_sq < nearest_sq) {
        second_nearest = nearest;
        second_nearest_sq = nearest_sq;
        nearest = &node;
        nearest_sq = distance_sq;
        continue;
      }

      if (distance_sq < second_nearest_sq) {
        second_nearest = &node;
        second_nearest_sq = distance_sq;
      }
    }

    return {nearest, second_nearest};
  }

  const std::vector<WaypointNode>& GetWaypoints() const {
    return nodes_;
  }

private:
  std::string zen_world_;
  std::vector<WaypointNode> nodes_;
  std::vector<FreepointNode> freepoints_;
  std::unordered_map<std::string, int> name_to_index_;
  std::unordered_map<std::string, int> freepoint_name_to_index_;
};

class WaynetRepository {
public:
  const WaynetGraph* GetForWorld(const std::string& world) {
    const std::filesystem::path path = WaypointsRootPath() / BuildWaynetFileName(world);
    const std::string key = path.generic_string();

    auto it = cache_.find(key);
    if (it == cache_.end()) {
      WaynetGraph graph;
      if (!graph.Load(path)) {
        return nullptr;
      }
      auto [insert_it, _] = cache_.emplace(key, std::move(graph));
      it = insert_it;
    }

    if (it->second.GetZenWorld() != world) {
      SPDLOG_WARN("Waynet '{}' has zen='{}' but world '{}' was requested", path.string(), it->second.GetZenWorld(), world);
      return nullptr;
    }

    return &it->second;
  }

  void PreloadDefaultWorld() {
    if (!g_server) {
      return;
    }

    const std::string world = g_server->GetServerWorld();
    if (world.empty()) {
      return;
    }

    const WaynetGraph* graph = GetForWorld(world);
    if (graph) {
      SPDLOG_INFO("Waynet for '{}' loaded from '{}/{}'", world, WaypointsRootPath().string(), BuildWaynetFileName(world));
    }
  }

private:
  static std::filesystem::path WaypointsRootPath() {
    return std::filesystem::current_path() / "data/navigation";
  }

  std::unordered_map<std::string, WaynetGraph> cache_;
};

WaynetRepository g_waynet_repository;

sol::table MakeWaypointTable(sol::state_view lua, const WaypointNode& waypoint) {
  sol::table tbl = ::lua::bind_helpers::MakeVec3Table(lua, waypoint.position, waypoint.angle);
  tbl["name"] = waypoint.name;
  return tbl;
}

sol::table MakeFreepointTable(sol::state_view lua, const FreepointNode& freepoint) {
  sol::table tbl = ::lua::bind_helpers::MakeVec3Table(lua, freepoint.position, freepoint.angle);
  tbl["name"] = freepoint.name;
  return tbl;
}

/* luagmp (func)
 *
 * Retrieve world position of a waypoint by name.
 *
 * @version  0.3.0
 * @name     getWaypoint
 * @side     server
 * @category World
 * @param    (string) world      World name in which the waypoint exists.
 * @param    (string) name       Waypoint name.
 * @return   ({x, y, z, angle}|nil)     Waypoint position or nil.
 *
 */
sol::object Function_GetWaypoint(const std::string& world, const std::string& waypoint_name, sol::this_state ts) {
  sol::state_view lua(ts);
  const WaynetGraph* graph = g_waynet_repository.GetForWorld(world);
  if (!graph) {
    return sol::nil;
  }

  const WaypointNode* waypoint = graph->FindWaypointByName(waypoint_name);
  if (!waypoint) {
    return sol::nil;
  }

  return sol::make_object(lua, ::lua::bind_helpers::MakeVec3Table(lua, waypoint->position, waypoint->angle));
}

/* luagmp (func)
 *
 * Retrieve world position of a freepoint by name.
 *
 * @version  0.3.0
 * @name     getFreepoint
 * @side     server
 * @category World
 * @param    (string) world      World name in which the freepoint exists.
 * @param    (string) name       Freepoint name.
 * @return   ({x, y, z, angle}|nil)     Freepoint position or nil.
 *
 */
sol::object Function_GetFreepoint(const std::string& world, const std::string& freepoint_name, sol::this_state ts) {
  sol::state_view lua(ts);
  const WaynetGraph* graph = g_waynet_repository.GetForWorld(world);
  if (!graph) {
    return sol::nil;
  }

  const FreepointNode* freepoint = graph->FindFreepointByName(freepoint_name);
  if (!freepoint) {
    return sol::nil;
  }

  return sol::make_object(lua, ::lua::bind_helpers::MakeVec3Table(lua, freepoint->position, freepoint->angle));
}

/* luagmp (func)
 *
 * Retrieve nearest waypoint for a given position.
 *
 * @version  0.3.0
 * @name     getNearestWaypoint
 * @side     server
 * @category World
 * @param    (string) world            World name in which the waypoint exists.
 * @param    (number) x                Position X.
 * @param    (number) y                Position Y.
 * @param    (number) z                Position Z.
 * @param    (number|nil) distance     Optional maximum search distance.
 * @return   ({name, x, y, z, angle}|nil)     Waypoint information or nil.
 *
 */
sol::object Function_GetNearestWaypoint(const std::string& world, float x, float y, float z, sol::optional<float> distance, sol::this_state ts) {
  sol::state_view lua(ts);
  const WaynetGraph* graph = g_waynet_repository.GetForWorld(world);
  if (!graph) {
    return sol::nil;
  }

  const auto [nearest, _] = graph->FindNearestPair(glm::vec3{x, y, z}, distance.value_or(-1.0f));
  if (!nearest) {
    return sol::nil;
  }

  return sol::make_object(lua, MakeWaypointTable(lua, *nearest));
}

/* luagmp (func)
 *
 * Retrieve nearest freepoint for a given position.
 *
 * @version  0.3.0
 * @name     getNearestFreepoint
 * @side     server
 * @category World
 * @param    (string) world            World name in which the freepoint exists.
 * @param    (number) x                Position X.
 * @param    (number) y                Position Y.
 * @param    (number) z                Position Z.
 * @param    (number|nil) distance     Optional maximum search distance.
 * @return   ({name, x, y, z, angle}|nil)     Freepoint information or nil.
 *
 */
sol::object Function_GetNearestFreepoint(const std::string& world, float x, float y, float z, sol::optional<float> distance, sol::this_state ts) {
  sol::state_view lua(ts);
  const WaynetGraph* graph = g_waynet_repository.GetForWorld(world);
  if (!graph) {
    return sol::nil;
  }

  const auto [nearest, _] = graph->FindNearestFreepointPair(glm::vec3{x, y, z}, distance.value_or(-1.0f));
  if (!nearest) {
    return sol::nil;
  }

  return sol::make_object(lua, MakeFreepointTable(lua, *nearest));
}

/* luagmp (func)
 *
 * Retrieve second nearest waypoint for a given position.
 *
 * @version  0.3.0
 * @name     getNextNearestWaypoint
 * @side     server
 * @category World
 * @param    (string) world            World name in which the waypoint exists.
 * @param    (number) x                Position X.
 * @param    (number) y                Position Y.
 * @param    (number) z                Position Z.
 * @return   ({name, x, y, z, angle}|nil)     Waypoint information or nil.
 *
 */
sol::object Function_GetNextNearestWaypoint(const std::string& world, float x, float y, float z, sol::this_state ts) {
  sol::state_view lua(ts);
  const WaynetGraph* graph = g_waynet_repository.GetForWorld(world);
  if (!graph) {
    return sol::nil;
  }

  const auto [_, second_nearest] = graph->FindNearestPair(glm::vec3{x, y, z}, -1.0f);
  if (!second_nearest) {
    return sol::nil;
  }

  return sol::make_object(lua, MakeWaypointTable(lua, *second_nearest));
}

/* luagmp (func)
 *
 * Retrieve second nearest freepoint for a given position.
 *
 * @version  0.3.0
 * @name     getNextNearestFreepoint
 * @side     server
 * @category World
 * @param    (string) world            World name in which the freepoint exists.
 * @param    (number) x                Position X.
 * @param    (number) y                Position Y.
 * @param    (number) z                Position Z.
 * @return   ({name, x, y, z, angle}|nil)     Freepoint information or nil.
 *
 */
sol::object Function_GetNextNearestFreepoint(const std::string& world, float x, float y, float z, sol::this_state ts) {
  sol::state_view lua(ts);
  const WaynetGraph* graph = g_waynet_repository.GetForWorld(world);
  if (!graph) {
    return sol::nil;
  }

  const auto [_, second_nearest] = graph->FindNearestFreepointPair(glm::vec3{x, y, z}, -1.0f);
  if (!second_nearest) {
    return sol::nil;
  }

  return sol::make_object(lua, MakeFreepointTable(lua, *second_nearest));
}

/* luagmp (class)
 *
 * Represents a complete route between two waypoint names computed from server-side waynet JSON data.
 *
 * @version  0.3.0
 * @name     Route
 * @side     server
 * @category World
 *
 */
class Route {
public:
  /* luagmp (constructor)
   * Computes a route in a server world.
   * @name Route
   * @param (string) world World name.
   * @param (string) startWp Start waypoint name.
   * @param (string) endWp End waypoint name.
   */
  Route(std::string world, std::string start_wp, std::string end_wp)
      : world_(std::move(world)), start_wp_(std::move(start_wp)), end_wp_(std::move(end_wp)) {
    const WaynetGraph* graph = g_waynet_repository.GetForWorld(world_);
    if (!graph) {
      SPDLOG_WARN("Route('{}', '{}', '{}') failed: missing or invalid waynet", world_, start_wp_, end_wp_);
      return;
    }
    waypoints_ = graph->FindPathAStar(start_wp_, end_wp_);
  }

  /* luagmp (method)
   * Returns the start waypoint name.
   * @name getStart
   * @return (string)
   */
  const std::string& getStart() const {
    return start_wp_;
  }

  /* luagmp (method)
   * Returns the end waypoint name.
   * @name getEnd
   * @return (string)
   */
  const std::string& getEnd() const {
    return end_wp_;
  }

  /* luagmp (method)
   * Returns the number of waypoint names in the route.
   * @name getWaypointCount
   * @return (number)
   */
  int getWaypointCount() const {
    return static_cast<int>(waypoints_.size());
  }

  /* luagmp (method)
   * Returns a one-based array of waypoint names.
   * @name getWaypointNames
   * @return (table)
   */
  sol::as_table_t<std::vector<std::string>> getWaypointNames() const {
    return sol::as_table(waypoints_);
  }

private:
  std::string world_;
  std::string start_wp_;
  std::string end_wp_;
  std::vector<std::string> waypoints_;
};

}  // namespace

void BindRoute(sol::state& lua) {
  sol::usertype<Route> route_type =
      lua.new_usertype<Route>("Route", sol::constructors<Route(const std::string&, const std::string&, const std::string&)>());
  route_type["getStart"] = &Route::getStart;
  route_type["getEnd"] = &Route::getEnd;
  route_type["getWaypointCount"] = &Route::getWaypointCount;
  route_type["getWaypointNames"] = &Route::getWaypointNames;
  lua["getWaypoint"] = Function_GetWaypoint;
  lua["getNearestWaypoint"] = Function_GetNearestWaypoint;
  lua["getNextNearestWaypoint"] = Function_GetNextNearestWaypoint;
  lua["getFreepoint"] = Function_GetFreepoint;
  lua["getNearestFreepoint"] = Function_GetNearestFreepoint;
  lua["getNextNearestFreepoint"] = Function_GetNextNearestFreepoint;
  g_waynet_repository.PreloadDefaultWorld();
}

}  // namespace lua::bindings
