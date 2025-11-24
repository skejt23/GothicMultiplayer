
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

#include "function_bind.h"

#include <algorithm>
#include <optional>
#include <vector>
#include <glm/glm.hpp>
#include <optional>

#include "game_server.h"
#include "packet.h"
#include "shared/lua_runtime/shared_bind.h"
#include "shared/lua_runtime/timer_manager.h"

using namespace std;

namespace {

std::uint8_t ClampColorComponent(int value) {
  return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

std::optional<float> GetOptionalFloat(const sol::table& table, const char* lowerKey, const char* upperKey) {
  if (auto value = table.get<sol::optional<float>>(lowerKey); value) {
    return std::optional<float>(*value);
  }
  if (auto value = table.get<sol::optional<float>>(upperKey); value) {
    return std::optional<float>(*value);
  }
  return std::nullopt;
}

std::optional<glm::vec3> ParseSpawnPosition(sol::variadic_args args) {
  if (args.size() == 0) {
    return std::nullopt;
  }

  if (args.size() == 1) {
    sol::object arg = args[0];
    if (arg.get_type() == sol::type::table) {
      sol::table tbl = arg;
      auto x = GetOptionalFloat(tbl, "x", "X");
      auto y = GetOptionalFloat(tbl, "y", "Y");
      auto z = GetOptionalFloat(tbl, "z", "Z");
      if (x && y && z) {
        return glm::vec3(*x, *y, *z);
      }
      SPDLOG_WARN("spawnPlayer table argument must contain x, y, z fields");
      return std::nullopt;
    }
    SPDLOG_WARN("spawnPlayer expects a table with coordinates or three numeric arguments");
    return std::nullopt;
  }

  if (args.size() == 3) {
    try {
      float x = args[0].as<float>();
      float y = args[1].as<float>();
      float z = args[2].as<float>();
      return glm::vec3(x, y, z);
    } catch (const sol::error& err) {
      SPDLOG_ERROR("spawnPlayer received invalid coordinate arguments: {}", err.what());
      return std::nullopt;
    }
  }

  SPDLOG_WARN("spawnPlayer called with unsupported arguments");
  return std::nullopt;
}

std::optional<glm::vec3> Function_ParsePositionTable(const sol::table& table) {
  auto x = GetOptionalFloat(table, "x", "X");
  auto y = GetOptionalFloat(table, "y", "Y");
  auto z = GetOptionalFloat(table, "z", "Z");
  if (x && y && z) {
    return glm::vec3(*x, *y, *z);
  }

  SPDLOG_WARN("Position table must contain x, y, z fields");
  return std::nullopt;
}

}  // namespace

// Functions
int Function_Log(std::string name, std::string text) {
  std::ofstream logfile;
  logfile.open(name, std::ios_base::app);
  if (logfile.is_open()) {
    logfile << text << "\n";
    logfile.close();
  }
  return 0;
}


bool Function_SendMessageToAll(int r, int g, int b, const std::string& text) {
  if (!g_server) {
    SPDLOG_WARN("Cannot send message before the server is initialized");
    return false;
  }

  g_server->SendMessageToAll(ClampColorComponent(r), ClampColorComponent(g), ClampColorComponent(b), text);
  return true;
}

bool Function_SendMessageToPlayer(std::uint32_t player_id, int r, int g, int b, const std::string& text) {
  if (!g_server) {
    SPDLOG_WARN("Cannot send message before the server is initialized");
    return false;
  }

  g_server->SendMessageToPlayer(player_id, ClampColorComponent(r), ClampColorComponent(g), ClampColorComponent(b), text);
  return true;
}

bool Function_SendPlayerMessageToAll(std::uint32_t sender_id, int r, int g, int b, const std::string& text) {
  if (!g_server) {
    SPDLOG_WARN("Cannot send player message before the server is initialized");
    return false;
  }

  g_server->SendPlayerMessageToAll(sender_id, ClampColorComponent(r), ClampColorComponent(g), ClampColorComponent(b), text);
  return true;
}

bool Function_SendPlayerMessageToPlayer(std::uint32_t sender_id, std::uint32_t receiver_id, int r, int g, int b,
                                        const std::string& text) {
  if (!g_server) {
    SPDLOG_WARN("Cannot send player message before the server is initialized");
    return false;
  }

  g_server->SendPlayerMessageToPlayer(sender_id, receiver_id, ClampColorComponent(r), ClampColorComponent(g),
                                      ClampColorComponent(b), text);
  return true;
}

bool Function_SpawnPlayer(std::uint32_t player_id, sol::variadic_args args) {
  if (!g_server) {
    SPDLOG_WARN("Cannot spawn player before the server is initialized");
    return false;
  }

  auto position_override = ParseSpawnPosition(args);
  return g_server->SpawnPlayer(player_id, position_override);
}

bool Function_SetPlayerPosition(std::uint32_t player_id, float x, float y, float z) {
  if (!g_server) {
    SPDLOG_WARN("Cannot set player position before the server is initialized");
    return false;
  }

  return g_server->SetPlayerPosition(player_id, glm::vec3{x, y, z});
}

sol::object Function_GetPlayerPosition(std::uint32_t player_id, sol::this_state ts) {
  if (!g_server) {
    SPDLOG_WARN("Cannot get player position before the server is initialized");
    return sol::nil;
  }

  auto position = g_server->GetPlayerPosition(player_id);
  if (!position.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  sol::table position_table = lua.create_table();
  position_table["x"] = position->x;
  position_table["y"] = position->y;
  position_table["z"] = position->z;
  return position_table;
}

void Function_SetServerWorld(const std::string& world) {
  if (!g_server) {
    SPDLOG_WARN("Cannot set server world before the server is initialized");
    return;
  }

  g_server->SetServerWorld(world);
}

std::string Function_GetServerWorld() {
  if (!g_server) {
    SPDLOG_WARN("Cannot get server world before the server is initialized");
    return std::string{};
  }

  return g_server->GetServerWorld();
}

std::vector<std::uint32_t> Function_FindNearbyPlayers(const sol::table& position_table, int radius,
                                                      const std::string& world, sol::optional<int> virtual_world) {
  if (!g_server) {
    SPDLOG_WARN("Cannot find players before the server is initialized");
    return {};
  }

  auto position = Function_ParsePositionTable(position_table);
  if (!position.has_value()) {
    return {};
  }

  return g_server->FindNearbyPlayers(*position, static_cast<float>(radius), world, virtual_world.value_or(0));
}

std::vector<std::uint32_t> Function_GetSpawnedPlayersForPlayer(std::uint32_t player_id) {
  if (!g_server) {
    SPDLOG_WARN("Cannot get spawned players before the server is initialized");
    return {};
  }

  return g_server->GetSpawnedPlayersForPlayer(player_id);
}

std::vector<std::uint32_t> Function_GetStreamedPlayersByPlayer(std::uint32_t player_id) {
  if (!g_server) {
    SPDLOG_WARN("Cannot get streamed players before the server is initialized");
    return {};
  }

  return g_server->GetStreamedPlayersByPlayer(player_id);
}

bool Function_SetTime(int hour, int min, sol::optional<int> day) {
  if (!g_server) {
    SPDLOG_WARN("Cannot set time before the server is initialized");
    return false;
  }

  return g_server->SetTime(hour, min, day.value_or(0));
}

sol::object Function_GetTime(sol::this_state ts) {
  if (!g_server) {
    SPDLOG_WARN("Cannot get time before the server is initialized");
    return sol::nil;
  }

  auto time = g_server->GetTime();
  sol::state_view lua(ts);
  sol::table time_table = lua.create_table();
  time_table["day"] = time.day_;
  time_table["hour"] = time.hour_;
  time_table["min"] = time.min_;
  return time_table;
}

// Register Functions
void lua::bindings::BindFunctions(sol::state& lua, TimerManager& timer_manager) {
  SetServerInfoProvider({
      [] { return g_server ? g_server->GetHostname() : std::string{}; },
      [] { return g_server ? static_cast<int>(g_server->GetMaxSlots()) : 0; },
      [] {
        if (!g_server) {
          return std::vector<int>{};
        }

        std::vector<int> players;
        g_server->GetPlayerManager().ForEachIngamePlayer(
            [&players](const PlayerManager::Player& player) { players.push_back(player.player_id); });
        return players;
      },
      [] {
        if (!g_server) {
          return 0;
        }

        std::uint32_t count = 0;
        g_server->GetPlayerManager().ForEachIngamePlayer([&count](const PlayerManager::Player&) { ++count; });
        return static_cast<int>(count);
      },
  });
  lua["Log"] = Function_Log;
  lua.new_usertype<Packet>("Packet", sol::constructors<Packet()>(), "reset", &Packet::reset, 
                           "send", &Packet::send, "sendToAll", &Packet::sendToAll, 
                           "writeBool", &Packet::writeBool, "writeInt8", &Packet::writeInt8,
                           "writeUInt8", &Packet::writeUInt8, "writeInt16", &Packet::writeInt16, 
                           "writeUInt16", &Packet::writeUInt16, "writeInt32", &Packet::writeInt32, 
                           "writeUInt32", &Packet::writeUInt32, "writeFloat", &Packet::writeFloat,
                           "writeString", &Packet::writeString, "writeBlob", &Packet::writeBlob, 
                           "readBool", &Packet::readBool, "readInt8", &Packet::readInt8, 
                           "readUInt8", &Packet::readUInt8, "readInt16", &Packet::readInt16,
                           "readUInt16", &Packet::readUInt16, "readInt32", &Packet::readInt32, 
                           "readUInt32", &Packet::readUInt32, "readFloat", &Packet::readFloat, 
                           "readString", &Packet::readString, "readBlob", &Packet::readBlob);

  lua["sendMessageToAll"] = Function_SendMessageToAll;
  lua["sendMessageToPlayer"] = Function_SendMessageToPlayer;
  lua["sendPlayerMessageToAll"] = Function_SendPlayerMessageToAll;
  lua["sendPlayerMessageToPlayer"] = Function_SendPlayerMessageToPlayer;

  lua["spawnPlayer"] = Function_SpawnPlayer;

  lua["setPlayerPosition"] = Function_SetPlayerPosition;
  lua["getPlayerPosition"] = Function_GetPlayerPosition;

  lua["setServerWorld"] = Function_SetServerWorld;
  lua["getServerWorld"] = Function_GetServerWorld;
  
  lua["findNearbyPlayers"] = Function_FindNearbyPlayers;
  lua["getSpawnedPlayersForPlayer"] = Function_GetSpawnedPlayersForPlayer;
  lua["getStreamedPlayersByPlayer"] = Function_GetStreamedPlayersByPlayer;

  lua["setTime"] = Function_SetTime;
  lua["getTime"] = Function_GetTime;
}
