
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

#include <cmath>
#include <glm/glm.hpp>
#include <optional>
#include <vector>

#include "Shared/lua_runtime/bind_helpers.h"
#include "Shared/lua_runtime/lua_constants.h"
#include "Shared/lua_runtime/shared_bind.h"
#include "Shared/lua_runtime/timer_manager.h"
#include "game_server.h"

namespace {

std::optional<glm::vec3> ParseSpawnPosition(sol::variadic_args args) {
  if (args.size() == 0) {
    return std::nullopt;
  }

  if (args.size() == 1) {
    sol::object arg = args[0];
    if (arg.get_type() == sol::type::table) {
      sol::table tbl = arg;
      auto x = ::lua::bind_helpers::GetOptionalFloat(tbl, "x", "X");
      auto y = ::lua::bind_helpers::GetOptionalFloat(tbl, "y", "Y");
      auto z = ::lua::bind_helpers::GetOptionalFloat(tbl, "z", "Z");
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

std::optional<glm::vec3> ParsePositionTable(const sol::table& table) {
  auto x = ::lua::bind_helpers::GetOptionalFloat(table, "x", "X");
  auto y = ::lua::bind_helpers::GetOptionalFloat(table, "y", "Y");
  auto z = ::lua::bind_helpers::GetOptionalFloat(table, "z", "Z");
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

std::optional<std::reference_wrapper<PlayerManager::Player>> GetPlayerOrWarn(std::uint32_t player_id, const char* action) {
  auto player_opt = g_server->GetPlayerManager().GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("{} called for missing player id {}", action, player_id);
  }
  return player_opt;
}

std::optional<std::reference_wrapper<Character>> GetCharacterOrWarn(std::uint32_t character_id, const char* action) {
  auto character = g_server->GetCharacter(character_id);
  if (!character) {
    SPDLOG_WARN("{} called for missing character id {}", action, character_id);
  }
  return character;
}

sol::object EquipmentInstanceOrNil(const std::string& instance, sol::state_view lua) {
  return instance.empty() ? sol::make_object(lua, sol::nil) : sol::make_object(lua, instance);
}

}  // namespace

/* luagmp (func)
 *
 * This function will send a colored chat message to all connected players.
 *
 * @version  0.3.0
 * @name     sendMessageToAll
 * @side     server
 * @category Chat
 * @param    (number) r        Red component (0-255).
 * @param    (number) g        Green component (0-255).
 * @param    (number) b        Blue component (0-255).
 * @param    (string) text     Message text to send.
 * @return   (boolean)         True on success.
 *
 */
bool Function_SendMessageToAll(int r, int g, int b, const std::string& text) {
  g_server->SendMessageToAll(::lua::bind_helpers::ClampByte(r), ::lua::bind_helpers::ClampByte(g), ::lua::bind_helpers::ClampByte(b), text);
  return true;
}

/* luagmp (func)
 *
 * This function will send a colored chat message to a specific player.
 *
 * @version  0.3.0
 * @name     sendMessageToPlayer
 * @side     server
 * @category Chat
 * @param    (number) player_id  Target player id; NPC ids are not supported.
 * @param    (number) r          Red component (0-255).
 * @param    (number) g          Green component (0-255).
 * @param    (number) b          Blue component (0-255).
 * @param    (string) text       Message text to send.
 * @return   (boolean)           True on success.
 *
 */
bool Function_SendMessageToPlayer(std::uint32_t player_id, int r, int g, int b, const std::string& text) {
  if (!GetPlayerOrWarn(player_id, "sendMessageToPlayer")) {
    return false;
  }

  g_server->SendMessageToPlayer(player_id, ::lua::bind_helpers::ClampByte(r), ::lua::bind_helpers::ClampByte(g), ::lua::bind_helpers::ClampByte(b),
                                text);
  return true;
}

/* luagmp (func)
 *
 * This function will send a player-sourced colored message to all players (includes sender id).
 *
 * @version  0.3.0
 * @name     sendPlayerMessageToAll
 * @side     server
 * @category Chat
 * @param    (number) sender_id  Sender player id.
 * @param    (number) r          Red component (0-255).
 * @param    (number) g          Green component (0-255).
 * @param    (number) b          Blue component (0-255).
 * @param    (string) text       Message text.
 * @return   (boolean)           True on success.
 *
 */
bool Function_SendPlayerMessageToAll(std::uint32_t sender_id, int r, int g, int b, const std::string& text) {
  if (!GetPlayerOrWarn(sender_id, "sendPlayerMessageToAll")) {
    return false;
  }

  g_server->SendPlayerMessageToAll(sender_id, ::lua::bind_helpers::ClampByte(r), ::lua::bind_helpers::ClampByte(g), ::lua::bind_helpers::ClampByte(b),
                                   text);
  return true;
}

/* luagmp (func)
 *
 * This function will send a player-sourced colored message to a specific player.
 *
 * @version  0.3.0
 * @name     sendPlayerMessageToPlayer
 * @side     server
 * @category Chat
 * @param    (number) sender_id    Sender player id.
 * @param    (number) receiver_id  Receiver player id.
 * @param    (number) r            Red component (0-255).
 * @param    (number) g            Green component (0-255).
 * @param    (number) b            Blue component (0-255).
 * @param    (string) text         Message text.
 * @return   (boolean)             True on success.
 *
 */
bool Function_SendPlayerMessageToPlayer(std::uint32_t sender_id, std::uint32_t receiver_id, int r, int g, int b, const std::string& text) {
  if (!GetPlayerOrWarn(sender_id, "sendPlayerMessageToPlayer (sender)")) {
    return false;
  }
  if (!GetPlayerOrWarn(receiver_id, "sendPlayerMessageToPlayer (receiver)")) {
    return false;
  }

  g_server->SendPlayerMessageToPlayer(sender_id, receiver_id, ::lua::bind_helpers::ClampByte(r), ::lua::bind_helpers::ClampByte(g),
                                      ::lua::bind_helpers::ClampByte(b), text);
  return true;
}

/* luagmp (func)
 *
 * This function will spawn the player, optionally overriding the spawn position.
 *
 * @version  0.3.0
 * @name     spawnPlayer
 * @side     server
 * @category Player
 * @note     If the player is not spawned, server doesn't recognize his presence at all.
 * @param    (number) player_id    Target player or NPC id.
 * @param    ({x, y, z}|nil) position  Optional position table or three numeric coords.
 * @return   (boolean)             True on success.
 *
 */
bool Function_SpawnPlayer(std::uint32_t player_id, sol::variadic_args args) {
  if (!GetCharacterOrWarn(player_id, "spawnPlayer")) {
    return false;
  }

  auto position_override = ParseSpawnPosition(args);
  if (args.size() != 0 && !position_override) {
    return false;
  }
  return g_server->SpawnCharacter(player_id, position_override);
}

/* luagmp (func)
 *
 * This function will unspawn the player without disconnecting him.
 *
 * @version  0.3.0
 * @name     unspawnPlayer
 * @side     server
 * @category Player
 * @param    (number) player_id    Target player or NPC id.
 * @return   (boolean)             True on success.
 *
 */
bool Function_UnspawnPlayer(std::uint32_t player_id) {
  if (!GetCharacterOrWarn(player_id, "unspawnPlayer")) {
    return false;
  }

  return g_server->UnspawnCharacter(player_id);
}

/* luagmp (func)
 *
 * This function will set the player/NPC instance.
 *
 * @version  0.3.0
 * @name     setPlayerInstance
 * @side     server
 * @category Player
 * @param    (number) player_id     Target player or NPC id.
 * @param    (string) instance      Instance name.
 * @return   (boolean)              True on success.
 *
 */
bool Function_SetPlayerInstance(std::uint32_t player_id, const std::string& instance) {
  if (!GetCharacterOrWarn(player_id, "setPlayerInstance")) {
    return false;
  }

  return g_server->SetCharacterInstance(player_id, ClampLuaText(instance, 255));
}

/* luagmp (func)
 *
 * This function will return the player/NPC's instance name, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerInstance
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @return   (string|nil)        Instance name or nil.
 *
 */
sol::object Function_GetPlayerInstance(std::uint32_t player_id, sol::this_state ts) {
  auto player_opt = GetCharacterOrWarn(player_id, "getPlayerInstance");
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, player_opt->get().instance);
}

/* luagmp (func)
 *
 * This function will set the player/NPC name.
 *
 * @version  0.3.0
 * @name     setPlayerName
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @param    (string) name       New player name.
 * @return   (boolean)           True on success.
 *
 */
bool Function_SetPlayerName(std::uint32_t player_id, const std::string& name) {
  if (!GetCharacterOrWarn(player_id, "setPlayerName")) {
    return false;
  }

  return g_server->SetCharacterName(player_id, name);
}

/* luagmp (func)
 *
 * This function will return the player/NPC's name, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerName
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @return   (string|nil)        Player name or nil.
 *
 */
sol::object Function_GetPlayerName(std::uint32_t player_id, sol::this_state ts) {
  auto player_opt = GetCharacterOrWarn(player_id, "getPlayerName");
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, player_opt->get().name);
}

/* luagmp (func)
 *
 * This function will return the player's IP address or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerIP
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player id; NPC ids are not supported.
 * @return   (string|nil)        Player IP or nil.
 *
 */
sol::object Function_GetPlayerIP(std::uint32_t player_id, sol::this_state ts) {
  if (!GetPlayerOrWarn(player_id, "getPlayerIP")) {
    return sol::nil;
  }

  const auto ip = g_server->GetPlayerIp(player_id);
  if (ip.empty()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, ip);
}

/* luagmp (func)
 *
 * This function will return the player's MAC address or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerMacAddress
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player id; NPC ids are not supported.
 * @return   (string|nil)        Player MAC address or nil.
 *
 */
sol::object Function_GetPlayerMacAddress(std::uint32_t player_id, sol::this_state ts) {
  if (!GetPlayerOrWarn(player_id, "getPlayerMacAddress")) {
    return sol::nil;
  }

  const auto mac = g_server->GetPlayerMacAddress(player_id);
  if (mac.empty()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, mac);
}

/* luagmp (func)
 *
 * Returns the player's stable OS installation identity, or nil if unavailable.
 * The 64-character lowercase hex value comes from the peer's GNS certificate.
 * It is self-asserted and can be spoofed by a modified client. OS reinstalls,
 * cloned installations, or changes to the OS identifier affect this identity.
 *
 * @version  0.3.0
 * @name     getPlayerIdentity
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player id; NPC ids are not supported.
 * @return   (string|nil)        Machine identity or nil.
 *
 */
sol::object Function_GetPlayerIdentity(std::uint32_t player_id, sol::this_state ts) {
  if (!GetPlayerOrWarn(player_id, "getPlayerIdentity")) {
    return sol::nil;
  }

  const auto identity = g_server->GetPlayerIdentity(player_id);
  if (identity.empty()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, identity);
}

/* luagmp (func)
 *
 * This function will return the player's current network ping.
 *
 * @version  0.3.0
 * @name     getPlayerPing
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player id; NPC ids are not supported.
 * @return   (number)            Average of all ping times read, or -1 if unavailable.
 *
 */
std::int32_t Function_GetPlayerPing(std::uint32_t player_id) {
  if (!GetPlayerOrWarn(player_id, "getPlayerPing")) {
    return -1;
  }

  return g_server->GetPlayerPing(player_id);
}

/* luagmp (func)
 *
 * This function will set the player/NPC's name color in RGB format.
 *
 * @version  0.3.0
 * @name     setPlayerNameColor
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @param    (number) r          Red (0-255).
 * @param    (number) g          Green (0-255).
 * @param    (number) b          Blue (0-255).
 * @return   (boolean)           True on success.
 *
 */
bool Function_SetPlayerNameColor(std::uint32_t player_id, int r, int g, int b) {
  if (!GetCharacterOrWarn(player_id, "setPlayerNameColor")) {
    return false;
  }

  return g_server->SetCharacterNameColor(player_id, ::lua::bind_helpers::ClampByte(r), ::lua::bind_helpers::ClampByte(g),
                                         ::lua::bind_helpers::ClampByte(b));
}

/* luagmp (func)
 *
 * This function will return the player/NPC's name color, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerNameColor
 * @side     server
 * @category Player
 * @param    (number) player_id   Target player or NPC id.
 * @return   ({r, g, b}|nil)      RGB color table or nil.
 *
 */
sol::object Function_GetPlayerNameColor(std::uint32_t player_id, sol::this_state ts) {
  auto player_opt = GetCharacterOrWarn(player_id, "getPlayerNameColor");
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

/* luagmp (func)
 *
 * This function will set the player/NPC's current health.
 *
 * @version  0.3.0
 * @name     setPlayerHealth
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @param    (number) health     New health value.
 * @return   (boolean)           True on success.
 *
 */
bool Function_SetPlayerHealth(std::uint32_t player_id, int health) {
  if (!GetCharacterOrWarn(player_id, "setPlayerHealth")) {
    return false;
  }

  return g_server->SetCharacterHealth(player_id, health);
}

/* luagmp (func)
 *
 * This function will return the player/NPC's current health, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerHealth
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @return   (number|nil)        Health value or nil.
 *
 */
sol::object Function_GetPlayerHealth(std::uint32_t player_id, sol::this_state ts) {
  auto player_opt = GetCharacterOrWarn(player_id, "getPlayerHealth");
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, player_opt->get().health);
}

/* luagmp (func)
 *
 * This function will set the player/NPC's maximum health.
 *
 * @version  0.3.0
 * @name     setPlayerMaxHealth
 * @side     server
 * @category Player
 * @param    (number) player_id   Target player or NPC id.
 * @param    (number) max_health  New maximum health.
 * @return   (boolean)            True on success.
 *
 */
bool Function_SetPlayerMaxHealth(std::uint32_t player_id, int max_health) {
  if (!GetCharacterOrWarn(player_id, "setPlayerMaxHealth")) {
    return false;
  }

  return g_server->SetCharacterMaxHealth(player_id, max_health);
}

/* luagmp (func)
 *
 * This function will return the player/NPC's maximum health, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerMaxHealth
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @return   (number|nil)        Max health or nil.
 *
 */
sol::object Function_GetPlayerMaxHealth(std::uint32_t player_id, sol::this_state ts) {
  auto player_opt = GetCharacterOrWarn(player_id, "getPlayerMaxHealth");
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, player_opt->get().max_health);
}

