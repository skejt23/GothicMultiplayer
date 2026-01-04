
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
#include <cmath>
#include <optional>
#include <vector>
#include <glm/glm.hpp>

#include "game_server.h"
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

std::string ClampLuaText(const std::string& text, std::size_t max_len) {
  if (text.size() <= max_len) {
    return text;
  }
  return text.substr(0, max_len);
}

}  // namespace


/* luadoc (func)
*
* This function will store the given text into a log file with the specified name.
*
* @name     Log
* @side     server
* @category Utility
* @param    (string) name   The name of the log file.
* @param    (string) text   The text to log.
*
*/
int Function_Log(std::string name, std::string text) {
  std::ofstream logfile;
  logfile.open(name, std::ios_base::app);
  if (logfile.is_open()) {
    logfile << text << "\n";
    logfile.close();
  }
  return 0;
}


/* luadoc (func)
*
* Send a colored chat message to all connected players.
*
* @name     sendMessageToAll
* @side     server
* @category Chat
* @param    (int) r      Red component (0-255).
* @param    (int) g      Green component (0-255).
* @param    (int) b      Blue component (0-255).
* @param    (string) text Message text to send.
* @return   (boolean)    True on success.
*
*/
bool Function_SendMessageToAll(int r, int g, int b, const std::string& text) {
  if (!g_server) {
    SPDLOG_WARN("Cannot send message before the server is initialized");
    return false;
  }

  g_server->SendMessageToAll(ClampColorComponent(r), ClampColorComponent(g), ClampColorComponent(b), text);
  return true;
}

/* luadoc (func)
*
* Send a colored chat message to a specific player.
*
* @name     sendMessageToPlayer
* @side     server
* @category Chat
* @param    (int) player_id  Target player id.
* @param    (int) r            Red component (0-255).
* @param    (int) g            Green component (0-255).
* @param    (int) b            Blue component (0-255).
* @param    (string) text      Message text to send.
* @return   (boolean)          True on success.
*
*/
bool Function_SendMessageToPlayer(std::uint32_t player_id, int r, int g, int b, const std::string& text) {
  if (!g_server) {
    SPDLOG_WARN("Cannot send message before the server is initialized");
    return false;
  }

  g_server->SendMessageToPlayer(player_id, ClampColorComponent(r), ClampColorComponent(g), ClampColorComponent(b), text);
  return true;
}

/* luadoc (func)
*
* Send a player-sourced colored message to all players (includes sender id).
*
* @name     sendPlayerMessageToAll
* @side     server
* @category Chat
* @param    (int) sender_id  Sender player id.
* @param    (int) r             Red component (0-255).
* @param    (int) g             Green component (0-255).
* @param    (int) b             Blue component (0-255).
* @param    (string) text       Message text.
* @return   (boolean)           True on success.
*
*/
bool Function_SendPlayerMessageToAll(std::uint32_t sender_id, int r, int g, int b, const std::string& text) {
  if (!g_server) {
    SPDLOG_WARN("Cannot send player message before the server is initialized");
    return false;
  }

  g_server->SendPlayerMessageToAll(sender_id, ClampColorComponent(r), ClampColorComponent(g), ClampColorComponent(b), text);
  return true;
}

/* luadoc (func)
*
* Send a player-sourced colored message to a specific player.
*
* @name     sendPlayerMessageToPlayer
* @side     server
* @category Chat
* @param    (int) sender_id    Sender player id.
* @param    (int) receiver_id  Receiver player id.
* @param    (int) r               Red component (0-255).
* @param    (int) g               Green component (0-255).
* @param    (int) b               Blue component (0-255).
* @param    (string) text         Message text.
* @return   (boolean)             True on success.
*
*/
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

/* luadoc (func)
*
* Spawn a player, optionally overriding the spawn position.
*
* @name     spawnPlayer
* @side     server
* @category Player
* @param    (int) player_id    Player id to spawn.
* @param    ({x, y, z})   Optional position table or three numeric coords.
* @return   (boolean)             True on success.
*
*/
bool Function_SpawnPlayer(std::uint32_t player_id, sol::variadic_args args) {
  if (!g_server) {
    SPDLOG_WARN("Cannot spawn player before the server is initialized");
    return false;
  }

  auto position_override = ParseSpawnPosition(args);
  return g_server->SpawnPlayer(player_id, position_override);
}

/* luadoc (func)
*
* Set a player's instance name.
*
* @name     setPlayerInstance
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @param    (string) instance  Instance name.
* @return   (boolean)           True on success.
*
*/
bool Function_SetPlayerInstance(std::uint32_t player_id, const std::string& instance) {
  if (!g_server) {
    SPDLOG_WARN("Cannot set player instance before the server is initialized");
    return false;
  }

  return g_server->SetPlayerInstance(player_id, ClampLuaText(instance, 255));
}

/* luadoc (func)
*
* Get a player's instance name or nil if unavailable.
*
* @name     getPlayerInstance
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @return   (string|nil)        Instance name or nil.
*
*/
sol::object Function_GetPlayerInstance(std::uint32_t player_id, sol::this_state ts) {
  if (!g_server) {
    SPDLOG_WARN("Cannot get player instance before the server is initialized");
    return sol::nil;
  }

  auto player_opt = g_server->GetPlayerManager().GetPlayer(player_id);
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, player_opt->get().instance);
}

