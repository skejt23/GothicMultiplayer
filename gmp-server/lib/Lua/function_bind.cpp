
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

#include <glm/glm.hpp>

#include "game_server.h"
#include "packet.h"
#include "shared/lua_runtime/timer_manager.h"

using namespace std;

namespace {

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

void Function_SendServerMessage(const std::string& message) {
  if (!g_server) {
    SPDLOG_WARN("Cannot send server message before the server is initialized");
    return;
  }

  g_server->SendServerMessage(message);
}

bool Function_SpawnPlayer(std::uint32_t player_id, sol::variadic_args args) {
  if (!g_server) {
    SPDLOG_WARN("Cannot spawn player before the server is initialized");
    return false;
  }

  auto position_override = ParseSpawnPosition(args);
  return g_server->SpawnPlayer(player_id, position_override);
}

// Register Functions
void lua::bindings::BindFunctions(sol::state& lua, TimerManager& timer_manager) {
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


  lua["SendServerMessage"] = Function_SendServerMessage;
  lua["spawnPlayer"] = Function_SpawnPlayer;
}