/* luagmp (func)
 *
 * This function will set the player/NPC's current mana.
 *
 * @version  0.3.0
 * @name     setPlayerMana
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @param    (number) mana       New mana value.
 * @return   (boolean)           True on success.
 *
 */
bool Function_SetPlayerMana(std::uint32_t player_id, int mana) {
  if (!GetCharacterOrWarn(player_id, "setPlayerMana")) {
    return false;
  }

  return g_server->SetCharacterMana(player_id, mana);
}

/* luagmp (func)
 *
 * This function will return the player/NPC's current mana, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerMana
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @return   (number|nil)        Mana value or nil.
 *
 */
sol::object Function_GetPlayerMana(std::uint32_t player_id, sol::this_state ts) {
  auto player_opt = GetCharacterOrWarn(player_id, "getPlayerMana");
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, player_opt->get().mana);
}

/* luagmp (func)
 *
 * This function will set the player/NPC's maximum mana.
 *
 * @version  0.3.0
 * @name     setPlayerMaxMana
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @param    (number) max_mana   New maximum mana.
 * @return   (boolean)           True on success.
 *
 */
bool Function_SetPlayerMaxMana(std::uint32_t player_id, int max_mana) {
  if (!GetCharacterOrWarn(player_id, "setPlayerMaxMana")) {
    return false;
  }

  return g_server->SetCharacterMaxMana(player_id, max_mana);
}

/* luagmp (func)
 *
 * This function will get the player/NPC's maximum mana, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerMaxMana
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @return   (number|nil)        Max mana or nil.
 *
 */
sol::object Function_GetPlayerMaxMana(std::uint32_t player_id, sol::this_state ts) {
  auto player_opt = GetCharacterOrWarn(player_id, "getPlayerMaxMana");
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, player_opt->get().max_mana);
}

/* luagmp (func)
 *
 * This function will set the player/NPC's strength attribute.
 *
 * @version  0.3.0
 * @name     setPlayerStrength
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @param    (number) strength   New strength value.
 * @return   (boolean)           True on success.
 *
 */
bool Function_SetPlayerStrength(std::uint32_t player_id, int strength) {
  if (!GetCharacterOrWarn(player_id, "setPlayerStrength")) {
    return false;
  }

  return g_server->SetCharacterStrength(player_id, strength);
}

/* luagmp (func)
 *
 * This function will return the player/NPC's strength attribute, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerStrength
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @return   (number|nil)        Strength value or nil.
 *
 */
sol::object Function_GetPlayerStrength(std::uint32_t player_id, sol::this_state ts) {
  auto player_opt = GetCharacterOrWarn(player_id, "getPlayerStrength");
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, player_opt->get().strength);
}

/* luagmp (func)
 *
 * This function will set the player/NPC's dexterity attribute.
 *
 * @version  0.3.0
 * @name     setPlayerDexterity
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @param    (number) dexterity  New dexterity value.
 * @return   (boolean)           True on success.
 *
 */
bool Function_SetPlayerDexterity(std::uint32_t player_id, int dexterity) {
  if (!GetCharacterOrWarn(player_id, "setPlayerDexterity")) {
    return false;
  }

  return g_server->SetCharacterDexterity(player_id, dexterity);
}

/* luagmp (func)
 *
 * This function will return the player/NPC's dexterity attribute, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerDexterity
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @return   (number|nil)        Dexterity value or nil.
 *
 */
sol::object Function_GetPlayerDexterity(std::uint32_t player_id, sol::this_state ts) {
  auto player_opt = GetCharacterOrWarn(player_id, "getPlayerDexterity");
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, player_opt->get().dexterity);
}

/* luagmp (func)
 *
 * This function will set the player/NPC's weapon skill hit chance.
 *
 * @version  0.3.0
 * @name     setPlayerSkillWeapon
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @param    (number) skill_id   Skill identifier, for more information check [Weapon Constants](../../shared-constants/Weapon.md).
 * @param    (number) percentage Hit chance amount.
 * @return   (boolean)           True on success.
 *
 */
bool Function_SetPlayerSkillWeapon(std::uint32_t player_id, int skill_id, int percentage) {
  if (!GetCharacterOrWarn(player_id, "setPlayerSkillWeapon")) {
    return false;
  }

  return g_server->SetCharacterSkillWeapon(player_id, skill_id, percentage);
}

/* luagmp (func)
 *
 * This function will return the player/NPC's weapon skill hit chance, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerSkillWeapon
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @param    (number) skill_id   Skill identifier, for more information check [Weapon Constants](../../shared-constants/Weapon.md).
 * @return   (number|nil)        Hit chance amount or nil.
 *
 */
sol::object Function_GetPlayerSkillWeapon(std::uint32_t player_id, int skill_id, sol::this_state ts) {
  auto player_opt = GetCharacterOrWarn(player_id, "getPlayerSkillWeapon");
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  auto& skills = player_opt->get().weapon_skills;
  const auto it = skills.find(skill_id);
  const int value = (it != skills.end()) ? it->second : 0;
  sol::state_view lua(ts);
  return sol::make_object(lua, value);
}