/* luadoc (func)
*
* Set a player's display name.
*
* @name     setPlayerName
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @param    (string) name       New player name.
* @return   (boolean)           True on success.
*
*/
bool Function_SetPlayerName(std::uint32_t player_id, const std::string& name) {
  if (!g_server) {
    SPDLOG_WARN("Cannot set player name before the server is initialized");
    return false;
  }

  return g_server->SetPlayerName(player_id, name);
}

/* luadoc (func)
*
* Get a player's name or nil if unavailable.
*
* @name     getPlayerName
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @return   (string|nil)        Player name or nil.
*
*/
sol::object Function_GetPlayerName(std::uint32_t player_id, sol::this_state ts) {
  if (!g_server) {
    SPDLOG_WARN("Cannot get player name before the server is initialized");
    return sol::nil;
  }

  auto player_opt = g_server->GetPlayerManager().GetPlayer(player_id);
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, player_opt->get().name);
}

/* luadoc (func)
*
* Set a player's name color (RGB 0-255).
*
* @name     setPlayerColor
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @param    (int) r          Red (0-255).
* @param    (int) g          Green (0-255).
* @param    (int) b          Blue (0-255).
* @return   (boolean)           True on success.
*
*/
bool Function_SetPlayerColor(std::uint32_t player_id, int r, int g, int b) {
  if (!g_server) {
    SPDLOG_WARN("Cannot set player color before the server is initialized");
    return false;
  }

  return g_server->SetPlayerColor(player_id, ClampColorComponent(r), ClampColorComponent(g), ClampColorComponent(b));
}

/* luadoc (func)
*
* Get a player's name color as a table {r, g, b} or nil if unavailable.
*
* @name     getPlayerColor
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @return   ({r, g, b}|nil)        RGB color or nil.
*
*/
sol::object Function_GetPlayerColor(std::uint32_t player_id, sol::this_state ts) {
  if (!g_server) {
    SPDLOG_WARN("Cannot get player color before the server is initialized");
    return sol::nil;
  }

  auto player_opt = g_server->GetPlayerManager().GetPlayer(player_id);
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  sol::table color_table = lua.create_table();
  const auto& player = player_opt->get();
  color_table["r"] = static_cast<int>(player.name_color_r);
  color_table["g"] = static_cast<int>(player.name_color_g);
  color_table["b"] = static_cast<int>(player.name_color_b);
  return color_table;
}

/* luadoc (func)
*
* Set a player's current health.
*
* @name     setPlayerHealth
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @param    (int) health        New health value.
* @return   (boolean)           True on success.
*
*/
bool Function_SetPlayerHealth(std::uint32_t player_id, int health) {
  if (!g_server) {
    SPDLOG_WARN("Cannot set player health before the server is initialized");
    return false;
  }

  return g_server->SetPlayerHealth(player_id, health);
}

/* luadoc (func)
*
* Get a player's current health or nil if unavailable.
*
* @name     getPlayerHealth
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @return   (int|nil)        Health value or nil.
*
*/
sol::object Function_GetPlayerHealth(std::uint32_t player_id, sol::this_state ts) {
  if (!g_server) {
    SPDLOG_WARN("Cannot get player health before the server is initialized");
    return sol::nil;
  }

  auto player_opt = g_server->GetPlayerManager().GetPlayer(player_id);
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, player_opt->get().health);
}

/* luadoc (func)
*
* Set a player's maximum health.
*
* @name     setPlayerMaxHealth
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @param    (int) max_health    New maximum health.
* @return   (boolean)           True on success.
*
*/
bool Function_SetPlayerMaxHealth(std::uint32_t player_id, int max_health) {
  if (!g_server) {
    SPDLOG_WARN("Cannot set player max health before the server is initialized");
    return false;
  }

  return g_server->SetPlayerMaxHealth(player_id, max_health);
}

/* luadoc (func)
*
* Get a player's maximum health or nil if unavailable.
*
* @name     getPlayerMaxHealth
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @return   (int|nil)        Max health or nil.
*
*/
sol::object Function_GetPlayerMaxHealth(std::uint32_t player_id, sol::this_state ts) {
  if (!g_server) {
    SPDLOG_WARN("Cannot get player max health before the server is initialized");
    return sol::nil;
  }

  auto player_opt = g_server->GetPlayerManager().GetPlayer(player_id);
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, player_opt->get().max_health);
}

/* luadoc (func)
*
* Set a player's current mana.
*
* @name     setPlayerMana
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @param    (int) mana          New mana value.
* @return   (boolean)           True on success.
*
*/
bool Function_SetPlayerMana(std::uint32_t player_id, int mana) {
  if (!g_server) {
    SPDLOG_WARN("Cannot set player mana before the server is initialized");
    return false;
  }

  return g_server->SetPlayerMana(player_id, mana);
}

