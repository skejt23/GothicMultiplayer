/*
MIT License

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

#include "shared_bind.h"

#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

#include <spdlog/spdlog.h>

namespace lua {
namespace bindings {

namespace {

std::optional<ServerInfoProvider> g_server_info_provider;

std::string Function_GetHostname() {
  if (!g_server_info_provider || !g_server_info_provider->get_hostname) {
    SPDLOG_WARN("Cannot get hostname before the server info provider is set");
    return {};
  }

  return g_server_info_provider->get_hostname();
}

int Function_GetMaxSlots() {
  if (!g_server_info_provider || !g_server_info_provider->get_max_slots) {
    SPDLOG_WARN("Cannot get max slots before the server info provider is set");
    return 0;
  }

  return g_server_info_provider->get_max_slots();
}

sol::object Function_GetOnlinePlayers(sol::this_state ts) {
  sol::state_view lua(ts);
  if (!g_server_info_provider || !g_server_info_provider->get_online_players) {
    SPDLOG_WARN("Cannot get online players before the server info provider is set");
    return sol::make_object(lua, sol::lua_nil);
  }

  const auto players = g_server_info_provider->get_online_players();
  sol::table players_table = lua.create_table();
  std::uint32_t index = 1;
  for (int player_id : players) {
    players_table[index++] = player_id;
  }

  return players_table;
}

int Function_GetPlayersCount() {
  if (!g_server_info_provider || !g_server_info_provider->get_players_count) {
    SPDLOG_WARN("Cannot get players count before the server info provider is set");
    return 0;
  }

  return g_server_info_provider->get_players_count();
}

float Function_GetDistance2d(float x1, float y1, float x2, float y2) {
  const float dx = x1 - x2;
  const float dy = y1 - y2;
  return std::sqrt(dx * dx + dy * dy);
}

float Function_GetDistance3d(float x1, float y1, float z1, float x2, float y2, float z2) {
  const float dx = x1 - x2;
  const float dy = y1 - y2;
  const float dz = z1 - z2;
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

float Function_GetVectorAngle(float x1, float y1, float x2, float y2) {
  return std::atan2(y2 - y1, x2 - x1);
}

}  // namespace

void BindSharedFunctions(sol::state& lua) {
  lua["getHostname"] = Function_GetHostname;
  lua["getMaxSlots"] = Function_GetMaxSlots;
  lua["getOnlinePlayers"] = Function_GetOnlinePlayers;
  lua["getPlayersCount"] = Function_GetPlayersCount;
  lua["getDistance2d"] = Function_GetDistance2d;
  lua["getDistance3d"] = Function_GetDistance3d;
  lua["getVectorAngle"] = Function_GetVectorAngle;
}

void SetServerInfoProvider(ServerInfoProvider provider) {
  g_server_info_provider = std::move(provider);
}

}  // namespace bindings
}  // namespace lua