/* luagmp (func)
 *
 * This function will set the player/NPC's talent value.
 *
 * @version  0.3.0
 * @name     setPlayerTalent
 * @side     server
 * @category Player
 * @param    (number) player_id     Target player or NPC id.
 * @param    (number) talent_id     Talent identifier, for more information check [Talent Constants](../../shared-constants/Talent.md).
 * @param    (number) talent_value  Talent value.
 * @return   (boolean)              True on success.
 *
 */
bool Function_SetPlayerTalent(std::uint32_t player_id, int talent_id, int talent_value) {
  if (!GetCharacterOrWarn(player_id, "setPlayerTalent")) {
    return false;
  }

  return g_server->SetCharacterTalent(player_id, talent_id, talent_value);
}

/* luagmp (func)
 *
 * This function will return the player/NPC's talent value, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerTalent
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @param    (number) talent_id  Talent identifier, for more information check [Talent Constants](../../shared-constants/Talent.md).
 * @return   (number|nil)        Talent value or nil.
 *
 */
sol::object Function_GetPlayerTalent(std::uint32_t player_id, int talent_id, sol::this_state ts) {
  auto player_opt = GetCharacterOrWarn(player_id, "getPlayerTalent");
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  auto& talents = player_opt->get().talents;
  const auto it = talents.find(talent_id);
  const int value = (it != talents.end()) ? it->second : 0;
  sol::state_view lua(ts);
  return sol::make_object(lua, value);
}

/* luagmp (func)
 *
 * This function will set the player/NPC's level.
 *
 * @version  0.3.0
 * @name     setPlayerLevel
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @param    (number) level      New level.
 * @return   (boolean)           True on success.
 *
 */
bool Function_SetPlayerLevel(std::uint32_t player_id, int level) {
  if (!GetCharacterOrWarn(player_id, "setPlayerLevel")) {
    return false;
  }

  return g_server->SetCharacterLevel(player_id, level);
}

/* luagmp (func)
 *
 * This function will return the player/NPC's level, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerLevel
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @return   (number|nil)        Level or nil.
 *
 */
sol::object Function_GetPlayerLevel(std::uint32_t player_id, sol::this_state ts) {
  auto player_opt = GetCharacterOrWarn(player_id, "getPlayerLevel");
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, player_opt->get().level);
}

/* luagmp (func)
 *
 * This function will set the player/NPC's experience points.
 *
 * @version  0.3.0
 * @name     setPlayerExp
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @param    (number) exp        New exp value.
 * @return   (boolean)           True on success.
 *
 */
bool Function_SetPlayerExp(std::uint32_t player_id, int exp) {
  if (!GetCharacterOrWarn(player_id, "setPlayerExp")) {
    return false;
  }

  return g_server->SetCharacterExperience(player_id, exp);
}

/* luagmp (func)
 *
 * This function will return the player/NPC's experience points, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerExp
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @return   (number|nil)        Exp value or nil.
 *
 */
sol::object Function_GetPlayerExp(std::uint32_t player_id, sol::this_state ts) {
  auto player_opt = GetCharacterOrWarn(player_id, "getPlayerExp");
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, player_opt->get().exp);
}

/* luagmp (func)
 *
 * This function will set the experience required for the player/NPC's next level.
 *
 * @version  0.3.0
 * @name     setPlayerNextLevelExp
 * @side     server
 * @category Player
 * @param    (number) player_id      Target player or NPC id.
 * @param    (number) next_level_exp Required exp for next level.
 * @return   (boolean)               True on success.
 *
 */
bool Function_SetPlayerNextLevelExp(std::uint32_t player_id, int next_level_exp) {
  if (!GetCharacterOrWarn(player_id, "setPlayerNextLevelExp")) {
    return false;
  }

  return g_server->SetCharacterNextLevelExperience(player_id, next_level_exp);
}

/* luagmp (func)
 *
 * This function will return the experience required for the player/NPC's next level, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerNextLevelExp
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @return   (number|nil)        Next level exp or nil.
 *
 */
sol::object Function_GetPlayerNextLevelExp(std::uint32_t player_id, sol::this_state ts) {
  auto player_opt = GetCharacterOrWarn(player_id, "getPlayerNextLevelExp");
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, player_opt->get().next_level_exp);
}

/* luagmp (func)
 *
 * This function will set the player/NPC's learn points.
 *
 * @version  0.3.0
 * @name     setPlayerLearnPoints
 * @side     server
 * @category Player
 * @param    (number) player_id     Target player or NPC id.
 * @param    (number) learn_points  New learn points value.
 * @return   (boolean)              True on success.
 *
 */
bool Function_SetPlayerLearnPoints(std::uint32_t player_id, int learn_points) {
  if (!GetCharacterOrWarn(player_id, "setPlayerLearnPoints")) {
    return false;
  }

  return g_server->SetCharacterLearnPoints(player_id, learn_points);
}

/* luagmp (func)
 *
 * This function will return the player/NPC's learn points, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerLearnPoints
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @return   (number|nil)        Learn points or nil.
 *
 */
sol::object Function_GetPlayerLearnPoints(std::uint32_t player_id, sol::this_state ts) {
  auto player_opt = GetCharacterOrWarn(player_id, "getPlayerLearnPoints");
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, player_opt->get().learn_points);
}

/* luagmp (func)
 *
 * This function will set the player/NPC's visual model and textures.
 *
 * @version  0.3.0
 * @name     setPlayerVisual
 * @side     server
 * @category Player
 * @param    (number) player_id       Target player or NPC id.
 * @param    (string) bodyModel      Body model name.
 * @param    (number) bodyTexture    Body texture index.
 * @param    (string) headModel      Head model name.
 * @param    (number) headTexture    Head texture index.
 * @param    (number|nil) teethTexture   Optional teeth texture file numeric id. Defaults to 0 if omitted.
 * @param    (number|nil) skinColor      Optional color variant of head & body texture files. Defaults to 0 if omitted.
 * @return   (boolean)                    True on success.
 *
 */
bool Function_SetPlayerVisual(std::uint32_t player_id, const std::string& body_model, int body_texture, const std::string& head_model,
                              int head_texture, sol::optional<int> teeth_texture, sol::optional<int> skin_color) {
  if (!GetCharacterOrWarn(player_id, "setPlayerVisual")) {
    return false;
  }

  return g_server->SetCharacterVisual(player_id, body_model, static_cast<std::int16_t>(body_texture), head_model,
                                      static_cast<std::int16_t>(head_texture), static_cast<std::int16_t>(teeth_texture.value_or(0)),
                                      static_cast<std::int16_t>(skin_color.value_or(0)));
}

/* luagmp (func)
 *
 * This function will return the player/NPC's visual information as a table, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerVisual
 * @side     server
 * @category Player
 * @param    (number) player_id       Target player or NPC id.
 * @return   ({bodyModel, bodyTexture, headModel, headTexture, teethTexture, skinColor}|nil)  Table with visual data or nil.
 *
 */
sol::object Function_GetPlayerVisual(std::uint32_t player_id, sol::this_state ts) {
  auto player_opt = GetCharacterOrWarn(player_id, "getPlayerVisual");
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
  visual_table["teethTexture"] = player.teeth_texture;
  visual_table["skinColor"] = player.skin_color;
  return visual_table;
}

/* luagmp (func)
 *
 * This function will set the player/NPC's model fatness.
 *
 * @version  0.3.0
 * @name     setPlayerFatness
 * @side     server
 * @category Player
 * @param    (number) player_id   Target player or NPC id.
 * @param    (number) fatness     Fatness value.
 * @return   (boolean)            True on success.
 *
 */
bool Function_SetPlayerFatness(std::uint32_t player_id, float fatness) {
  if (!GetCharacterOrWarn(player_id, "setPlayerFatness")) {
    return false;
  }

  return g_server->SetCharacterFatness(player_id, fatness);
}

/* luagmp (func)
 *
 * This function will return the player/NPC's model fatness, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerFatness
 * @side     server
 * @category Player
 * @param    (number) player_id   Target player or NPC id.
 * @return   (float|nil)          Fatness value or nil.
 *
 */
sol::object Function_GetPlayerFatness(std::uint32_t player_id, sol::this_state ts) {
  auto player_opt = GetCharacterOrWarn(player_id, "getPlayerFatness");
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, player_opt->get().fatness);
}

/* luagmp (func)
 *
 * This function will set the player/NPC's model scale.
 *
 * @version  0.3.0
 * @name     setPlayerScale
 * @side     server
 * @category Player
 * @param    (number) player_id   Target player or NPC id.
 * @param    (number) x           Scale factor on x axis.
 * @param    (number) y           Scale factor on y axis.
 * @param    (number) z           Scale factor on z axis.
 * @return   (boolean)            True on success.
 *
 */
bool Function_SetPlayerScale(std::uint32_t player_id, float x, float y, float z) {
  if (!GetCharacterOrWarn(player_id, "setPlayerScale")) {
    return false;
  }

  return g_server->SetCharacterScale(player_id, glm::vec3{x, y, z});
}

/* luagmp (func)
 *
 * This function will return the player/NPC's model scale.
 *
 * @version  0.3.0
 * @name     getPlayerScale
 * @side     server
 * @category Player
 * @param    (number) player_id   Target player or NPC id.
 * @return   ({x, y, z}|nil)      Scale table or nil.
 *
 */
sol::object Function_GetPlayerScale(std::uint32_t player_id, sol::this_state ts) {
  auto player_opt = GetCharacterOrWarn(player_id, "getPlayerScale");
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  const auto& scale = player_opt->get().scale;
  sol::state_view lua(ts);
  return ::lua::bind_helpers::MakeVec3Table(lua, scale);
}