/* luadoc (func)
*
* Get a player's current mana or nil if unavailable.
*
* @name     getPlayerMana
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @return   (int|nil)        Mana value or nil.
*
*/
sol::object Function_GetPlayerMana(std::uint32_t player_id, sol::this_state ts) {
  if (!g_server) {
    SPDLOG_WARN("Cannot get player mana before the server is initialized");
    return sol::nil;
  }

  auto player_opt = g_server->GetPlayerManager().GetPlayer(player_id);
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, player_opt->get().mana);
}

/* luadoc (func)
*
* Set a player's maximum mana.
*
* @name     setPlayerMaxMana
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @param    (int) max_mana      New maximum mana.
* @return   (boolean)           True on success.
*
*/
bool Function_SetPlayerMaxMana(std::uint32_t player_id, int max_mana) {
  if (!g_server) {
    SPDLOG_WARN("Cannot set player max mana before the server is initialized");
    return false;
  }

  return g_server->SetPlayerMaxMana(player_id, max_mana);
}

/* luadoc (func)
*
* Get a player's maximum mana or nil if unavailable.
*
* @name     getPlayerMaxMana
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @return   (int|nil)        Max mana or nil.
*
*/
sol::object Function_GetPlayerMaxMana(std::uint32_t player_id, sol::this_state ts) {
  if (!g_server) {
    SPDLOG_WARN("Cannot get player max mana before the server is initialized");
    return sol::nil;
  }

  auto player_opt = g_server->GetPlayerManager().GetPlayer(player_id);
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, player_opt->get().max_mana);
}

/* luadoc (func)
*
* Set a player's strength attribute.
*
* @name     setPlayerStrength
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @param    (int) strength      New strength value.
* @return   (boolean)           True on success.
*
*/
bool Function_SetPlayerStrength(std::uint32_t player_id, int strength) {
  if (!g_server) {
    SPDLOG_WARN("Cannot set player strength before the server is initialized");
    return false;
  }

  return g_server->SetPlayerStrength(player_id, strength);
}

/* luadoc (func)
*
* Get a player's strength attribute or nil if unavailable.
*
* @name     getPlayerStrength
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @return   (int|nil)        Strength value or nil.
*
*/
sol::object Function_GetPlayerStrength(std::uint32_t player_id, sol::this_state ts) {
  if (!g_server) {
    SPDLOG_WARN("Cannot get player strength before the server is initialized");
    return sol::nil;
  }

  auto player_opt = g_server->GetPlayerManager().GetPlayer(player_id);
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, player_opt->get().strength);
}

/* luadoc (func)
*
* Set a player's dexterity attribute.
*
* @name     setPlayerDexterity
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @param    (int) dexterity     New dexterity value.
* @return   (boolean)           True on success.
*
*/
bool Function_SetPlayerDexterity(std::uint32_t player_id, int dexterity) {
  if (!g_server) {
    SPDLOG_WARN("Cannot set player dexterity before the server is initialized");
    return false;
  }

  return g_server->SetPlayerDexterity(player_id, dexterity);
}

/* luadoc (func)
*
* Get a player's dexterity attribute or nil if unavailable.
*
* @name     getPlayerDexterity
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @return   (int|nil)        Dexterity value or nil.
*
*/
sol::object Function_GetPlayerDexterity(std::uint32_t player_id, sol::this_state ts) {
  if (!g_server) {
    SPDLOG_WARN("Cannot get player dexterity before the server is initialized");
    return sol::nil;
  }

  auto player_opt = g_server->GetPlayerManager().GetPlayer(player_id);
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, player_opt->get().dexterity);
}

/* luadoc (func)
*
* Set a player's weapon skill hit chance (0-100).
*
* @name     setPlayerSkillWeapon
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @param    (int) skill_id   Skill identifier.
* @param    (int) percentage Hit chance (0-100).
* @return   (boolean)           True on success.
*
*/
bool Function_SetPlayerSkillWeapon(std::uint32_t player_id, int skill_id, int percentage) {
  if (!g_server) {
    SPDLOG_WARN("Cannot set player weapon skill before the server is initialized");
    return false;
  }

  return g_server->SetPlayerSkillWeapon(player_id, skill_id, percentage);
}

/* luadoc (func)
*
* Get a player's weapon skill hit chance.
*
* @name     getPlayerSkillWeapon
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @param    (int) skill_id   Skill identifier.
* @return   (int|nil)        Hit chance (0-100) or nil.
*
*/
sol::object Function_GetPlayerSkillWeapon(std::uint32_t player_id, int skill_id, sol::this_state ts) {
  if (!g_server) {
    SPDLOG_WARN("Cannot get player weapon skill before the server is initialized");
    return sol::nil;
  }

  auto player_opt = g_server->GetPlayerManager().GetPlayer(player_id);
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  auto& skills = player_opt->get().weapon_skills;
  const auto it = skills.find(skill_id);
  const int value = (it != skills.end()) ? it->second : 0;
  sol::state_view lua(ts);
  return sol::make_object(lua, value);
}

/* luadoc (func)
*
* Set a player's talent value.
*
* @name     setPlayerTalent
* @side     server
* @category Player
* @param    (int) player_id    Target player id.
* @param    (int) talent_id    Talent identifier.
* @param    (int) talent_value Talent value.
* @return   (boolean)           True on success.
*
*/
bool Function_SetPlayerTalent(std::uint32_t player_id, int talent_id, int talent_value) {
  if (!g_server) {
    SPDLOG_WARN("Cannot set player talent before the server is initialized");
    return false;
  }

  return g_server->SetPlayerTalent(player_id, talent_id, talent_value);
}

/* luadoc (func)
*
* Get a player's talent value.
*
* @name     getPlayerTalent
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @param    (int) talent_id  Talent identifier.
* @return   (int|nil)        Talent value or nil.
*
*/
sol::object Function_GetPlayerTalent(std::uint32_t player_id, int talent_id, sol::this_state ts) {
  if (!g_server) {
    SPDLOG_WARN("Cannot get player talent before the server is initialized");
    return sol::nil;
  }

  auto player_opt = g_server->GetPlayerManager().GetPlayer(player_id);
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  auto& talents = player_opt->get().talents;
  const auto it = talents.find(talent_id);
  const int value = (it != talents.end()) ? it->second : 0;
  sol::state_view lua(ts);
  return sol::make_object(lua, value);
}

/* luadoc (func)
*
* Set a player's experience level.
*
* @name     setPlayerLevel
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @param    (int) level         New level.
* @return   (boolean)           True on success.
*
*/
bool Function_SetPlayerLevel(std::uint32_t player_id, int level) {
  if (!g_server) {
    SPDLOG_WARN("Cannot set player level before the server is initialized");
    return false;
  }

  return g_server->SetPlayerLevel(player_id, level);
}

/* luadoc (func)
*
* Get a player's level or nil if unavailable.
*
* @name     getPlayerLevel
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @return   (int|nil)        Level or nil.
*
*/
sol::object Function_GetPlayerLevel(std::uint32_t player_id, sol::this_state ts) {
  if (!g_server) {
    SPDLOG_WARN("Cannot get player level before the server is initialized");
    return sol::nil;
  }

  auto player_opt = g_server->GetPlayerManager().GetPlayer(player_id);
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, player_opt->get().level);
}

/* luadoc (func)
*
* Set a player's experience points.
*
* @name     setPlayerExp
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @param    (int) exp           New exp value.
* @return   (boolean)           True on success.
*
*/
bool Function_SetPlayerExp(std::uint32_t player_id, int exp) {
  if (!g_server) {
    SPDLOG_WARN("Cannot set player exp before the server is initialized");
    return false;
  }

  return g_server->SetPlayerExp(player_id, exp);
}

/* luadoc (func)
*
* Get a player's experience points or nil if unavailable.
*
* @name     getPlayerExp
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @return   (int|nil)        Exp value or nil.
*
*/
sol::object Function_GetPlayerExp(std::uint32_t player_id, sol::this_state ts) {
  if (!g_server) {
    SPDLOG_WARN("Cannot get player exp before the server is initialized");
    return sol::nil;
  }

  auto player_opt = g_server->GetPlayerManager().GetPlayer(player_id);
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, player_opt->get().exp);
}

/* luadoc (func)
*
* Set the experience required for a player's next level.
*
* @name     setPlayerNextLevelExp
* @side     server
* @category Player
* @param    (int) player_id      Target player id.
* @param    (int) next_level_exp    Required exp for next level.
* @return   (boolean)               True on success.
*
*/
bool Function_SetPlayerNextLevelExp(std::uint32_t player_id, int next_level_exp) {
  if (!g_server) {
    SPDLOG_WARN("Cannot set player next level exp before the server is initialized");
    return false;
  }

  return g_server->SetPlayerNextLevelExp(player_id, next_level_exp);
}

/* luadoc (func)
*
* Get the experience required for a player's next level or nil if unavailable.
*
* @name     getPlayerNextLevelExp
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @return   (int|nil)        Next level exp or nil.
*
*/
sol::object Function_GetPlayerNextLevelExp(std::uint32_t player_id, sol::this_state ts) {
  if (!g_server) {
    SPDLOG_WARN("Cannot get player next level exp before the server is initialized");
    return sol::nil;
  }

  auto player_opt = g_server->GetPlayerManager().GetPlayer(player_id);
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, player_opt->get().next_level_exp);
}

/* luadoc (func)
*
* Set a player's learn points.
*
* @name     setPlayerLearnPoints
* @side     server
* @category Player
* @param    (int) player_id   Target player id.
* @param    (int) learn_points   New learn points value.
* @return   (boolean)            True on success.
*
*/
bool Function_SetPlayerLearnPoints(std::uint32_t player_id, int learn_points) {
  if (!g_server) {
    SPDLOG_WARN("Cannot set player learn points before the server is initialized");
    return false;
  }

  return g_server->SetPlayerLearnPoints(player_id, learn_points);
}

/* luadoc (func)
*
* Get a player's learn points or nil if unavailable.
*
* @name     getPlayerLearnPoints
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @return   (int|nil)        Learn points or nil.
*
*/
sol::object Function_GetPlayerLearnPoints(std::uint32_t player_id, sol::this_state ts) {
  if (!g_server) {
    SPDLOG_WARN("Cannot get player learn points before the server is initialized");
    return sol::nil;
  }

  auto player_opt = g_server->GetPlayerManager().GetPlayer(player_id);
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, player_opt->get().learn_points);
}