/* luagmp (func)
 *
 * This function will set the player/NPC's weapon mode.
 *
 * @version  0.3.0
 * @name     setPlayerWeaponMode
 * @side     server
 * @category Player
 * @param    (number) player_id   Target player or NPC id.
 * @param    (number) weapon_mode Weapon mode constant. For more information, see [Weapon Mode Constants](../../shared-constants/WeaponMode.md).
 * @return   (boolean)            True on success.
 *
 */
bool Function_SetPlayerWeaponMode(std::uint32_t player_id, int weapon_mode) {
  if (!GetCharacterOrWarn(player_id, "setPlayerWeaponMode")) {
    return false;
  }

  return g_server->SetCharacterWeaponMode(player_id, weapon_mode);
}

/* luagmp (func)
 *
 * This function will return the player/NPC's weapon mode, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerWeaponMode
 * @side     server
 * @category Player
 * @param    (number) player_id   Target player or NPC id.
 * @return   (number|nil)         Weapon mode from [Weapon Mode Constants](../../shared-constants/WeaponMode.md), or nil.
 *
 */
sol::object Function_GetPlayerWeaponMode(std::uint32_t player_id, sol::this_state ts) {
  auto player_opt = GetCharacterOrWarn(player_id, "getPlayerWeaponMode");
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, static_cast<int>(player_opt->get().state.weapon_mode));
}

/* luagmp (func)
 *
 * This function will return the last reported player/NPC pose animation name.
 *
 * @version  0.3.0
 * @name     getPlayerAni
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @return   (string|nil)        Animation name, or nil if unavailable.
 *
 */
sol::object Function_GetPlayerAni(std::uint32_t player_id, sol::this_state ts) {
  auto player_opt = GetCharacterOrWarn(player_id, "getPlayerAni");
  if (!player_opt.has_value() || player_opt->get().state.animation_name.empty()) {
    return sol::nil;
  }

  return sol::make_object(sol::state_view(ts), player_opt->get().state.animation_name);
}

/* luagmp (func)
 *
 * This function will return the player/NPC's equipped armor instance name.
 *
 * @version  0.3.0
 * @name     getPlayerArmor
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @return   (string|nil)        Equipped armor instance, or nil if no armor is equipped.
 *
 */
sol::object Function_GetPlayerArmor(std::uint32_t player_id, sol::this_state ts) {
  auto player_opt = GetCharacterOrWarn(player_id, "getPlayerArmor");
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return EquipmentInstanceOrNil(player_opt->get().state.equipped_armor_instance, lua);
}

/* luagmp (func)
 *
 * This function will return the player/NPC's equipped melee weapon instance name.
 *
 * @version  0.3.0
 * @name     getPlayerMeleeWeapon
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @return   (string|nil)        Equipped melee weapon instance, or nil if none is equipped.
 *
 */
sol::object Function_GetPlayerMeleeWeapon(std::uint32_t player_id, sol::this_state ts) {
  auto player_opt = GetCharacterOrWarn(player_id, "getPlayerMeleeWeapon");
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return EquipmentInstanceOrNil(player_opt->get().state.melee_weapon_instance, lua);
}

/* luagmp (func)
 *
 * This function will return the player/NPC's equipped ranged weapon instance name.
 *
 * @version  0.3.0
 * @name     getPlayerRangedWeapon
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @return   (string|nil)        Equipped ranged weapon instance, or nil if none is equipped.
 *
 */
sol::object Function_GetPlayerRangedWeapon(std::uint32_t player_id, sol::this_state ts) {
  auto player_opt = GetCharacterOrWarn(player_id, "getPlayerRangedWeapon");
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return EquipmentInstanceOrNil(player_opt->get().state.ranged_weapon_instance, lua);
}

/* luagmp (func)
 *
 * This function will return the player/NPC's equipped helmet instance name.
 *
 * @version  0.3.0
 * @name     getPlayerHelmet
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @return   (string|nil)        Equipped helmet instance, or nil if no helmet is equipped.
 *
 */
sol::object Function_GetPlayerHelmet(std::uint32_t player_id, sol::this_state ts) {
  auto player_opt = GetCharacterOrWarn(player_id, "getPlayerHelmet");
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return EquipmentInstanceOrNil(player_opt->get().state.equipped_helmet_instance, lua);
}

/* luagmp (func)
 *
 * This function will return the player/NPC's equipped shield instance name.
 *
 * @version  0.3.0
 * @name     getPlayerShield
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @return   (string|nil)        Equipped shield instance, or nil if no shield is equipped.
 *
 */
sol::object Function_GetPlayerShield(std::uint32_t player_id, sol::this_state ts) {
  auto player_opt = GetCharacterOrWarn(player_id, "getPlayerShield");
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return EquipmentInstanceOrNil(player_opt->get().state.equipped_shield_instance, lua);
}

/* luagmp (func)
 *
 * This function will apply animation overlay on the player.
 *
 * @version  0.3.0
 * @name     applyPlayerOverlay
 * @side     server
 * @category Player
 * @param    (number) player_id    Target player or NPC id.
 * @param    (string) overlay      Overlay name.
 * @return   (boolean)             True on success.
 *
 */
bool Function_ApplyPlayerOverlay(std::uint32_t player_id, const std::string& overlay) {
  if (!GetCharacterOrWarn(player_id, "applyPlayerOverlay")) {
    return false;
  }

  return g_server->ApplyCharacterOverlay(player_id, ClampLuaText(overlay, 255));
}

/* luagmp (func)
 *
 * This function will return the player/NPC's active animation overlays.
 *
 * @version  0.3.0
 * @name     getPlayerOverlays
 * @side     server
 * @category Player
 * @param    (number) player_id
 Target player or NPC id.
 *
 */