/* luadoc (func)
*
* Set a player's visual model and textures.
*
* @name     setPlayerVisual
* @side     server
* @category Player
* @param    (int) player_id    Target player id.
* @param    (string) body_model   Body model name.
* @param    (int) body_texture    Body texture index.
* @param    (string) head_model   Head model name.
* @param    (int) head_texture    Head texture index.
* @return   (boolean)             True on success.
*
*/
bool Function_SetPlayerVisual(std::uint32_t player_id, const std::string& body_model, int body_texture, const std::string& head_model,
                              int head_texture) {
  if (!g_server) {
    SPDLOG_WARN("Cannot set player visual before the server is initialized");
    return false;
  }

  return g_server->SetPlayerVisual(player_id, body_model, static_cast<std::int16_t>(body_texture), head_model,
                                   static_cast<std::int16_t>(head_texture));
}

/* luadoc (func)
*
* Get a player's visual information as a table or nil if unavailable.
*
* @name     getPlayerVisual
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @return   ({bodyModel, bodyTexture, headModel, headTexture}|nil)         Table with bodyModel, bodyTexture, headModel, headTexture or nil.
*
*/
sol::object Function_GetPlayerVisual(std::uint32_t player_id, sol::this_state ts) {
  if (!g_server) {
    SPDLOG_WARN("Cannot get player visual before the server is initialized");
    return sol::nil;
  }

  auto player_opt = g_server->GetPlayerManager().GetPlayer(player_id);
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  sol::table visual_table = lua.create_table();
  const auto& player = player_opt->get();
  visual_table["bodyModel"] = player.body_model;
  visual_table["bodyTexture"] = player.body_texture;
  visual_table["headModel"] = player.head_model;
  visual_table["headTexture"] = player.head_texture;
  return visual_table;
}

/* luadoc (func)
*
* Set a player's model fatness.
*
* @name     setPlayerFatness
* @side     server
* @category Player
* @param    (int) player_id   Target player id.
* @param    (float) fatness   Fatness value.
* @return   (boolean)           True on success.
*
*/
bool Function_SetPlayerFatness(std::uint32_t player_id, float fatness) {
  if (!g_server) {
    SPDLOG_WARN("Cannot set player fatness before the server is initialized");
    return false;
  }

  return g_server->SetPlayerFatness(player_id, fatness);
}

/* luadoc (func)
*
* Get a player's model fatness.
*
* @name     getPlayerFatness
* @side     server
* @category Player
* @param    (int) player_id   Target player id.
* @return   (float|nil)       Fatness value or nil.
*
*/
sol::object Function_GetPlayerFatness(std::uint32_t player_id, sol::this_state ts) {
  if (!g_server) {
    SPDLOG_WARN("Cannot get player fatness before the server is initialized");
    return sol::nil;
  }

  auto player_opt = g_server->GetPlayerManager().GetPlayer(player_id);
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, player_opt->get().fatness);
}

/* luadoc (func)
*
* Set a player's model scale.
*
* @name     setPlayerScale
* @side     server
* @category Player
* @param    (int) player_id   Target player id.
* @param    (float) x         Scale factor on x axis.
* @param    (float) y         Scale factor on y axis.
* @param    (float) z         Scale factor on z axis.
* @return   (boolean)           True on success.
*
*/
bool Function_SetPlayerScale(std::uint32_t player_id, float x, float y, float z) {
  if (!g_server) {
    SPDLOG_WARN("Cannot set player scale before the server is initialized");
    return false;
  }

  return g_server->SetPlayerScale(player_id, glm::vec3{x, y, z});
}

/* luadoc (func)
*
* Get a player's model scale.
*
* @name     getPlayerScale
* @side     server
* @category Player
* @param    (int) player_id   Target player id.
* @return   ({x, y, z}|nil)   Scale table or nil.
*
*/
sol::object Function_GetPlayerScale(std::uint32_t player_id, sol::this_state ts) {
  if (!g_server) {
    SPDLOG_WARN("Cannot get player scale before the server is initialized");
    return sol::nil;
  }

  auto player_opt = g_server->GetPlayerManager().GetPlayer(player_id);
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  const auto& scale = player_opt->get().scale;
  sol::state_view lua(ts);
  sol::table scale_table = lua.create_table();
  scale_table["x"] = scale.x;
  scale_table["y"] = scale.y;
  scale_table["z"] = scale.z;
  return scale_table;
}

/* luadoc (func)
*
* Apply animation overlay on a player.
*
* @name     applyPlayerOverlay
* @side     server
* @category Player
* @param    (int) player_id    Target player id.
* @param    (string) overlay   Overlay name.
* @return   (boolean)           True on success.
*
*/
bool Function_ApplyPlayerOverlay(std::uint32_t player_id, const std::string& overlay) {
  if (!g_server) {
    SPDLOG_WARN("Cannot apply player overlay before the server is initialized");
    return false;
  }

  return g_server->ApplyPlayerOverlay(player_id, ClampLuaText(overlay, 255));
}

/* luadoc (func)
*
* Get a player's active animation overlays.
*
* @name     getPlayerOverlays
* @side     server
* @category Player
* @param    (int) player_id
* @return   ({...}|nil)         Array of overlay names or nil.
*
*/
sol::object Function_GetPlayerOverlays(std::uint32_t player_id, sol::this_state ts) {
  if (!g_server) {
    SPDLOG_WARN("Cannot get player overlays before the server is initialized");
    return sol::nil;
  }

  auto player_opt = g_server->GetPlayerManager().GetPlayer(player_id);
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  const auto& overlays = player_opt->get().overlays;
  if (overlays.empty()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  sol::table overlay_table = lua.create_table(overlays.size(), 0);
  for (std::size_t i = 0; i < overlays.size(); ++i) {
    overlay_table[i + 1] = overlays[i];
  }
  return overlay_table;
}

/* luadoc (func)
*
* Remove animation overlay on a player.
*
* @name     removePlayerOverlay
* @side     server
* @category Player
* @param    (int) player_id    Target player id.
* @param    (string) overlay   Overlay name.
* @return   (boolean)           True on success.
*
*/
bool Function_RemovePlayerOverlay(std::uint32_t player_id, const std::string& overlay) {
  if (!g_server) {
    SPDLOG_WARN("Cannot remove player overlay before the server is initialized");
    return false;
  }

  return g_server->RemovePlayerOverlay(player_id, ClampLuaText(overlay, 255));
}

/* luadoc (func)
*
* Set a player's world position.
*
* @name     setPlayerPosition
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @param    (int) x          X coordinate.
* @param    (int) y          Y coordinate.
* @param    (int) z          Z coordinate.
* @return   (boolean)           True on success.
*
*/
bool Function_SetPlayerPosition(std::uint32_t player_id, float x, float y, float z) {
  if (!g_server) {
    SPDLOG_WARN("Cannot set player position before the server is initialized");
    return false;
  }

  return g_server->SetPlayerPosition(player_id, glm::vec3{x, y, z});
}

/* luadoc (func)
*
* Get a player's position as a table {x,y,z} or nil if unavailable.
*
* @name     getPlayerPosition
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @return   ({x, y, z}|nil)         Table containing x,y,z or nil.
*
*/
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

/* luadoc (func)
*
* Set a player's facing angle (radians).
*
* @name     setPlayerAngle
* @side     server
* @category Player
* @param    (int) player_id      Target player id.
* @param    (int) angle_radians  Angle in radians.
* @return   (boolean)               True on success.
*
*/
bool Function_SetPlayerAngle(std::uint32_t player_id, float angle_radians) {
  if (!g_server) {
    SPDLOG_WARN("Cannot set player angle before the server is initialized");
    return false;
  }

  // Angle is expressed in radians to match client-side behavior.
  return g_server->SetPlayerAngle(player_id, angle_radians);
}

/* luadoc (func)
*
* Get a player's facing angle in radians or nil if unavailable.
*
* @name     getPlayerAngle
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @return   (int|nil)        Angle in radians or nil.
*
*/
sol::object Function_GetPlayerAngle(std::uint32_t player_id, sol::this_state ts) {
  if (!g_server) {
    SPDLOG_WARN("Cannot get player angle before the server is initialized");
    return sol::nil;
  }

  auto player_opt = g_server->GetPlayerManager().GetPlayer(player_id);
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  const auto& nrot = player_opt->get().state.nrot;
  const float angle_radians = std::atan2(-nrot.z, nrot.x);
  sol::state_view lua(ts);
  return sol::make_object(lua, angle_radians);
}

/* luadoc (func)
*
* Move a player to a different world, optionally specifying a start point.
*
* @name     setPlayerWorld
* @side     server
* @category Player
* @param    (int) player_id     Target player id.
* @param    (string) world         World name.
* @param    (string) start_point  Optional start point name.
* @return   (boolean)              True on success.
*
*/
bool Function_SetPlayerWorld(std::uint32_t player_id, const std::string& world, std::optional<std::string> start_point) {
  if (!g_server) {
    SPDLOG_WARN("Cannot set player world before the server is initialized");
    return false;
  }

  return g_server->SetPlayerWorld(player_id, world, start_point);
}

/* luadoc (func)
*
* Get the current world name for a player or nil if player missing.
*
* @name     getPlayerWorld
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @return   (string|nil)        World name or nil.
*
*/
sol::object Function_GetPlayerWorld(std::uint32_t player_id, sol::this_state ts) {
  if (!g_server) {
    SPDLOG_WARN("Cannot get player world before the server is initialized");
    return sol::nil;
  }

  auto player_opt = g_server->GetPlayerManager().GetPlayer(player_id);
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, player_opt->get().world);
}

/* luadoc (func)
*
* Set a player's virtual world id.
*
* @name     setPlayerVirtualWorld
* @side     server
* @category Player
* @param    (int) player_id      Target player id.
* @param    (int) virtual_world  Virtual world id (0-65535).
* @return   (boolean)            True on success.
*
*/
bool Function_SetPlayerVirtualWorld(std::uint32_t player_id, int virtual_world) {
  if (!g_server) {
    SPDLOG_WARN("Cannot set player virtual world before the server is initialized");
    return false;
  }

  return g_server->SetPlayerVirtualWorld(player_id, virtual_world);
}