sol::object Function_GetPlayerOverlays(std::uint32_t player_id, sol::this_state ts) {
  auto player_opt = GetCharacterOrWarn(player_id, "getPlayerOverlays");
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

/* luagmp (func)
 *
 * This function will remove a specified animation overlay from the player.
 *
 * @version  0.3.0
 * @name     removePlayerOverlay
 * @side     server
 * @category Player
 * @param    (number) player_id     Target player or NPC id.
 * @param    (string) overlay       Overlay name.
 * @return   (boolean)              True on success.
 *
 */
bool Function_RemovePlayerOverlay(std::uint32_t player_id, const std::string& overlay) {
  if (!GetCharacterOrWarn(player_id, "removePlayerOverlay")) {
    return false;
  }

  return g_server->RemoveCharacterOverlay(player_id, ClampLuaText(overlay, 255));
}

/* luagmp (func)
 *
 * This function will play an animation on the player/NPC.
 *
 * @version  0.3.0
 * @name     playAni
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @param    (string) aniName    Animation name (e.g. "T_STAND_2_SIT").
 * @return   (boolean)           True on success.
 *
 */
bool Function_PlayAni(std::uint32_t player_id, const std::string& ani_name) {
  if (!GetCharacterOrWarn(player_id, "playAni")) {
    return false;
  }

  return g_server->PlayAnimation(player_id, ClampLuaText(ani_name, 255));
}

/* luagmp (func)
 *
 * This function will stop a played animation on the player/NPC.
 *
 * @version  0.3.0
 * @name     stopAni
 * @side     server
 * @category Player
 * @param    (number) player_id    Target player or NPC id.
 * @param    (string|nil) aniName  Animation name to stop. Defaults to "" for first active animation.
 * @return   (boolean)             True on success.
 *
 */
bool Function_StopAni(std::uint32_t player_id, sol::optional<std::string> ani_name) {
  if (!GetCharacterOrWarn(player_id, "stopAni")) {
    return false;
  }

  return g_server->StopAnimation(player_id, ClampLuaText(ani_name.value_or(""), 255));
}

/* luagmp (func)
 *
 * This function will play a face animation on the player's.
 *
 * @version  0.3.0
 * @name     playFaceAni
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player id; NPC ids are not supported.
 * @param    (string) aniName    Face animation name (e.g. "S_FRIENDLY").
 * @return   (boolean)           True on success.
 *
 */
bool Function_PlayFaceAni(std::uint32_t player_id, const std::string& ani_name) {
  if (!GetPlayerOrWarn(player_id, "playFaceAni")) {
    return false;
  }

  return g_server->PlayFaceAnimation(player_id, ClampLuaText(ani_name, 255));
}

/* luagmp (func)
 *
 * This function will stop a played face animation on the player's.
 *
 * @version  0.3.0
 * @name     stopFaceAni
 * @side     server
 * @category Player
 * @param    (number) player_id    Target player id; NPC ids are not supported.
 * @param    (string|nil) aniName  Face animation name to stop. Defaults to "" for first active animation.
 * @return   (boolean)             True on success.
 *
 */
bool Function_StopFaceAni(std::uint32_t player_id, sol::optional<std::string> ani_name) {
  if (!GetPlayerOrWarn(player_id, "stopFaceAni")) {
    return false;
  }

  return g_server->StopFaceAnimation(player_id, ClampLuaText(ani_name.value_or(""), 255));
}

/* luagmp (func)
 *
 * This function will play gesticulation animation on the player/NPC.
 *
 * @version  0.3.0
 * @name     playGesticulation
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @return   (boolean)           True on success.
 *
 */
bool Function_PlayGesticulation(std::uint32_t player_id) {
  if (!GetCharacterOrWarn(player_id, "playGesticulation")) {
    return false;
  }

  return g_server->PlayGesticulation(player_id);
}

/* luagmp (func)
 *
 * This function will set the player/npc position.
 *
 * @version  0.3.0
 * @name     setPlayerPosition
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @param    (number) x          X coordinate.
 * @param    (number) y          Y coordinate.
 * @param    (number) z          Z coordinate.
 * @param    (boolean|nil) interpolate  Whether to interpolate movement. Defaults to false for players and true for NPCs.
 * @return   (boolean)           True on success.
 *
 */
bool Function_SetPlayerPosition(std::uint32_t player_id, float x, float y, float z, sol::optional<bool> interpolate) {
  if (!GetCharacterOrWarn(player_id, "setPlayerPosition")) {
    return false;
  }

  const auto teleport = interpolate ? std::optional<bool>{!*interpolate} : std::nullopt;
  return g_server->SetCharacterPosition(player_id, glm::vec3{x, y, z}, teleport);
}

/* luagmp (func)
 *
 * This function will return the player/NPC's position, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerPosition
 * @side     server
 * @category Player
 * @param    (number) player_id   Target player or NPC id.
 * @return   ({x, y, z}|nil)      Table containing x,y,z or nil.
 *
 */
sol::object Function_GetPlayerPosition(std::uint32_t player_id, sol::this_state ts) {
  if (!GetCharacterOrWarn(player_id, "getPlayerPosition")) {
    return sol::nil;
  }

  auto position = g_server->GetCharacterPosition(player_id);
  if (!position.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return ::lua::bind_helpers::MakeVec3Table(lua, *position);
}

/* luagmp (func)
 *
 * This function will set the player/NPC's facing angle (degrees).
 *
 * @version  0.3.0
 * @name     setPlayerAngle
 * @side     server
 * @category Player
 * @param    (number) player_id        Target player or NPC id.
 * @param    (number) angle_degrees    Angle in degrees.
 * @return   (boolean)                 True on success.
 *
 */
bool Function_SetPlayerAngle(std::uint32_t player_id, float angle_degrees) {
  if (!GetCharacterOrWarn(player_id, "setPlayerAngle")) {
    return false;
  }

  // Expose degrees in Lua (more ergonomic for script authors), convert to radians for the server.
  return g_server->SetCharacterAngle(player_id, glm::radians(angle_degrees));
}

/* luagmp (func)
 *
 * This function will return the player/NPC's facing angle in degrees, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerAngle
 * @side     server
 * @category Player
 * @param    (number) player_id    Target player or NPC id.
 * @return   (number|nil)          Angle in degrees or nil.
 *
 */
sol::object Function_GetPlayerAngle(std::uint32_t player_id, sol::this_state ts) {
  auto player_opt = GetCharacterOrWarn(player_id, "getPlayerAngle");
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  const auto& nrot = player_opt->get().state.nrot;
  const float angle_radians = std::atan2(nrot.x, nrot.z);
  const float angle_degrees = ::lua::bind_helpers::NormalizeDegrees(glm::degrees(angle_radians));
  sol::state_view lua(ts);
  return sol::make_object(lua, angle_degrees);
}

/* luagmp (func)
 *
 * This function will move the player to a different world, optionally specifying a start point.
 *
 * @version  0.3.0
 * @name     setPlayerWorld
 * @side     server
 * @category Player
 * @param    (number) player_id      Target player or NPC id.
 * @param    (string) world          World name.
 * @param    (string|nil) start_point Optional start point name.
 * @return   (boolean)               True on success.
 *
 */
bool Function_SetPlayerWorld(std::uint32_t player_id, const std::string& world, std::optional<std::string> start_point) {
  if (!GetCharacterOrWarn(player_id, "setPlayerWorld")) {
    return false;
  }

  return g_server->SetCharacterWorld(player_id, world, start_point);
}

/* luagmp (func)
 *
 * This function will return the player/NPC's current world name, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerWorld
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @return   (string|nil)        World name or nil.
 *
 */
sol::object Function_GetPlayerWorld(std::uint32_t player_id, sol::this_state ts) {
  auto player_opt = GetCharacterOrWarn(player_id, "getPlayerWorld");
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, player_opt->get().world);
}

/* luagmp (func)
 *
 * This function will set the player/NPC's virtual world id.
 *
 * @version  0.3.0
 * @name     setPlayerVirtualWorld
 * @side     server
 * @category Player
 * @param    (number) player_id       Target player or NPC id.
 * @param    (number) virtual_world   Virtual world id (0-65535).
 * @return   (boolean)                True on success.
 *
 */
bool Function_SetPlayerVirtualWorld(std::uint32_t player_id, int virtual_world) {
  if (!GetCharacterOrWarn(player_id, "setPlayerVirtualWorld")) {
    return false;
  }

  return g_server->SetCharacterVirtualWorld(player_id, virtual_world);
}

/* luagmp (func)
 *
 * This function will return the player/NPC's virtual world id, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerVirtualWorld
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @return   (number|nil)        Virtual world id or nil.
 *
 */
sol::object Function_GetPlayerVirtualWorld(std::uint32_t player_id, sol::this_state ts) {
  auto player_opt = GetCharacterOrWarn(player_id, "getPlayerVirtualWorld");
  if (!player_opt.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, player_opt->get().virtual_world);
}

/* luagmp (func)
 *
 * This function will ban the player on the server.
 *
 * @version  0.3.0
 * @name     ban
 * @side     server
 * @category Player
 * @note     The reason string can't be longer than 255 characters.
 * @param    (number) player_id  Target player id; NPC ids are not supported.
 * @param    (string|nil) reason Optional reason why the player was banned.
 *
 */
void Function_Ban(std::uint32_t player_id, sol::optional<std::string> reason) {
  if (!GetPlayerOrWarn(player_id, "ban")) {
    return;
  }

  auto reason_text = reason.value_or(std::string{});
  auto clamped_reason = ClampLuaText(reason_text, 255);
  if (clamped_reason.size() != reason_text.size()) {
    SPDLOG_WARN("ban reason too long ({}), clamping to 255 characters", reason_text.size());
  }

  g_server->BanPlayer(player_id, clamped_reason);
}

/* luagmp (func)
 *
 * This function will kick the player from the server.
 *
 * @version  0.3.0
 * @name     kick
 * @side     server
 * @category Player
 * @note     The reason string can't be longer than 255 characters.
 * @param    (number) player_id  Target player id; NPC ids are not supported.
 * @param    (string|nil) reason Optional reason why the player was kicked.
 *
 */
void Function_Kick(std::uint32_t player_id, sol::optional<std::string> reason) {
  if (!GetPlayerOrWarn(player_id, "kick")) {
    return;
  }

  auto reason_text = reason.value_or(std::string{});
  auto clamped_reason = ClampLuaText(reason_text, 255);
  if (clamped_reason.size() != reason_text.size()) {
    SPDLOG_WARN("kick reason too long ({}), clamping to 255 characters", reason_text.size());
  }

  g_server->KickPlayer(player_id, clamped_reason);
}

/* luagmp (func)
 *
 * This function will check whether player is connected to the server.
 *
 * @version  0.3.0
 * @name     isPlayerConnected
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player id; NPC ids are not supported.
 * @return   (boolean)           True when player is connected, otherwise false.
 *
 */
bool Function_IsPlayerConnected(std::uint32_t player_id) {
  if (!GetPlayerOrWarn(player_id, "isPlayerConnected")) {
    return false;
  }

  return g_server->IsPlayerConnected(player_id);
}

/* luagmp (func)
 *
 * This function will check whether player is logged in through rcon.
 *
 * @version  0.3.0
 * @name     isPlayerAdmin
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player id; NPC ids are not supported.
 * @return   (boolean)           True when player is rcon-authenticated, otherwise false.
 *
 */
bool Function_IsPlayerAdmin(std::uint32_t player_id) {
  if (!GetPlayerOrWarn(player_id, "isPlayerAdmin")) {
    return false;
  }

  return g_server->IsPlayerAdmin(player_id);
}

/* luagmp (func)
 *
 * This function will check whether player is dead.
 *
 * @version  0.3.0
 * @name     isPlayerDead
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @return   (boolean)           True when player is dead, otherwise false.
 *
 */
bool Function_IsPlayerDead(std::uint32_t player_id) {
  if (!GetCharacterOrWarn(player_id, "isPlayerDead")) {
    return false;
  }

  return g_server->IsPlayerDead(player_id);
}

/* luagmp (func)
 *
 * This function will check whether player is spawned.
 *
 * @version  0.3.0
 * @name     isPlayerSpawned
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @return   (boolean)           True when player is spawned, otherwise false.
 *
 */
bool Function_IsPlayerSpawned(std::uint32_t player_id) {
  if (!GetCharacterOrWarn(player_id, "isPlayerSpawned")) {
    return false;
  }

  return g_server->IsPlayerSpawned(player_id);
}

/* luagmp (func)
 *
 * This function will check whether player is in unconscious state. The player will be unconscious, when it gets beaten up, but not killed.
 *
 * @version  0.3.0
 * @name     isPlayerUnconscious
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @return   (boolean)           True when player is unconscious, otherwise false.
 *
 */
bool Function_IsPlayerUnconscious(std::uint32_t player_id) {
  if (!GetCharacterOrWarn(player_id, "isPlayerUnconscious")) {
    return false;
  }

  return g_server->IsPlayerUnconscious(player_id);
}

/* luagmp (func)
 *
 * This function will immediately respawn the player if he is dead.
 *
 * @version  0.3.0
 * @name     respawnPlayer
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player id; NPC ids are not supported.
 *
 */
void Function_RespawnPlayer(std::uint32_t player_id) {
  if (!GetPlayerOrWarn(player_id, "respawnPlayer")) {
    return;
  }

  g_server->RespawnPlayer(player_id);
}

/* luagmp (func)
 *
 * This function will set a player's automatic respawn delay in milliseconds. Zero disables automatic respawn for that player.
 *
 * @version  0.3.0
 * @name     setPlayerRespawnTime
 * @side     server
 * @category Player
 * @param    (number) player_id     Target player id; NPC ids are not supported.
 * @param    (number) respawn_time  New respawn time in milliseconds.
 * @return   (boolean)              True on success; false for an unknown player or a negative duration.
 *
 */
bool Function_SetPlayerRespawnTime(std::uint32_t player_id, std::int32_t respawn_time_ms) {
  if (!GetPlayerOrWarn(player_id, "setPlayerRespawnTime")) {
    return false;
  }
  return g_server->SetPlayerRespawnTime(player_id, respawn_time_ms);
}

/* luagmp (func)
 *
 * This function will return the player time to respawn after death.
 *
 * @version  0.3.0
 * @name     getPlayerRespawnTime
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player id; NPC ids are not supported.
 * @return   (number|nil)        The player respawn time or nil if player isn't created.
 *
 */
sol::object Function_GetPlayerRespawnTime(std::uint32_t player_id, sol::this_state ts) {
  if (!GetPlayerOrWarn(player_id, "getPlayerRespawnTime")) {
    return sol::nil;
  }

  auto respawn_time = g_server->GetPlayerRespawnTime(player_id);
  if (!respawn_time.has_value()) {
    return sol::nil;
  }

  sol::state_view lua(ts);
  return sol::make_object(lua, respawn_time.value());
}

/* luagmp (func)
 *
 * This function will set the player/npc invulnerability.
 *
 * @version  0.3.0
 * @name     setPlayerInvulnerable
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @param    (boolean) enabled   Whether damage should be blocked.
 * @return   (boolean)           True on success; false for an unknown actor.
 *
 */
bool Function_SetPlayerInvulnerable(std::uint32_t player_id, bool enabled) {
  if (!GetCharacterOrWarn(player_id, "setPlayerInvulnerable"))
    return false;
  return g_server->SetCharacterInvulnerable(player_id, enabled);
}

/* luagmp (func)
 *
 * This function will return the current player/npc invulnerability.
 *
 * @version  0.3.0
 * @name     isPlayerInvulnerable
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @return   (boolean)           True when invulnerable; false when disabled or the actor does not exist.
 *
 */
bool Function_IsPlayerInvulnerable(std::uint32_t player_id) {
  const auto actor = GetCharacterOrWarn(player_id, "isPlayerInvulnerable");
  return actor && actor->get().invulnerable;
}

/* luagmp (func)
 *
 * This function will give an item to the player/npc.
 *
 * @version  0.3.0
 * @name     giveItem
 * @side     server
 * @category Inventory
 * @param    (number) player_id    Target player or NPC id.
 * @param    (string) instance     Item instance name from scripts.
 * @param    (number) amount       Amount to give.
 * @return   (boolean)             True on success.
 *
 */
bool Function_GiveItem(std::uint32_t player_id, const std::string& instance, std::int32_t amount) {
  if (!GetCharacterOrWarn(player_id, "giveItem")) {
    return false;
  }

  return g_server->GiveItem(player_id, ClampLuaText(instance, 255), amount);
}

/* luagmp (func)
 *
 * This function requests equipment for a player or server NPC. The item must exist in the actor's server inventory.
 *
 * @version  0.3.0
 * @name     equipItem
 * @side     server
 * @category Player
 * @param    (number) player_id    Target player or NPC id.
 * @param    (string) instance     Item instance name from scripts.
 * @param    (number|nil) slot_id  Optional slot id. Defaults to -1 for first free slot.
 * @return   (boolean)             True on success.
 *
 */
bool Function_EquipItem(std::uint32_t player_id, const std::string& instance, sol::optional<std::int32_t> slot_id) {
  if (!GetCharacterOrWarn(player_id, "equipItem")) {
    return false;
  }

  return g_server->EquipItem(player_id, ClampLuaText(instance, 255), slot_id.value_or(-1));
}

/* luagmp (func)
 *
 * This function will unequip an item for the player/npc.
 *
 * @version  0.3.0
 * @name     unequipItem
 * @side     server
 * @category Player
 * @param    (number) player_id    Target player or NPC id.
 * @param    (string) instance     Item instance name from scripts.
 * @param    (number|nil) slot_id   Equipment slot. Omit to clear all matching slots.
 * @return   (boolean)             True on success.
 *
 */
bool Function_UnequipItem(std::uint32_t player_id, const std::string& instance, sol::optional<std::int32_t> slot_id) {
  if (!GetCharacterOrWarn(player_id, "unequipItem")) {
    return false;
  }

  return g_server->UnequipItem(player_id, ClampLuaText(instance, 255), slot_id.value_or(Net::EQUIP_SLOT_GENERIC));
}

/* luagmp (func)
 *
 * This function will equip an armor item for the player.
 *
 * @version  0.3.0
 * @name     equipArmor
 * @side     server
 * @category Player
 * @param    (number) player_id    Target player or NPC id.
 * @param    (string) instance     Armor item instance name.
 * @return   (boolean)             True when the equip packet was sent.
 *
 */
bool Function_EquipArmor(std::uint32_t player_id, const std::string& instance) {
  if (!GetCharacterOrWarn(player_id, "equipArmor")) {
    return false;
  }

  return g_server->EquipArmor(player_id, ClampLuaText(instance, 255));
}

/* luagmp (func)
 *
 * This function will unequip the player/NPC's current armor.
 *
 * @version  0.3.0
 * @name     unequipArmor
 * @side     server
 * @category Player
 * @param    (number) player_id    Target player or NPC id.
 * @return   (boolean)             True when an equipped armor item was found and unequipped.
 *
 */
bool Function_UnequipArmor(std::uint32_t player_id) {
  if (!GetCharacterOrWarn(player_id, "unequipArmor")) {
    return false;
  }

  return g_server->UnequipArmor(player_id);
}

/* luagmp (func)
 *
 * This function will equip a melee weapon for the player.
 *
 * @version  0.3.0
 * @name     equipMeleeWeapon
 * @side     server
 * @category Player
 * @param    (number) player_id    Target player or NPC id.
 * @param    (string) instance     Melee weapon item instance name.
 * @return   (boolean)             True when the equip packet was sent.
 *
 */
bool Function_EquipMeleeWeapon(std::uint32_t player_id, const std::string& instance) {
  if (!GetCharacterOrWarn(player_id, "equipMeleeWeapon")) {
    return false;
  }

  return g_server->EquipMeleeWeapon(player_id, ClampLuaText(instance, 255));
}

/* luagmp (func)
 *
 * This function will unequip the player/NPC's current melee weapon.
 *
 * @version  0.3.0
 * @name     unequipMeleeWeapon
 * @side     server
 * @category Player
 * @param    (number) player_id    Target player or NPC id.
 * @return   (boolean)             True when an equipped melee weapon was found and unequipped.
 *
 */
bool Function_UnequipMeleeWeapon(std::uint32_t player_id) {
  if (!GetCharacterOrWarn(player_id, "unequipMeleeWeapon")) {
    return false;
  }

  return g_server->UnequipMeleeWeapon(player_id);
}

/* luagmp (func)
 *
 * This function will equip a ranged weapon for the player.
 *
 * @version  0.3.0
 * @name     equipRangedWeapon
 * @side     server
 * @category Player
 * @param    (number) player_id    Target player or NPC id.
 * @param    (string) instance     Ranged weapon item instance name.
 * @return   (boolean)             True when the equip packet was sent.
 *
 */
bool Function_EquipRangedWeapon(std::uint32_t player_id, const std::string& instance) {
  if (!GetCharacterOrWarn(player_id, "equipRangedWeapon")) {
    return false;
  }

  return g_server->EquipRangedWeapon(player_id, ClampLuaText(instance, 255));
}

/* luagmp (func)
 *
 * This function will unequip the player/NPC's current ranged weapon.
 *
 * @version  0.3.0
 * @name     unequipRangedWeapon
 * @side     server
 * @category Player
 * @param    (number) player_id    Target player or NPC id.
 * @return   (boolean)             True when an equipped ranged weapon was found and unequipped.
 *
 */
bool Function_UnequipRangedWeapon(std::uint32_t player_id) {
  if (!GetCharacterOrWarn(player_id, "unequipRangedWeapon")) {
    return false;
  }

  return g_server->UnequipRangedWeapon(player_id);
}

/* luagmp (func)
 *
 * This function will equip a helmet item for the player.
 *
 * @version  0.3.0
 * @name     equipHelmet
 * @side     server
 * @category Player
 * @param    (number) player_id    Target player or NPC id.
 * @param    (string) instance     Helmet item instance name.
 * @return   (boolean)             True when the equip packet was sent.
 *
 */
bool Function_EquipHelmet(std::uint32_t player_id, const std::string& instance) {
  if (!GetCharacterOrWarn(player_id, "equipHelmet")) {
    return false;
  }

  return g_server->EquipHelmet(player_id, ClampLuaText(instance, 255));
}

/* luagmp (func)
 *
 * This function will unequip the player/NPC's current helmet.
 *
 * @version  0.3.0
 * @name     unequipHelmet
 * @side     server
 * @category Player
 * @param    (number) player_id    Target player or NPC id.
 * @return   (boolean)             True when an equipped helmet item was found and unequipped.
 *
 */
bool Function_UnequipHelmet(std::uint32_t player_id) {
  if (!GetCharacterOrWarn(player_id, "unequipHelmet")) {
    return false;
  }

  return g_server->UnequipHelmet(player_id);
}

/* luagmp (func)
 *
 * This function will equip a shield item for the player.
 *
 * @version  0.3.0
 * @name     equipShield
 * @side     server
 * @category Player
 * @param    (number) player_id    Target player or NPC id.
 * @param    (string) instance     Shield item instance name.
 * @return   (boolean)             True when the equip packet was sent.
 *
 */
bool Function_EquipShield(std::uint32_t player_id, const std::string& instance) {
  if (!GetCharacterOrWarn(player_id, "equipShield")) {
    return false;
  }

  return g_server->EquipShield(player_id, ClampLuaText(instance, 255));
}

/* luagmp (func)
 *
 * This function will unequip the player/NPC's current shield.
 *
 * @version  0.3.0
 * @name     unequipShield
 * @side     server
 * @category Player
 * @param    (number) player_id    Target player or NPC id.
 * @return   (boolean)             True when an equipped shield item was found and unequipped.
 *
 */
bool Function_UnequipShield(std::uint32_t player_id) {
  if (!GetCharacterOrWarn(player_id, "unequipShield")) {
    return false;
  }

  return g_server->UnequipShield(player_id);
}

/* luagmp (func)
 *
 * This function will return the amount of a specific item in the player/NPC's inventory.
 *
 * @version  0.3.0
 * @name     hasItem
 * @side     server
 * @category Inventory
 * @param    (number) player_id    Target player or NPC id.
 * @param    (string) instance     Item instance name from scripts.
 * @return   (number)              Item quantity, or 0 if missing.
 *
 */
int Function_HasItem(std::uint32_t player_id, const std::string& instance) {
  if (!GetCharacterOrWarn(player_id, "hasItem")) {
    return 0;
  }

  return g_server->HasItem(player_id, ClampLuaText(instance, 255));
}

/* luagmp (func)
 *
 * This function will remove an item from the player/NPC's inventory.
 *
 * @version  0.3.0
 * @name     removeItem
 * @side     server
 * @category Inventory
 * @param    (number) player_id    Target player or NPC id.
 * @param    (string) instance     Item instance name from scripts.
 * @param    (number) amount       Amount to remove.
 * @return   (boolean)             True on success.
 *
 */
bool Function_RemoveItem(std::uint32_t player_id, const std::string& instance, std::int32_t amount) {
  if (!GetCharacterOrWarn(player_id, "removeItem")) {
    return false;
  }

  return g_server->RemoveItem(player_id, ClampLuaText(instance, 255), amount);
}

/* luagmp (func)
 *
 * This function will set the server's current world name.
 *
 * @version  0.3.0
 * @name     setServerWorld
 * @side     server
 * @category Game
 * @param    (string) world      World name to set.
 * @return   (boolean)           True on success.
 *
 */
bool Function_SetServerWorld(const std::string& world) {
  return g_server->SetServerWorld(world);
}

/* luagmp (func)
 *
 * This function will return the server's current world name.
 *
 * @version  0.3.0
 * @name     getServerWorld
 * @side     server
 * @category Game
 * @return   (string)        Current server world or empty string.
 *
 */
std::string Function_GetServerWorld() {
  return g_server->GetServerWorld();
}

/* luagmp (func)
 *
 * This function will return player ids within a radius of a given position in a world.
 *
 * @version  0.3.0
 * @name     findNearbyPlayers
 * @side     server
 * @category Streamer
 * @param    ({x, y, z}) position_table    Table with x,y,z coordinates.
 * @param    (number) radius               Search radius.
 * @param    (string) world                World name to search in.
 * @param    (number|nil) virtual_world    Optional virtual world id.
 * @return   ({...})                       Array of player ids.
 *
 */
sol::as_table_t<std::vector<std::uint32_t>> Function_FindNearbyPlayers(const sol::table& position_table, int radius, const std::string& world,
                                                                       sol::optional<int> virtual_world) {
  auto position = ParsePositionTable(position_table);
  if (!position.has_value()) {
    return sol::as_table(std::vector<std::uint32_t>{});
  }

  return sol::as_table(g_server->FindNearbyPlayers(*position, static_cast<float>(radius), world, virtual_world.value_or(0)));
}

/* luagmp (func)
 *
 * This function will return the list of players that have been spawned for the given player.
 *
 * @version  0.3.0
 * @name     getPlayersStreamedToPlayer
 * @side     server
 * @category Player
 * @param    (number) player_id   Target player id; NPC ids are not supported.
 * @return   ({...})              One-based array of player and server NPC ids visible to this player.
 *
 */
sol::as_table_t<std::vector<std::uint32_t>> Function_GetPlayersStreamedToPlayer(std::uint32_t player_id) {
  if (!GetPlayerOrWarn(player_id, "getPlayersStreamedToPlayer")) {
    return sol::as_table(std::vector<std::uint32_t>{});
  }

  return sol::as_table(g_server->GetCharactersStreamedToPlayer(player_id));
}

/* luagmp (func)
 *
 * This function will return the list of players currently observing the given player or NPC.
 *
 * @version  0.3.0
 * @name     getPlayersStreamingPlayer
 * @side     server
 * @category Player
 * @param    (number) player_id  Target player or NPC id.
 * @return   ({...})             Array of player ids.
 *
 */
sol::as_table_t<std::vector<std::uint32_t>> Function_GetPlayersStreamingPlayer(std::uint32_t player_id) {
  if (!GetCharacterOrWarn(player_id, "getPlayersStreamingPlayer")) {
    return sol::as_table(std::vector<std::uint32_t>{});
  }

  return sol::as_table(g_server->GetPlayersStreamingCharacter(player_id));
}

/* luagmp (func)
 *
 * This function sets the global player streaming radius.
 *
 * @version  0.3.0
 * @name     setStreamerRadius
 * @side     server
 * @category Streamer
 * @param    (number) radius   Streaming radius in world units.
 * @return   (boolean)         True if the value was accepted.
 *
 */
bool Function_SetStreamerRadius(int radius) {
  return g_server->SetStreamerRadius(radius);
}

/* luagmp (func)
 *
 * This function returns the global player streaming radius.
 *
 * @version  0.3.0
 * @name     getStreamerRadius
 * @side     server
 * @category Streamer
 * @return   (number)          Streaming radius in world units.
 *
 */
int Function_GetStreamerRadius() {
  return g_server->GetStreamerRadius();
}

/* luagmp (func)
 *
 * This function sets the global player streaming height. A value of 0 disables the vertical limit.
 *
 * @version  0.3.0
 * @name     setStreamerHeight
 * @side     server
 * @category Streamer
 * @param    (number) height   Streaming height in world units, or 0 for unlimited.
 * @return   (boolean)         True if the value was accepted.
 *
 */
bool Function_SetStreamerHeight(int height) {
  return g_server->SetStreamerHeight(height);
}

/* luagmp (func)
 *
 * This function returns the global player streaming height.
 *
 * @version  0.3.0
 * @name     getStreamerHeight
 * @side     server
 * @category Streamer
 * @return   (number)          Streaming height in world units, or 0 for unlimited.
 *
 */
int Function_GetStreamerHeight() {
  return g_server->GetStreamerHeight();
}

/* luagmp (func)
 *
 * This function enables or disables the server voice chat system for every player.
 *
 * @version  0.3.0
 * @name     setVoiceEnabled
 * @side     server
 * @category Voice
 * @param    (boolean) enabled   Whether voice chat is enabled globally.
 * @return   (boolean)           True when the setting was applied.
 *
 */
bool Function_SetVoiceEnabled(bool enabled) {
  return g_server->SetVoiceEnabled(enabled);
}

/* luagmp (func)
 *
 * This function returns whether the server voice chat system is enabled.
 *
 * @version  0.3.0
 * @name     getVoiceEnabled
 * @side     server
 * @category Voice
 * @return   (boolean)           True when voice chat is enabled globally.
 *
 */
bool Function_GetVoiceEnabled() {
  return g_server->GetVoiceEnabled();
}

/* luagmp (func)
 *
 * This function returns whether a player is allowed to transmit voice.
 *
 * @version  0.3.0
 * @name     getVoiceEnabledForPlayer
 * @side     server
 * @category Voice
 * @param    (number) player_id  Target player id; NPC ids are not supported.
 * @return   (boolean|nil)       True when the player may transmit, or nil when missing.
 *
 */
sol::optional<bool> Function_GetVoiceEnabledForPlayer(std::uint32_t player_id) {
  if (!GetPlayerOrWarn(player_id, "getVoiceEnabledForPlayer")) {
    return {};
  }

  const auto enabled = g_server->GetVoiceEnabledForPlayer(player_id);
  if (!enabled.has_value()) {
    return {};
  }
  return sol::optional<bool>{*enabled};
}

/* luagmp (func)
 *
 * This function enables or mutes voice transmission for one player. A disabled
 * player can still hear other players.
 *
 * @version  0.3.0
 * @name     setVoiceEnabledForPlayer
 * @side     server
 * @category Voice
 * @param    (number) player_id  Target player id; NPC ids are not supported.
 * @param    (boolean) enabled   Whether this player may transmit voice.
 * @return   (boolean)           True when the player exists and the state was applied.
 *
 */
bool Function_SetVoiceEnabledForPlayer(std::uint32_t player_id, bool enabled) {
  if (!GetPlayerOrWarn(player_id, "setVoiceEnabledForPlayer")) {
    return false;
  }
  return g_server->SetVoiceEnabledForPlayer(player_id, enabled);
}

/* luagmp (func)
 *
 * This function changes the proximity range and assigns it to every player.
 *
 * @version  0.3.0
 * @name     setVoiceRange
 * @side     server
 * @category Voice
 * @param    (number) range      voice range in game units; must be non-negative.
 * @return   (boolean)           True when the range was accepted.
 *
 */
bool Function_SetVoiceRange(int range) {
  return g_server->SetVoiceRange(range);
}

/* luagmp (func)
 *
 * This function returns the global proximity voice range.
 *
 * @version  0.3.0
 * @name     getVoiceRange
 * @side     server
 * @category Voice
 * @return   (number)            voice range in game units.
 *
 */
int Function_GetVoiceRange() {
  return g_server->GetVoiceRange();
}

/* luagmp (func)
 *
 * This function will set the server time (hour, minute, optional day offset).
 *
 * @version  0.3.0
 * @name     setTime
 * @side     server
 * @category Game
 * @param    (number) hour      Hour (0-23).
 * @param    (number) min       Minute (0-59).
 * @param    (number|nil) day   Optional day offset.
 * @return   (boolean)          True on success.
 *
 */
bool Function_SetTime(int hour, int min, sol::optional<int> day) {
  return g_server->SetTime(hour, min, day.value_or(0));
}

/* luagmp (func)
 *
 * This function will return the current server time as a table {day,hour,min}.
 *
 * @version  0.3.0
 * @name     getTime
 * @side     server
 * @category Game
 * @return   ({day, hour, minute})  Table containing day, hour, minute.
 *
 */
sol::object Function_GetTime(sol::this_state ts) {
  auto time = g_server->GetTime();
  sol::state_view lua(ts);
  sol::table time_table = lua.create_table();
  time_table["day"] = time.day_;
  time_table["hour"] = time.hour_;
  time_table["minute"] = time.min_;
  return time_table;
}

/* luagmp (func)
 *
 * This function will set the duration of a full in-game day in milliseconds.
 *
 * @version  0.3.0
 * @name     setDayLength
 * @side     server
 * @category Game
 * @param    (number) miliseconds   Day length in milliseconds (min 10000 ms).
 * @return   (boolean)              True on success.
 *
 */
bool Function_SetDayLength(float day_length_ms) {
  constexpr float kMinDayLengthMs = 10000.0f;
  if (day_length_ms < kMinDayLengthMs) {
    SPDLOG_WARN("setDayLength called with invalid value {} ms, clamping to {} ms", day_length_ms, kMinDayLengthMs);
    day_length_ms = kMinDayLengthMs;
  }

  return g_server->SetDayLength(day_length_ms);
}

/* luagmp (func)
 *
 * This function will return the current duration of a full in-game day in milliseconds.
 *
 * @version  0.3.0
 * @name     getDayLength
 * @side     server
 * @category Game
 * @return   (number)  Day length in milliseconds.
 *
 */
sol::object Function_GetDayLength(sol::this_state ts) {
  sol::state_view lua(ts);
  return sol::make_object(lua, g_server->GetDayLength());
}

// Values supplied to the shared server-information functions.
std::string ServerHostname() {
  return g_server ? g_server->GetHostname() : std::string{};
}
int ServerMaxSlots() {
  return g_server ? static_cast<int>(g_server->GetMaxSlots()) : 0;
}
std::vector<int> ServerOnlinePlayers() {
  std::vector<int> players;
  if (g_server) {
    g_server->GetPlayerManager().ForEachIngamePlayer([&players](const PlayerManager::Player& player) { players.push_back(player.player_id); });
  }
  return players;
}
int ServerPlayersCount() {
  return static_cast<int>(ServerOnlinePlayers().size());
}

void lua::bindings::BindFunctions(sol::state& lua, TimerManager& timer_manager) {
  SetServerInfoProvider({ServerHostname, ServerMaxSlots, ServerOnlinePlayers, ServerPlayersCount});

  lua["sendMessageToAll"] = Function_SendMessageToAll;
  lua["sendMessageToPlayer"] = Function_SendMessageToPlayer;
  lua["sendPlayerMessageToAll"] = Function_SendPlayerMessageToAll;
  lua["sendPlayerMessageToPlayer"] = Function_SendPlayerMessageToPlayer;

  lua["spawnPlayer"] = Function_SpawnPlayer;
  lua["unspawnPlayer"] = Function_UnspawnPlayer;

  lua["setPlayerInstance"] = Function_SetPlayerInstance;
  lua["getPlayerInstance"] = Function_GetPlayerInstance;
  lua["setPlayerName"] = Function_SetPlayerName;
  lua["getPlayerName"] = Function_GetPlayerName;
  lua["getPlayerIP"] = Function_GetPlayerIP;
  lua["getPlayerPing"] = Function_GetPlayerPing;
  lua["getPlayerMacAddress"] = Function_GetPlayerMacAddress;
  lua["getPlayerIdentity"] = Function_GetPlayerIdentity;
  lua["setPlayerNameColor"] = Function_SetPlayerNameColor;
  lua["getPlayerNameColor"] = Function_GetPlayerNameColor;
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
  lua["setPlayerWeaponMode"] = Function_SetPlayerWeaponMode;
  lua["getPlayerWeaponMode"] = Function_GetPlayerWeaponMode;
  lua["getPlayerAni"] = Function_GetPlayerAni;
  lua["getPlayerArmor"] = Function_GetPlayerArmor;
  lua["getPlayerMeleeWeapon"] = Function_GetPlayerMeleeWeapon;
  lua["getPlayerRangedWeapon"] = Function_GetPlayerRangedWeapon;
  lua["getPlayerHelmet"] = Function_GetPlayerHelmet;
  lua["getPlayerShield"] = Function_GetPlayerShield;
  lua["applyPlayerOverlay"] = Function_ApplyPlayerOverlay;
  lua["getPlayerOverlays"] = Function_GetPlayerOverlays;
  lua["removePlayerOverlay"] = Function_RemovePlayerOverlay;
  lua["playAni"] = Function_PlayAni;
  lua["stopAni"] = Function_StopAni;
  lua["playFaceAni"] = Function_PlayFaceAni;
  lua["stopFaceAni"] = Function_StopFaceAni;
  lua["playGesticulation"] = Function_PlayGesticulation;
  lua["setPlayerPosition"] = Function_SetPlayerPosition;
  lua["getPlayerPosition"] = Function_GetPlayerPosition;
  lua["setPlayerAngle"] = Function_SetPlayerAngle;
  lua["getPlayerAngle"] = Function_GetPlayerAngle;
  lua["setPlayerWorld"] = Function_SetPlayerWorld;
  lua["getPlayerWorld"] = Function_GetPlayerWorld;
  lua["setPlayerVirtualWorld"] = Function_SetPlayerVirtualWorld;
  lua["getPlayerVirtualWorld"] = Function_GetPlayerVirtualWorld;

  lua["ban"] = Function_Ban;
  lua["kick"] = Function_Kick;
  lua["isPlayerConnected"] = Function_IsPlayerConnected;
  lua["isPlayerAdmin"] = Function_IsPlayerAdmin;
  lua["isPlayerDead"] = Function_IsPlayerDead;
  lua["isPlayerSpawned"] = Function_IsPlayerSpawned;
  lua["isPlayerUnconscious"] = Function_IsPlayerUnconscious;
  lua["respawnPlayer"] = Function_RespawnPlayer;
  lua["setPlayerRespawnTime"] = Function_SetPlayerRespawnTime;
  lua["getPlayerRespawnTime"] = Function_GetPlayerRespawnTime;

  lua["giveItem"] = Function_GiveItem;
  lua["setPlayerInvulnerable"] = Function_SetPlayerInvulnerable;
  lua["isPlayerInvulnerable"] = Function_IsPlayerInvulnerable;
  lua["equipItem"] = Function_EquipItem;
  lua["unequipItem"] = Function_UnequipItem;
  lua["equipArmor"] = Function_EquipArmor;
  lua["unequipArmor"] = Function_UnequipArmor;
  lua["equipMeleeWeapon"] = Function_EquipMeleeWeapon;
  lua["unequipMeleeWeapon"] = Function_UnequipMeleeWeapon;
  lua["equipRangedWeapon"] = Function_EquipRangedWeapon;
  lua["unequipRangedWeapon"] = Function_UnequipRangedWeapon;
  lua["equipHelmet"] = Function_EquipHelmet;
  lua["unequipHelmet"] = Function_UnequipHelmet;
  lua["equipShield"] = Function_EquipShield;
  lua["unequipShield"] = Function_UnequipShield;
  lua["hasItem"] = Function_HasItem;
  lua["removeItem"] = Function_RemoveItem;

  lua["setServerWorld"] = Function_SetServerWorld;
  lua["getServerWorld"] = Function_GetServerWorld;

  lua["findNearbyPlayers"] = Function_FindNearbyPlayers;
  lua["getPlayersStreamedToPlayer"] = Function_GetPlayersStreamedToPlayer;
  lua["getPlayersStreamingPlayer"] = Function_GetPlayersStreamingPlayer;
  lua["setStreamerRadius"] = Function_SetStreamerRadius;
  lua["getStreamerRadius"] = Function_GetStreamerRadius;
  lua["setStreamerHeight"] = Function_SetStreamerHeight;
  lua["getStreamerHeight"] = Function_GetStreamerHeight;

  lua["setVoiceEnabled"] = Function_SetVoiceEnabled;
  lua["getVoiceEnabled"] = Function_GetVoiceEnabled;
  lua["setVoiceRange"] = Function_SetVoiceRange;
  lua["getVoiceRange"] = Function_GetVoiceRange;
  lua["setVoiceEnabledForPlayer"] = Function_SetVoiceEnabledForPlayer;
  lua["getVoiceEnabledForPlayer"] = Function_GetVoiceEnabledForPlayer;

  lua["setTime"] = Function_SetTime;
  lua["getTime"] = Function_GetTime;
  lua["setDayLength"] = Function_SetDayLength;
  lua["getDayLength"] = Function_GetDayLength;
}