/* luadoc (func)
*
* Get a player's virtual world id or nil if unavailable.
*
* @name     getPlayerVirtualWorld
* @side     server
* @category Player
* @param    (int) player_id  Target player id.
* @return   (int|nil)        Virtual world id or nil.
*
*/
sol::object Function_GetPlayerVirtualWorld(std::uint32_t player_id, sol::this_state ts) {
  if (!g_server) {
    SPDLOG_WARN("Cannot get player virtual world before the server is initialized");
    return sol::nil;
  }

  auto player_opt = g_server->GetPlayerManager().GetPlayer(player_id);
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, player_opt->get().virtual_world);
}

/* luadoc (func)
*
* Give an item to a player or NPC.
*
* @name     giveItem
* @side     server
* @category Inventory
* @param    (int) player_id     Target player id.
* @param    (string) instance   Item instance name from scripts.
* @param    (int) amount        Amount to give.
* @return   (boolean)           True on success.
*
*/
bool Function_GiveItem(std::uint32_t player_id, const std::string& instance, std::int32_t amount) {
  if (!g_server) {
    SPDLOG_WARN("Cannot give item before the server is initialized");
    return false;
  }

  return g_server->GiveItem(player_id, ClampLuaText(instance, 255), amount);
}

/* luadoc (func)
*
* Equip an item for all players.
*
* @name     equipItem
* @side     server
* @category Inventory
* @param    (int) player_id     Target player id.
* @param    (string) instance   Item instance name from scripts.
* @param    (int) slot_id       Optional slot id. Defaults to -1 for first free slot.
* @return   (boolean)           True on success.
*
*/
bool Function_EquipItem(std::uint32_t player_id, const std::string& instance, sol::optional<std::int32_t> slot_id) {
  if (!g_server) {
    SPDLOG_WARN("Cannot equip item before the server is initialized");
    return false;
  }

  return g_server->EquipItem(player_id, ClampLuaText(instance, 255), slot_id.value_or(-1));
}

/* luadoc (func)
*
* Unequip an item for all players.
*
* @name     unequipItem
* @side     server
* @category Inventory
* @param    (int) player_id     Target player id.
* @param    (string) instance   Item instance name from scripts.
* @return   (boolean)           True on success.
*
*/
bool Function_UnequipItem(std::uint32_t player_id, const std::string& instance) {
  if (!g_server) {
    SPDLOG_WARN("Cannot unequip item before the server is initialized");
    return false;
  }

  return g_server->UnequipItem(player_id, ClampLuaText(instance, 255));
}

/* luadoc (func)
*
* Set the server's current world name.
*
* @name     setServerWorld
* @side     server
* @category Game
* @param    (string) world   World name to set.
*
*/
void Function_SetServerWorld(const std::string& world) {
  if (!g_server) {
    SPDLOG_WARN("Cannot set server world before the server is initialized");
    return;
  }

  g_server->SetServerWorld(world);
}

/* luadoc (func)
*
* Get the server's current world name.
*
* @name     getServerWorld
* @side     server
* @category Game
* @return   (string)         Current server world or empty string.
*
*/
std::string Function_GetServerWorld() {
  if (!g_server) {
    SPDLOG_WARN("Cannot get server world before the server is initialized");
    return std::string{};
  }

  return g_server->GetServerWorld();
}

/* luadoc (func)
*
* Find player ids within a radius of a given position in a world.
*
* @name     findNearbyPlayers
* @side     server
* @category Streamer
* @param    ({x, y, z}) position_table  Table with x,y,z coordinates.
* @param    (int) radius            Search radius.
* @param    (string) world          World name to search in.
* @param    (int) virtual_world    Optional virtual world id.
* @return   ({...})                Array of player ids.
*
*/
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

/* luadoc (func)
*
* Get the list of players that have been spawned for the given player.
*
* @name     getSpawnedPlayersForPlayer
* @side     server
* @category Streamer
* @param    (int) player_id  Target player id.
* @return   ({...})            Array of player ids.
*
*/
std::vector<std::uint32_t> Function_GetSpawnedPlayersForPlayer(std::uint32_t player_id) {
  if (!g_server) {
    SPDLOG_WARN("Cannot get spawned players before the server is initialized");
    return {};
  }

  return g_server->GetSpawnedPlayersForPlayer(player_id);
}

/* luadoc (func)
*
* Get the list of players currently streamed to the given player.
*
* @name     getStreamedPlayersByPlayer
* @side     server
* @category Streamer
* @param    (int) player_id  Target player id.
* @return   ({...})            Array of player ids.
*
*/
std::vector<std::uint32_t> Function_GetStreamedPlayersByPlayer(std::uint32_t player_id) {
  if (!g_server) {
    SPDLOG_WARN("Cannot get streamed players before the server is initialized");
    return {};
  }

  return g_server->GetStreamedPlayersByPlayer(player_id);
}

/* luadoc (func)
*
* Set the server time (hour, minute, optional day offset).
*
* @name     setTime
* @side     server
* @category Game
* @param    (int) hour      Hour (0-23).
* @param    (int) min       Minute (0-59).
* @param    (int) day      Optional day offset.
* @return   (boolean)       True on success.
*
*/
bool Function_SetTime(int hour, int min, sol::optional<int> day) {
  if (!g_server) {
    SPDLOG_WARN("Cannot set time before the server is initialized");
    return false;
  }

  return g_server->SetTime(hour, min, day.value_or(0));
}

/* luadoc (func)
*
* Get the current server time as a table {day,hour,min}.
*
* @name     getTime
* @side     server
* @category Game
* @return   ({day, hour, min})        Table containing day, hour, min.
*
*/
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

  lua["sendMessageToAll"] = Function_SendMessageToAll;
  lua["sendMessageToPlayer"] = Function_SendMessageToPlayer;
  lua["sendPlayerMessageToAll"] = Function_SendPlayerMessageToAll;
  lua["sendPlayerMessageToPlayer"] = Function_SendPlayerMessageToPlayer;

  lua["spawnPlayer"] = Function_SpawnPlayer;

  lua["setPlayerInstance"] = Function_SetPlayerInstance;
  lua["getPlayerInstance"] = Function_GetPlayerInstance;
  lua["setPlayerName"] = Function_SetPlayerName;
  lua["getPlayerName"] = Function_GetPlayerName;
  lua["setPlayerColor"] = Function_SetPlayerColor;
  lua["getPlayerColor"] = Function_GetPlayerColor;
  lua["setPlayerHealth"] = Function_SetPlayerHealth;
  lua["getPlayerHealth"] = Function_GetPlayerHealth;
  lua["setPlayerMaxHealth"] = Function_SetPlayerMaxHealth;
  lua["getPlayerMaxHealth"] = Function_GetPlayerMaxHealth;
  lua["setPlayerMana"] = Function_SetPlayerMana;
  lua["getPlayerMana"] = Function_GetPlayerMana;
  lua["setPlayerMaxMana"] = Function_SetPlayerMaxMana;
  lua["getPlayerMaxMana"] = Function_GetPlayerMaxMana;
  lua["setPlayerStrength"] = Function_SetPlayerStrength;
  lua["getPlayerStrength"] = Function_GetPlayerStrength;
  lua["setPlayerDexterity"] = Function_SetPlayerDexterity;
  lua["getPlayerDexterity"] = Function_GetPlayerDexterity;
  lua["setPlayerSkillWeapon"] = Function_SetPlayerSkillWeapon;
  lua["getPlayerSkillWeapon"] = Function_GetPlayerSkillWeapon;
  lua["setPlayerTalent"] = Function_SetPlayerTalent;
  lua["getPlayerTalent"] = Function_GetPlayerTalent;
  lua["setPlayerLevel"] = Function_SetPlayerLevel;
  lua["getPlayerLevel"] = Function_GetPlayerLevel;
  lua["setPlayerExp"] = Function_SetPlayerExp;
  lua["getPlayerExp"] = Function_GetPlayerExp;
  lua["setPlayerNextLevelExp"] = Function_SetPlayerNextLevelExp;
  lua["getPlayerNextLevelExp"] = Function_GetPlayerNextLevelExp;
  lua["setPlayerLearnPoints"] = Function_SetPlayerLearnPoints;
  lua["getPlayerLearnPoints"] = Function_GetPlayerLearnPoints;
  lua["setPlayerVisual"] = Function_SetPlayerVisual;
  lua["getPlayerVisual"] = Function_GetPlayerVisual;
  lua["setPlayerFatness"] = Function_SetPlayerFatness;
  lua["getPlayerFatness"] = Function_GetPlayerFatness;
  lua["setPlayerScale"] = Function_SetPlayerScale;
  lua["getPlayerScale"] = Function_GetPlayerScale;
  lua["applyPlayerOverlay"] = Function_ApplyPlayerOverlay;
  lua["getPlayerOverlays"] = Function_GetPlayerOverlays;
  lua["removePlayerOverlay"] = Function_RemovePlayerOverlay;
  lua["setPlayerPosition"] = Function_SetPlayerPosition;
  lua["getPlayerPosition"] = Function_GetPlayerPosition;
  lua["setPlayerAngle"] = Function_SetPlayerAngle;
  lua["getPlayerAngle"] = Function_GetPlayerAngle;
  lua["setPlayerWorld"] = Function_SetPlayerWorld;
  lua["getPlayerWorld"] = Function_GetPlayerWorld;
  lua["setPlayerVirtualWorld"] = Function_SetPlayerVirtualWorld;
  lua["getPlayerVirtualWorld"] = Function_GetPlayerVirtualWorld;

  lua["giveItem"] = Function_GiveItem;
  lua["equipItem"] = Function_EquipItem;
  lua["unequipItem"] = Function_UnequipItem;

  lua["setServerWorld"] = Function_SetServerWorld;
  lua["getServerWorld"] = Function_GetServerWorld;
  
  lua["findNearbyPlayers"] = Function_FindNearbyPlayers;
  lua["getSpawnedPlayersForPlayer"] = Function_GetSpawnedPlayersForPlayer;
  lua["getStreamedPlayersByPlayer"] = Function_GetStreamedPlayersByPlayer;

  lua["setTime"] = Function_SetTime;
  lua["getTime"] = Function_GetTime;
}
